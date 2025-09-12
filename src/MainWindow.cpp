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
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <qapplication.h>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) { setupUi(); }
void MainWindow::setupUi()
{
    auto *tb = addToolBar("Main");
    auto *actOpenImg = tb->addAction("Open Image");
    auto *actOpenDicom = tb->addAction("Open DICOM");
    auto *actOpenFolder = tb->addAction("Open Folder");

    tb->addSeparator();

    auto *actLoadFAI = tb->addAction("Load FAI ONNX");
    m_actRun = tb->addAction("Run Inference");

    tb->addSeparator();

    auto *actClear = tb->addAction("Clear");

    connect(actOpenImg, &QAction::triggered, this, &MainWindow::openImage);
    connect(actOpenDicom, &QAction::triggered, this, &MainWindow::openDicom);
    connect(actOpenFolder, &QAction::triggered, this, &MainWindow::openFolder);
    connect(actLoadFAI, &QAction::triggered, this, &MainWindow::loadFAIModel);
    connect(m_actRun, &QAction::triggered, this, &MainWindow::runInference);
    connect(actClear, &QAction::triggered, this, &MainWindow::clearAll);

    // ---- 中心区域：左图右信息 ----
    m_splitter = new QSplitter(this);
    m_viewTabs = new QTabWidget(this);
    m_inputView = new ImageView(this);
    m_outputView = new ImageView(this);
    m_viewTabs->addTab(m_inputView, "Input");
    m_viewTabs->addTab(m_outputView, "Output");

    // 右侧：上下分割（上=Metadata，下=文件列表）
    QSplitter *rightSplit = new QSplitter(Qt::Vertical, this);

    // --- 上：Metadata 面板 ---
    auto *metaPanel = new QWidget(this);
    auto *metaLayout = new QVBoxLayout(metaPanel);
    metaLayout->setContentsMargins(12, 12, 12, 12);
    metaLayout->setSpacing(8);
    auto *metaLabel = new QLabel("Metadata (DICOM tags or file info)", this);
    m_meta = new MetaTable(this);
    metaLayout->addWidget(metaLabel);
    metaLayout->addWidget(m_meta, 1);
    rightSplit->addWidget(metaPanel);

    // --- 下：Batch Files（从左侧 Dock 移到这里） ---
    auto *filesPanel = new QWidget(this);
    auto *filesLayout = new QVBoxLayout(filesPanel);
    filesLayout->setContentsMargins(12, 8, 12, 12);
    filesLayout->setSpacing(8);

    auto *filesHeader = new QHBoxLayout();
    auto *filesTitle = new QLabel("Batch Files", this);
    auto *filterEdit = new QLineEdit(this);
    filterEdit->setPlaceholderText("Filter...");
    filesHeader->addWidget(filesTitle);
    filesHeader->addStretch();
    filesHeader->addWidget(filterEdit);

    m_list = new QListWidget(this);
    filesLayout->addLayout(filesHeader);
    filesLayout->addWidget(m_list, 1);
    rightSplit->addWidget(filesPanel);

    // 简单的文件名过滤
    connect(filterEdit, &QLineEdit::textChanged, this, [this](const QString &s)
            {
    for (int i = 0; i < m_list->count(); ++i) {
        auto *it = m_list->item(i);
        it->setHidden(!it->text().contains(s, Qt::CaseInsensitive));
    } });
    // 保持原有选择回调
    connect(m_list, &QListWidget::itemSelectionChanged, this, &MainWindow::onListActivated);

    // 拼到主分割器
    m_splitter->addWidget(m_viewTabs);
    m_splitter->addWidget(rightSplit);

    // 让图片更大
    // 让左侧图像区域更大
    m_splitter->setStretchFactor(0, 7); // 左侧
    m_splitter->setStretchFactor(1, 3); // 右侧

    // 初始显示时就给出更大的左侧宽度（需在窗口显示后设置）
    QTimer::singleShot(0, this, [this]()
                       {
        const int w = this->width();
        m_splitter->setSizes({ int(w*0.75), int(w*0.25) }); });

    rightSplit->setStretchFactor(0, 2); // 上面板权重大些（Metadata）
    rightSplit->setStretchFactor(1, 1); // 下面板（文件列表）

    setCentralWidget(m_splitter);
    statusBar()->showMessage("Ready");

    m_actExport = tb->addAction("Export Result");
    m_actExportAll = tb->addAction("Batch Export");

    connect(m_actExport, &QAction::triggered, this, &MainWindow::exportCurrent);
    connect(m_actExportAll, &QAction::triggered, this, &MainWindow::exportBatch);

    m_actBatch = tb->addAction("Batch Infer");
    connect(m_actBatch, &QAction::triggered, this, &MainWindow::runBatchInference);

    // 初始：未加载模型时禁用推理/导出/批处理
    for (QAction *a : {m_actRun, m_actBatch, m_actExport, m_actExportAll})
        if (a)
            a->setEnabled(false);

    // 亮色 Fusion
    qApp->setStyle("Fusion");
    QPalette pal = qApp->palette(); // Fusion 默认就是亮色，这里只微调
    pal.setColor(QPalette::Highlight, QColor(52, 120, 246));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    qApp->setPalette(pal);

    // 轻量 QSS：圆角 + 浅边框 + 明亮背景
    setStyleSheet(R"(
QMainWindow { background:#F7F8FA; }
QToolBar { background:#F7F8FA; border:none; padding:6px; spacing:8px; }

QToolButton { border-radius:10px; padding:6px 10px; }
QToolButton:hover { background:#EBEEF2; }

QTabWidget::pane { border:1px solid #E4E7EC; border-radius:12px; padding:6px; background:#FFFFFF; }
QTabBar::tab {
  background:#FFFFFF; border:1px solid #E4E7EC; border-bottom:none;
  padding:6px 12px; margin-right:4px; border-top-left-radius:10px; border-top-right-radius:10px;
}
QTabBar::tab:selected { background:#F2F4F7; }

QListWidget, QTableWidget, QLineEdit, QPlainTextEdit {
  background:#FFFFFF; border:1px solid #E4E7EC; border-radius:12px; padding:8px;
}
QHeaderView::section {
  background:#F7F8FA; border:1px solid #E4E7EC; padding:6px; border-radius:8px;
}

QStatusBar { background:#F7F8FA; border-top:1px solid #E4E7EC; }
)");
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
    const QString sel = items.first()->text();

    loadPath(sel); // 会更新输入视图、元数据、m_currentPath

    if (m_cacheImg.contains(sel))
    {
        m_output = m_cacheImg.value(sel);
        m_lastDets = m_cacheDets.value(sel);
        setOutputImage(m_output);
    }
    else if (m_modelReady)
    {
        // 自动对当前文件推理一次
        runInference();
    }
    else
    {
        // 未加载模型且没有缓存：清空输出
        m_output = QImage();
        m_lastDets.clear();
        m_outputView->clearImage();
    }
}
void MainWindow::runInference()
{
    if (m_input.isNull())
    {
        QMessageBox::information(this, "Run Inference", "Please load an image first.");
        return;
    }

    if (!m_modelReady)
    {
        QMessageBox::information(this, "Run Inference", "Please load an ONNX model first.");
        return;
    }
    auto result = m_engine.run(m_input, InferenceEngine::Task::FAI_XRay);

    // —— 写入缓存（以当前路径为 key）——
    if (!m_currentPath.isEmpty())
    {
        m_cacheImg[m_currentPath] = result.outputImage;
        m_cacheDets[m_currentPath] = result.dets;
    }

    m_output = result.outputImage;
    m_lastDets = result.dets;

    setOutputImage(result.outputImage);
    log(QString("Inference: %1").arg(result.summary));
    statusBar()->showMessage("Inference done");
}

void MainWindow::runBatchInference()
{
    if (!m_modelReady)
    {
        QMessageBox::information(this, "Batch Export", "Please load an ONNX model first.");
        return;
    }

    if (!m_list || m_list->count() == 0)
    {
        QMessageBox::information(this, "Batch Infer", "Batch list is empty.");
        return;
    }
    int ok = 0, fail = 0;
    for (int i = 0; i < m_list->count(); ++i)
    {
        const QString path = m_list->item(i)->text();

        // 已有缓存则跳过（如需强制重跑，可清缓存）
        if (m_cacheImg.contains(path))
        {
            ++ok;
            continue;
        }

        QImage in;
        if (isDicomFile(path))
        {
#ifdef HAVE_GDCM
            QMap<QString, QString> dummy;
            if (!DicomUtils::loadDicomToQImage(path, in, &dummy))
            {
                ++fail;
                continue;
            }
#else
            ++fail;
            continue;
#endif
        }
        else if (isImageFile(path))
        {
            if (!in.load(path))
            {
                ++fail;
                continue;
            }
        }
        else
        {
            ++fail;
            continue;
        }

        auto res = m_engine.run(in, InferenceEngine::Task::FAI_XRay);
        m_cacheImg[path] = res.outputImage;
        m_cacheDets[path] = res.dets;
        ++ok;

        // UI 反馈（可选）：边跑边显示当前
        if (path == m_currentPath)
        {
            m_output = res.outputImage;
            m_lastDets = res.dets;
            setOutputImage(m_output);
        }
        qApp->processEvents();
    }
    QMessageBox::information(this, "Batch Infer",
                             QString("Done. Success: %1, Failed: %2").arg(ok).arg(fail));
}

void MainWindow::loadFAIModel()
{
    // 使用相对路径加载模型，避免嵌入大文件到资源中
    QString modelPath = QCoreApplication::applicationDirPath() + "/models/fai_xray.onnx";

    // 检查文件是否存在
    if (!QFileInfo::exists(modelPath))
    {
        QMessageBox::warning(this, "Load Model",
                             QString("Model file not found at: %1").arg(modelPath));
        return;
    }

    m_faiOnnxPath = modelPath;
    m_modelReady = m_engine.loadModel(m_faiOnnxPath, InferenceEngine::Task::FAI_XRay);
    if (!m_modelReady)
    {
        QMessageBox::warning(this, "Load Model", "Failed to load ONNX model.");
        return;
    }
    for (QAction *a : {m_actRun, m_actBatch, m_actExport, m_actExportAll})
        if (a)
            a->setEnabled(true);

    log(QString("FAI ONNX loaded: %1").arg(modelPath));
}

void MainWindow::clearAll()
{
    m_input = QImage();
    m_inputView->clearImage();
    m_outputView->clearImage();
    m_meta->clearAll();
    m_currentPath.clear();
    m_batch.clear();
    m_list->clear();
    statusBar()->showMessage("Cleared");
}

void MainWindow::log(const QString &s)
{
    statusBar()->showMessage(s, 5000);
    qDebug() << s;
}

void MainWindow::setInputImage(const QImage &img)
{
    m_input = img;
    m_inputView->setImage(img, true);
    m_viewTabs->setCurrentWidget(m_inputView);
}
void MainWindow::setOutputImage(const QImage &img)
{
    // 结果图不再重新 fit，直接保持与输入图相同缩放
    m_outputView->setImage(img, false);
    m_outputView->setTransform(m_inputView->transform());
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

void MainWindow::exportCurrent()
{
    if (m_output.isNull() || m_lastDets.empty())
    {
        QMessageBox::warning(this, "Export", "No inference result to export.");
        return;
    }
    QString defStem = m_currentPath.isEmpty() ? "result" : QFileInfo(m_currentPath).completeBaseName();
    QString defDir = m_currentPath.isEmpty() ? QDir::homePath() : QFileInfo(m_currentPath).absolutePath();
    QString outImg = QFileDialog::getSaveFileName(this, "Save Result Image",
                                                  defDir + "/" + defStem + "_pred.png",
                                                  "PNG Image (*.png);;JPEG Image (*.jpg *.jpeg)");
    if (outImg.isEmpty())
        return;

    if (!m_output.save(outImg))
    {
        QMessageBox::warning(this, "Export", "Failed to save image.");
        return;
    }
    QString outJson = QFileInfo(outImg).absolutePath() + "/" +
                      QFileInfo(outImg).completeBaseName() + ".json";
    if (!saveJson(outJson, m_currentPath, m_input.size(), m_lastDets))
    {
        QMessageBox::warning(this, "Export", "Failed to save JSON.");
        return;
    }
    statusBar()->showMessage("Exported: " + outImg + " & " + outJson, 5000);
}

void MainWindow::exportBatch()
{
    if (!m_list || m_list->count() == 0)
    {
        QMessageBox::information(this, "Batch Export", "Batch list is empty.");
        return;
    }

    if (!m_modelReady)
    {
        QMessageBox::information(this, "Batch Infer", "Please load an ONNX model first.");
        return;
    }

    QString outDir = QFileDialog::getExistingDirectory(this, "Select Output Folder");
    if (outDir.isEmpty())
        return;

    int ok = 0, fail = 0;
    for (int i = 0; i < m_list->count(); ++i)
    {
        QString path = m_list->item(i)->text();
        QImage in;
        if (isDicomFile(path))
        {
#ifdef HAVE_GDCM
            QMap<QString, QString> dummy;
            if (!DicomUtils::loadDicomToQImage(path, in, &dummy))
            {
                ++fail;
                continue;
            }
#else
            ++fail;
            continue;
#endif
        }
        else if (isImageFile(path))
        {
            if (!in.load(path))
            {
                ++fail;
                continue;
            }
        }
        else
        {
            ++fail;
            continue;
        }

        auto itI = m_cacheImg.find(path);
        auto itD = m_cacheDets.find(path);
        InferenceEngine::Result res;
        if (itI != m_cacheImg.end() && itD != m_cacheDets.end())
        {
            res.outputImage = itI.value();
            res.dets = itD.value();
        }
        else
        {
            res = m_engine.run(in, InferenceEngine::Task::FAI_XRay);
            m_cacheImg[path] = res.outputImage;
            m_cacheDets[path] = res.dets;
        }

        QString stem = QFileInfo(path).completeBaseName();
        QString outPng = outDir + "/" + stem + "_pred.png";
        QString outJs = outDir + "/" + stem + "_pred.json";

        if (!res.outputImage.save(outPng))
        {
            ++fail;
            continue;
        }
        if (!saveJson(outJs, path, in.size(), res.dets))
        {
            ++fail;
            continue;
        }
        ++ok;
    }
    QMessageBox::information(this, "Batch Export",
                             QString("Done. Success: %1, Failed: %2").arg(ok).arg(fail));
}

bool MainWindow::saveJson(const QString &jsonPath,
                          const QString &srcPath,
                          const QSize &imgSize,
                          const std::vector<InferenceEngine::Detection> &dets)
{
    QJsonObject root;
    QJsonObject image;
    image["path"] = srcPath;
    image["width"] = imgSize.width();
    image["height"] = imgSize.height();
    root["image"] = image;

    QJsonArray arr;
    for (const auto &d : dets)
    {
        QJsonObject o;
        o["class_id"] = d.cls;
        o["class_name"] = InferenceEngine::className(d.cls);
        o["score"] = d.score;
        QJsonObject b;
        b["x1"] = d.x1;
        b["y1"] = d.y1;
        b["x2"] = d.x2;
        b["y2"] = d.y2;
        b["w"] = d.x2 - d.x1;
        b["h"] = d.y2 - d.y1;
        o["bbox"] = b;
        arr.append(o);
    }
    root["predictions"] = arr;

    QJsonDocument doc(root);
    QFile f(jsonPath);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    return true;
}