#include <QApplication>
#include "MainWindow.h"
#include "TaskSelectionDialog.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Med YOLO11 Qt");
    QApplication::setOrganizationName("YourLab");

    // 显示任务选择对话框
    TaskSelectionDialog taskDialog;
    if (taskDialog.exec() != QDialog::Accepted)
        return 0;

    MainWindow w;
    w.setTaskType(taskDialog.selectedTask());
    w.resize(1280, 860);
    w.show();

    // 根据选择的任务类型设置窗口标题
    if (taskDialog.selectedTask() == TaskSelectionDialog::MRI_Segmentation)
        w.setWindowTitle("Med YOLO11 Qt - MRI分割模式");
    else
        w.setWindowTitle("Med YOLO11 Qt - X光检测模式");

    return app.exec();
}