#pragma once
#include <QTableWidget>
#include <QMap>
#include <QString>
class MetaTable : public QTableWidget
{
    Q_OBJECT
public:
    explicit MetaTable(QWidget *parent = nullptr);
    void setData(const QMap<QString, QString> &kv);
    void clearAll();
};
