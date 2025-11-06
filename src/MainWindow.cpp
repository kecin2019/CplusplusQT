#include "MainWindow.h"
#include "ImageView.h"
#include "InferenceEngine.h"
#include "DicomUtils.h"
#include "MetaTable.h"
#include <QCoreApplication>
#include <QAction>
#include <QDateTime>
#include <QDirIterator>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <qapplication.h>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setupUi();
    setupDockWidgets();
    setupConnections();
}

void MainWindow::setTaskType(TaskSelectionDialog::TaskType taskType)
{
    m_currentTask = taskType;
    updateTaskUi(taskType);
}

void MainWindow::setupUi()
{
    auto *tb = addToolBar(tr("Main"));
    tb->setObjectName("MainToolbar");
    auto *actOpenImg = tb->addAction(tr("Open Image"));
    auto *actOpenDicom = tb->addAction(tr("Open DICOM"));
    auto *actOpenFolder = tb->addAction(tr("Open Folder"));

    tb->addSeparator();

    m_actLoadFAI = tb->addAction(tr("Load FAI ONNX"));
    m_actLoadFAI->setVisible(false);
    m_actRun = tb->addAction(tr("Run Inference"));

    tb->addSeparator();

    auto *actClear = tb->addAction(tr("Clear"));

    connect(actOpenImg, &QAction::triggered, this, &MainWindow::openImage);
    connect(actOpenDicom, &QAction::triggered, this, &MainWindow::openDicom);
    connect(actOpenFolder, &QAction::triggered, this, &MainWindow::openFolder);
    connect(m_actLoadFAI, &QAction::triggered, this, &MainWindow::loadFAIModel);
    connect(m_actRun, &QAction::triggered, this, &MainWindow::runInference);
    connect(actClear, &QAction::triggered, this, &MainWindow::clearAll);

    tb->addSeparator();
    m_actLogDock = tb->addAction(tr("Show Log"));
    m_actLogDock->setCheckable(true);
    m_actLogDock->setChecked(true);
    connect(m_actLogDock, &QAction::toggled, this, &MainWindow::toggleLogDock);

    tb->addSeparator();
    m_overlaySlider = new QSlider(Qt::Horizontal, this);
    m_overlaySlider->setRange(0, 100);
    m_overlaySlider->setValue(100);
    m_overlaySlider->setToolTip(tr("Segmentation overlay opacity"));
    m_overlayAction = new QWidgetAction(this);
    m_overlayAction->setDefaultWidget(m_overlaySlider);
    m_overlayAction->setText(tr("Overlay Opacity"));
    tb->addAction(m_overlayAction);
    m_overlayAction->setVisible(false);
    connect(m_overlaySlider, &QSlider::valueChanged, this, &MainWindow::updateOverlayOpacity);

    // ---- ����������ͼ����Ϣ ----
    m_splitter = new QSplitter(this);
    m_viewTabs = new QTabWidget(this);
    m_inputView = new ImageView(this);
    m_outputView = new ImageView(this);
    m_viewTabs->addTab(m_inputView, tr("Input"));
    m_viewTabs->addTab(m_outputView, tr("Output"));

    // �Ҳࣺ���·ָ��=Metadata����=�ļ��б���
    QSplitter *rightSplit = new QSplitter(Qt::Vertical, this);

    // --- �ϣ�Metadata ��� ---
    auto *metaPanel = new QWidget(this);
    auto *metaLayout = new QVBoxLayout(metaPanel);
    metaLayout->setContentsMargins(12, 12, 12, 12);
    metaLayout->setSpacing(8);
    auto *metaLabel = new QLabel(tr("Metadata (DICOM tags or file info)"), this);
    m_meta = new MetaTable(this);
    metaLayout->addWidget(metaLabel);
    metaLayout->addWidget(m_meta, 1);
    rightSplit->addWidget(metaPanel);

    // --- �£�Batch Files ---
    auto *filesPanel = new QWidget(this);
    auto *filesLayout = new QVBoxLayout(filesPanel);
    filesLayout->setContentsMargins(12, 8, 12, 12);
    filesLayout->setSpacing(8);

    auto *filesHeader = new QHBoxLayout();
    auto *filesTitle = new QLabel(tr("Batch Files"), this);
    auto *filterEdit = new QLineEdit(this);
    filterEdit->setPlaceholderText(tr("Filter..."));
    filesHeader->addWidget(filesTitle);
    filesHeader->addStretch();
    filesHeader->addWidget(filterEdit);

    m_list = new QListWidget(this);
    filesLayout->addLayout(filesHeader);
    filesLayout->addWidget(m_list, 1);
    rightSplit->addWidget(filesPanel);

    connect(filterEdit, &QLineEdit::textChanged, this, [this](const QString &s) {
        for (int i = 0; i < m_list->count(); ++i)
        {
            auto *it = m_list->item(i);
            it->setHidden(!it->text().contains(s, Qt::CaseInsensitive));
        }
    });
    connect(m_list, &QListWidget::itemSelectionChanged, this, &MainWindow::onListActivated);

    m_splitter->addWidget(m_viewTabs);
    m_splitter->addWidget(rightSplit);
    m_splitter->setStretchFactor(0, 7);
    m_splitter->setStretchFactor(1, 3);

    QTimer::singleShot(0, this, [this]() {
        const int w = this->width();
        m_splitter->setSizes({int(w * 0.75), int(w * 0.25)});
    });

    rightSplit->setStretchFactor(0, 2);
    rightSplit->setStretchFactor(1, 1);

    setCentralWidget(m_splitter);
    statusBar()->showMessage(tr("Ready"));

    m_actExport = tb->addAction(tr("Export Result"));
    m_actExportAll = tb->addAction(tr("Batch Export"));
    connect(m_actExport, &QAction::triggered, this, &MainWindow::exportCurrent);
    connect(m_actExportAll, &QAction::triggered, this, &MainWindow::exportBatch);

    m_actBatch = tb->addAction(tr("Batch Infer"));
    connect(m_actBatch, &QAction::triggered, this, &MainWindow::runBatchInference);

    tb->addSeparator();
    m_actLoadMRI = tb->addAction(tr("Load MRI Model"));
    connect(m_actLoadMRI, &QAction::triggered, this, &MainWindow::loadMRIModel);
    m_actLoadMRI->setVisible(false);

    tb->addSeparator();
    auto *actSwitchTask = tb->addAction(tr("Switch Task"));
    connect(actSwitchTask, &QAction::triggered, this, &MainWindow::switchTask);

    for (QAction *a : {m_actRun, m_actBatch, m_actExport, m_actExportAll})
        if (a)
            a->setEnabled(false);

    m_progress = new QProgressBar(this);
    m_progress->setObjectName("InferenceProgress");
    m_progress->setTextVisible(false);
    m_progress->setMinimumWidth(140);
    m_progress->setVisible(false);
    statusBar()->addPermanentWidget(m_progress, 0);

    updateTaskUi(m_currentTask);
}

void MainWindow::setupDockWidgets()
{
    if (m_logDock)
        return;

    m_logDock = new QDockWidget(tr("Runtime Log"), this);
    m_logDock->setObjectName("LogDock");
    m_logView = new QPlainTextEdit(m_logDock);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(2000);
    m_logDock->setWidget(m_logView);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);

    connect(m_logDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_actLogDock && m_actLogDock->isChecked() != visible)
            m_actLogDock->setChecked(visible);
    });
}

void MainWindow::setupConnections()
{
    connect(&m_inferWatcher, &QFutureWatcher<InferenceEngine::Result>::finished,
            this, &MainWindow::onInferenceFinished);

    connect(&m_modelManager, &ModelManager::modelLoaded, this,
            [this](ModelManager::ModelType, const QString &path) {
                appendLog(tr("Model loaded: %1").arg(path));
            });

    connect(&m_modelManager, &ModelManager::modelLoadFailed, this,
            [this](ModelManager::ModelType, const QString &path, const QString &reason) {
                const QString msg = tr("Model load failed: %1 (%2)").arg(path, reason);
                appendLog(msg);
                QMessageBox::warning(this, tr("Load Model"), msg);
            });

    m_modelManager.setRootDirectory(QCoreApplication::applicationDirPath() + "/models");
}

void MainWindow::appendLog(const QString &line)
{
    if (!m_logView)
        return;
    const QString timeStamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_logView->appendPlainText(QStringLiteral("[%1] %2").arg(timeStamp, line));
}

void MainWindow::toggleLogDock(bool checked)
{
    if (!m_logDock)
        return;
    m_logDock->setVisible(checked);
}

void MainWindow::updateTaskUi(TaskSelectionDialog::TaskType taskType)
{
    const bool isSegmentation = (taskType == TaskSelectionDialog::MRI_Segmentation);

    if (m_actLoadFAI)
        m_actLoadFAI->setVisible(!isSegmentation);
    if (m_actLoadMRI)
        m_actLoadMRI->setVisible(isSegmentation);

    if (m_overlayAction)
        m_overlayAction->setVisible(isSegmentation);
    if (m_overlaySlider)
    {
        m_overlaySlider->setEnabled(isSegmentation);
        if (!isSegmentation)
        {
            m_overlaySlider->blockSignals(true);
            m_overlaySlider->setValue(100);
            m_overlaySlider->blockSignals(false);
        }
    }

    if (isSegmentation)
        setWindowTitle(tr("Med YOLO11 Qt - MRI Segmentation"));
    else
        setWindowTitle(tr("Med YOLO11 Qt - FAI X-Ray Detection"));

    toggleInferenceActions(isSegmentation ? m_mriModelReady : m_modelReady);
}

void MainWindow::toggleInferenceActions(bool enabled)
{
    for (QAction *action : {m_actRun, m_actBatch, m_actExport, m_actExportAll})
        if (action)
            action->setEnabled(enabled);
}

bool MainWindow::ensureActiveModel()
{
    if (m_currentTask == TaskSelectionDialog::MRI_Segmentation)
    {
        if (!m_mriModelReady)
        {
            QMessageBox::information(this, tr("Load Model"), tr("Please load the MRI model first."));
            return false;
        }
    }
    else if (!m_modelReady)
    {
        QMessageBox::information(this, tr("Load Model"), tr("Please load the FAI model first."));
        return false;
    }
    return true;
}

void MainWindow::startAsyncInference()
{
    m_isRunning = true;
    toggleInferenceActions(false);

    if (m_progress)
    {
        m_progress->setRange(0, 0);
        m_progress->setVisible(true);
    }

    const auto task = (m_currentTask == TaskSelectionDialog::MRI_Segmentation)
                          ? InferenceEngine::Task::HipMRI_Seg
                          : InferenceEngine::Task::FAI_XRay;

    auto future = QtConcurrent::run([engine = &activeEngine(), img = m_input, task]() {
        return engine->run(img, task);
    });
    m_inferWatcher.setFuture(future);
    const QString tag = m_currentPath.isEmpty() ? tr("current image") : m_currentPath;
    appendLog(tr("Inference started: %1").arg(tag));
}

void MainWindow::onInferenceFinished()
{
    if (m_progress)
    {
        m_progress->setVisible(false);
        m_progress->setRange(0, 1);
        m_progress->setValue(0);
    }

    toggleInferenceActions(true);
    m_isRunning = false;

    const auto result = m_inferWatcher.result();

    if (!m_currentPath.isEmpty())
    {
        m_cacheImg[m_currentPath] = result.outputImage;
        m_cacheDets[m_currentPath] = result.dets;
        if (!result.segmentationMask.isNull())
            m_cacheSegmentation[m_currentPath] = result.segmentationMask;
        else
            m_cacheSegmentation.remove(m_currentPath);
    }

    m_output = result.outputImage;
    m_lastDets = result.dets;
    if (m_currentTask == TaskSelectionDialog::MRI_Segmentation)
        m_segmentationMask = result.segmentationMask;
    else
        m_segmentationMask = QImage();

    setOutputImage(result.outputImage);
    statusBar()->showMessage(tr("Inference done"), 3000);
    appendLog(result.summary);
}

void MainWindow::resetInferenceState()
{
    m_isRunning = false;
    if (m_progress)
    {
        m_progress->setVisible(false);
        m_progress->setRange(0, 1);
        m_progress->setValue(0);
    }
    const bool ready = (m_currentTask == TaskSelectionDialog::MRI_Segmentation) ? m_mriModelReady : m_modelReady;
    toggleInferenceActions(ready);
}

void MainWindow::updateOverlayOpacity(int value)
{
    value = std::clamp(value, 0, 100);
    m_overlayOpacity = value / 100.0f;
    if (!m_output.isNull() && m_currentTask == TaskSelectionDialog::MRI_Segmentation && !m_segmentationMask.isNull())
        setOutputImage(m_output);
}

InferenceEngine &MainWindow::activeEngine()
{
    if (m_currentTask == TaskSelectionDialog::MRI_Segmentation)
        return m_mriEngine;
    return m_engine;
}

ModelManager::ModelType MainWindow::resolveModelType() const
{
    return (m_currentTask == TaskSelectionDialog::MRI_Segmentation)
               ? ModelManager::ModelType::MRI_Segmentation
               : ModelManager::ModelType::FAI_XRay;
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
    m_cacheImg.clear();
    m_cacheDets.clear();
    m_cacheSegmentation.clear();
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

    auto itImg = m_cacheImg.find(sel);
    if (itImg != m_cacheImg.end())
    {
        m_output = itImg.value();
        m_lastDets = m_cacheDets.value(sel);
        if (m_currentTask == TaskSelectionDialog::MRI_Segmentation)
            m_segmentationMask = m_cacheSegmentation.value(sel);
        setOutputImage(m_output);
    }
    else if (m_modelReady || m_mriModelReady)
    {
        // 自动对当前文件推理一次
        runInference();
    }
    else
    {
        // 未加载模型且没有缓存：清空输出
        m_output = QImage();
        m_lastDets.clear();
        m_segmentationMask = QImage();
        m_outputView->clearImage();
    }
}
void MainWindow::runInference()
{
    if (m_isRunning)
    {
        statusBar()->showMessage(tr("Inference already running"), 2000);
        return;
    }

    if (m_input.isNull())
    {
        QMessageBox::information(this, tr("Run Inference"), tr("Please load an image first."));
        return;
    }

    if (!ensureActiveModel())
        return;

    startAsyncInference();
}

void MainWindow::runBatchInference()
{
    if (!ensureActiveModel())
        return;

    if (!m_list || m_list->count() == 0)
    {
        QMessageBox::information(this, tr("Batch Infer"), tr("Batch list is empty."));
        return;
    }

    const bool segmentation = (m_currentTask == TaskSelectionDialog::MRI_Segmentation);
    int ok = 0, fail = 0;

    for (int i = 0; i < m_list->count(); ++i)
    {
        const QString path = m_list->item(i)->text();
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

        InferenceEngine::Result res;
        auto itImg = m_cacheImg.find(path);
        auto itDet = m_cacheDets.find(path);
        auto itMask = m_cacheSegmentation.find(path);
        if (segmentation && itImg != m_cacheImg.end() && itMask != m_cacheSegmentation.end())
        {
            res.outputImage = itImg.value();
            res.segmentationMask = itMask.value();
        }
        else if (!segmentation && itImg != m_cacheImg.end() && itDet != m_cacheDets.end())
        {
            res.outputImage = itImg.value();
            res.dets = itDet.value();
        }
        else
        {
            res = segmentation ? m_mriEngine.run(in, InferenceEngine::Task::HipMRI_Seg)
                                : m_engine.run(in, InferenceEngine::Task::FAI_XRay);
            m_cacheImg[path] = res.outputImage;
            if (segmentation)
            {
                if (!res.segmentationMask.isNull())
                    m_cacheSegmentation[path] = res.segmentationMask;
            }
            else
            {
                m_cacheDets[path] = res.dets;
                m_cacheSegmentation.remove(path);
            }
        }
        ++ok;

        if (path == m_currentPath)
        {
            m_output = res.outputImage;
            m_lastDets = res.dets;
            if (segmentation)
                m_segmentationMask = res.segmentationMask;
            setOutputImage(m_output);
        }

        qApp->processEvents();
    }

    QMessageBox::information(this, tr("Batch Infer"),
                             tr("Done. Success: %1, Failed: %2").arg(ok).arg(fail));
    appendLog(tr("Batch inference finished. Success: %1, Failed: %2").arg(ok).arg(fail));
}

void MainWindow::loadFAIModel()
{
    if (m_modelManager.loadModel(ModelManager::ModelType::FAI_XRay, m_engine))
    {
        m_modelReady = true;
        toggleInferenceActions(m_currentTask == TaskSelectionDialog::FAI_XRay);
        const QString modelPath = m_modelManager.loadedModelPath(ModelManager::ModelType::FAI_XRay);
        log(tr("FAI model ready: %1").arg(modelPath));
    }
}

void MainWindow::loadMRIModel()
{
    if (m_modelManager.loadModel(ModelManager::ModelType::MRI_Segmentation, m_mriEngine))
    {
        m_mriModelReady = true;
        toggleInferenceActions(m_currentTask == TaskSelectionDialog::MRI_Segmentation);
        const QString modelPath = m_modelManager.loadedModelPath(ModelManager::ModelType::MRI_Segmentation);
        log(tr("MRI model ready: %1").arg(modelPath));
    }
}

void MainWindow::loadMRIModel()
{
    // 使用相对路径加载MRI分割模型
    QString modelPath = QCoreApplication::applicationDirPath() + "/models/mri_segmentation.onnx";

    // 检查文件是否存在
    if (!QFileInfo::exists(modelPath))
    {
        QMessageBox::warning(this, "Load MRI Model",
                             QString("MRI model file not found at: %1").arg(modelPath));
        return;
    }

    m_mriOnnxPath = modelPath;
    m_mriModelReady = m_mriEngine.loadModel(m_mriOnnxPath, InferenceEngine::Task::HipMRI_Seg);
    if (!m_mriModelReady)
    {
        QMessageBox::warning(this, "Load MRI Model", "Failed to load MRI ONNX model.");
        return;
    }
    for (QAction *a : {m_actRun, m_actBatch, m_actExport, m_actExportAll})
        if (a)
            a->setEnabled(true);

    log(QString("MRI Segmentation ONNX loaded: %1").arg(modelPath));
}

void MainWindow::switchTask()
{
    TaskSelectionDialog dialog;
    if (dialog.exec() == QDialog::Accepted)
    {
        setTaskType(dialog.selectedTask());

        // 清除当前状态，因为切换任务需要重新开始
        clearAll();

        // 根据新任务类型更新窗口标题
        if (dialog.selectedTask() == TaskSelectionDialog::MRI_Segmentation)
        {
            setWindowTitle("Med YOLO11 Qt - MRI Segmentation");
        }
        else
        {
            setWindowTitle("Med YOLO11 Qt - FAI X-Ray Detection");
        }

        log("Task switched");
    }
}

void MainWindow::clearAll()
{
    resetInferenceState();
    m_input = QImage();
    m_inputView->clearImage();
    m_outputView->clearImage();
    m_meta->clearAll();
    m_currentPath.clear();
    m_batch.clear();
    m_list->clear();
    m_cacheImg.clear();
    m_cacheDets.clear();
    m_cacheSegmentation.clear();
    m_segmentationMask = QImage();
    statusBar()->showMessage(tr("Cleared"));
    appendLog(tr("Workspace cleared"));
}

void MainWindow::log(const QString &s)
{
    statusBar()->showMessage(s, 5000);
    appendLog(s);
}

void MainWindow::setInputImage(const QImage &img)
{
    m_input = img;
    m_inputView->setImage(img, true);
    m_viewTabs->setCurrentWidget(m_inputView);
}
void MainWindow::setOutputImage(const QImage &img)
{
    QImage display = img;

    if (m_currentTask == TaskSelectionDialog::MRI_Segmentation && !m_segmentationMask.isNull())
    {
        QImage base = img.convertToFormat(QImage::Format_ARGB32);
        QImage mask = m_segmentationMask;
        if (mask.size() != base.size())
            mask = mask.scaled(base.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        QPainter painter(&base);
        painter.setOpacity(m_overlayOpacity);
        painter.drawImage(QPoint(0, 0), mask);
        painter.end();
        display = base;
    }

    m_outputView->setImage(display, false);
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
        m_segmentationMask = QImage();
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
        m_segmentationMask = QImage();
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
