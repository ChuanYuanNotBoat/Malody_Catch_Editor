#include "Logger.h"
#include <QDateTime>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <iostream>

QFile Logger::m_file;
QTextStream Logger::m_stream;
QMutex Logger::m_mutex;
QtMessageHandler Logger::m_previousHandler = nullptr;
bool Logger::s_initialized = false;

void Logger::init(const QString& logsDir) {
    QMutexLocker locker(&m_mutex);

    // 如果已经初始化过，则直接返回
    if (s_initialized) {
        return;
    }

    // 获取可执行文件所在目录
    QString appDir = QCoreApplication::applicationDirPath();
    QString logDirPath = appDir + "/" + logsDir;

    // 创建日志文件夹
    QDir logDir(logDirPath);
    if (!logDir.exists()) {
        if (!logDir.mkpath(".")) {
            std::cerr << "Failed to create log directory: " << logDirPath.toStdString() << std::endl;
            return;
        }
    }

    // 创建日志文件，使用日期和时间作为文件名
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    QString logFilePath = logDirPath + "/CatchEditor_" + timestamp + ".log";

    // 打开日志文件
    m_file.setFileName(logFilePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
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

    // 安装 Qt 消息处理器
    m_previousHandler = qInstallMessageHandler(qtMessageHandler);

    // 标记为已初始化
    s_initialized = true;

    std::cout << "Log file created: " << logFilePath.toStdString() << std::endl;
}

void Logger::shutdown() {
    QMutexLocker locker(&m_mutex);
    if (m_file.isOpen()) {
        m_stream << "======================================" << Qt::endl;
        m_stream << "Log ended at " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << Qt::endl;
        m_stream << "======================================" << Qt::endl;
        m_stream.flush();
        m_file.close();
    }
    // 恢复之前的消息处理器
    if (m_previousHandler) {
        qInstallMessageHandler(m_previousHandler);
    }
}

QString Logger::logFilePath() {
    return m_file.fileName();
}

bool Logger::isInitialized() {
    return s_initialized;
}

void Logger::log(Level level, const QString& message) {
    QMutexLocker locker(&m_mutex);
    if (!m_file.isOpen()) {
        qDebug() << message;
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString levelStr;
    switch (level) {
        case Debug:   levelStr = "[DEBUG] "; break;
        case Info:    levelStr = "[INFO] "; break;
        case Warning: levelStr = "[WARN] "; break;
        case Error:   levelStr = "[ERROR] "; break;
    }

    m_stream << timestamp << " " << levelStr << message << Qt::endl;
    m_stream.flush();

    // 同时输出到控制台
    std::cout << timestamp.toStdString() << " " << levelStr.toStdString() << message.toStdString() << std::endl;
}

void Logger::debug(const QString& msg) { log(Debug, msg); }
void Logger::info(const QString& msg) { log(Info, msg); }
void Logger::warn(const QString& msg) { log(Warning, msg); }
void Logger::error(const QString& msg) { log(Error, msg); }

void Logger::qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    QMutexLocker locker(&m_mutex);

    // 格式化消息
    QString formattedMsg = msg;
    if (context.file && context.line > 0) {
        formattedMsg = QString("[%1:%2] %3").arg(context.file).arg(context.line).arg(msg);
    }

    // 根据消息类型写入日志
    Level level = Debug;
    switch (type) {
        case QtDebugMsg:    level = Debug; break;
        case QtInfoMsg:     level = Info; break;
        case QtWarningMsg:  level = Warning; break;
        case QtCriticalMsg: level = Error; break;
        case QtFatalMsg:    level = Error; break;
    }

    if (m_file.isOpen()) {
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
        QString levelStr;
        switch (level) {
            case Debug:   levelStr = "[DEBUG] "; break;
            case Info:    levelStr = "[INFO] "; break;
            case Warning: levelStr = "[WARN] "; break;
            case Error:   levelStr = "[ERROR] "; break;
        }
        m_stream << timestamp << " " << levelStr << formattedMsg << Qt::endl;
        m_stream.flush();
    }

    // 同时输出到控制台
    std::cerr << formattedMsg.toStdString() << std::endl;

    // 调用之前的处理器
    if (m_previousHandler) {
        m_previousHandler(type, context, msg);
    }
}
