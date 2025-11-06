#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>
#include <QSettings>

/**
 * @brief 应用程序配置管理类
 * @details 单例模式，用于管理应用程序的配置参数
 */
class AppConfig
{
public:
    /**
     * @brief 获取配置管理器的单例实例
     * @return AppConfig的引用
     */
    static AppConfig &instance();

    /**
     * @brief 析构函数，清理资源
     */
    ~AppConfig();

    /**
     * @brief 加载配置文件
     * @param configPath 配置文件路径，如果为空则使用默认路径
     * @return 是否加载成功
     */
    bool loadConfig(const QString &configPath = QString());

    /**
     * @brief 保存当前配置到文件
     * @param configPath 配置文件路径，如果为空则使用当前路径
     * @return 是否保存成功
     */
    bool saveConfig(const QString &configPath = QString());

    // Qt相关配置
    QString getQtInstallPath() const;
    void setQtInstallPath(const QString &path);

    // GDCM相关配置
    QString getGdcmInstallPath() const;
    void setGdcmInstallPath(const QString &path);

    // ONNXRuntime相关配置
    QString getOnnxRuntimeInstallPath() const;
    void setOnnxRuntimeInstallPath(const QString &path);

    // 模型路径相关配置
    QString getFaiModelPath() const;
    void setFaiModelPath(const QString &path);

    QString getMriModelPath() const;
    void setMriModelPath(const QString &path);

    // 推理参数配置
    float getConfidenceThreshold() const;
    void setConfidenceThreshold(float threshold);

    float getIoUThreshold() const;
    void setIoUThreshold(float threshold);

    // 模型保护相关配置
    QString getModelProtectionKey() const;
    void setModelProtectionKey(const QString &key);

    // 调试模式配置
    bool isDebugModeEnabled() const;
    void setDebugModeEnabled(bool enabled);

private:
    // 私有构造函数（单例模式）
    AppConfig();

    // 禁止拷贝和赋值
    AppConfig(const AppConfig &) = delete;
    AppConfig &operator=(const AppConfig &) = delete;

    // 配置存储（改为指针类型）
    QSettings *m_settings;

    // 默认配置路径
    QString m_configFilePath;
};

#endif // APPCONFIG_H