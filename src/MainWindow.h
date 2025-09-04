#pragma once
#include <QMainWindow>
#include <QImage>
#include <QSplitter>
#include <QList>
class QAction;
class QTabWidget;
class ImageView;
class QPlainTextEdit;
class MetaTable;
class QListWidget;
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
private slots:
    void openImage();
    void openDicom();
    void openFolder();
    void onListActivated();
    void runInference();
    void loadFAIModel();
    void clearAll();

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
    QPlainTextEdit *m_log{nullptr};
    MetaTable *m_meta{nullptr};
    QListWidget *m_list{nullptr};
    QImage m_input;
    QString m_currentPath;
    QStringList m_batch;
    QString m_faiOnnxPath;
};
