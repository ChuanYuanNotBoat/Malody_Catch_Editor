#include "PerformanceTimer.h"
#include "Logger.h"

// 静态成员初始化
bool PerformanceTimer::s_enabled = true;
QHash<QString, PerformanceTimer::Statistics> PerformanceTimer::s_statistics;

PerformanceTimer::PerformanceTimer(const QString &operationName,
                                   const QString &category,
                                   bool autoLog)
    : m_operationName(operationName), m_category(category), m_autoLog(autoLog), m_counted(false)
{
    if (s_enabled)
    {
        m_timer.start();
    }
}

PerformanceTimer::~PerformanceTimer()
{
    if (s_enabled && m_timer.isValid())
    {
        qint64 elapsedMs = m_timer.elapsed();

        // 记录统计信息
        recordResult(elapsedMs);

        // 输出日志
        if (m_autoLog)
        {
            outputLog(elapsedMs);
        }
    }
}

qint64 PerformanceTimer::elapsed()
{
    if (!s_enabled || !m_timer.isValid())
    {
        return -1;
    }
    return m_timer.elapsed();
}

void PerformanceTimer::restart()
{
    m_timer.start();
    m_counted = false;
}

void PerformanceTimer::disableAutoLog()
{
    m_autoLog = false;
}

void PerformanceTimer::setEnabled(bool enabled)
{
    s_enabled = enabled;
}

bool PerformanceTimer::isEnabled()
{
    return s_enabled;
}

void PerformanceTimer::recordResult(qint64 elapsedMs)
{
    if (m_counted)
        return; // 避免重复计入
    m_counted = true;

    if (!s_statistics.contains(m_category))
    {
        s_statistics[m_category] = Statistics();
    }

    Statistics &stats = s_statistics[m_category];

    if (stats.count == 0)
    {
        stats.minTime = elapsedMs;
        stats.maxTime = elapsedMs;
    }
    else
    {
        if (elapsedMs < stats.minTime)
        {
            stats.minTime = elapsedMs;
        }
        if (elapsedMs > stats.maxTime)
        {
            stats.maxTime = elapsedMs;
        }
    }

    stats.totalTime += elapsedMs;
    stats.count++;
    stats.averageTime = static_cast<double>(stats.totalTime) / stats.count;
}

void PerformanceTimer::outputLog(qint64 elapsedMs)
{
    QString logMsg = QString("PERF [%1] %2ms").arg(m_operationName, QString::number(elapsedMs));
    Logger::debug(logMsg);
}

QString PerformanceTimer::Statistics::toString() const
{
    return QString("count=%1, min=%2ms, max=%3ms, avg=%.2f ms, total=%4ms")
        .arg(count)
        .arg(minTime)
        .arg(maxTime)
        .arg(averageTime)
        .arg(totalTime);
}

PerformanceTimer::Statistics PerformanceTimer::getStatistics(const QString &category)
{
    if (s_statistics.contains(category))
    {
        return s_statistics[category];
    }
    return Statistics();
}

QHash<QString, PerformanceTimer::Statistics> PerformanceTimer::getAllStatistics()
{
    return s_statistics;
}

void PerformanceTimer::clearStatistics()
{
    s_statistics.clear();
}

void PerformanceTimer::logAllStatistics()
{
    Logger::info("========== Performance Statistics ==========");
    for (auto it = s_statistics.constBegin(); it != s_statistics.constEnd(); ++it)
    {
        QString msg = QString("Category [%1]: %2").arg(it.key(), it.value().toString());
        Logger::info(msg);
    }
    Logger::info("============================================");
}
