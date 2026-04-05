#pragma once

#include <QString>
#include <QMutex>
#include <QFile>
#include <QTextStream>
#include <QtMessageHandler>

class Logger {
public:
    enum Level { Debug, Info, Warning, Error };

    /**
     * @brief 初始化日志系统，自动创建日志文件夹
     * 多次调用安全，仅第一次调用会真正初始化
     * @param logsDir 日志文件夹路径（相对于可执行文件目录），默认为 "logs"
     */
    static void init(const QString& logsDir = "logs");

    /**
     * @brief 关闭日志文件
     */
    static void shutdown();

    /**
     * @brief 获取日志文件路径
     */
    static QString logFilePath();

    /**
     * @brief 检查日志系统是否已初始化
     */
    static bool isInitialized();

    /**
     * @brief 写入日志消息
     */
    static void log(Level level, const QString& message);
    static void debug(const QString& msg);
    static void info(const QString& msg);
    static void warn(const QString& msg);
    static void error(const QString& msg);

    /**
     * @brief Qt 消息处理回调（供 qInstallMessageHandler 使用）
     */
    static void qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg);

private:
    static QFile m_file;
    static QTextStream m_stream;
    static QMutex m_mutex;
    static QtMessageHandler m_previousHandler;
    static bool s_initialized;  // 标记是否已初始化
};
