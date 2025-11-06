#include "MainWindow.h"
#include "ImageView.h"
#include "InferenceEngine.h"
#include "DicomUtils.h"
#include "MetaTable.h"
#include "AppConfig.h"
#include "ErrorHandler.h"
#include <QToolBar>
#include <QFileDialog>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QAction>
#include <QMessageBox>
#include <QLabel>
#include <QDockWidget>
#include <QListWidget>
#include <QDirIterator>
#include <QFileInfo>
#include <QWidget>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDateTime>
#include <QProgressBar>
#include <qapplication.h>
#include <QTimer>
#include <QStringList>
#include <QProgressDialog>
#include <QPointer>
#include <QtConcurrent>
#include <QCoreApplication>
#include <QPalette>
#include <QStyleFactory>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    // 初始化配置和错误处理器
    AppConfig::instance().loadConfig();

    m_singleWatcher.setParent(this);
    m_batchWatcher.setParent(this);
    connect(&m_singleWatcher, &QFutureWatcher<InferenceEngine::Result>::finished,
            this, &MainWindow::handleSingleInferenceFinished);
    connect(&m_batchWatcher, &QFutureWatcher<std::vector<BatchItem>>::finished,
            this, &MainWindow::handleBatchInferenceFinished);

    setupUi();
}

MainWindow::~MainWindow()
{
    if (m_singleWatcher.isRunning())
    {
        m_singleWatcher.cancel();
        m_singleWatcher.waitForFinished();
    }
    if (m_batchWatcher.isRunning())
    {
        m_batchWatcher.cancel();
        m_batchWatcher.waitForFinished();
    }
}

void MainWindow::setTaskType(TaskSelectionDialog::TaskType taskType)
{
    m_currentTask = taskType;
    updateTaskUi(taskType);
    refreshActionStates();
}

void MainWindow::setupUi()
{
    applyPalette();
    applyStyleSheet(":/styles/modern_light.qss");
    createToolBar();
    createCentralViews();
    createDockWidgets();
    createStatusBar();

    updateTaskUi(m_currentTask);
    refreshActionStates();
}

void MainWindow::createToolBar()
{
    auto *tb = addToolBar(tr("Main"));
    tb->setObjectName("MainToolbar");
    tb->setMovable(false);

    auto *actOpenImg = tb->addAction(tr("Open Image"));
    auto *actOpenDicom = tb->addAction(tr("Open DICOM"));
    auto *actOpenFolder = tb->addAction(tr("Open Folder"));

    connect(actOpenImg, &QAction::triggered, this, &MainWindow::openImage);
    connect(actOpenDicom, &QAction::triggered, this, &MainWindow::openDicom);
    connect(actOpenFolder, &QAction::triggered, this, &MainWindow::openFolder);

    tb->addSeparator();

    m_actLoadFAI = tb->addAction(tr("Load FAI Model"));
    connect(m_actLoadFAI, &QAction::triggered, this, &MainWindow::loadFAIModel);

    m_actLoadMRI = tb->addAction(tr("Load MRI Model"));
    connect(m_actLoadMRI, &QAction::triggered, this, &MainWindow::loadMRIModel);

    tb->addSeparator();

    m_actRun = tb->addAction(tr("Run Inference"));
    connect(m_actRun, &QAction::triggered, this, &MainWindow::runInference);

    m_actBatch = tb->addAction(tr("Batch Infer"));
    connect(m_actBatch, &QAction::triggered, this, &MainWindow::runBatchInference);

    tb->addSeparator();

    m_actExport = tb->addAction(tr("Export Result"));
    connect(m_actExport, &QAction::triggered, this, &MainWindow::exportCurrent);

    m_actExportAll = tb->addAction(tr("Batch Export"));
    connect(m_actExportAll, &QAction::triggered, this, &MainWindow::exportBatch);

    tb->addSeparator();

    auto *actClear = tb->addAction(tr("Clear"));
    connect(actClear, &QAction::triggered, this, &MainWindow::clearAll);

    tb->addSeparator();

    auto *actSwitchTask = tb->addAction(tr("Switch Task"));
    connect(actSwitchTask, &QAction::triggered, this, &MainWindow::switchTask);

    tb->addSeparator();

    m_actToggleLog = tb->addAction(tr("Show Log"));
    m_actToggleLog->setCheckable(true);
    connect(m_actToggleLog, &QAction::toggled, this, &MainWindow::toggleLogDock);
}

void MainWindow::createCentralViews()
{
    m_viewTabs = new QTabWidget(this);
    m_viewTabs->setObjectName("ImageTabs");
    m_inputView = new ImageView(this);
    m_outputView = new ImageView(this);
    m_viewTabs->addTab(m_inputView, tr("Input"));
    m_viewTabs->addTab(m_outputView, tr("Output"));
    setCentralWidget(m_viewTabs);
}

void MainWindow::createDockWidgets()
{
    // Metadata dock
    m_metadataDock = new QDockWidget(tr("Metadata"), this);
    m_metadataDock->setObjectName("MetadataDock");
    m_metadataDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    auto *metaContainer = new QWidget(m_metadataDock);
    auto *metaLayout = new QVBoxLayout(metaContainer);
    metaLayout->setContentsMargins(12, 12, 12, 12);
    metaLayout->setSpacing(8);
    auto *metaLabel = new QLabel(tr("Metadata (DICOM tags or file info)"), metaContainer);
    m_meta = new MetaTable(metaContainer);
    metaLayout->addWidget(metaLabel);
    metaLayout->addWidget(m_meta, 1);
    metaContainer->setLayout(metaLayout);
    m_metadataDock->setWidget(metaContainer);
    addDockWidget(Qt::RightDockWidgetArea, m_metadataDock);

    // Batch dock
    m_batchDock = new QDockWidget(tr("Batch Files"), this);
    m_batchDock->setObjectName("BatchDock");
    m_batchDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    auto *batchContainer = new QWidget(m_batchDock);
    auto *batchLayout = new QVBoxLayout(batchContainer);
    batchLayout->setContentsMargins(12, 8, 12, 12);
    batchLayout->setSpacing(8);

    auto *headerLayout = new QHBoxLayout();
    auto *filesTitle = new QLabel(tr("Batch Files"), batchContainer);
    m_batchFilter = new QLineEdit(batchContainer);
    m_batchFilter->setPlaceholderText(tr("Filter..."));
    headerLayout->addWidget(filesTitle);
    headerLayout->addStretch();
    headerLayout->addWidget(m_batchFilter);

    m_list = new QListWidget(batchContainer);
    batchLayout->addLayout(headerLayout);
    batchLayout->addWidget(m_list, 1);
    batchContainer->setLayout(batchLayout);
    m_batchDock->setWidget(batchContainer);
    addDockWidget(Qt::RightDockWidgetArea, m_batchDock);
    tabifyDockWidget(m_metadataDock, m_batchDock);
    m_metadataDock->raise();

    connect(m_batchFilter, &QLineEdit::textChanged, this, [this](const QString &text)
            {
        if (!m_list)
            return;
        for (int i = 0; i < m_list->count(); ++i)
        {
            auto *item = m_list->item(i);
            item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
        } });
    connect(m_list, &QListWidget::itemSelectionChanged, this, &MainWindow::onListActivated);

    // Log dock
    m_logDock = new QDockWidget(tr("Runtime Log"), this);
    m_logDock->setObjectName("LogDock");
    m_logDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    m_logView = new QPlainTextEdit(m_logDock);
    m_logView->setReadOnly(true);
    m_logDock->setWidget(m_logView);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    m_logDock->hide();
    if (m_actToggleLog)
        m_actToggleLog->setChecked(false);

    connect(m_logDock, &QDockWidget::visibilityChanged, this, [this](bool visible)
            {
        if (m_actToggleLog && m_actToggleLog->isChecked() != visible)
            m_actToggleLog->setChecked(visible); });
}

void MainWindow::createStatusBar()
{
    statusBar()->setContentsMargins(8, 0, 8, 0);
    m_statusMode = new QLabel(tr("Mode: --"), this);
    m_statusModel = new QLabel(tr("Model: --"), this);
    m_statusProgress = new QProgressBar(this);
    m_statusProgress->setFixedWidth(140);
    m_statusProgress->setTextVisible(false);
    m_statusProgress->setVisible(false);

    statusBar()->addPermanentWidget(m_statusMode);
    statusBar()->addPermanentWidget(m_statusModel);
    statusBar()->addPermanentWidget(m_statusProgress);
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::applyPalette()
{
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#f4f6fb"));
    palette.setColor(QPalette::WindowText, QColor("#1b1f29"));
    palette.setColor(QPalette::Base, QColor("#ffffff"));
    palette.setColor(QPalette::AlternateBase, QColor("#edf1fc"));
    palette.setColor(QPalette::ToolTipBase, QColor("#ffffff"));
    palette.setColor(QPalette::ToolTipText, QColor("#1b1f29"));
    palette.setColor(QPalette::Text, QColor("#1b1f29"));
    palette.setColor(QPalette::Button, QColor("#ffffff"));
    palette.setColor(QPalette::ButtonText, QColor("#1b1f29"));
    palette.setColor(QPalette::Highlight, QColor("#3b82f6"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(QPalette::Link, QColor("#2563eb"));
    palette.setColor(QPalette::BrightText, QColor("#ffffff"));
    palette.setColor(QPalette::Midlight, QColor("#e2e8f0"));
    palette.setColor(QPalette::Dark, QColor("#cbd5e1"));
    palette.setColor(QPalette::Shadow, QColor("#94a3b8"));

    qApp->setPalette(palette);
}

void MainWindow::applyStyleSheet(const QString &resourcePath)
{
    QFile styleFile(resourcePath);
    if (!styleFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        LOG_WARNING(QStringLiteral("Unable to load stylesheet: %1").arg(resourcePath), "UI", 1101);
        return;
    }
    const QString style = QString::fromUtf8(styleFile.readAll());
    qApp->setStyleSheet(style);
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
    if (m_isInferenceRunning || m_isBatchRunning)
    {
        statusBar()->showMessage("Inference already running...");
        return;
    }

    if (m_input.isNull())
    {
        QMessageBox::information(this, "Run Inference", "Please load an image first.");
        return;
    }

    if (!m_modelReady && !m_mriModelReady)
    {
        QMessageBox::information(this, "Run Inference", "Please load an ONNX model first.");
        return;
    }

    m_isInferenceRunning = true;
    m_pendingInferencePath = m_currentPath;
    m_pendingTask = m_modelReady ? InferenceEngine::Task::FAI_XRay : InferenceEngine::Task::HipMRI_Seg;

    setBusyState(true, "Running inference...");

    const QImage inputCopy = m_input;
    const InferenceEngine::Task task = m_pendingTask;

    auto future = QtConcurrent::run([this, inputCopy, task]()
                                    {
        if (task == InferenceEngine::Task::HipMRI_Seg)
            return m_mriEngine.run(inputCopy, task);
        return m_engine.run(inputCopy, task); });
    m_singleWatcher.setFuture(future);
}

void MainWindow::runBatchInference()
{
    if (m_isInferenceRunning || m_isBatchRunning)
    {
        statusBar()->showMessage("Inference already running...");
        return;
    }

    if (!m_modelReady && !m_mriModelReady)
    {
        QMessageBox::information(this, "Batch Export", "Please load an ONNX model first.");
        return;
    }

    if (!m_list || m_list->count() == 0)
    {
        QMessageBox::information(this, "Batch Infer", "Batch list is empty.");
        return;
    }

    QStringList paths;
    paths.reserve(m_list->count());
    for (int i = 0; i < m_list->count(); ++i)
        paths.append(m_list->item(i)->text());

    const int total = paths.size();
    m_isBatchRunning = true;
    setBusyState(true, "Running batch inference...", total);

    const InferenceEngine::Task task = m_modelReady ? InferenceEngine::Task::FAI_XRay : InferenceEngine::Task::HipMRI_Seg;
    QPointer<MainWindow> guard(this);

    auto future = QtConcurrent::run([this, guard, task, paths = std::move(paths)]() -> std::vector<BatchItem>
                                    {
        std::vector<BatchItem> results;
        results.reserve(static_cast<size_t>(paths.size()));
        int index = 0;
        const int totalCount = static_cast<int>(paths.size());
        for (const QString &path : paths)
        {
            BatchItem item;
            item.path = path;
            QImage image;
            QString error;
            bool ok = false;

            if (isImageFile(path))
            {
                ok = image.load(path);
                if (!ok)
                    error = QStringLiteral("Failed to load image: %1").arg(path);
            }
            else if (isDicomFile(path))
            {
#ifdef HAVE_GDCM
                ok = DicomUtils::loadDicomToQImage(path, image, nullptr);
                if (!ok)
                    error = QStringLiteral("Failed to load DICOM: %1").arg(path);
#else
                error = QStringLiteral("Built without GDCM support");
#endif
            }
            else
            {
                error = QStringLiteral("Unsupported file: %1").arg(path);
            }

            if (ok)
            {
                item.success = true;
                item.result = (task == InferenceEngine::Task::HipMRI_Seg)
                                   ? m_mriEngine.run(image, task)
                                   : m_engine.run(image, task);
            }
            else
            {
                item.success = false;
                item.error = error;
            }

            results.push_back(item);
            ++index;
            if (guard)
            {
                QMetaObject::invokeMethod(guard, [guard, index, totalCount]() {
                    if (guard)
guard->updateProgressValue(index, totalCount);
                }, Qt::QueuedConnection);
            }
        }
        return results; });

    m_batchWatcher.setFuture(future);
}

void MainWindow::refreshActionStates()
{
    const bool busy = m_isInferenceRunning || m_isBatchRunning;
    const bool hasModel = m_modelReady || m_mriModelReady;

    if (m_actRun)
        m_actRun->setEnabled(hasModel && !busy);
    if (m_actBatch)
        m_actBatch->setEnabled(hasModel && !busy);
    if (m_actExportAll)
        m_actExportAll->setEnabled(hasModel && !busy);
    if (m_actExport)
        m_actExport->setEnabled(!m_output.isNull() && !busy);

    updateStatusSummary();
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
    if (m_batchDock)
    {
        const QString title = isSegmentation ? tr("Batch Files (Segmentation)") : tr("Batch Files");
        m_batchDock->setWindowTitle(title);
    }
    setWindowTitle(isSegmentation ? tr("Med YOLO11 Qt - MRI Segmentation")
                                  : tr("Med YOLO11 Qt - FAI X-Ray Detection"));
    updateStatusSummary();
}

void MainWindow::updateStatusSummary()
{
    if (m_statusMode)
    {
        const QString modeText = (m_currentTask == TaskSelectionDialog::MRI_Segmentation)
                                     ? tr("Mode: MRI Segmentation")
                                     : tr("Mode: FAI X-Ray");
        m_statusMode->setText(modeText);
    }

    if (m_statusModel)
    {
        QString info;
        if (m_currentTask == TaskSelectionDialog::MRI_Segmentation)
            info = m_mriModelReady ? tr("MRI Model: Ready") : tr("MRI Model: Not loaded");
        else
            info = m_modelReady ? tr("FAI Model: Ready") : tr("FAI Model: Not loaded");
        m_statusModel->setText(info);
    }
}

void MainWindow::setBusyState(bool busy, const QString &message, int maximum)
{
    if (busy)
    {
        if (!m_progressDialog)
        {
            m_progressDialog = new QProgressDialog(message, QString(), 0, 0, this);
            m_progressDialog->setCancelButton(nullptr);
            m_progressDialog->setWindowModality(Qt::WindowModal);
            m_progressDialog->setMinimumDuration(0);
            m_progressDialog->setAutoClose(false);
            m_progressDialog->setAutoReset(false);
        }
        m_progressDialog->setLabelText(message);
        if (maximum > 0)
        {
            m_progressDialog->setRange(0, maximum);
            m_progressDialog->setValue(0);
        }
        else
        {
            m_progressDialog->setRange(0, 0);
        }
        m_progressDialog->show();
        statusBar()->showMessage(message);

        if (m_statusProgress)
        {
            if (maximum > 0)
            {
                m_statusProgress->setRange(0, maximum);
                m_statusProgress->setValue(0);
            }
            else
            {
                m_statusProgress->setRange(0, 0);
            }
            m_statusProgress->setVisible(true);
        }
    }
    else if (m_progressDialog)
    {
        m_progressDialog->hide();
        m_progressDialog->reset();
        statusBar()->showMessage(tr("Ready"), 2000);
        if (m_statusProgress)
        {
            m_statusProgress->setVisible(false);
            m_statusProgress->setRange(0, 1);
            m_statusProgress->setValue(0);
        }
    }

    if (m_list)
        m_list->setEnabled(!busy);
    if (m_actLoadFAI)
        m_actLoadFAI->setEnabled(!busy);
    if (m_actLoadMRI)
        m_actLoadMRI->setEnabled(!busy);

    refreshActionStates();
}

bool MainWindow::promptForModelFile(QString &path, const QString &title) const
{
    const QString startDir = path.isEmpty() ? QCoreApplication::applicationDirPath()
                                            : QFileInfo(path).absolutePath();
    const QString selected = QFileDialog::getOpenFileName(const_cast<MainWindow *>(this),
                                                          title,
                                                          startDir,
                                                          tr("Model Files (*.onnx *.encrypted);;ONNX Models (*.onnx);;Encrypted Models (*.encrypted)"));
    if (selected.isEmpty())
        return false;
    path = selected;
    return true;
}

void MainWindow::updateProgressValue(int value, int maximum)
{
    if (!m_progressDialog)
        return;
    if (maximum > 0)
    {
        if (m_progressDialog->maximum() != maximum)
            m_progressDialog->setRange(0, maximum);
        m_progressDialog->setValue(value);
    }
    else
    {
        m_progressDialog->setRange(0, 0);
    }

    statusBar()->showMessage(
        maximum > 0
            ? tr("Progress: %1 / %2").arg(value).arg(maximum)
            : tr("Processing..."));

    if (m_statusProgress)
    {
        if (maximum > 0)
        {
            m_statusProgress->setRange(0, maximum);
            m_statusProgress->setValue(value);
        }
        else
        {
            m_statusProgress->setRange(0, 0);
        }
        m_statusProgress->setVisible(true);
    }
}

void MainWindow::handleSingleInferenceFinished()
{
    m_isInferenceRunning = false;
    setBusyState(false, QString());

    if (!m_singleWatcher.future().isFinished())
    {
        refreshActionStates();
        return;
    }

    InferenceEngine::Result result = m_singleWatcher.result();

    if (!m_pendingInferencePath.isEmpty())
    {
        m_cacheImg[m_pendingInferencePath] = result.outputImage;
        m_cacheDets[m_pendingInferencePath] = result.dets;
    }
    m_pendingInferencePath.clear();

    m_output = result.outputImage;
    m_lastDets = result.dets;
    m_segmentationMask = result.segmentationMask;

    if (!result.outputImage.isNull())
        setOutputImage(result.outputImage);
    else
        m_outputView->clearImage();

    statusBar()->showMessage(result.summary, 5000);
    LOG_INFO(QStringLiteral("Inference summary: %1").arg(result.summary), "Inference");

    refreshActionStates();
}

void MainWindow::handleBatchInferenceFinished()
{
    m_isBatchRunning = false;
    setBusyState(false, QString());

    if (!m_batchWatcher.future().isFinished())
    {
        refreshActionStates();
        return;
    }

    std::vector<BatchItem> results = m_batchWatcher.result();
    int ok = 0;
    int fail = 0;

    for (const auto &item : results)
    {
        if (!item.success)
        {
            ++fail;
            LOG_WARNING(QStringLiteral("Batch inference failed: %1 (%2)").arg(item.path, item.error), "BatchInference", 1003);
            continue;
        }

        ++ok;
        m_cacheImg[item.path] = item.result.outputImage;
        m_cacheDets[item.path] = item.result.dets;

        if (item.path == m_currentPath)
        {
            m_output = item.result.outputImage;
            m_lastDets = item.result.dets;
            m_segmentationMask = item.result.segmentationMask;
            setOutputImage(item.result.outputImage);
            statusBar()->showMessage(item.result.summary, 5000);
        }
    }

    refreshActionStates();

    QMessageBox::information(this, "Batch Infer",
                             QString("Done. Success: %1, Failed: %2").arg(ok).arg(fail));
}

void MainWindow::loadFAIModel()
{
    QString modelPath = AppConfig::instance().getFaiModelPath();
    if (!QFileInfo::exists(modelPath))
    {
        log(tr("配置的FAI模型文件不存在，尝试使用默认路径"));
        // 检查默认路径
        QString defaultPath = QCoreApplication::applicationDirPath() + "/models/encrypted/fai_xray.encrypted";
        if (QFileInfo::exists(defaultPath))
        {
            modelPath = defaultPath;
        }
        else
        {
            if (!promptForModelFile(modelPath, tr("Select FAI ONNX Model")))
            {
                LOG_WARNING("FAI model selection canceled", "Model", 2001);
                return;
            }
        }
    }

    try
    {
        bool keepPrompting = true;
        while (keepPrompting)
        {
            m_faiOnnxPath = modelPath;
            log(tr("正在加载FAI模型: %1").arg(modelPath));
            m_modelReady = m_engine.loadModel(m_faiOnnxPath);
            if (m_modelReady)
            {
                keepPrompting = false;
            }
            else
            {
                const auto choice = QMessageBox::warning(this,
                                                         tr("Load FAI Model"),
                                                         tr("Failed to load the selected FAI model.\n\nPossible reasons:\n"
                                                            "1. The model file is not encrypted\n"
                                                            "2. The encryption key is incorrect\n"
                                                            "3. The model file is corrupted\n\n"
                                                            "Would you like to choose another file?"),
                                                         QMessageBox::Retry | QMessageBox::Cancel);
                if (choice == QMessageBox::Retry)
                {
                    if (!promptForModelFile(modelPath, tr("Select FAI ONNX Model")))
                    {
                        LOG_WARNING("FAI model selection canceled after failure", "Model", 2002);
                        return;
                    }
                    continue;
                }
                LOG_WARNING("User canceled FAI model loading after failure", "Model", 2003);
                return;
            }
        }

        AppConfig::instance().setFaiModelPath(modelPath);
        AppConfig::instance().saveConfig();
        refreshActionStates();

        log(tr("FAI model loaded: %1").arg(modelPath));
    }
    catch (const std::exception &e)
    {
        const QString errorMsg = tr("Failed to load FAI model: %1").arg(e.what());
        LOG_ERROR(errorMsg, "Model", 2004);
        QMessageBox::critical(this, tr("Model Loading Error"), errorMsg);
    }
}

void MainWindow::loadMRIModel()
{
    QString modelPath = AppConfig::instance().getMriModelPath();
    if (!QFileInfo::exists(modelPath))
    {
        if (!promptForModelFile(modelPath, tr("Select MRI Segmentation Model")))
        {
            LOG_WARNING("MRI model selection canceled", "Model", 2004);
            return;
        }
    }

    try
    {
        bool keepPrompting = true;
        while (keepPrompting)
        {
            m_mriOnnxPath = modelPath;
            m_mriModelReady = m_mriEngine.loadModel(m_mriOnnxPath);
            if (m_mriModelReady)
            {
                keepPrompting = false;
            }
            else
            {
                const auto choice = QMessageBox::warning(this,
                                                         tr("Load MRI Model"),
                                                         tr("Failed to load the selected MRI model.\nWould you like to choose another file?"),
                                                         QMessageBox::Retry | QMessageBox::Cancel);
                if (choice == QMessageBox::Retry)
                {
                    if (!promptForModelFile(modelPath, tr("Select MRI Segmentation Model")))
                    {
                        LOG_WARNING("MRI model selection canceled after failure", "Model", 2005);
                        return;
                    }
                    continue;
                }
                LOG_WARNING("User canceled MRI model loading after failure", "Model", 2006);
                return;
            }
        }

        AppConfig::instance().setMriModelPath(modelPath);
        AppConfig::instance().saveConfig();
        refreshActionStates();

        log(tr("MRI model loaded: %1").arg(modelPath));
    }
    catch (const std::exception &e)
    {
        const QString errorMsg = tr("Failed to load MRI model: %1").arg(e.what());
        LOG_ERROR(errorMsg, "Model", 2007);
        QMessageBox::critical(this, tr("Model Loading Error"), errorMsg);
    }
}

void MainWindow::switchTask()
{
    TaskSelectionDialog dialog;
    if (dialog.exec() == QDialog::Accepted)
    {
        setTaskType(dialog.selectedTask());
        clearAll();
        log(tr("Task switched"));
    }
}

void MainWindow::clearAll()
{
    m_input = QImage();
    m_output = QImage();
    m_lastDets.clear();
    m_inputView->clearImage();
    m_outputView->clearImage();
    m_meta->clearAll();
    m_currentPath.clear();
    m_batch.clear();
    if (m_list)
        m_list->clear();
    if (m_batchFilter)
        m_batchFilter->clear();
    m_segmentationMask = QImage();
    statusBar()->showMessage(tr("Workspace cleared"));
    appendLog(tr("Workspace cleared"));
    refreshActionStates();
}

void MainWindow::appendLog(const QString &message)
{
    if (!m_logView)
        return;
    const QString line = QStringLiteral("[%1] %2")
                             .arg(QDateTime::currentDateTime().toString("HH:mm:ss"),
                                  message);
    m_logView->appendPlainText(line);
}

void MainWindow::log(const QString &s)
{
    statusBar()->showMessage(s, 5000);
    appendLog(s);
    LOG_INFO(s, "MainWindow");
}

void MainWindow::setInputImage(const QImage &img)
{
    m_input = img;
    m_inputView->setImage(img, true);
    m_viewTabs->setCurrentWidget(m_inputView);
}
void MainWindow::setOutputImage(const QImage &img)
{
    // ���ͼ�������� fit��ֱ�ӱ���������ͼ��ͬ����
    m_outputView->setImage(img, false);
    m_outputView->setTransform(m_inputView->transform());
    m_viewTabs->setCurrentWidget(m_outputView);
    refreshActionStates();
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
    try
    {
        if (isImageFile(path))
        {
            QImage img(path);
            if (img.isNull())
            {
                QString errorMsg = QString("Failed to load image: %1").arg(path);
                LOG_ERROR(errorMsg, "ImageLoader", 3001);
                QMessageBox::warning(this, "Open Image", errorMsg);
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
                QString errorMsg = QString("Failed to load DICOM: %1").arg(path);
                LOG_ERROR(errorMsg, "DicomLoader", 3002);
                QMessageBox::warning(this, "Open DICOM", errorMsg);
                return;
            }
#else
            QString infoMsg = "Built without GDCM. Reconfigure with -DUSE_GDCM=ON.";
            LOG_INFO(infoMsg, "DicomLoader");
            QMessageBox::information(this, "DICOM disabled", infoMsg);
            return;
#endif
            m_currentPath = path;
            setInputImage(img);
            updateMetaTable(meta);
            log(QString("Loaded DICOM: %1").arg(path));
        }
        else
        {
            QString errorMsg = QString("Unsupported file type: %1").arg(path);
            LOG_ERROR(errorMsg, "FileLoader", 3003);
            QMessageBox::warning(this, "Open", errorMsg);
        }
    }
    catch (const std::exception &e)
    {
        QString errorMsg = QString("Failed to load file: %1, Error: %2").arg(path).arg(e.what());
        LOG_ERROR(errorMsg, "FileLoader", 3004);
        QMessageBox::critical(this, "File Loading Error", errorMsg);
    }
}

void MainWindow::exportCurrent()
{
    try
    {
        if (m_output.isNull() || m_lastDets.empty())
        {
            QString errorMsg = "No inference result to export.";
            LOG_WARNING(errorMsg, "Export", 4001);
            QMessageBox::warning(this, "Export", errorMsg);
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
            QString errorMsg = QString("Failed to save image: %1").arg(outImg);
            LOG_ERROR(errorMsg, "Export", 4002);
            QMessageBox::warning(this, "Export", errorMsg);
            return;
        }
        QString outJson = QFileInfo(outImg).absolutePath() + "/" +
                          QFileInfo(outImg).completeBaseName() + ".json";
        if (!saveJson(outJson, m_currentPath, m_input.size(), m_lastDets))
        {
            QString errorMsg = QString("Failed to save JSON: %1").arg(outJson);
            LOG_ERROR(errorMsg, "Export", 4003);
            QMessageBox::warning(this, "Export", errorMsg);
            return;
        }
        statusBar()->showMessage("Exported: " + outImg + " & " + outJson, 5000);
    }
    catch (const std::exception &e)
    {
        QString errorMsg = QString("Export error: %1").arg(e.what());
        LOG_ERROR(errorMsg, "Export", 4004);
        QMessageBox::critical(this, "Export Error", errorMsg);
    }
}

void MainWindow::exportBatch()
{
    try
    {
        if (!m_list || m_list->count() == 0)
        {
            QString errorMsg = "Batch list is empty.";
            LOG_WARNING(errorMsg, "BatchExport", 4005);
            QMessageBox::information(this, "Batch Export", errorMsg);
            return;
        }

        if (!m_modelReady && !m_mriModelReady)
        {
            QString errorMsg = "Please load an ONNX model first.";
            LOG_WARNING(errorMsg, "BatchExport", 4006);
            QMessageBox::information(this, "Batch Infer", errorMsg);
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
                if (m_modelReady)
                {
                    res = m_engine.run(in, InferenceEngine::Task::FAI_XRay);
                }
                else if (m_mriModelReady)
                {
                    res = m_mriEngine.run(in, InferenceEngine::Task::HipMRI_Seg);
                }
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
    catch (const std::exception &e)
    {
        QString errorMsg = QString("Batch export error: %1").arg(e.what());
        LOG_ERROR(errorMsg, "BatchExport", 4007);
        QMessageBox::critical(this, "Batch Export Error", errorMsg);
    }
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