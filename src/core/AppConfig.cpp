#include "AppConfig.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>

/**
 * @brief 获取配置管理器的单例实例
 * @return AppConfig的引用
 */
AppConfig &AppConfig::instance()
{
    static AppConfig config;
    return config;
}

/**
 * @brief 构造函数，初始化配置路径和设置
 */
AppConfig::AppConfig()
    : m_settings(nullptr)
{
    // 设置默认配置文件路径（应用程序目录下的config.ini）
    QString appDir = QCoreApplication::applicationDirPath();
    m_configFilePath = appDir + "/config.ini";

    // 创建QSettings对象
    m_settings = new QSettings(m_configFilePath, QSettings::IniFormat);

    // 设置默认值（如果配置文件不存在，使用这些默认值）
    if (!QFile::exists(m_configFilePath))
    {
        // 设置默认值
        setFaiModelPath("./models/fai_xray.onnx");
        setMriModelPath("./models/mri_segmentation.onnx");
        setConfidenceThreshold(0.25f);
        setIoUThreshold(0.45f);
        setModelProtectionKey("MedYOLO11Qt_Model_Protection_Key_2024");
        setDebugModeEnabled(false);
        saveConfig();
    }
}

/**
 * @brief 析构函数，清理资源
 */
AppConfig::~AppConfig()
{
    delete m_settings;
    m_settings = nullptr;
}

/**
 * @brief 加载配置文件
 * @param configPath 配置文件路径，如果为空则使用默认路径
 * @return 是否加载成功
 */
bool AppConfig::loadConfig(const QString &configPath)
{
    QString path = configPath.isEmpty() ? m_configFilePath : configPath;

    if (!QFile::exists(path))
    {
        return false;
    }

    m_configFilePath = path;

    // 释放旧的QSettings对象并创建新的
    delete m_settings;
    m_settings = new QSettings(path, QSettings::IniFormat);
    return true;
}

/**
 * @brief 保存当前配置到文件
 * @param configPath 配置文件路径，如果为空则使用当前路径
 * @return 是否保存成功
 */
bool AppConfig::saveConfig(const QString &configPath)
{
    QString path = configPath.isEmpty() ? m_configFilePath : configPath;

    // 确保目录存在
    QFileInfo fileInfo(path);
    QDir dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath("."))
    {
        return false;
    }

    // 如果指定了新路径，创建新的QSettings对象
    if (!configPath.isEmpty())
    {
        m_configFilePath = path;
        delete m_settings;
        m_settings = new QSettings(path, QSettings::IniFormat);
    }

    // 保存配置
    m_settings->sync();
    return true;
}

/**
 * @brief 获取Qt安装路径
 * @return Qt安装路径
 */
QString AppConfig::getQtInstallPath() const
{
    return m_settings->value("Paths/Qt", "QT_INSTALL_PATH").toString();
}

/**
 * @brief 设置Qt安装路径
 * @param path Qt安装路径
 */
void AppConfig::setQtInstallPath(const QString &path)
{
    m_settings->setValue("Paths/Qt", path);
}

/**
 * @brief 获取GDCM安装路径
 * @return GDCM安装路径
 */
QString AppConfig::getGdcmInstallPath() const
{
    return m_settings->value("Paths/GDCM", "GDCM_INSTALL_PATH").toString();
}

/**
 * @brief 设置GDCM安装路径
 * @param path GDCM安装路径
 */
void AppConfig::setGdcmInstallPath(const QString &path)
{
    m_settings->setValue("Paths/GDCM", path);
}

/**
 * @brief 获取ONNXRuntime安装路径
 * @return ONNXRuntime安装路径
 */
QString AppConfig::getOnnxRuntimeInstallPath() const
{
    return m_settings->value("Paths/ONNXRuntime", "ONNXRUNTIME_INSTALL_PATH").toString();
}

/**
 * @brief 设置ONNXRuntime安装路径
 * @param path ONNXRuntime安装路径
 */
void AppConfig::setOnnxRuntimeInstallPath(const QString &path)
{
    m_settings->setValue("Paths/ONNXRuntime", path);
}

/**
 * @brief 获取FAI模型路径
 * @return FAI模型路径
 */
QString AppConfig::getFaiModelPath() const
{
    return m_settings->value("Models/FAI", "./models/fai_xray.onnx").toString();
}

/**
 * @brief 设置FAI模型路径
 * @param path FAI模型路径
 */
void AppConfig::setFaiModelPath(const QString &path)
{
    m_settings->setValue("Models/FAI", path);
}

/**
 * @brief 获取MRI模型路径
 * @return MRI模型路径
 */
QString AppConfig::getMriModelPath() const
{
    return m_settings->value("Models/MRI", "./models/mri_segmentation.onnx").toString();
}

/**
 * @brief 设置MRI模型路径
 * @param path MRI模型路径
 */
void AppConfig::setMriModelPath(const QString &path)
{
    m_settings->setValue("Models/MRI", path);
}

/**
 * @brief 获取置信度阈值
 * @return 置信度阈值
 */
float AppConfig::getConfidenceThreshold() const
{
    return m_settings->value("Inference/ConfidenceThreshold", 0.25f).toFloat();
}

/**
 * @brief 设置置信度阈值
 * @param threshold 置信度阈值
 */
void AppConfig::setConfidenceThreshold(float threshold)
{
    m_settings->setValue("Inference/ConfidenceThreshold", threshold);
}

/**
 * @brief 获取IoU阈值
 * @return IoU阈值
 */
float AppConfig::getIoUThreshold() const
{
    return m_settings->value("Inference/IoUThreshold", 0.45f).toFloat();
}

/**
 * @brief 设置IoU阈值
 * @param threshold IoU阈值
 */
void AppConfig::setIoUThreshold(float threshold)
{
    m_settings->setValue("Inference/IoUThreshold", threshold);
}

/**
 * @brief 获取模型保护密钥
 * @return 模型保护密钥
 */
QString AppConfig::getModelProtectionKey() const
{
    return m_settings->value("Security/ModelProtectionKey", "MedYOLO11Qt_Model_Protection_Key_2024").toString();
}

/**
 * @brief 设置模型保护密钥
 * @param key 模型保护密钥
 */
void AppConfig::setModelProtectionKey(const QString &key)
{
    m_settings->setValue("Security/ModelProtectionKey", key);
}

/**
 * @brief 检查是否启用调试模式
 * @return 是否启用调试模式
 */
bool AppConfig::isDebugModeEnabled() const
{
    return m_settings->value("Debug/Enabled", false).toBool();
}

/**
 * @brief 设置是否启用调试模式
 * @param enabled 是否启用调试模式
 */
void AppConfig::setDebugModeEnabled(bool enabled)
{
    m_settings->setValue("Debug/Enabled", enabled);
}