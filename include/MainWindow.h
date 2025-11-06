#pragma once
#include <QMainWindow>
#include <memory>

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
class QToolBar;
class QStatusBar;
class QDockWidget;
QT_END_NAMESPACE

namespace medyolo11qt
{
    namespace core
    {
        class AppConfig;
    }
    namespace ai
    {
        class InferenceEngine;
    }
}

namespace medyolo11qt::ui
{

    class ImageView;
    class MetaTable;
    class TaskSelectionDialog;

    /**
     * @brief 主窗口类
     * 应用程序的主界面，负责整体布局和用户交互
     */
    class MainWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        explicit MainWindow(QWidget *parent = nullptr);
        ~MainWindow() override;

    protected:
        void closeEvent(QCloseEvent *event) override;
        void dragEnterEvent(QDragEnterEvent *event) override;
        void dropEvent(QDropEvent *event) override;

    private slots:
        void openImage();
        void openDicom();
        void saveResults();
        void selectTask();
        void runInference();
        void showSettings();
        void about();
        void updateRecentFiles();
        void openRecentFile();
        void clearRecentFiles();
        void toggleToolbar();
        void toggleStatusbar();
        void zoomIn();
        void zoomOut();
        void zoomFit();
        void zoomOriginal();

    private:
        void createActions();
        void createMenus();
        void createToolbars();
        void createStatusBar();
        void createDockWindows();
        void readSettings();
        void writeSettings();
        void loadFile(const QString &fileName);
        bool saveFile(const QString &fileName);
        void setCurrentFile(const QString &fileName);
        void updateWindowTitle();
        void updateUIState();
        void showInferenceResults(const QString &results);

        // UI 组件
        ImageView *m_imageView;
        MetaTable *m_metaTable;
        TaskSelectionDialog *m_taskDialog;

        // 菜单
        QMenu *m_fileMenu;
        QMenu *m_editMenu;
        QMenu *m_viewMenu;
        QMenu *m_toolsMenu;
        QMenu *m_helpMenu;
        QMenu *m_recentFilesMenu;

        // 工具栏
        QToolBar *m_fileToolbar;
        QToolBar *m_viewToolbar;
        QToolBar *m_aiToolbar;

        // 状态栏
        QStatusBar *m_statusBar;

        // 停靠窗口
        QDockWidget *m_metaDock;
        QDockWidget *m_resultsDock;

        // 动作
        QAction *m_openAct;
        QAction *m_openDicomAct;
        QAction *m_saveAct;
        QAction *m_exitAct;
        QAction *m_selectTaskAct;
        QAction *m_runInferenceAct;
        QAction *m_settingsAct;
        QAction *m_aboutAct;
        QAction *m_aboutQtAct;
        QAction *m_zoomInAct;
        QAction *m_zoomOutAct;
        QAction *m_zoomFitAct;
        QAction *m_zoomOriginalAct;
        QAction *m_toggleToolbarAct;
        QAction *m_toggleStatusbarAct;
        QAction *m_clearRecentAct;

        // 最近文件动作列表
        QList<QAction *> m_recentFileActs;

        // 数据成员
        std::unique_ptr<core::AppConfig> m_config;
        std::unique_ptr<ai::InferenceEngine> m_inferenceEngine;
        QString m_currentFile;
        QString m_currentTask;
        bool m_inferenceRunning;

        // 常量
        static const int MaxRecentFiles = 10;
    };

} // namespace medyolo11qt::ui