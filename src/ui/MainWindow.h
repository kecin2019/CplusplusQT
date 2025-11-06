#pragma once
#include <QImage>
#include <QMainWindow>
#include <QSplitter>
#include <QList>
#include <QMap>
#include <QLineEdit>
#include <QFutureWatcher>

#include "InferenceEngine.h"     // 新增：为了用 Detection
#include "TaskSelectionDialog.h" // 新增：任务选择对话框

class QAction;
class QTabWidget;
class ImageView;
// class QPlainTextEdit;
class MetaTable;
class QListWidget;
class QProgressDialog;
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void setTaskType(TaskSelectionDialog::TaskType taskType);
    // —— 缓存每个文件的推理结果 ——
    QMap<QString, QImage> m_cacheImg;
    QMap<QString, std::vector<InferenceEngine::Detection>> m_cacheDets;

private slots:
    void openImage();
    void openDicom();
    void openFolder();
    void onListActivated();
    void runInference();
    void loadFAIModel();
    void loadMRIModel(); // 新增：加载MRI分割模型
    void clearAll();
    void exportCurrent();     // 新增：导出当前
    void exportBatch();       // 新增：批量导出
    void runBatchInference(); // 新增：批量推理
    void switchTask();        // 新增：切换任务

private:
    void setupUi();
    void log(const QString &s);
    void setInputImage(const QImage &img);
    void setOutputImage(const QImage &img);
    void updateMetaTable(const QMap<QString, QString> &meta);
    void loadPath(const QString &path);
    static bool isImageFile(const QString &path);
    static bool isDicomFile(const QString &path);
    void refreshActionStates();
    void handleSingleInferenceFinished();
    void handleBatchInferenceFinished();
    void setBusyState(bool busy, const QString &message, int maximum = 0);
    void updateProgressValue(int value, int maximum);

    QSplitter *m_splitter{nullptr};
    QTabWidget *m_viewTabs{nullptr};
    ImageView *m_inputView{nullptr};
    ImageView *m_outputView{nullptr};

    MetaTable *m_meta{nullptr};
    QListWidget *m_list{nullptr};
    QImage m_input;
    QString m_currentPath;
    QStringList m_batch;
    QString m_faiOnnxPath;
    QString m_mriOnnxPath;

    bool m_modelReady{false};    // 新增：模型是否已成功加载
    bool m_mriModelReady{false}; // 新增：MRI分割模型是否已加载
    QAction *m_actRun{nullptr};
    QAction *m_actBatch{nullptr};
    QAction *m_actExport{nullptr};
    QAction *m_actExportAll{nullptr};
    QAction *m_actLoadFAI{nullptr}; // 新增：加载FAI模型按钮
    QAction *m_actLoadMRI{nullptr}; // 新增：加载MRI模型按钮

    InferenceEngine m_engine;                           // 复用同一会话
    InferenceEngine m_mriEngine;                        // 新增：MRI分割引擎
    QImage m_output;                                    // 最近一次输出图
    std::vector<InferenceEngine::Detection> m_lastDets; // 最近一次检测
    QImage m_segmentationMask;                          // �������ָ�����ͼ��
    QProgressDialog *m_progressDialog{nullptr};
    bool m_isInferenceRunning{false};
    bool m_isBatchRunning{false};
    QString m_pendingInferencePath;
    InferenceEngine::Task m_pendingTask{InferenceEngine::Task::Auto};
    struct BatchItem
    {
        QString path;
        bool success{false};
        InferenceEngine::Result result;
        QString error;
    };
    QFutureWatcher<InferenceEngine::Result> m_singleWatcher;
    QFutureWatcher<std::vector<BatchItem>> m_batchWatcher;
    static bool saveJson(const QString &jsonPath, // 保存 JSON 小工具
                         const QString &srcPath,
                         const QSize &imgSize,
                         const std::vector<InferenceEngine::Detection> &dets);
};


