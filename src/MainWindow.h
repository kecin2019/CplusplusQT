#pragma once
#include <QMainWindow>
#include <QImage>
#include <QSplitter>
#include <QList>
#include <QMap>
#include <QLineEdit>

#include "InferenceEngine.h" // 新增：为了用 Detection

class QAction;
class QTabWidget;
class ImageView;
// class QPlainTextEdit;
class MetaTable;
class QListWidget;
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
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
    void clearAll();
    void exportCurrent();     // 新增：导出当前
    void exportBatch();       // 新增：批量导出
    void runBatchInference(); // 新增：批量推理

private:
    void setupUi();
    void log(const QString &s);
    void setInputImage(const QImage &img);
    void setOutputImage(const QImage &img);
    void updateMetaTable(const QMap<QString, QString> &meta);
    void loadPath(const QString &path);
    static bool isImageFile(const QString &path);
    static bool isDicomFile(const QString &path);

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

    bool m_modelReady{false}; // 新增：模型是否已成功加载
    QAction *m_actRun{nullptr};
    QAction *m_actBatch{nullptr};
    QAction *m_actExport{nullptr};
    QAction *m_actExportAll{nullptr};

    InferenceEngine m_engine;                           // 复用同一会话
    QImage m_output;                                    // 最近一次输出图
    std::vector<InferenceEngine::Detection> m_lastDets; // 最近一次检测

    static bool saveJson(const QString &jsonPath, // 保存 JSON 小工具
                         const QString &srcPath,
                         const QSize &imgSize,
                         const std::vector<InferenceEngine::Detection> &dets);
};
