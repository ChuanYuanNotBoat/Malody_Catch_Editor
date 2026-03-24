#include "DensityCurve.h"
#include "model/Chart.h"
#include "utils/MathUtils.h"
#include <QPainter>
#include <QMouseEvent>
#include <QDebug>

DensityCurve::DensityCurve(QWidget* parent)
    : QWidget(parent), m_chart(nullptr)
{
    setMinimumWidth(40);
    setMaximumWidth(80);
}

void DensityCurve::setChart(const Chart* chart)
{
    m_chart = chart;
    computeDensity();
    update();
}

void DensityCurve::computeDensity()
{
    if (!m_chart || m_chart->notes().isEmpty()) {
        m_densityData.clear();
        return;
    }
    // 简单实现：根据时间范围均匀采样
    double totalDuration = 0;
    const auto& notes = m_chart->notes();
    // 粗略计算总时长（最后一个音符时间 + 1秒）
    double lastTime = 0;
    for (const Note& note : notes) {
        double t = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator,
                                       m_chart->bpmList(), m_chart->meta().offset);
        if (t > lastTime) lastTime = t;
    }
    totalDuration = lastTime + 1000;
    int numBins = width();
    m_densityData.resize(numBins, 0);
    if (totalDuration <= 0) return;
    for (const Note& note : notes) {
        double t = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator,
                                       m_chart->bpmList(), m_chart->meta().offset);
        int bin = static_cast<int>((t / totalDuration) * numBins);
        if (bin >= 0 && bin < numBins)
            m_densityData[bin]++;
    }
    // 归一化
    double maxDensity = 1;
    for (double v : m_densityData)
        if (v > maxDensity) maxDensity = v;
    if (maxDensity > 0) {
        for (double& v : m_densityData)
            v /= maxDensity;
    }
}

void DensityCurve::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), Qt::darkGray);
    if (m_densityData.isEmpty()) return;
    int w = width();
    int h = height();
    for (int i = 0; i < m_densityData.size(); ++i) {
        int barHeight = static_cast<int>(m_densityData[i] * h);
        painter.fillRect(i, h - barHeight, 1, barHeight, Qt::white);
    }
}

void DensityCurve::mousePressEvent(QMouseEvent* event)
{
    if (!m_chart || m_densityData.isEmpty()) return;
    double yRatio = static_cast<double>(event->pos().y()) / height();
    // 粗略映射到时间：密度图 y 轴对应时间，但实际需要更精确
    // 简单发出信号，由外部处理
    double totalDuration = 0;
    const auto& notes = m_chart->notes();
    double lastTime = 0;
    for (const Note& note : notes) {
        double t = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator,
                                       m_chart->bpmList(), m_chart->meta().offset);
        if (t > lastTime) lastTime = t;
    }
    totalDuration = lastTime + 1000;
    double clickTime = totalDuration * yRatio;
    emit timeClicked(clickTime);
}