#ifndef ERRORHANDLER_H
#define ERRORHANDLER_H

#include <QString>
#include <QObject>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QMutex>

/**
 * @brief 错误类型枚举
 */
enum class ErrorType
{
    INFO,
    WARNING,
    ERROR,
    CRITICAL,
    DEBUG
};

/**
 * @brief 错误处理类
 * @details 单例模式，用于统一管理和记录应用程序中的错误和日志
 */
class ErrorHandler : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 获取错误处理器的单例实例
     * @return ErrorHandler的引用
     */
    static ErrorHandler &instance();

    /**
     * @brief 设置日志文件路径
     * @param logPath 日志文件路径
     */
    void setLogFilePath(const QString &logPath);

    /**
     * @brief 设置是否在控制台显示日志
     * @param enable 是否启用控制台输出
     */
    void setConsoleOutputEnabled(bool enable);

    /**
     * @brief 设置日志级别
     * @param level 最低记录的日志级别
     */
    void setLogLevel(ErrorType level);

    /**
     * @brief 记录一条日志
     * @param type 错误类型
     * @param message 错误消息
     * @param module 产生错误的模块
     * @param code 错误代码
     */
    void log(ErrorType type, const QString &message,
             const QString &module = "General", int code = 0);

    /**
     * @brief 记录信息日志
     * @param message 信息消息
     * @param module 模块名称
     */
    void info(const QString &message, const QString &module = "General");

    /**
     * @brief 记录警告日志
     * @param message 警告消息
     * @param module 模块名称
     * @param code 警告代码
     */
    void warning(const QString &message, const QString &module = "General", int code = 0);

    /**
     * @brief 记录错误日志
     * @param message 错误消息
     * @param module 模块名称
     * @param code 错误代码
     */
    void error(const QString &message, const QString &module = "General", int code = 0);

    /**
     * @brief 记录严重错误日志
     * @param message 严重错误消息
     * @param module 模块名称
     * @param code 错误代码
     */
    void critical(const QString &message, const QString &module = "General", int code = 0);

    /**
     * @brief 记录调试日志
     * @param message 调试消息
     * @param module 模块名称
     */
    void debug(const QString &message, const QString &module = "General");

signals:
    /**
     * @brief 当新的错误被记录时发出的信号
     * @param type 错误类型
     * @param message 错误消息
     * @param module 模块名称
     * @param code 错误代码
     */
    void errorLogged(ErrorType type, const QString &message,
                     const QString &module, int code);

private:
    // 私有构造函数（单例模式）
    ErrorHandler();

    // 禁止拷贝和赋值
    ErrorHandler(const ErrorHandler &) = delete;
    ErrorHandler &operator=(const ErrorHandler &) = delete;

    // 将错误类型转换为字符串
    QString errorTypeToString(ErrorType type) const;

    // 写入日志到文件
    void writeToLogFile(const QString &logEntry);

    // 成员变量
    QString m_logFilePath;
    QFile m_logFile;
    QMutex m_mutex; // 用于线程安全
    bool m_consoleOutputEnabled;
    ErrorType m_logLevel;
};

// 便捷宏定义
#define LOG_INFO(msg, module) ErrorHandler::instance().info(msg, module)
#define LOG_WARNING(msg, module, code) ErrorHandler::instance().warning(msg, module, code)
#define LOG_ERROR(msg, module, code) ErrorHandler::instance().error(msg, module, code)
#define LOG_CRITICAL(msg, module, code) ErrorHandler::instance().critical(msg, module, code)
#define LOG_DEBUG(msg, module) ErrorHandler::instance().debug(msg, module)

#endif // ERRORHANDLER_H