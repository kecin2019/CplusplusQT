#include "DicomUtils.h"
#include <QDebug>
#include <QVector>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <limits>

#ifdef HAVE_GDCM
#include <gdcmImageReader.h>
#include <gdcmReader.h>
#include <gdcmImage.h>
#include <gdcmPixelFormat.h>
#include <gdcmAttribute.h>
#include <gdcmPhotometricInterpretation.h>
#include <gdcmDataSet.h>
#include <gdcmTag.h>
#endif

#ifdef HAVE_GDCM
static bool readFirstNumber(const gdcm::DataSet &ds, uint16_t g, uint16_t e, double &outVal)
{
    gdcm::Tag t(g, e);
    if (!ds.FindDataElement(t))
        return false;
    const gdcm::DataElement &de = ds.GetDataElement(t);
    const gdcm::ByteValue *bv = de.GetByteValue();
    if (!bv)
        return false;
    std::string s(bv->GetPointer(), bv->GetLength());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\0' || s.back() == '\r' || s.back() == '\n'))
        s.pop_back();
    size_t p = s.find('\\');
    if (p != std::string::npos)
        s = s.substr(0, p);
    try
    {
        outVal = std::stod(s);
        return true;
    }
    catch (...)
    {
        return false;
    }
}
#endif

// no additional processing namespace needed
#ifdef HAVE_GDCM
QString readStringTag(const gdcm::DataSet &ds, uint16_t group, uint16_t element)
{
    gdcm::Tag tag(group, element);
    if (!ds.FindDataElement(tag))
        return {};
    const gdcm::DataElement &de = ds.GetDataElement(tag);
    const gdcm::ByteValue *bv = de.GetByteValue();
    if (!bv)
        return {};
    std::string s(bv->GetPointer(), bv->GetLength());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\0' || s.back() == '\r' || s.back() == '\n'))
        s.pop_back();
    return QString::fromStdString(s);
}

void parsePixelSpacing(const QString &spacingStr, double &rowSpacing, double &colSpacing)
{
    if (spacingStr.isEmpty())
        return;
    QString cleaned = spacingStr;
    cleaned.replace(',', ' ');
    QStringList parts = cleaned.split('\\');
    if (parts.size() < 2)
        parts = cleaned.split(' ', Qt::SkipEmptyParts);
    if (parts.size() >= 2)
    {
        bool okRow = false;
        bool okCol = false;
        double row = parts[0].toDouble(&okRow);
        double col = parts[1].toDouble(&okCol);
        if (okRow && row > 0)
            rowSpacing = row;
        if (okCol && col > 0)
            colSpacing = col;
    }
}

DicomUtils::SliceInfo extractSliceInfo(const gdcm::DataSet &ds)
{
    DicomUtils::SliceInfo info;
    info.seriesInstanceUID = readStringTag(ds, 0x0020, 0x000E);
    QString spacing = readStringTag(ds, 0x0028, 0x0030);
    parsePixelSpacing(spacing, info.spacingY, info.spacingX);
    double instance = 0.0;
    if (readFirstNumber(ds, 0x0020, 0x0013, instance))
        info.instanceNumber = static_cast<int>(std::round(instance));
    double loc = 0.0;
    if (readFirstNumber(ds, 0x0020, 0x1041, loc))
        info.sliceLocation = loc;
    else
    {
        QString ipp = readStringTag(ds, 0x0020, 0x0032);
        const QStringList parts = ipp.split('\\');
        if (parts.size() == 3)
        {
            bool ok = false;
            double z = parts[2].toDouble(&ok);
            if (ok)
                info.sliceLocation = z;
        }
    }
    return info;
}
#endif

namespace DicomUtils
{

    static QImage toQImage8bit(const uint8_t *data, int w, int h)
    {
        QImage img(w, h, QImage::Format_Indexed8);
        QVector<QRgb> table(256);
        for (int i = 0; i < 256; ++i)
            table[i] = qRgb(i, i, i);
        img.setColorTable(table);
        for (int y = 0; y < h; ++y)
        {
            std::memcpy(img.scanLine(y), data + y * w, static_cast<size_t>(w));
        }
        return img;
    }

#ifdef HAVE_GDCM
    bool loadDicomToQImage(const QString &path, QImage &out, QMap<QString, QString> *meta, SliceInfo *info)
    {
        gdcm::ImageReader ir;
        ir.SetFileName(path.toStdString().c_str());
        if (!ir.Read())
        {
            qWarning() << "GDCM: read failed:" << path;
            return false;
        }

        const gdcm::Image &gimg = ir.GetImage();
        const unsigned int *dims = gimg.GetDimensions();
        const int w = static_cast<int>(dims[0]);
        const int h = static_cast<int>(dims[1]);
        if (w <= 0 || h <= 0)
        {
            qWarning() << "GDCM: invalid image size";
            return false;
        }

        const size_t pixelCount = static_cast<size_t>(w) * static_cast<size_t>(h);
        const size_t bufferLen = gimg.GetBufferLength();
        const gdcm::PixelFormat &pf = gimg.GetPixelFormat();
        const int bitsAlloc = pf.GetBitsAllocated();
        const bool is16 = bitsAlloc > 8;
        const bool isSigned = (pf.GetPixelRepresentation() == 1);
        const unsigned int spp = std::max(1u, static_cast<unsigned int>(pf.GetSamplesPerPixel()));
        const unsigned int planar = gimg.GetPlanarConfiguration();
        const gdcm::PhotometricInterpretation::PIType pi =
            gimg.GetPhotometricInterpretation().GetType();

        std::vector<uint16_t> buffer16;
        std::vector<uint8_t> buffer8;
        if (is16)
        {
            buffer16.resize(bufferLen / sizeof(uint16_t));
            if (!gimg.GetBuffer(reinterpret_cast<char *>(buffer16.data())))
            {
                qWarning() << "GDCM: GetBuffer failed";
                return false;
            }
        }
        else
        {
            buffer8.resize(bufferLen);
            if (!gimg.GetBuffer(reinterpret_cast<char *>(buffer8.data())))
            {
                qWarning() << "GDCM: GetBuffer failed";
                return false;
            }
        }

        const gdcm::DataSet &ds = ir.GetFile().GetDataSet();
        double wc = 0.0, ww = 0.0, slope = 1.0, inter = 0.0;

        (void)readFirstNumber(ds, 0x0028, 0x1050, wc);
        (void)readFirstNumber(ds, 0x0028, 0x1051, ww);

        if (ds.FindDataElement(gdcm::Tag(0x0028, 0x1053)))
        {
            gdcm::Attribute<0x0028, 0x1053> at;
            at.SetFromDataElement(ds.GetDataElement(gdcm::Tag(0x0028, 0x1053)));
            slope = at.GetValue();
        }
        if (ds.FindDataElement(gdcm::Tag(0x0028, 0x1052)))
        {
            gdcm::Attribute<0x0028, 0x1052> at;
            at.SetFromDataElement(ds.GetDataElement(gdcm::Tag(0x0028, 0x1052)));
            inter = at.GetValue();
        }

        auto readChannel = [&](size_t idx, unsigned int channel) -> double
        {
            if (channel >= spp)
                return 0.0;
            size_t offset = (planar == 0) ? idx * spp + channel
                                          : static_cast<size_t>(channel) * pixelCount + idx;
            if (is16)
            {
                if (offset >= buffer16.size())
                    return 0.0;
                const uint16_t raw = buffer16[offset];
                return isSigned ? static_cast<int16_t>(raw) : raw;
            }
            if (offset >= buffer8.size())
                return 0.0;
            return buffer8[offset];
        };

        auto sampleAt = [&](size_t idx) -> double
        {
            if (spp >= 3)
            {
                double r = readChannel(idx, 0);
                double g = readChannel(idx, 1);
                double b = readChannel(idx, 2);
                using PI = gdcm::PhotometricInterpretation;
                double y;
                if (pi == PI::YBR_FULL || pi == PI::YBR_FULL_422 || pi == PI::YBR_PARTIAL_422 ||
                    pi == PI::YBR_ICT || pi == PI::YBR_RCT)
                {
                    y = r;
                }
                else
                {
                    y = 0.299 * r + 0.587 * g + 0.114 * b;
                }
                return inter + slope * y;
            }

            double raw = is16 ? (isSigned ? static_cast<int16_t>(buffer16[idx]) : buffer16[idx])
                              : buffer8[idx];
            return inter + slope * raw;
        };

        double minSample = std::numeric_limits<double>::max();
        double maxSample = std::numeric_limits<double>::lowest();
        for (size_t idx = 0; idx < pixelCount; ++idx)
        {
            double v = sampleAt(idx);
            minSample = std::min(minSample, v);
            maxSample = std::max(maxSample, v);
        }

        double useWc = wc;
        double useWw = ww;
        if (!(useWw > 0.0))
        {
            useWw = maxSample - minSample;
            useWc = 0.5 * (maxSample + minSample);
            if (!(useWw > 0.0))
                useWw = 1.0;
        }

        const double low = useWc - useWw / 2.0;
        const double high = useWc + useWw / 2.0;

        auto mapToU8 = [&](double v) -> uint8_t
        {
            if (v <= low)
                return 0;
            if (v >= high)
                return 255;
            double norm = (v - low) / (high - low);
            int mapped = static_cast<int>(std::round(norm * 255.0));
            return static_cast<uint8_t>(std::clamp(mapped, 0, 255));
        };

        std::vector<uint8_t> outbuf(pixelCount);
        for (size_t idx = 0; idx < pixelCount; ++idx)
            outbuf[idx] = mapToU8(sampleAt(idx));

        if (pi == gdcm::PhotometricInterpretation::MONOCHROME1)
        {
            for (auto &v : outbuf)
                v = 255 - v;
        }

        out = toQImage8bit(outbuf.data(), w, h);
        // out = enhanceMonochrome(out);

        SliceInfo sliceInfo;
        sliceInfo.spacingX = 1.0;
        sliceInfo.spacingY = 1.0;
        sliceInfo.sliceLocation = 0.0;
        sliceInfo.instanceNumber = 0;
        sliceInfo.seriesInstanceUID.clear();
        sliceInfo = extractSliceInfo(ds);
        if (info)
            *info = sliceInfo;

        // -------- 元数据（可选）--------
        if (meta)
        {
            meta->insert("Modality", readStringTag(ds, 0x0008, 0x0060));
            meta->insert("PatientName", readStringTag(ds, 0x0010, 0x0010));
            meta->insert("StudyDate", readStringTag(ds, 0x0008, 0x0020));
            meta->insert("SeriesDescription", readStringTag(ds, 0x0008, 0x103E));
            meta->insert("WindowCenter", QString::number(wc));
            meta->insert("WindowWidth", QString::number(ww));
            meta->insert("RescaleSlope", QString::number(slope));
            meta->insert("RescaleIntercept", QString::number(inter));
            meta->insert("Size", QString("%1 x %2").arg(w).arg(h));
            meta->insert("SamplesPerPixel", QString::number(spp));
            meta->insert("PlanarConfiguration", QString::number(planar));
            meta->insert("BitsAllocated", QString::number(bitsAlloc));
            meta->insert("PixelRepresentation", isSigned ? "SIGNED" : "UNSIGNED");
            meta->insert("Photometric", QString::fromLatin1(gdcm::PhotometricInterpretation::GetPIString(pi)));
            meta->insert("Path", path);
            meta->insert("PixelSpacing", QString("%1 mm / %2 mm").arg(sliceInfo.spacingY, 0, 'f', 3).arg(sliceInfo.spacingX, 0, 'f', 3));
            meta->insert("InstanceNumber", QString::number(sliceInfo.instanceNumber));
            meta->insert("SliceLocation", QString::number(sliceInfo.sliceLocation, 'f', 3));
            if (!sliceInfo.seriesInstanceUID.isEmpty())
                meta->insert("SeriesInstanceUID", sliceInfo.seriesInstanceUID);
        }

        return true;
    }
#else
    bool loadDicomToQImage(const QString &path, QImage &out, QMap<QString, QString> *meta, SliceInfo *info)
    {
        Q_UNUSED(path);
        Q_UNUSED(out);
        Q_UNUSED(info);
        if (meta)
            meta->insert("Info", "Built without GDCM support");
        return false;
    }
#endif

#ifdef HAVE_GDCM
    bool probeSliceInfo(const QString &path, SliceInfo &info)
    {
        gdcm::Reader reader;
        reader.SetFileName(path.toStdString().c_str());
        if (!reader.Read())
            return false;
        info = extractSliceInfo(reader.GetFile().GetDataSet());
        return true;
    }
#else
    bool probeSliceInfo(const QString &path, SliceInfo &info)
    {
        Q_UNUSED(path);
        Q_UNUSED(info);
        return false;
    }
#endif

} // namespace DicomUtils
