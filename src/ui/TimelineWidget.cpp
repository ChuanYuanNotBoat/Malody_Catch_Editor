#include "TimelineWidget.h"
#include "controller/ChartController.h"
#include "controller/PlaybackController.h"
#include "utils/MathUtils.h"
#include <QPainter>
#include <QMouseEvent>
#include <QStyleOption>

TimelineWidget::TimelineWidget(QWidget* parent)
    : QWidget(parent), m_chartController(nullptr), m_playbackController(nullptr), m_currentTime(0), m_totalDuration(0), m_offset(0)
{
    setFixedHeight(30);
}

void TimelineWidget::setChartController(ChartController* controller)
{
    m_chartController = controller;
    m_offset = 0;
    // 计算总时长
    if (controller && controller->chart()) {
        m_offset = controller->chart()->meta().offset;
        if (!controller->chart()->notes().isEmpty()) {
            double maxChartTime = 0; // 最大谱面时间（不含offset）
            for (const Note& note : controller->chart()->notes()) {
                double t = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator,
                                               controller->chart()->bpmList(), 0); // 不使用offset
                if (t > maxChartTime) maxChartTime = t;
            }
            m_totalDuration = maxChartTime + m_offset + 1000; // 总时长 = 谱面时间 + offset + 缓冲
        } else {
            m_totalDuration = 60000 + m_offset; // 默认1分钟加上offset
        }
    } else {
        m_totalDuration = 60000; // 默认1分钟
    }
    update();
}

void TimelineWidget::setPlaybackController(PlaybackController* controller)
{
    m_playbackController = controller;
    connect(controller, &PlaybackController::positionChanged, this, &TimelineWidget::updateFromPlayback);
}

void TimelineWidget::updateFromPlayback(double timeMs)
{
    // 音频位置转谱面时间：减去offset
    m_currentTime = timeMs - m_offset;
    if (m_currentTime < 0) m_currentTime = 0;
    update();
}

void TimelineWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(40, 40, 40));
    if (m_totalDuration <= 0) return;
    // 画时间标尺
    int totalPixels = width();
    double pixelPerMs = totalPixels / m_totalDuration;
    // 画刻度线（每秒）
    for (int sec = 0; sec <= static_cast<int>(m_totalDuration / 1000); ++sec) {
        int x = static_cast<int>(sec * 1000 * pixelPerMs);
        painter.setPen(Qt::gray);
        painter.drawLine(x, 0, x, height());
        painter.drawText(x + 2, 12, QString::number(sec));
    }
    // 画播放头（需要转换为音频位置）
    double audioPos = m_currentTime + m_offset;
    int headX = static_cast<int>(audioPos * pixelPerMs);
    painter.setPen(Qt::red);
    painter.drawLine(headX, 0, headX, height());
}

void TimelineWidget::mousePressEvent(QMouseEvent* event)
{
    if (!m_playbackController) return;
    double ratio = static_cast<double>(event->pos().x()) / width();
    // 谱面时间 = 比例 * (总时长 - offset)
    double chartTime = ratio * (m_totalDuration - m_offset);
    // 音频位置 = 谱面时间 + offset
    double audioPos = chartTime + m_offset;
    m_playbackController->seekTo(audioPos);
    update();
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        double ratio = static_cast<double>(event->pos().x()) / width();
        // 谱面时间 = 比例 * (总时长 - offset)
        double chartTime = ratio * (m_totalDuration - m_offset);
        // 音频位置 = 谱面时间 + offset
        double audioPos = chartTime + m_offset;
        m_playbackController->seekTo(audioPos);
        update();
    }
}