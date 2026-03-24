#include "Logger.h"
#include <QDateTime>
#include <QTextStream>
#include <QDebug>

QFile Logger::m_file;
QMutex Logger::m_mutex;

void Logger::init(const QString& logFilePath) {
    m_file.setFileName(logFilePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        qWarning() << "Failed to open log file:" << logFilePath;
    }
}

void Logger::log(Level level, const QString& message) {
    QMutexLocker locker(&m_mutex);
    if (!m_file.isOpen()) return;
    QTextStream out(&m_file);
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " ";
    switch (level) {
    case Debug: out << "[DEBUG] "; break;
    case Info:  out << "[INFO] "; break;
    case Warning: out << "[WARN] "; break;
    case Error: out << "[ERROR] "; break;
    }
    out << message << Qt::endl;
    qDebug() << message;
}

void Logger::debug(const QString& msg) { log(Debug, msg); }
void Logger::info(const QString& msg) { log(Info, msg); }
void Logger::warn(const QString& msg) { log(Warning, msg); }
void Logger::error(const QString& msg) { log(Error, msg); }