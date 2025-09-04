#include "MetaTable.h"
#include <QHeaderView>
MetaTable::MetaTable(QWidget *parent) : QTableWidget(parent)
{
    setColumnCount(2);
    setHorizontalHeaderLabels({"Key", "Value"});
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    verticalHeader()->setVisible(false);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
}
void MetaTable::setData(const QMap<QString, QString> &kv)
{
    setRowCount(0);
    int row = 0;
    for (auto it = kv.constBegin(); it != kv.constEnd(); ++it)
    {
        insertRow(row);
        setItem(row, 0, new QTableWidgetItem(it.key()));
        setItem(row, 1, new QTableWidgetItem(it.value()));
        ++row;
    }
}
void MetaTable::clearAll() { setRowCount(0); }
