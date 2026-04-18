#include "ChartCanvas.h"
#include "controller/ChartController.h"
#include "controller/SelectionController.h"
#include "controller/PlaybackController.h"
#include "audio/NoteSoundPlayer.h"
#include "utils/MathUtils.h"
#include <QDateTime>
#include <QKeyEvent>
#include <algorithm>
#include <cmath>

void ChartCanvas::playbackPositionChanged(double timeMs)
{
    if (m_timesDirty || m_noteDataDirty)
        rebuildNoteTimesCache();

    if (!m_playbackController || m_playbackController->state() != PlaybackController::Playing)
    {
        m_hasPlaybackAnchor = false;
        m_currentPlayTime = timeMs;
        m_lastNoteSoundTimeMs = timeMs;
        m_nextPlayableNoteIndex = static_cast<int>(std::lower_bound(
            m_playableNoteTimesMs.begin(),
            m_playableNoteTimesMs.end(),
            m_lastNoteSoundTimeMs) - m_playableNoteTimesMs.begin());
        update();
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const double speed = m_playbackController->speed();

    if (!m_hasPlaybackAnchor)
    {
        m_playbackAnchorMs = timeMs;
        m_playbackAnchorWallMs = nowMs;
        m_hasPlaybackAnchor = true;
        m_currentPlayTime = timeMs;
        return;
    }

    const double predicted =
        m_playbackAnchorMs + (nowMs - m_playbackAnchorWallMs) * speed;
    const double delta = timeMs - predicted;

    // Small negative delta is usually backend jitter; keep monotonic progression.
    if (delta > -24.0 && delta < 24.0)
    {
        m_playbackAnchorMs = predicted + delta * 0.08;
        m_playbackAnchorWallMs = nowMs;
    }
    else if (delta < -24.0 && delta > -220.0)
    {
        m_playbackAnchorMs = predicted;
        m_playbackAnchorWallMs = nowMs;
    }
    else if (delta >= 24.0 && delta < 220.0)
    {
        m_playbackAnchorMs = predicted + delta * 0.12;
        m_playbackAnchorWallMs = nowMs;
    }
    else
    {
        // Large jump: treat as explicit seek or real discontinuity.
        m_playbackAnchorMs = timeMs;
        m_playbackAnchorWallMs = nowMs;
    }

    if (!m_autoScrollEnabled)
    {
        m_currentPlayTime = timeMs;
        update();
    }

    if (m_noteSoundPlayer &&
        m_noteSoundPlayer->isEnabled() &&
        m_noteSoundPlayer->hasValidSound() &&
        !m_playableNoteTimesMs.isEmpty())
    {
        if (timeMs < m_lastNoteSoundTimeMs - 2.0)
        {
            m_nextPlayableNoteIndex = static_cast<int>(std::lower_bound(
                m_playableNoteTimesMs.begin(),
                m_playableNoteTimesMs.end(),
                timeMs) - m_playableNoteTimesMs.begin());
        }

        bool hasHit = false;
        while (m_nextPlayableNoteIndex < m_playableNoteTimesMs.size() &&
               m_playableNoteTimesMs[m_nextPlayableNoteIndex] <= timeMs + 0.5)
        {
            if (m_playableNoteTimesMs[m_nextPlayableNoteIndex] > m_lastNoteSoundTimeMs + 0.5)
                hasHit = true;
            ++m_nextPlayableNoteIndex;
        }

        if (hasHit)
            m_noteSoundPlayer->playHitSound();
    }

    m_lastNoteSoundTimeMs = timeMs;
}

void ChartCanvas::playFromReferenceLine()
{
    if (!m_playbackController)
        return;

    if (m_currentPlayTime < 0)
        m_currentPlayTime = 0;

    if (m_playbackController->state() == PlaybackController::Playing)
    {
        m_playbackController->pause();
    }

    m_autoScrollEnabled = true;
    m_playbackController->playFromTime(m_currentPlayTime);
}

double ChartCanvas::currentPlayTime() const
{
    return m_currentPlayTime;
}

void ChartCanvas::onSelectionChanged()
{
}

void ChartCanvas::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        cancelOperation();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete)
    {
        if (m_selectionController && !m_selectionController->selectedIndices().isEmpty())
        {
            QSet<int> selected = m_selectionController->selectedIndices();
            const auto &notes = m_chartController->chart()->notes();
            QList<int> sorted = selected.values();
            std::sort(sorted.begin(), sorted.end(), std::greater<int>());

            QVector<Note> notesToDelete;
            for (int idx : sorted)
            {
                if (idx >= 0 && idx < notes.size())
                {
                    notesToDelete.append(notes[idx]);
                }
            }

            if (!notesToDelete.isEmpty())
            {
                m_chartController->removeNotes(notesToDelete);
            }

            m_selectionController->clearSelection();
        }
    }
    QWidget::keyPressEvent(event);
}

int ChartCanvas::leftMargin() const
{
    return width() / 20;
}

int ChartCanvas::rightMargin() const
{
    return width() / 20;
}

void ChartCanvas::snapPlayheadToGrid()
{
    if (!m_chartController || !m_chartController->chart() || !m_snapToGrid)
    {
        return;
    }

    double currentTime = m_currentPlayTime;
    const auto &bpmList = m_chartController->chart()->bpmList();
    int offset = m_chartController->chart()->meta().offset;

    double snappedTime = MathUtils::snapTimeToGrid(currentTime, bpmList, offset, m_timeDivision);

    if (std::abs(snappedTime - currentTime) > 1e-6)
    {
        m_currentPlayTime = snappedTime;
        if (m_playbackController)
        {
            m_playbackController->seekTo(snappedTime);
        }
        update();
    }
}

void ChartCanvas::startSnapTimer()
{
    stopSnapTimer();
    m_snapTimerId = startTimer(300);
}

void ChartCanvas::stopSnapTimer()
{
    if (m_snapTimerId != 0)
    {
        killTimer(m_snapTimerId);
        m_snapTimerId = 0;
    }
}

void ChartCanvas::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_snapTimerId)
    {
        m_isScrolling = false;
        snapPlayheadToGrid();
        stopSnapTimer();
    }
    QWidget::timerEvent(event);
}

void ChartCanvas::invalidateCache()
{
}

void ChartCanvas::updateNotePosCacheIfNeeded()
{
}

void ChartCanvas::resizeEvent(QResizeEvent *event)
{
    m_backgroundCacheDirty = true;
    QWidget::resizeEvent(event);
}

void ChartCanvas::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_isPlaying)
    {
        requestNextFrame();
    }
}

void ChartCanvas::cancelPaste()
{
    if (m_isPasting)
    {
        m_isPasting = false;
        m_pasteNotes.clear();
        m_pasteOriginalTimesMs.clear();
        m_pasteBaseOriginalTimeMs = std::numeric_limits<double>::max();
        m_pasteTimeOffsetRaw = 0.0;
        m_pasteXOffsetRaw = 0.0;
        m_pasteAnchorBeat = 0.0;
        update();
        emit statusMessage(tr("Paste cancelled."));
    }
}



