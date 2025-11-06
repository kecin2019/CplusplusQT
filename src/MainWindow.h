#pragma once

#include <QFutureWatcher>
#include <QImage>
#include <QMainWindow>
#include <QMap>
#include <QSplitter>
#include <vector>

#include "InferenceEngine.h"
#include "ModelManager.h"
#include "TaskSelectionDialog.h"

class QAction;
class QDockWidget;
class QPlainTextEdit;
class QProgressBar;
class QSlider;
class QTabWidget;
class QWidgetAction;
class ImageView;
class MetaTable;
class QListWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void setTaskType(TaskSelectionDialog::TaskType taskType);
    QMap<QString, QImage> m_cacheImg;
    QMap<QString, std::vector<InferenceEngine::Detection>> m_cacheDets;
    QMap<QString, QImage> m_cacheSegmentation;

private slots:
    void openImage();
    void openDicom();
    void openFolder();
    void onListActivated();
    void runInference();
    void loadFAIModel();
    void loadMRIModel();
    void clearAll();
    void exportCurrent();
    void exportBatch();
    void runBatchInference();
    void switchTask();
    void onInferenceFinished();
    void updateOverlayOpacity(int value);
    void toggleLogDock(bool checked);

private:
    void setupUi();
    void setupDockWidgets();
    void setupConnections();
    void appendLog(const QString &line);
    void log(const QString &line);
    void setInputImage(const QImage &img);
    void setOutputImage(const QImage &img);
    void updateMetaTable(const QMap<QString, QString> &meta);
    void loadPath(const QString &path);
    static bool isImageFile(const QString &path);
    static bool isDicomFile(const QString &path);
    void updateTaskUi(TaskSelectionDialog::TaskType taskType);
    void toggleInferenceActions(bool enabled);
    bool ensureActiveModel();
    void startAsyncInference();
    void resetInferenceState();
    InferenceEngine &activeEngine();
    ModelManager::ModelType resolveModelType() const;

    QSplitter *m_splitter{nullptr};
    QTabWidget *m_viewTabs{nullptr};
    ImageView *m_inputView{nullptr};
    ImageView *m_outputView{nullptr};

    MetaTable *m_meta{nullptr};
    QListWidget *m_list{nullptr};
    QPlainTextEdit *m_logView{nullptr};
    QDockWidget *m_logDock{nullptr};
    QProgressBar *m_progress{nullptr};
    QSlider *m_overlaySlider{nullptr};
    QWidgetAction *m_overlayAction{nullptr};

    QImage m_input;
    QString m_currentPath;
    QStringList m_batch;

    bool m_modelReady{false};
    bool m_mriModelReady{false};
    QAction *m_actRun{nullptr};
    QAction *m_actBatch{nullptr};
    QAction *m_actExport{nullptr};
    QAction *m_actExportAll{nullptr};
    QAction *m_actLoadFAI{nullptr};
    QAction *m_actLoadMRI{nullptr};
    QAction *m_actLogDock{nullptr};

    InferenceEngine m_engine;
    InferenceEngine m_mriEngine;
    QImage m_output;
    std::vector<InferenceEngine::Detection> m_lastDets;
    QImage m_segmentationMask;
    TaskSelectionDialog::TaskType m_currentTask{TaskSelectionDialog::FAI_XRay};

    QFutureWatcher<InferenceEngine::Result> m_inferWatcher;
    bool m_isRunning{false};
    float m_overlayOpacity{1.0f};

    static bool saveJson(const QString &jsonPath,
                         const QString &srcPath,
                         const QSize &imgSize,
                         const std::vector<InferenceEngine::Detection> &dets);

    ModelManager m_modelManager;
};
