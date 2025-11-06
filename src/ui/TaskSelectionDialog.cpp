#include "TaskSelectionDialog.h"
#include <QApplication>
#include <QStyle>

TaskSelectionDialog::TaskSelectionDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("选择任务类型");
    setFixedSize(400, 300);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(20);
    layout->setContentsMargins(30, 30, 30, 30);

    auto *titleLabel = new QLabel("请选择要执行的任务类型", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold;");

    auto *faiButton = new QPushButton("X光检测任务", this);
    faiButton->setFixedHeight(60);
    faiButton->setStyleSheet(R"(
        QPushButton {
            font-size: 14px;
            font-weight: bold;
            background: #3498db;
            color: white;
            border: none;
            border-radius: 10px;
        }
        QPushButton:hover {
            background: #2980b9;
        }
        QPushButton:pressed {
            background: #21618c;
        }
    )");

    auto *mriButton = new QPushButton("MRI分割任务", this);
    mriButton->setFixedHeight(60);
    mriButton->setStyleSheet(R"(
        QPushButton {
            font-size: 14px;
            font-weight: bold;
            background: #e74c3c;
            color: white;
            border: none;
            border-radius: 10px;
        }
        QPushButton:hover {
            background: #c0392b;
        }
        QPushButton:pressed {
            background: #922b21;
        }
    )");

    layout->addWidget(titleLabel);
    layout->addWidget(faiButton);
    layout->addWidget(mriButton);
    layout->addStretch();

    connect(faiButton, &QPushButton::clicked, this, &TaskSelectionDialog::onFAISelected);
    connect(mriButton, &QPushButton::clicked, this, &TaskSelectionDialog::onMRISelected);
}

void TaskSelectionDialog::onFAISelected()
{
    m_selectedTask = FAI_XRay;
    accept();
}

void TaskSelectionDialog::onMRISelected()
{
    m_selectedTask = MRI_Segmentation;
    accept();
}