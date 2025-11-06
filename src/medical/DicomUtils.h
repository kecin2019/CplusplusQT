#pragma once
#include <QImage>
#include <QMap>
#include <QString>
namespace DicomUtils
{
    bool loadDicomToQImage(const QString &path, QImage &out, QMap<QString, QString> *meta = nullptr);
}
