#include "Logger.h"
#include <QDateTime>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>
#include <iostream>

QFile Logger::m_file;
QTextStream Logger::m_stream;
QMutex Logger::m_mutex;
QtMessageHandler Logger::m_previousHandler = nullptr;
bool Logger::s_initialized = false;
bool Logger::s_verbose = false; // 默认不输出详细日志，减少日志量
QString Logger::s_logsDir = "logs";

// JSON日志相关静态成员
bool Logger::s_jsonLoggingEnabled = false;
QFile Logger::m_jsonFile;
QTextStream Logger::m_jsonStream;

void Logger::init(const QString &logsDir)
{
    QMutexLocker locker(&m_mutex);

    // 如果已经初始化过，则直接返回
    if (s_initialized)
    {
        return;
    }

    // 保存日志文件夹路径
    s_logsDir = logsDir;

    // 获取可执行文件所在目录
    QString appDir = QCoreApplication::applicationDirPath();
    QString logDirPath = appDir + "/" + logsDir;

    // 创建日志文件夹
    QDir logDir(logDirPath);
    if (!logDir.exists())
    {
        if (!logDir.mkpath("."))
        {
            std::cerr << "Failed to create log directory: " << logDirPath.toStdString() << std::endl;
            return;
        }
    }

    // 创建日志文件，使用日期和时间作为文件名
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    QString logFilePath = logDirPath + "/CatchEditor_" + timestamp + ".log";

    // 打开日志文件
    m_file.setFileName(logFilePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
    {
        std::cerr << "Failed to open log file: " << logFilePath.toStdString() << std::endl;
        return;
    }

    m_stream.setDevice(&m_file);
    m_stream << "======================================" << Qt::endl;
    m_stream << "Log started at " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << Qt::endl;
    m_stream << "Application: " << QCoreApplication::applicationName() << Qt::endl;
    m_stream << "Version: " << QCoreApplication::applicationVersion() << Qt::endl;
    m_stream << "======================================" << Qt::endl;
    m_stream.flush();

    // 初始化JSON日志文件（默认可选）
    if (s_jsonLoggingEnabled)
    {
        QString jsonLogPath = logDirPath + "/CatchEditor_" + timestamp + ".jsonl";
        m_jsonFile.setFileName(jsonLogPath);
        if (m_jsonFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
        {
            m_jsonStream.setDevice(&m_jsonFile);
            m_jsonStream.flush();
        }
    }

    // 安装 Qt 消息处理器
    m_previousHandler = qInstallMessageHandler(qtMessageHandler);

    // 标记为已初始化
    s_initialized = true;

    std::cout << "Log file created: " << logFilePath.toStdString() << std::endl;
}

void Logger::shutdown()
{
    QMutexLocker locker(&m_mutex);
    if (m_file.isOpen())
    {
        m_stream << "======================================" << Qt::endl;
        m_stream << "Log ended at " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << Qt::endl;
        m_stream << "======================================" << Qt::endl;
        m_stream.flush();
        m_file.close();
    }

    // 关闭JSON日志文件
    if (m_jsonFile.isOpen())
    {
        m_jsonFile.close();
    }

    // 恢复之前的消息处理器
    if (m_previousHandler)
    {
        qInstallMessageHandler(m_previousHandler);
    }
}

QString Logger::logFilePath()
{
    return m_file.fileName();
}

bool Logger::isInitialized()
{
    return s_initialized;
}

void Logger::setVerbose(bool verbose)
{
    QMutexLocker locker(&m_mutex);
    s_verbose = verbose;
}

bool Logger::isVerbose()
{
    return s_verbose;
}

void Logger::log(Level level, const QString &message)
{
    QMutexLocker locker(&m_mutex);
    // 若非详细模式且为调试级别，则跳过
    if (level == Debug && !s_verbose)
    {
        return;
    }
    if (!m_file.isOpen())
    {
        qDebug() << message;
        return;
    }

    // 检查日志文件大小，如果超过限制则轮换
    rotateLogIfNeeded();

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString levelStr;
    switch (level)
    {
    case Debug:
        levelStr = "[DEBUG] ";
        break;
    case Info:
        levelStr = "[INFO] ";
        break;
    case Warning:
        levelStr = "[WARN] ";
        break;
    case Error:
        levelStr = "[ERROR] ";
        break;
    }

    m_stream << timestamp << " " << levelStr << message << Qt::endl;
    m_stream.flush();

    // 同时输出到控制台
    std::cout << timestamp.toStdString() << " " << levelStr.toStdString() << message.toStdString() << std::endl;
}

void Logger::debug(const QString &msg) { log(Debug, msg); }
void Logger::info(const QString &msg) { log(Info, msg); }
void Logger::warn(const QString &msg) { log(Warning, msg); }
void Logger::error(const QString &msg) { log(Error, msg); }

void Logger::qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QMutexLocker locker(&m_mutex);

    // 格式化消息
    QString formattedMsg = msg;
    if (context.file && context.line > 0)
    {
        formattedMsg = QString("[%1:%2] %3").arg(context.file).arg(context.line).arg(msg);
    }

    // 根据消息类型写入日志
    Level level = Debug;
    switch (type)
    {
    case QtDebugMsg:
        level = Debug;
        break;
    case QtInfoMsg:
        level = Info;
        break;
    case QtWarningMsg:
        level = Warning;
        break;
    case QtCriticalMsg:
        level = Error;
        break;
    case QtFatalMsg:
        level = Error;
        break;
    }

    if (m_file.isOpen())
    {
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
        QString levelStr;
        switch (level)
        {
        case Debug:
            levelStr = "[DEBUG] ";
            break;
        case Info:
            levelStr = "[INFO] ";
            break;
        case Warning:
            levelStr = "[WARN] ";
            break;
        case Error:
            levelStr = "[ERROR] ";
            break;
        }
        m_stream << timestamp << " " << levelStr << formattedMsg << Qt::endl;
        m_stream.flush();
    }

    // 同时输出到控制台
    std::cerr << formattedMsg.toStdString() << std::endl;

    // 调用之前的处理器
    if (m_previousHandler)
    {
        m_previousHandler(type, context, msg);
    }
}

void Logger::rotateLogIfNeeded()
{
    // 检查当前日志文件大小
    if (!m_file.isOpen())
    {
        return;
    }

    qint64 fileSize = m_file.size();
    if (fileSize < MAX_LOG_FILE_SIZE)
    {
        return; // 文件大小未超过限制，无需轮换
    }

    // 关闭当前日志文件
    m_stream << "======================================" << Qt::endl;
    m_stream << "Log rotated at " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << Qt::endl;
    m_stream << "======================================" << Qt::endl;
    m_stream.flush();
    m_file.close();

    // 创建新的日志文件
    QString appDir = QCoreApplication::applicationDirPath();
    QString logDirPath = appDir + "/" + s_logsDir;

    // 生成新文件名，添加序列号以避免覆盖
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss-zzz");
    QString newLogFilePath = logDirPath + "/CatchEditor_" + timestamp + ".log";

    m_file.setFileName(newLogFilePath);
    if (m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
    {
        m_stream.setDevice(&m_file);
        m_stream << "======================================" << Qt::endl;
        m_stream << "Log rotated from previous file" << Qt::endl;
        m_stream << "Rotated at " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << Qt::endl;
        m_stream << "======================================" << Qt::endl;
        m_stream.flush();

        // 输出到控制台
        std::cout << "Log file rotated to: " << newLogFilePath.toStdString() << std::endl;
    }
    else
    {
        // 如果打开失败，尝试恢复
        std::cerr << "Failed to open new log file: " << newLogFilePath.toStdString() << std::endl;
    }
}

// JSON日志功能实现

void Logger::setJsonLoggingEnabled(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    s_jsonLoggingEnabled = enabled;
}

bool Logger::isJsonLoggingEnabled()
{
    return s_jsonLoggingEnabled;
}

QString Logger::jsonLogFilePath()
{
    return m_jsonFile.fileName();
}

void Logger::logStructured(Level level,
                           const QString &message,
                           const QString &module,
                           const QMap<QString, QString> &context)
{
    QMutexLocker locker(&m_mutex);

    // 先输出到普通日志
    log(level, message);

    // 如果启用JSON日志，输出JSON格式
    if (!s_jsonLoggingEnabled || !m_jsonFile.isOpen())
    {
        return;
    }

    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

    QString levelStr;
    switch (level)
    {
    case Debug:
        levelStr = "DEBUG";
        break;
    case Info:
        levelStr = "INFO";
        break;
    case Warning:
        levelStr = "WARN";
        break;
    case Error:
        levelStr = "ERROR";
        break;
    }
    logEntry["level"] = levelStr;
    logEntry["module"] = module;
    logEntry["message"] = message;

    // 添加上下文信息
    QJsonObject contextObj;
    for (auto it = context.constBegin(); it != context.constEnd(); ++it)
    {
        contextObj[it.key()] = it.value();
    }
    logEntry["context"] = contextObj;

    QJsonDocument doc(logEntry);
    m_jsonStream << doc.toJson(QJsonDocument::Compact) << "\n";
    m_jsonStream.flush();
}
