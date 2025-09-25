#pragma once
#include <QDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>

class TaskSelectionDialog : public QDialog
{
    Q_OBJECT
public:
    enum TaskType
    {
        FAI_XRay,
        MRI_Segmentation
    };

    explicit TaskSelectionDialog(QWidget *parent = nullptr);

    TaskType selectedTask() const { return m_selectedTask; }

private slots:
    void onFAISelected();
    void onMRISelected();

private:
    TaskType m_selectedTask{FAI_XRay};
};