#include "ui/CustomWidgets/RealtimePreviewWidget.h"
#include "controller/ChartController.h"
#include "controller/PlaybackController.h"
#include "model/Chart.h"
#include "model/Skin.h"
#include "render/HyperfruitDetector.h"
#include "render/NoteRenderer.h"
#include "utils/MathUtils.h"
#include <cmath>
#include <limits>
#include <QPainter>
#include <QPen>

RealtimePreviewWidget::RealtimePreviewWidget(QWidget *parent)
    : QWidget(parent),
      m_noteRenderer(new NoteRenderer()),
      m_hyperfruitDetector(new HyperfruitDetector())
{
    setMinimumWidth(170);
    setMaximumWidth(260);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    m_hyperfruitDetector->setCS(3.2);
    // Preview uses unified fallback style only (no skin texture).
    m_noteRenderer->setSkin(nullptr);
    m_noteRenderer->setShowColors(false);
}

RealtimePreviewWidget::~RealtimePreviewWidget()
{
    delete m_noteRenderer;
    delete m_hyperfruitDetector;
}

void RealtimePreviewWidget::setChartController(ChartController *controller)
{
    if (m_chartController == controller)
        return;

    if (m_chartController)
        disconnect(m_chartController, nullptr, this, nullptr);

    m_chartController = controller;
    invalidateNoteCache();
    invalidateHyperCache();
    if (m_chartController)
    {
        connect(m_chartController, &ChartController::chartChanged, this, [this]() {
            invalidateNoteCache();
            invalidateHyperCache();
            update();
        });
    }
    update();
}

void RealtimePreviewWidget::setPlaybackController(PlaybackController *controller)
{
    if (m_playbackController == controller)
        return;

    if (m_playbackController)
        disconnect(m_playbackController, nullptr, this, nullptr);

    m_playbackController = controller;
    if (m_playbackController)
    {
        connect(m_playbackController, &PlaybackController::positionChanged, this, [this](double timeMs) {
            m_currentTimeMs = qMax(0.0, timeMs);
            update();
        });
    }
}

void RealtimePreviewWidget::setSkin(Skin *skin)
{
    Q_UNUSED(skin);
    // Intentionally ignore external skin to keep preview style consistent.
    m_noteRenderer->setSkin(nullptr);
    update();
}

void RealtimePreviewWidget::setColorMode(bool enabled)
{
    Q_UNUSED(enabled);
    // Keep preview in fixed monochrome style.
    m_colorMode = false;
    m_noteRenderer->setShowColors(false);
}

void RealtimePreviewWidget::setHyperfruitEnabled(bool enabled)
{
    m_hyperfruitEnabled = enabled;
    m_noteRenderer->setHyperfruitEnabled(enabled);
    invalidateHyperCache();
    update();
}

void RealtimePreviewWidget::setNoteSize(int size)
{
    m_noteRenderer->setNoteSize(size);
    update();
}

void RealtimePreviewWidget::setCurrentBeat(double beat)
{
    if (!m_chartController || !m_chartController->chart())
        return;
    // Avoid fighting playback time updates while audio is running.
    if (m_playbackController && m_playbackController->state() == PlaybackController::Playing)
        return;

    m_currentTimeMs = beatToTimeMs(beat);
    update();
}

void RealtimePreviewWidget::setCurrentTimeMs(double timeMs)
{
    m_currentTimeMs = qMax(0.0, timeMs);
    update();
}

double RealtimePreviewWidget::beatToTimeMs(double beat)
{
    ensureNoteCache();
    if (m_bpmSegments.isEmpty())
        return 0.0;

    int lo = 0;
    int hi = m_bpmSegments.size() - 1;
    while (lo < hi)
    {
        const int mid = (lo + hi + 1) / 2;
        if (m_bpmSegments[mid].beatPos <= beat)
            lo = mid;
        else
            hi = mid - 1;
    }

    const auto &seg = m_bpmSegments[lo];
    if (seg.bpm <= 0.0)
        return seg.accumulatedMs;
    return seg.accumulatedMs + (beat - seg.beatPos) * (60000.0 / seg.bpm);
}

double RealtimePreviewWidget::timeToY(double timeMs,
                                      const QRectF &laneRect,
                                      double referenceY,
                                      double upperSpanMs,
                                      double lowerSpanMs) const
{
    const double spanMs = qMax(1.0, qMax(upperSpanMs, lowerSpanMs));
    const double pxPerMs = qMax(1.0, laneRect.height()) / spanMs;
    return referenceY - (timeMs - m_currentTimeMs) * pxPerMs;
}

void RealtimePreviewWidget::invalidateHyperCache()
{
    m_hyperCacheValid = false;
    m_hyperIndices.clear();
}

void RealtimePreviewWidget::invalidateNoteCache()
{
    m_noteCacheValid = false;
    m_bpmSegments.clear();
    m_noteStartTimesMs.clear();
    m_noteEndTimesMs.clear();
    m_normalIndices.clear();
    m_rainIndices.clear();
}

void RealtimePreviewWidget::ensureNoteCache()
{
    if (m_noteCacheValid)
        return;

    m_bpmSegments.clear();
    m_noteStartTimesMs.clear();
    m_noteEndTimesMs.clear();
    m_normalIndices.clear();
    m_rainIndices.clear();

    if (!m_chartController || !m_chartController->chart())
    {
        m_noteCacheValid = true;
        return;
    }

    const Chart *chart = m_chartController->chart();
    const auto &notes = chart->notes();
    const auto &bpmList = chart->bpmList();
    if (bpmList.isEmpty())
    {
        m_noteCacheValid = true;
        return;
    }

    const QVector<MathUtils::BpmCacheEntry> bpmCache = MathUtils::buildBpmTimeCache(bpmList, chart->meta().offset);
    if (bpmCache.isEmpty())
    {
        m_noteCacheValid = true;
        return;
    }

    m_bpmSegments.reserve(bpmCache.size());
    for (const auto &entry : bpmCache)
    {
        m_bpmSegments.push_back(BpmSegment{entry.beatPos, entry.accumulatedMs, entry.bpm});
    }

    m_noteStartTimesMs.fill(std::numeric_limits<double>::quiet_NaN(), notes.size());
    m_noteEndTimesMs.fill(std::numeric_limits<double>::quiet_NaN(), notes.size());
    m_normalIndices.reserve(notes.size());
    m_rainIndices.reserve(notes.size());

    for (int i = 0; i < notes.size(); ++i)
    {
        const Note &note = notes[i];
        if (note.type == NoteType::SOUND)
            continue;

        const double startMs = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator, bpmCache);
        m_noteStartTimesMs[i] = startMs;

        if (note.type == NoteType::RAIN)
        {
            m_noteEndTimesMs[i] = MathUtils::beatToMs(note.endBeatNum, note.endNumerator, note.endDenominator, bpmCache);
            m_rainIndices.push_back(i);
        }
        else
        {
            m_noteEndTimesMs[i] = startMs;
            m_normalIndices.push_back(i);
        }
    }

    m_noteCacheValid = true;
}

void RealtimePreviewWidget::ensureHyperCache()
{
    if (m_hyperCacheValid)
        return;

    m_hyperIndices.clear();
    if (!m_hyperfruitEnabled || !m_chartController || !m_chartController->chart())
    {
        m_hyperCacheValid = true;
        return;
    }

    const Chart *chart = m_chartController->chart();
    const auto &bpmList = chart->bpmList();
    if (bpmList.isEmpty())
    {
        m_hyperCacheValid = true;
        return;
    }

    m_hyperIndices = m_hyperfruitDetector->detect(chart->notes(), bpmList, chart->meta().offset);
    m_hyperCacheValid = true;
}

void RealtimePreviewWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(rect(), QColor(10, 10, 10));

    const QRectF laneRect(10.0, 8.0, qMax(1.0, width() - 20.0), qMax(1.0, height() - 16.0));
    painter.setPen(QPen(QColor(50, 50, 50), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(laneRect);

    const double upperSpanMs = 2000.0;
    const double lowerSpanMs = 2000.0;
    const double referenceY = laneRect.top() + laneRect.height() * 0.8;

    painter.setPen(QPen(Qt::white, 2));
    painter.drawLine(QPointF(laneRect.left(), referenceY), QPointF(laneRect.right(), referenceY));

    if (!m_chartController || !m_chartController->chart())
        return;

    const Chart *chart = m_chartController->chart();
    const auto &notes = chart->notes();
    ensureNoteCache();
    if (notes.isEmpty())
        return;

    ensureHyperCache();

    for (int idx : m_rainIndices)
    {
        if (idx < 0 || idx >= notes.size())
            continue;
        const Note &note = notes[idx];

        const double startTime = m_noteStartTimesMs.value(idx, std::numeric_limits<double>::quiet_NaN());
        const double endTime = m_noteEndTimesMs.value(idx, std::numeric_limits<double>::quiet_NaN());
        if (!std::isfinite(startTime) || !std::isfinite(endTime))
            continue;
        const double minT = qMin(startTime, endTime);
        const double maxT = qMax(startTime, endTime);

        if (minT > m_currentTimeMs + upperSpanMs || maxT < m_currentTimeMs - lowerSpanMs)
            continue;

        const double yStart = timeToY(startTime, laneRect, referenceY, upperSpanMs, lowerSpanMs);
        const double yEnd = timeToY(endTime, laneRect, referenceY, upperSpanMs, lowerSpanMs);

        QRectF rainRect(laneRect.left(), qMin(yStart, yEnd), laneRect.width(), qAbs(yEnd - yStart));
        rainRect = rainRect.intersected(laneRect);
        if (rainRect.height() <= 0.1)
            continue;

        m_noteRenderer->drawRain(painter, note, rainRect, false);
    }

    const int noteSize = qMax(6, m_noteRenderer->getNoteSize());
    const qreal radius = noteSize * 0.5;
    const qreal hyperRadius = radius * 1.3;

    for (int idx : m_normalIndices)
    {
        if (idx < 0 || idx >= notes.size())
            continue;
        const Note &note = notes[idx];

        const double timeMs = m_noteStartTimesMs.value(idx, std::numeric_limits<double>::quiet_NaN());
        if (!std::isfinite(timeMs))
            continue;
        const double delta = timeMs - m_currentTimeMs;
        if (delta > upperSpanMs || delta < -lowerSpanMs)
            continue;

        const int clampedX = qBound(0, note.x, kLaneWidth);
        const double normX = clampedX / static_cast<double>(kLaneWidth);
        const double x = laneRect.left() + laneRect.width() * normX;
        const double y = timeToY(timeMs, laneRect, referenceY, upperSpanMs, lowerSpanMs);
        if (y < laneRect.top() - 64.0 || y > laneRect.bottom() + 64.0)
            continue;

        const QPointF center(x, y);
        if (m_hyperfruitEnabled && m_hyperIndices.contains(idx))
        {
            painter.setPen(QPen(Qt::red, 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(center, hyperRadius, hyperRadius);
        }

        painter.setPen(QPen(Qt::white, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, radius, radius);
    }
}
