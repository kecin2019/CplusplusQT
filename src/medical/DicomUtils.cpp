#include "DicomUtils.h"
#include <QDebug>
#include <QVector>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>
#include <functional>
#include <algorithm>

#ifdef HAVE_GDCM
#include <gdcmImageReader.h>
#include <gdcmImage.h>
#include <gdcmPixelFormat.h>
#include <gdcmAttribute.h>
#include <gdcmPhotometricInterpretation.h>
#include <gdcmDataSet.h>
#include <gdcmTag.h>
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
    // 读取形如 "40\80" 的 DS 标签，取第一个数字
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
        // 去尾部空白
        while (!s.empty() && (s.back() == ' ' || s.back() == '\0' || s.back() == '\r' || s.back() == '\n'))
            s.pop_back();
        // 取第一个值
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

#ifdef HAVE_GDCM
    bool loadDicomToQImage(const QString &path, QImage &out, QMap<QString, QString> *meta)
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

        // 取原始像素缓冲
        const size_t len = gimg.GetBufferLength();
        std::vector<char> buffer(len);
        if (!gimg.GetBuffer(buffer.data()))
        {
            qWarning() << "GDCM: GetBuffer failed";
            return false;
        }

        const gdcm::PixelFormat &pf = gimg.GetPixelFormat();
        const int bitsAlloc = pf.GetBitsAllocated(); // 8 或 16
        const bool is16 = (bitsAlloc > 8);
        const bool isSigned = (pf.GetPixelRepresentation() == 1);  // 0=UNSIGNED, 1=SIGNED
        const unsigned int spp = pf.GetSamplesPerPixel();          // 1/3
        const unsigned int planar = gimg.GetPlanarConfiguration(); // 0/1
        const gdcm::PhotometricInterpretation::PIType pi =
            gimg.GetPhotometricInterpretation().GetType();

        // 读取 WW/WC & 斜率/截距
        const gdcm::DataSet &ds = ir.GetFile().GetDataSet();
        double wc = 0.0, ww = 0.0, slope = 1.0, inter = 0.0;

        (void)readFirstNumber(ds, 0x0028, 0x1050, wc); // WindowCenter
        (void)readFirstNumber(ds, 0x0028, 0x1051, ww); // WindowWidth

        if (ds.FindDataElement(gdcm::Tag(0x0028, 0x1053)))
        { // RescaleSlope
            gdcm::Attribute<0x0028, 0x1053> at;
            at.SetFromDataElement(ds.GetDataElement(gdcm::Tag(0x0028, 0x1053)));
            slope = at.GetValue();
        }
        if (ds.FindDataElement(gdcm::Tag(0x0028, 0x1052)))
        { // RescaleIntercept
            gdcm::Attribute<0x0028, 0x1052> at;
            at.SetFromDataElement(ds.GetDataElement(gdcm::Tag(0x0028, 0x1052)));
            inter = at.GetValue();
        }

        // 像素获取器：返回 WL 前的线性值
        std::function<double(int)> fetch;

        // 自动窗口（当没有有效 WW/WC 时）
        auto ensureWindow = [&](auto getVal)
        {
            if (ww <= 0.0)
            {
                double minv = 1e300, maxv = -1e300;
                for (int i = 0; i < w * h; ++i)
                {
                    double v = getVal(i);
                    if (v < minv)
                        minv = v;
                    if (v > maxv)
                        maxv = v;
                }
                if (maxv > minv)
                {
                    wc = 0.5 * (minv + maxv);
                    ww = (maxv - minv);
                }
                else
                {
                    wc = 127.0;
                    ww = 255.0;
                }
            }
        };

        // 判断是否为 YBR 家族（常见：FULL / FULL_422 / PARTIAL_422 / ICT / RCT）
        auto isYBR = [&](gdcm::PhotometricInterpretation::PIType t) -> bool
        {
            using PI = gdcm::PhotometricInterpretation;
            return t == PI::YBR_FULL || t == PI::YBR_FULL_422 || t == PI::YBR_PARTIAL_422 || t == PI::YBR_ICT || t == PI::YBR_RCT;
        };

        if (spp == 1 || pi == gdcm::PhotometricInterpretation::PALETTE_COLOR)
        {
            // 单通道（最常见）或 Palette（当作索引灰度近似显示）
            if (is16)
            {
                const uint16_t *src = reinterpret_cast<const uint16_t *>(buffer.data());
                fetch = [&](int i) -> double
                {
                    double s = isSigned ? static_cast<int16_t>(src[i]) : static_cast<uint16_t>(src[i]);
                    return inter + slope * s;
                };
            }
            else
            {
                const uint8_t *src = reinterpret_cast<const uint8_t *>(buffer.data());
                fetch = [&](int i) -> double
                {
                    double s = static_cast<uint8_t>(src[i]);
                    return inter + slope * s;
                };
            }
        }
        else if (spp >= 3)
        {
            // 彩色：RGB / YBR_*   —— 转成灰度显示
            if (is16)
            {
                const uint16_t *src = reinterpret_cast<const uint16_t *>(buffer.data());
                fetch = [&](int i) -> double
                {
                    if (planar == 0)
                    {
                        // 交错：RGBRGB...
                        int off = i * 3;
                        double r = src[off + 0];
                        double g = src[off + 1];
                        double b = src[off + 2];
                        double y = isYBR(pi) ? r : (0.299 * r + 0.587 * g + 0.114 * b); // YBR: 第一个分量是 Y
                        return inter + slope * y;
                    }
                    else
                    {
                        // 平面：RRR... GGG... BBB...
                        size_t plane = static_cast<size_t>(w) * static_cast<size_t>(h);
                        double r = src[i];
                        double g = src[i + plane];
                        double b = src[i + 2 * plane];
                        double y = isYBR(pi) ? r : (0.299 * r + 0.587 * g + 0.114 * b);
                        return inter + slope * y;
                    }
                };
            }
            else
            {
                const uint8_t *src = reinterpret_cast<const uint8_t *>(buffer.data());
                fetch = [&](int i) -> double
                {
                    if (planar == 0)
                    {
                        int off = i * 3;
                        double r = src[off + 0];
                        double g = src[off + 1];
                        double b = src[off + 2];
                        double y = isYBR(pi) ? r : (0.299 * r + 0.587 * g + 0.114 * b);
                        return inter + slope * y;
                    }
                    else
                    {
                        size_t plane = static_cast<size_t>(w) * static_cast<size_t>(h);
                        double r = src[i];
                        double g = src[i + plane];
                        double b = src[i + 2 * plane];
                        double y = isYBR(pi) ? r : (0.299 * r + 0.587 * g + 0.114 * b);
                        return inter + slope * y;
                    }
                };
            }
        }
        else
        {
            // 其他稀有 spp，兜底按 1 通道处理
            const uint8_t *src = reinterpret_cast<const uint8_t *>(buffer.data());
            fetch = [&](int i) -> double
            { return inter + slope * double(src[i]); };
        }

        // 无 WW 的情况下自动窗口
        ensureWindow(fetch);

        // WL 映射到 0..255
        auto applyWL = [&](double val) -> uint8_t
        {
            if (ww <= 0.0)
            {
                return static_cast<uint8_t>(std::clamp(val, 0.0, 255.0));
            }
            const double low = wc - ww / 2.0;
            const double high = wc + ww / 2.0;
            if (val <= low)
                return 0;
            if (val >= high)
                return 255;
            return static_cast<uint8_t>(std::lround(((val - low) / (high - low)) * 255.0));
        };

        std::vector<uint8_t> outbuf(static_cast<size_t>(w) * static_cast<size_t>(h));
        for (int i = 0; i < w * h; ++i)
        {
            outbuf[static_cast<size_t>(i)] = applyWL(fetch(i));
        }

        // MONOCHROME1 需要反色
        if (pi == gdcm::PhotometricInterpretation::MONOCHROME1)
        {
            for (auto &v : outbuf)
                v = 255 - v;
        }

        out = toQImage8bit(outbuf.data(), w, h);

        // -------- 元数据（可选）--------
        if (meta)
        {
            auto getStr = [&](uint16_t g, uint16_t e) -> QString
            {
                gdcm::Tag t(g, e);
                if (!ds.FindDataElement(t))
                    return {};
                const gdcm::DataElement &de = ds.GetDataElement(t);
                const gdcm::ByteValue *bv = de.GetByteValue();
                if (!bv)
                    return {};
                std::string s(bv->GetPointer(), bv->GetLength());
                while (!s.empty() && (s.back() == ' ' || s.back() == '\0' || s.back() == '\r' || s.back() == '\n'))
                    s.pop_back();
                return QString::fromStdString(s);
            };

            meta->insert("Modality", getStr(0x0008, 0x0060));
            meta->insert("PatientName", getStr(0x0010, 0x0010));
            meta->insert("StudyDate", getStr(0x0008, 0x0020));
            meta->insert("SeriesDescription", getStr(0x0008, 0x103E));
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
        }

        return true;
    }
#else
    bool loadDicomToQImage(const QString &path, QImage &out, QMap<QString, QString> *meta)
    {
        Q_UNUSED(path);
        Q_UNUSED(out);
        if (meta)
            meta->insert("Info", "Built without GDCM support");
        return false;
    }
#endif

} // namespace DicomUtils
