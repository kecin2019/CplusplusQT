#pragma once
#include <QImage>
#include <QMap>
#include <QString>

namespace DicomUtils
{
    struct SliceInfo
    {
        double spacingX{1.0}; // 列方向（毫米/像素）
        double spacingY{1.0}; // 行方向（毫米/像素）
        double sliceLocation{0.0};
        int instanceNumber{0};
        QString seriesInstanceUID;
    };

    bool loadDicomToQImage(const QString &path,
                           QImage &out,
                           QMap<QString, QString> *meta = nullptr,
                           SliceInfo *info = nullptr);

    bool probeSliceInfo(const QString &path, SliceInfo &info);
}
