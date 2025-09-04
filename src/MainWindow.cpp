#include "MainWindow.h"
#include "ImageView.h"
#include "InferenceEngine.h"
#include "DicomUtils.h"
#include "MetaTable.h"
#include <QToolBar>
#include <QFileDialog>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QVBoxLayout>
#include <QStatusBar>
#include <QAction>
#include <QMessageBox>
#include <QLabel>
#include <QDockWidget>
#include <QListWidget>
#include <QDirIterator>
#include <QFileInfo>
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) { setupUi(); }
void MainWindow::setupUi()
{
    auto *tb = addToolBar("Main");
    auto *actOpenImg = tb->addAction("Open Image");
    auto *actOpenDicom = tb->addAction("Open DICOM");
    auto *actOpenFolder = tb->addAction("Open Folder");
    tb->addSeparator();
    auto *actLoadFAI = tb->addAction("Load FAI ONNX");
    auto *actRun = tb->addAction("Run Inference");
    tb->addSeparator();
    auto *actClear = tb->addAction("Clear");
    connect(actOpenImg, &QAction::triggered, this, &MainWindow::openImage);
    connect(actOpenDicom, &QAction::triggered, this, &MainWindow::openDicom);
    connect(actOpenFolder, &QAction::triggered, this, &MainWindow::openFolder);
    connect(actLoadFAI, &QAction::triggered, this, &MainWindow::loadFAIModel);
    connect(actRun, &QAction::triggered, this, &MainWindow::runInference);
    connect(actClear, &QAction::triggered, this, &MainWindow::clearAll);
    m_splitter = new QSplitter(this);
    m_viewTabs = new QTabWidget(this);
    m_inputView = new ImageView(this);
    m_outputView = new ImageView(this);
    m_viewTabs->addTab(m_inputView, "Input");
    m_viewTabs->addTab(m_outputView, "Output");
    auto *rightPanel = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(6, 6, 6, 6);
    rightLayout->setSpacing(6);
    m_meta = new MetaTable(this);
    auto *metaLabel = new QLabel("Metadata (DICOM tags or file info)", this);
    rightLayout->addWidget(metaLabel);
    rightLayout->addWidget(m_meta, 1);
    auto *logLabel = new QLabel("Log", this);
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    rightLayout->addWidget(logLabel);
    rightLayout->addWidget(m_log, 1);
    m_splitter->addWidget(m_viewTabs);
    m_splitter->addWidget(rightPanel);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 2);
    setCentralWidget(m_splitter);
    statusBar()->showMessage("Ready");
    auto *dock = new QDockWidget("Batch Files", this);
    m_list = new QListWidget(dock);
    dock->setWidget(m_list);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &MainWindow::onListActivated);
}
void MainWindow::openImage()
{
    QString f = QFileDialog::getOpenFileName(this, "Open Image", QString(),
                                             "Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)");
    if (f.isEmpty())
        return;
    loadPath(f);
}
void MainWindow::openDicom()
{
    QString f = QFileDialog::getOpenFileName(this, "Open DICOM", QString(),
                                             "DICOM (*.dcm);;All files (*.*)");
    if (f.isEmpty())
        return;
    loadPath(f);
}
void MainWindow::openFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Open Folder");
    if (dir.isEmpty())
        return;
    m_batch.clear();
    m_list->clear();
    QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        QString path = it.next();
        if (isImageFile(path) || isDicomFile(path))
            m_batch << path;
    }
    m_batch.sort();
    for (const auto &p : m_batch)
        m_list->addItem(p);
    log(QString("Folder loaded: %1 files").arg(m_batch.size()));
    if (!m_batch.isEmpty())
    {
        m_list->setCurrentRow(0);
        loadPath(m_batch.first());
    }
}
void MainWindow::onListActivated()
{
    auto items = m_list->selectedItems();
    if (items.isEmpty())
        return;
    loadPath(items.first()->text());
}
void MainWindow::runInference()
{
    if (m_input.isNull())
    {
        QMessageBox::information(this, "Run Inference", "Please load an image first.");
        return;
    }
    InferenceEngine engine;
    if (!m_faiOnnxPath.isEmpty())
        engine.loadModel(m_faiOnnxPath, InferenceEngine::Task::FAI_XRay);
    auto result = engine.run(m_input, InferenceEngine::Task::FAI_XRay);
    setOutputImage(result.outputImage);
    log(QString("Inference: %1").arg(result.summary));
    statusBar()->showMessage("Inference done");
}
void MainWindow::loadFAIModel()
{
    QString f = QFileDialog::getOpenFileName(this, "Select FAI ONNX", QString(),
                                             "ONNX model (*.onnx);;All files (*.*)");
    if (f.isEmpty())
        return;
    m_faiOnnxPath = f;
    log(QString("FAI ONNX set: %1").arg(f));
}
void MainWindow::clearAll()
{
    m_input = QImage();
    m_inputView->clearImage();
    m_outputView->clearImage();
    m_meta->clearAll();
    m_log->clear();
    m_currentPath.clear();
    m_batch.clear();
    m_list->clear();
    statusBar()->showMessage("Cleared");
}
void MainWindow::log(const QString &s) { m_log->appendPlainText(s); }
void MainWindow::setInputImage(const QImage &img)
{
    m_input = img;
    m_inputView->setImage(img);
    m_viewTabs->setCurrentWidget(m_inputView);
}
void MainWindow::setOutputImage(const QImage &img)
{
    m_outputView->setImage(img);
    m_viewTabs->setCurrentWidget(m_outputView);
}
void MainWindow::updateMetaTable(const QMap<QString, QString> &meta) { m_meta->setData(meta); }
bool MainWindow::isImageFile(const QString &path)
{
    QString ext = QFileInfo(path).suffix().toLower();
    return (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "tif" || ext == "tiff");
}
bool MainWindow::isDicomFile(const QString &path) { return QFileInfo(path).suffix().toLower() == "dcm"; }
void MainWindow::loadPath(const QString &path)
{
    if (isImageFile(path))
    {
        QImage img(path);
        if (img.isNull())
        {
            QMessageBox::warning(this, "Open Image", "Failed to load image.");
            return;
        }
        m_currentPath = path;
        setInputImage(img);
        QMap<QString, QString> meta;
        meta["Type"] = "Image";
        meta["Path"] = path;
        meta["Size"] = QString("%1 x %2").arg(img.width()).arg(img.height());
        updateMetaTable(meta);
        log(QString("Loaded image: %1").arg(path));
    }
    else if (isDicomFile(path))
    {
        QImage img;
        QMap<QString, QString> meta;
#ifdef HAVE_GDCM
        if (!DicomUtils::loadDicomToQImage(path, img, &meta))
        {
            QMessageBox::warning(this, "Open DICOM", "Failed to load DICOM.");
            return;
        }
#else
        QMessageBox::information(this, "DICOM disabled", "Built without GDCM. Reconfigure with -DUSE_GDCM=ON.");
        return;
#endif
        m_currentPath = path;
        setInputImage(img);
        updateMetaTable(meta);
        log(QString("Loaded DICOM: %1").arg(path));
    }
    else
    {
        QMessageBox::warning(this, "Open", "Unsupported file type.");
    }
}
