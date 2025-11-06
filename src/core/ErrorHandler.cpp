#include "ErrorHandler.h"
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QIODevice>
#include <QTextStream>

/**
 * @brief 获取错误处理器的单例实例
 * @return ErrorHandler的引用
 */
ErrorHandler &ErrorHandler::instance()
{
    static ErrorHandler handler;
    return handler;
}

/**
 * @brief 构造函数，初始化默认设置
 */
ErrorHandler::ErrorHandler()
    : m_consoleOutputEnabled(true),
      m_logLevel(ErrorType::INFO)
{
    // 设置默认日志文件路径（应用程序目录下的logs文件夹）
    QString appDir = QCoreApplication::applicationDirPath();
    QString logDir = appDir + "/logs";

    // 创建日志目录
    QDir dir;
    if (!dir.exists(logDir))
    {
        dir.mkpath(logDir);
    }

    // 设置日志文件名称（当前日期）
    QString currentDate = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    m_logFilePath = logDir + "/app_" + currentDate + ".log";
}

/**
 * @brief 设置日志文件路径
 * @param logPath 日志文件路径
 */
void ErrorHandler::setLogFilePath(const QString &logPath)
{
    QMutexLocker locker(&m_mutex);

    // 如果文件已打开，先关闭
    if (m_logFile.isOpen())
    {
        m_logFile.close();
    }

    m_logFilePath = logPath;
}

/**
 * @brief 设置是否在控制台显示日志
 * @param enable 是否启用控制台输出
 */
void ErrorHandler::setConsoleOutputEnabled(bool enable)
{
    m_consoleOutputEnabled = enable;
}

/**
 * @brief 设置日志级别
 * @param level 最低记录的日志级别
 */
void ErrorHandler::setLogLevel(ErrorType level)
{
    m_logLevel = level;
}

/**
 * @brief 记录一条日志
 * @param type 错误类型
 * @param message 错误消息
 * @param module 产生错误的模块
 * @param code 错误代码
 */
void ErrorHandler::log(ErrorType type, const QString &message,
                       const QString &module, int code)
{
    // 检查日志级别
    if (type > m_logLevel)
    {
        return;
    }

    // 格式化日志条目
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString logType = errorTypeToString(type);
    QString logEntry = QString("[%1] [%2] [%3] %4").arg(timestamp, logType, module, message);

    // 如果有错误代码，添加到日志
    if (code != 0)
    {
        logEntry += QString(" [Code: %1]").arg(code);
    }

    // 线程安全地写入日志文件
    writeToLogFile(logEntry);

    // 如果启用了控制台输出，打印到控制台
    if (m_consoleOutputEnabled)
    {
        switch (type)
        {
        case ErrorType::INFO:
            qInfo().noquote() << logEntry;
            break;
        case ErrorType::WARNING:
            qWarning().noquote() << logEntry;
            break;
        case ErrorType::ERROR:
        case ErrorType::CRITICAL:
            qCritical().noquote() << logEntry;
            break;
        case ErrorType::DEBUG:
            qDebug().noquote() << logEntry;
            break;
        }
    }

    // 发出信号，通知观察者
    emit errorLogged(type, message, module, code);
}

/**
 * @brief 记录信息日志
 * @param message 信息消息
 * @param module 模块名称
 */
void ErrorHandler::info(const QString &message, const QString &module)
{
    log(ErrorType::INFO, message, module);
}

/**
 * @brief 记录警告日志
 * @param message 警告消息
 * @param module 模块名称
 * @param code 警告代码
 */
void ErrorHandler::warning(const QString &message, const QString &module, int code)
{
    log(ErrorType::WARNING, message, module, code);
}

/**
 * @brief 记录错误日志
 * @param message 错误消息
 * @param module 模块名称
 * @param code 错误代码
 */
void ErrorHandler::error(const QString &message, const QString &module, int code)
{
    log(ErrorType::ERROR, message, module, code);
}

/**
 * @brief 记录严重错误日志
 * @param message 严重错误消息
 * @param module 模块名称
 * @param code 错误代码
 */
void ErrorHandler::critical(const QString &message, const QString &module, int code)
{
    log(ErrorType::CRITICAL, message, module, code);
}

/**
 * @brief 记录调试日志
 * @param message 调试消息
 * @param module 模块名称
 */
void ErrorHandler::debug(const QString &message, const QString &module)
{
    log(ErrorType::DEBUG, message, module);
}

/**
 * @brief 将错误类型转换为字符串
 * @param type 错误类型
 * @return 错误类型的字符串表示
 */
QString ErrorHandler::errorTypeToString(ErrorType type) const
{
    switch (type)
    {
    case ErrorType::INFO:
        return "INFO";
    case ErrorType::WARNING:
        return "WARNING";
    case ErrorType::ERROR:
        return "ERROR";
    case ErrorType::CRITICAL:
        return "CRITICAL";
    case ErrorType::DEBUG:
        return "DEBUG";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief 写入日志到文件
 * @param logEntry 日志条目
 */
void ErrorHandler::writeToLogFile(const QString &logEntry)
{
    QMutexLocker locker(&m_mutex);

    // 打开日志文件（如果未打开）
    if (!m_logFile.isOpen())
    {
        m_logFile.setFileName(m_logFilePath);
        if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        {
            // 如果无法打开日志文件，只能输出到控制台
            qWarning() << "Failed to open log file:" << m_logFilePath;
            return;
        }
    }

    // 写入日志条目
    QTextStream out(&m_logFile);
    out << logEntry << Qt::endl;
    out.flush();

    // 检查文件大小，防止日志文件过大
    if (m_logFile.size() > 10 * 1024 * 1024) // 10MB
    {
        m_logFile.close();

        // 创建新的日志文件，使用时间戳作为后缀
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
        QString backupPath = m_logFilePath + "." + timestamp + ".bak";

        QFile::rename(m_logFilePath, backupPath);
    }
}

// 为了Qt的信号槽系统，需要包含这个宏
#include "moc_ErrorHandler.cpp"