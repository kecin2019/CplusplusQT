#pragma once

#include <QFutureWatcher>
#include <QImage>
#include <QLineEdit>
#include <QList>
#include <QMainWindow>
#include <QMap>
#include <QStringList>
#include <QVector>

#include "InferenceEngine.h"
#include "TaskSelectionDialog.h"
#include "medical/DicomUtils.h"

class QAction;
class QLabel;
class QDockWidget;
class QPlainTextEdit;
class QProgressBar;
class QProgressDialog;
class QTabWidget;
class QTableWidget;
class ImageView;
class MetaTable;
class QListWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void setTaskType(TaskSelectionDialog::TaskType taskType);
    QMap<QString, QImage> m_cacheImg;
    QMap<QString, std::vector<InferenceEngine::Detection>> m_cacheDets;
    QMap<QString, QImage> m_cacheSegMasks;

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
    void toggleLogDock(bool checked);

private:
    void setupUi();
    void createToolBar();
    void createCentralViews();
    void createDockWidgets();
    void createStatusBar();
    void applyPalette();
    void applyStyleSheet(const QString &resourcePath);
    void appendLog(const QString &message);
    void log(const QString &s);
    void updateTaskUi(TaskSelectionDialog::TaskType taskType);
    void updateStatusSummary();
    void setInputImage(const QImage &img);
    void setOutputImage(const QImage &img, bool focusOutput = true);
    void updateMetaTable(const QMap<QString, QString> &meta);
    void loadPath(const QString &path);
    bool promptForModelFile(QString &path, const QString &title) const;
    static bool isImageFile(const QString &path);
    static bool isDicomFile(const QString &path);
    void refreshActionStates();
    void handleSingleInferenceFinished();
    void handleBatchInferenceFinished();
    void setBusyState(bool busy, const QString &message, int maximum = 0);
    void updateProgressValue(int value, int maximum);
    void updateSliceNavigationState();
    void updateSliceIndicator();
    void positionSliceIndicator();
    void handleSliceStep(int steps);
    void displayDicomSlice(int index);
    void setupDicomSeries(const QString &path, const DicomUtils::SliceInfo &info);
    void clearDicomSeries();
    void postProcessSegmentationResult(InferenceEngine::Result &result);
    double currentPixelArea() const;
    QString segmentationLabel(int cls) const;
    void annotateSegmentationImage(QImage &img,
                                   const std::vector<InferenceEngine::Detection> &dets,
                                   double areaFactor) const;
    void annotateCachedSegmentationIfNeeded(const QString &path);
    void updateSegmentationStats(const std::vector<InferenceEngine::Detection> &dets);
    void clearSegmentationStats();
    bool isModelReadyForTask(TaskSelectionDialog::TaskType task) const;
    bool ensureModelReadyForCurrentTask(const QString &contextTitle);
    void unloadModelForTask(TaskSelectionDialog::TaskType task);
    InferenceEngine::Task inferenceTaskForMode(TaskSelectionDialog::TaskType task) const;
    QString taskDisplayName(TaskSelectionDialog::TaskType task) const;
    void resizeEvent(QResizeEvent *event) override;

    QTabWidget *m_viewTabs{nullptr};
    ImageView *m_inputView{nullptr};
    ImageView *m_outputView{nullptr};

    MetaTable *m_meta{nullptr};
    QListWidget *m_list{nullptr};
    QLineEdit *m_batchFilter{nullptr};
    QPlainTextEdit *m_logView{nullptr};
    QDockWidget *m_metadataDock{nullptr};
    QDockWidget *m_batchDock{nullptr};
    QDockWidget *m_logDock{nullptr};
    QDockWidget *m_segStatsDock{nullptr};
    QTableWidget *m_segStats{nullptr};

    QLabel *m_statusMode{nullptr};
    QLabel *m_statusModel{nullptr};
    QProgressBar *m_statusProgress{nullptr};

    QImage m_input;
    QString m_currentPath;
    QStringList m_batch;
    QString m_faiOnnxPath;
    QString m_mriOnnxPath;

    bool m_modelReady{false};
    bool m_mriModelReady{false};
    bool m_isInferenceRunning{false};
    bool m_isBatchRunning{false};

    QAction *m_actRun{nullptr};
    QAction *m_actBatch{nullptr};
    QAction *m_actExport{nullptr};
    QAction *m_actExportAll{nullptr};
    QAction *m_actLoadFAI{nullptr};
    QAction *m_actLoadMRI{nullptr};
    QAction *m_actToggleLog{nullptr};

    TaskSelectionDialog::TaskType m_currentTask{TaskSelectionDialog::FAI_XRay};
    InferenceEngine m_engine;
    InferenceEngine m_mriEngine;
    QImage m_output;
    std::vector<InferenceEngine::Detection> m_lastDets;
    QImage m_segmentationMask;
    QProgressDialog *m_progressDialog{nullptr};
    QString m_pendingInferencePath;
    InferenceEngine::Task m_pendingTask{InferenceEngine::Task::Auto};
    InferenceEngine::Task m_lastBatchTask{InferenceEngine::Task::Auto};
    QLabel *m_sliceIndicator{nullptr};
    struct DicomSliceEntry
    {
        QString path;
        int instanceNumber{0};
        double sliceLocation{0.0};
    };
    QVector<DicomSliceEntry> m_dicomSeries;
    int m_currentSliceIndex{-1};
    double m_spacingX{1.0};
    double m_spacingY{1.0};
    QString m_currentSeriesUid;

    struct BatchItem
    {
        QString path;
        bool success{false};
        InferenceEngine::Result result;
        QString error;
    };

    QFutureWatcher<InferenceEngine::Result> m_singleWatcher;
    QFutureWatcher<std::vector<BatchItem>> m_batchWatcher;

    static bool saveJson(const QString &jsonPath,
                         const QString &srcPath,
                         const QSize &imgSize,
                         const std::vector<InferenceEngine::Detection> &dets);
};
