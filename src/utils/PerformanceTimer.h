#pragma once

#include <QString>
#include <QElapsedTimer>
#include <QHash>

/**
 * @brief RAII风格的性能计时器
 * 
 * 在构造时记录开始时间，在析构时自动记录到日志系统
 * 支持嵌套计时和统计（min/max/avg）
 * 
 * 使用示例：
 * {
 *     PerformanceTimer timer("ChartIO::load", "chart_loading");
 *     // ... 需要计时的代码 ...
 * }  // 析构时自动输出日志
 * 
 * 获取统计信息：
 * const auto& stats = PerformanceTimer::getStatistics("chart_loading");
 * qDebug() << "平均耗时:" << stats.averageTime << "ms";
 */
class PerformanceTimer {
public:
    /**
     * @brief 构造函数：开始计时
     * @param operationName 操作名称，用于日志输出（如"ChartIO::load"）
     * @param category 分类名称，用于统计分组（如"chart_loading"）
     * @param autoLog 是否在析构时自动输出日志，默认为true
     */
    PerformanceTimer(const QString& operationName, 
                     const QString& category = "default",
                     bool autoLog = true);

    /**
     * @brief 析构函数：结束计时并可选地输出日志
     */
    ~PerformanceTimer();

    /**
     * @brief 手动停止计时并获取耗时
     * @return 耗时的毫秒数
     */
    qint64 elapsed();

    /**
     * @brief 重新开始计时（重置计时器）
     */
    void restart();

    /**
     * @brief 禁用自动日志输出（用于不需要输出的场景）
     */
    void disableAutoLog();

    // 静态方法：统计和配置

    /**
     * @brief 启用/禁用整个计时系统
     */
    static void setEnabled(bool enabled);

    /**
     * @brief 获取计时系统是否启用
     */
    static bool isEnabled();

    /**
     * @brief 统计信息结构体
     */
    struct Statistics {
        qint64 minTime = 0;      ///< 最小耗时（毫秒）
        qint64 maxTime = 0;      ///< 最大耗时（毫秒）
        qint64 totalTime = 0;    ///< 总耗时（毫秒）
        int count = 0;           ///< 调用次数
        double averageTime = 0;  ///< 平均耗时（毫秒）

        /**
         * @brief 转换为易读的字符串
         */
        QString toString() const;
    };

    /**
     * @brief 获取指定分类的统计信息
     * @param category 分类名称
     * @return 统计信息，如果分类不存在则返回空的Statistics
     */
    static Statistics getStatistics(const QString& category);

    /**
     * @brief 获取所有分类的统计信息
     */
    static QHash<QString, Statistics> getAllStatistics();

    /**
     * @brief 清除所有统计信息
     */
    static void clearStatistics();

    /**
     * @brief 输出所有分类的统计信息到日志
     */
    static void logAllStatistics();

private:
    QString m_operationName;
    QString m_category;
    QElapsedTimer m_timer;
    bool m_autoLog;
    bool m_counted;  ///< 标记是否已计入统计

    static bool s_enabled;  ///< 全局启用/禁用开关
    static QHash<QString, Statistics> s_statistics;  ///< 分类统计数据
    
    /**
     * @brief 内部方法：记录一次计时结果
     */
    void recordResult(qint64 elapsedMs);

    /**
     * @brief 内部方法：输出到日志
     */
    void outputLog(qint64 elapsedMs);
};
