#pragma once

#include <QString>
#include <QMutex>
#include <QFile>

class Logger {
public:
    enum Level { Debug, Info, Warning, Error };

    static void init(const QString& logFilePath);
    static void log(Level level, const QString& message);
    static void debug(const QString& msg);
    static void info(const QString& msg);
    static void warn(const QString& msg);
    static void error(const QString& msg);

private:
    static QFile m_file;
    static QMutex m_mutex;
};