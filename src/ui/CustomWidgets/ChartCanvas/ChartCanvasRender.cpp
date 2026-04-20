#include "ChartCanvas.h"
#include "controller/ChartController.h"
#include "controller/SelectionController.h"
#include "render/NoteRenderer.h"
#include "render/GridRenderer.h"
#include "render/BackgroundRenderer.h"
#include "render/HyperfruitDetector.h"
#include "app/Application.h"
#include "plugin/PluginManager.h"
#include "utils/MathUtils.h"
#include "utils/Settings.h"
#include "utils/DiagnosticCollector.h"
#include "utils/Logger.h"
#include "model/Chart.h"
#include <QPainter>
#include <QPen>
#include <QFileInfo>
#include <algorithm>
#include <chrono>
#include <QCoreApplication>

namespace
{
PluginManager *activePluginManager()
{
    auto *app = qobject_cast<Application *>(QCoreApplication::instance());
    if (!app || !app->pluginSystemReady())
        return nullptr;
    return app->pluginManager();
}
}

void ChartCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    m_frameCount++;
    qint64 elapsed = m_fpsTimer.elapsed();
    if (elapsed >= 1000)
    {
        m_currentFps = m_frameCount * 1000.0 / elapsed;
        m_frameCount = 0;
        m_fpsTimer.restart();
    }

    QPainter painter(this);
    if (!chart())
    {
        painter.fillRect(rect(), Settings::instance().backgroundColor());
        return;
    }

    if (m_timesDirty || m_noteDataDirty)
        rebuildNoteTimesCache();

    const Chart *currentChart = chart();
    const auto &bpmList = currentChart->bpmList();
    const auto &notes = currentChart->notes();

    drawBackground(painter);
    drawGrid(painter);

    double startBeat = m_scrollBeat;
    double visibleRange = effectiveVisibleBeatRange();
    double endBeat = startBeat + visibleRange;

    

    if (m_hyperfruitEnabled && !bpmList.isEmpty())
    {
        if (!m_hyperCacheValid)
        {
            m_cachedHyperSet = m_hyperfruitDetector->detect(notes, bpmList, 0);
            m_hyperCacheValid = true;
        }
        m_noteRenderer->setHyperfruitIndices(m_cachedHyperSet);
    }

    const int canvasWidth = width();
    const int canvasHeight = height();
    const int lmargin = leftMargin();
    const int rmargin = rightMargin();
    const int availableWidth = qMax(1, canvasWidth - lmargin - rmargin);
    const double invVisibleRange = 1.0 / visibleRange;
    const double baseY = m_verticalFlip ? canvasHeight : 0;
    const double sign = m_verticalFlip ? -1.0 : 1.0;

    QSet<int> selectedSet;
    if (m_selectionController)
        selectedSet = m_selectionController->selectedIndices();

    painter.setClipRect(rect());

    int renderedNotesCount = 0;
    for (int i = 0; i < notes.size(); ++i)
    {
        NoteType type = m_noteTypes[i];
        if (type == NoteType::SOUND)
            continue;

        double beat = m_noteBeatPositions[i];
        double endBeatNote = m_noteEndBeatPositions[i];

        if (type == NoteType::NORMAL)
        {
            if (beat < startBeat - 0.5 || beat > endBeat + 0.5)
                continue;
        }
        else if (type == NoteType::RAIN)
        {
            if (endBeatNote <= startBeat || beat >= endBeat)
                continue;
        }

        double y = baseY + sign * ((beat - m_scrollBeat) * invVisibleRange * canvasHeight);

        if (type == NoteType::RAIN)
        {
            double visibleStartBeat = qMax(beat, startBeat);
            double visibleEndBeat = qMin(endBeatNote, endBeat);
            double yStart = baseY + sign * ((visibleStartBeat - m_scrollBeat) * invVisibleRange * canvasHeight);
            double yEnd = baseY + sign * ((visibleEndBeat - m_scrollBeat) * invVisibleRange * canvasHeight);
            double rectTop = qMin(yStart, yEnd);
            double rectHeight = qAbs(yEnd - yStart);
            if (rectHeight <= 0)
                continue;
            QRectF rainRect(lmargin, rectTop, availableWidth, rectHeight);
            bool selected = selectedSet.contains(i);
            m_noteRenderer->drawRain(painter, notes[i], rainRect, selected);
            renderedNotesCount++;
        }
        else
        {
            double x = lmargin + m_noteXPositions[i] * availableWidth;
            QPointF pos(x, y);
            bool selected = selectedSet.contains(i);
            m_noteRenderer->drawNote(painter, notes[i], pos, selected, i);
            renderedNotesCount++;
        }
    }

    if (m_isPasting && !m_pasteNotes.isEmpty())
        drawPastePreview(painter, canvasHeight, lmargin, availableWidth, invVisibleRange, baseY, sign);

    double baselineY = canvasHeight * kReferenceLineRatio;
    painter.setPen(QPen(QColor(0, 0, 255), 3));
    painter.drawLine(lmargin, baselineY, canvasWidth - rmargin, baselineY);

    if (m_playbackController && m_currentPlayTime > 0)
    {
        painter.setPen(Qt::black);
        painter.drawText(canvasWidth - rmargin - 50, baselineY - 5,
                         QString::number(m_currentPlayTime, 'f', 0) + "ms");
        QString autoScrollText = m_autoScrollEnabled ? tr("AutoScroll: ON") : tr("AutoScroll: OFF");
        painter.drawText(canvasWidth - rmargin - 200, baselineY - 5, autoScrollText);
    }

    if (m_isSelecting)
    {
        QRectF rect = QRectF(m_selectionStart, m_selectionEnd).normalized();
        painter.setPen(Qt::red);
        painter.setBrush(QColor(255, 255, 0, 80));
        painter.drawRect(rect);
    }

    drawPluginOverlays(painter, lmargin, rmargin);

    painter.setPen(Qt::white);
    painter.setBrush(QColor(0, 0, 0, 128));
    QString fpsText = QString("FPS: %1").arg(m_currentFps, 0, 'f', 1);
    QRect fpsRect(10, canvasHeight - 30, 80, 20);
    painter.fillRect(fpsRect, QColor(0, 0, 0, 128));
    painter.drawText(fpsRect, Qt::AlignCenter, fpsText);

    auto now = std::chrono::high_resolution_clock::now();
    static auto lastRecord = now;
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRecord).count();
    if (elapsedMs > 500)
    {
        DiagnosticCollector::instance().recordRenderMetrics(elapsedMs, renderedNotesCount);
        lastRecord = now;
    }

}

void ChartCanvas::drawPastePreview(QPainter &painter,
                                   int canvasHeight,
                                   int lmargin,
                                   int availableWidth,
                                   double invVisibleRange,
                                   double baseY,
                                   double sign)
{
    if (!chart())
        return;

    painter.setOpacity(0.5);

    const auto &bpmList = chart()->bpmList();
    const int offset = chart()->meta().offset;

    QVector<double> fallbackOriginalTimes;
    bool usingCachedOriginalTimes =
        (m_pasteOriginalTimesMs.size() == m_pasteNotes.size()) &&
        std::isfinite(m_pasteBaseOriginalTimeMs);
    if (!usingCachedOriginalTimes)
    {
        fallbackOriginalTimes.resize(m_pasteNotes.size());
        for (int i = 0; i < fallbackOriginalTimes.size(); ++i)
            fallbackOriginalTimes[i] = std::numeric_limits<double>::quiet_NaN();

        for (int i = 0; i < m_pasteNotes.size(); ++i)
        {
            const Note &note = m_pasteNotes[i];
            if (note.type == NoteType::SOUND)
                continue;
            const double t = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator, bpmList, offset);
            fallbackOriginalTimes[i] = t;
        }
    }

    const QVector<double> &sourceOriginalTimes = usingCachedOriginalTimes ? m_pasteOriginalTimesMs : fallbackOriginalTimes;
    double baseOriginalTime = m_pasteBaseOriginalTimeMs;
    if (!usingCachedOriginalTimes)
    {
        baseOriginalTime = std::numeric_limits<double>::max();
        for (double t : sourceOriginalTimes)
        {
            if (std::isfinite(t) && t < baseOriginalTime)
                baseOriginalTime = t;
        }
    }
    if (baseOriginalTime != std::numeric_limits<double>::max())
    {
        const QVector<MathUtils::BpmCacheEntry> &previewBpmCache = bpmTimeCache();
        auto previewBeatFromTimeMs = [&previewBpmCache, &bpmList, offset](double ms) -> double
        {
            if (!previewBpmCache.isEmpty())
            {
                int lo = 0;
                int hi = previewBpmCache.size() - 1;
                while (lo < hi)
                {
                    int mid = (lo + hi + 1) / 2;
                    if (previewBpmCache[mid].accumulatedMs <= ms)
                        lo = mid;
                    else
                        hi = mid - 1;
                }
                const auto &seg = previewBpmCache[lo];
                if (seg.bpm <= 0.0)
                    return seg.beatPos;
                return seg.beatPos + (ms - seg.accumulatedMs) * (seg.bpm / 60000.0);
            }
            int b = 0, n = 0, d = 1;
            MathUtils::msToBeat(ms, bpmList, offset, b, n, d);
            return MathUtils::beatToFloat(b, n, d);
        };
        const double baseOriginalBeat = previewBeatFromTimeMs(baseOriginalTime);
        const double referenceBeat = m_pasteAnchorBeat;
        const double baseBeatShift = referenceBeat - baseOriginalBeat;
        const double totalBeatShift = snapPasteTimeOffset(baseBeatShift + m_pasteTimeOffset);
        auto previewAssignBeatWithDen = [](double beatFloat, int targetDen, int &outBeatNum, int &outNum, int &outDen)
        {
            if (targetDen <= 0)
                targetDen = 1;
            const qint64 ticks = qRound64(beatFloat * targetDen);
            outBeatNum = static_cast<int>(ticks / targetDen);
            outNum = static_cast<int>(ticks % targetDen);
            if (outNum < 0)
            {
                outNum += targetDen;
                outBeatNum -= 1;
            }
            outDen = targetDen;
        };
        for (int i = 0; i < m_pasteNotes.size(); ++i)
        {
            const Note &note = m_pasteNotes[i];
            if (note.type == NoteType::SOUND)
                continue;
            if (i >= sourceOriginalTimes.size())
                continue;
            const double originalTime = sourceOriginalTimes[i];
            if (!std::isfinite(originalTime))
                continue;
            int previewBeatNum = 0, previewNum = 0, previewDen = 1;
            const int targetPreviewDen = Settings::instance().pasteUse288Division() ? 288 : qMax(1, note.denominator);
            const double originalBeatFloat = previewBeatFromTimeMs(originalTime);
            const double previewBeatFloat = originalBeatFloat + totalBeatShift;
            previewAssignBeatWithDen(previewBeatFloat, targetPreviewDen, previewBeatNum, previewNum, previewDen);
            const double beat = MathUtils::beatToFloat(previewBeatNum, previewNum, previewDen);
            const double y = baseY + sign * ((beat - m_scrollBeat) * invVisibleRange * canvasHeight);
            const int previewShiftedX = qBound(0, note.x + qRound(m_pasteXOffset), kLaneWidth);
            const double x = lmargin + (previewShiftedX / static_cast<double>(kLaneWidth)) * availableWidth;
            m_noteRenderer->drawNote(painter, note, QPointF(x, y), false, -1);
        }
    }

    painter.setOpacity(1.0);
    painter.fillRect(QRect(10, 10, 100, 30), QColor(200, 200, 200));
    painter.drawText(QRect(10, 10, 100, 30), Qt::AlignCenter, tr("Confirm"));
    painter.fillRect(QRect(120, 10, 100, 30), QColor(200, 200, 200));
    painter.drawText(QRect(120, 10, 100, 30), Qt::AlignCenter, tr("Cancel"));
}

void ChartCanvas::drawPluginOverlays(QPainter &painter, int lmargin, int rmargin)
{
    PluginManager *pm = activePluginManager();
    if (!pm)
        return;

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool canQuery = nowMs >= m_overlayQueryBlockedUntilMs;
    const bool dueForQuery =
        (m_lastOverlayQueryMs == 0) || (nowMs - m_lastOverlayQueryMs >= kOverlayQueryIntervalMs);

    if (canQuery && dueForQuery)
    {
        QVariantMap overlayContext;
        overlayContext.insert("canvas_width", width());
        overlayContext.insert("canvas_height", height());
        overlayContext.insert("scroll_beat", m_scrollBeat);
        overlayContext.insert("visible_beat_range", effectiveVisibleBeatRange());
        overlayContext.insert("vertical_flip", m_verticalFlip);
        overlayContext.insert("time_division", m_timeDivision);
        overlayContext.insert("grid_division", m_gridDivision);
        overlayContext.insert("left_margin", lmargin);
        overlayContext.insert("right_margin", rmargin);
        const QString chartPath = m_chartController ? m_chartController->chartFilePath() : QString();
        if (!chartPath.isEmpty())
            overlayContext.insert("chart_path", chartPath);

        QElapsedTimer requestTimer;
        requestTimer.start();
        m_overlayCache = pm->canvasOverlays(overlayContext);
        m_lastOverlayQueryMs = nowMs;

        const qint64 elapsedMs = requestTimer.elapsed();
        if (elapsedMs > kOverlaySlowCallThresholdMs)
        {
            m_overlayQueryBlockedUntilMs = nowMs + kOverlaySlowCallBackoffMs;
            Logger::warn(QString("Plugin overlay query is slow (%1 ms); temporarily throttling for %2 ms.")
                             .arg(elapsedMs)
                             .arg(kOverlaySlowCallBackoffMs));
        }
        else
        {
            m_overlayQueryBlockedUntilMs = 0;
        }
    }

    for (const auto &item : m_overlayCache)
    {
        QPen pen(item.color, item.width);
        painter.setPen(pen);
        if (item.kind == PluginInterface::CanvasOverlayItem::Rect)
        {
            painter.fillRect(item.rect, item.fillColor);
            painter.drawRect(item.rect);
        }
        else if (item.kind == PluginInterface::CanvasOverlayItem::Text)
        {
            QFont f = painter.font();
            f.setPixelSize(qMax(8, item.fontPx));
            painter.setFont(f);
            painter.drawText(item.from, item.text);
        }
        else
        {
            painter.drawLine(item.from, item.to);
        }
    }
}

void ChartCanvas::drawBackground(QPainter &painter)
{
    QSize sz = size();
    if (m_backgroundCacheDirty || m_backgroundCache.size() != sz)
    {
        if (!chart())
        {
            m_backgroundCache = m_backgroundRenderer->generateBackground(sz);
        }
        else
        {
            const MetaData &meta = chart()->meta();
            QString bgPath = meta.backgroundFile;
            if (!bgPath.isEmpty())
            {
                QString chartDir = QFileInfo(m_chartController->chartFilePath()).absolutePath();
                bgPath = chartDir + "/" + bgPath;
                m_backgroundRenderer->setBackgroundImage(bgPath);
            }
            else
            {
                m_backgroundRenderer->setBackgroundImage("");
            }
            m_backgroundRenderer->setBackgroundColor(Settings::instance().backgroundColor());
            m_backgroundRenderer->setImageEnabled(Settings::instance().backgroundImageEnabled());
            m_backgroundCache = m_backgroundRenderer->generateBackground(sz);
        }
        m_backgroundCacheDirty = false;
    }
    painter.drawPixmap(0, 0, m_backgroundCache);
}

void ChartCanvas::drawGrid(QPainter &painter)
{
    try
    {
        QRect rect = this->rect();
        int lmargin = leftMargin();
        int rmargin = rightMargin();
        if (lmargin > 0 || rmargin > 0)
        {
            rect.adjust(lmargin, 0, -rmargin, 0);
        }


        const QVector<MathUtils::BpmCacheEntry> &bpmCache = bpmTimeCache();
        if (bpmCache.isEmpty())
            return;

        int startBeatNum, startNum, startDen;
        MathUtils::floatToBeat(m_scrollBeat, startBeatNum, startNum, startDen);
        double startTime = MathUtils::beatToMs(startBeatNum, startNum, startDen, bpmCache);
        int endBeatNum, endNum, endDen;
        MathUtils::floatToBeat(m_scrollBeat + effectiveVisibleBeatRange(), endBeatNum, endNum, endDen);
        double endTime = MathUtils::beatToMs(endBeatNum, endNum, endDen, bpmCache);
        m_gridRenderer->drawGrid(painter, rect, m_gridDivision,
                                 startTime, endTime,
                                 m_timeDivision, bpmCache,
                                 m_verticalFlip,
                                 Settings::instance().timelineDivisionColorEnabled(),
                                 Settings::instance().timelineDivisionColorPreset(),
                                 Settings::instance().timelineDivisionColorCustomDivisions());
    }
    catch (const std::exception &e)
    {
        Logger::error(QString("ChartCanvas::drawGrid - Exception: %1").arg(e.what()));
    }
    catch (...)
    {
        Logger::error("ChartCanvas::drawGrid - Unknown exception");
    }
}

double ChartCanvas::getNoteTimeMs(const Note &note) const
{
    return MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator,
                               chart()->bpmList(),
                               chart()->meta().offset);
}

double ChartCanvas::yPosFromTime(double timeMs) const
{
    int beatNum, numerator, denominator;
    MathUtils::msToBeat(timeMs, chart()->bpmList(),
                        chart()->meta().offset,
                        beatNum, numerator, denominator);
    double beat = beatNum + static_cast<double>(numerator) / denominator;
    return beatToY(beat);
}

double ChartCanvas::beatToY(double beat) const
{
    double visibleRange = effectiveVisibleBeatRange();
    if (visibleRange <= 0)
        return 0;
    double y = (beat - m_scrollBeat) / visibleRange * height();
    if (m_verticalFlip)
        y = height() - y;
    return y;
}

double ChartCanvas::yToBeat(double y) const
{
    if (height() <= 0)
        return m_scrollBeat;

    if (m_verticalFlip)
        y = height() - y;

    return m_scrollBeat + (y / height()) * effectiveVisibleBeatRange();
}

double ChartCanvas::yToTime(double y) const
{
    double beat = yToBeat(y);
    int beatNum, num, den;
    MathUtils::floatToBeat(beat, beatNum, num, den);
    return MathUtils::beatToMs(beatNum, num, den,
                               chart()->bpmList(),
                               chart()->meta().offset);
}

QPointF ChartCanvas::noteToPos(const Note &note) const
{
    double beat = MathUtils::beatToFloat(note.beatNum, note.numerator, note.denominator);
    double y = beatToY(beat);
    int lmargin = leftMargin();
    int rmargin = rightMargin();
    int availableWidth = qMax(1, width() - lmargin - rmargin);
    double x = lmargin + (note.x / static_cast<double>(kLaneWidth)) * availableWidth;
    return QPointF(x, y);
}

Note ChartCanvas::posToNote(const QPointF &pos) const
{
    double beat = yToBeat(pos.y());
    int beatNum, num, den;
    MathUtils::floatToBeat(beat, beatNum, num, den);
    int lmargin = leftMargin();
    int rmargin = rightMargin();
    int availableWidth = qMax(1, width() - lmargin - rmargin);
    int x = static_cast<int>((pos.x() - lmargin) / availableWidth * kLaneWidth);

    if (m_gridSnap)
    {
        x = MathUtils::snapXToGrid(x, m_gridDivision);
    }

    x = qBound(0, x, kLaneWidth);

    Note note(beat, num, den, x);

    if (m_timeDivision > 0)
    {
        note = MathUtils::snapNoteToTimeWithBoundary(note, m_timeDivision);
    }
    else
    {
        note = MathUtils::snapNoteToTimeWithBoundary(note, 1);
    }

    return note;
}

QRectF ChartCanvas::getRainNoteRect(const Note &note) const
{
    if (!chart())
        return QRectF();

    const auto &bpmList = chart()->bpmList();
    int offset = chart()->meta().offset;

    double startTime = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator, bpmList, offset);
    double endTime = MathUtils::beatToMs(note.endBeatNum, note.endNumerator, note.endDenominator, bpmList, offset);

    double yStart = yPosFromTime(startTime);
    double yEnd = yPosFromTime(endTime);

    double rectTop = qMin(yStart, yEnd);
    double rectHeight = qAbs(yEnd - yStart);

    int lmargin = leftMargin();
    int rmargin = rightMargin();
    double rainWidth = qMax(1, width() - lmargin - rmargin);

    return QRectF(lmargin, rectTop, rainWidth, rectHeight);
}

int ChartCanvas::hitTestNote(const QPointF &pos) const
{
    if (!chart())
        return -1;

    const auto &notes = chart()->notes();
    int noteSize = m_noteRenderer->getNoteSize();
    double minDist = noteSize * 0.6;
    int hit = -1;

    for (int i = 0; i < notes.size(); ++i)
    {
        const Note &note = notes[i];
        if (note.type == NoteType::SOUND)
            continue;

        if (note.type == NoteType::RAIN)
        {
            QRectF rainRect = getRainNoteRect(note);
            if (rainRect.contains(pos))
                return i;
        }
        else
        {
            QPointF notePos = noteToPos(note);
            double dist = QLineF(notePos, pos).length();
            if (dist < minDist)
            {
                minDist = dist;
                hit = i;
            }
        }
    }
    return hit;
}



