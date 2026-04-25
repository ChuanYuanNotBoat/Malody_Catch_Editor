#include "ChartCanvas.h"
#include "controller/ChartController.h"
#include "controller/SelectionController.h"
#include "controller/PlaybackController.h"
#include "audio/NoteSoundPlayer.h"
#include "utils/MathUtils.h"
#include "app/Application.h"
#include "plugin/PluginManager.h"
#include <QDateTime>
#include <QKeyEvent>
#include <QKeySequence>
#include <QCoreApplication>
#include <algorithm>
#include <cmath>

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

void ChartCanvas::playbackPositionChanged(double timeMs)
{
    constexpr double kPlaybackVisualEpsilonMs = 0.05;
    constexpr double kAnchorDeadZoneMs = 2.0;

    if (m_timesDirty || m_noteDataDirty)
        rebuildNoteTimesCache();

    if (!m_playbackController || m_playbackController->state() != PlaybackController::Playing)
    {
        const bool visualChanged = std::abs(m_currentPlayTime - timeMs) > kPlaybackVisualEpsilonMs;
        m_hasPlaybackAnchor = false;
        m_currentPlayTime = timeMs;
        m_lastNoteSoundTimeMs = timeMs;
        m_nextPlayableNoteIndex = static_cast<int>(std::lower_bound(
            m_playableNoteTimesMs.begin(),
            m_playableNoteTimesMs.end(),
            m_lastNoteSoundTimeMs) - m_playableNoteTimesMs.begin());
        if (visualChanged)
            update();
        return;
    }

    const qint64 nowMs = m_playbackMonoClock.elapsed();
    const double speed = m_playbackController->speed();

    if (!m_hasPlaybackAnchor)
    {
        m_playbackAnchorMs = timeMs;
        m_playbackAnchorMonoMs = nowMs;
        m_hasPlaybackAnchor = true;
        m_currentPlayTime = timeMs;
        return;
    }

    const double predicted =
        m_playbackAnchorMs + (nowMs - m_playbackAnchorMonoMs) * speed;
    const double delta = timeMs - predicted;

    // Keep a dead-zone around prediction to avoid beat-like micro tugging.
    if (std::abs(delta) <= kAnchorDeadZoneMs)
    {
        m_playbackAnchorMs = predicted;
        m_playbackAnchorMonoMs = nowMs;
    }
    else if (std::abs(delta) < 48.0)
    {
        // Moderate drift: very gentle correction to keep motion visually stable.
        m_playbackAnchorMs = predicted + delta * 0.04;
        m_playbackAnchorMonoMs = nowMs;
    }
    else if (std::abs(delta) < 220.0)
    {
        // Larger drift: faster catch-up while still avoiding hard snaps.
        m_playbackAnchorMs = predicted + delta * 0.10;
        m_playbackAnchorMonoMs = nowMs;
    }
    else
    {
        // Real discontinuity / seek.
        m_playbackAnchorMs = timeMs;
        m_playbackAnchorMonoMs = nowMs;
    }

    if (!m_autoScrollEnabled)
    {
        const bool visualChanged = std::abs(m_currentPlayTime - timeMs) > kPlaybackVisualEpsilonMs;
        m_currentPlayTime = timeMs;
        if (visualChanged)
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
    if (m_pluginToolModeActive && event->key() == Qt::Key_Return)
    {
        if (triggerPluginBatchAction("commit_curve_to_notes", tr("Commit Curve -> Notes")))
        {
            event->accept();
            return;
        }
    }
    if (m_pluginToolModeActive && event->key() == Qt::Key_Enter)
    {
        if (triggerPluginBatchAction("commit_curve_to_notes", tr("Commit Curve -> Notes")))
        {
            event->accept();
            return;
        }
    }
    if (m_pluginToolModeActive && event->key() == Qt::Key_Escape)
    {
        PluginInterface::CanvasInputEvent cancelEvent;
        cancelEvent.type = "cancel";
        cancelEvent.modifiers = static_cast<int>(event->modifiers());
        cancelEvent.timestampMs = QDateTime::currentMSecsSinceEpoch();
        bool consumedCancel = false;
        if (dispatchPluginCanvasInput(cancelEvent, &consumedCancel) && consumedCancel)
        {
            event->accept();
            return;
        }
    }

    PluginInterface::CanvasInputEvent pluginEvent;
    pluginEvent.type = "key_down";
    pluginEvent.key = event->key();
    pluginEvent.modifiers = static_cast<int>(event->modifiers());
    pluginEvent.timestampMs = QDateTime::currentMSecsSinceEpoch();
    bool consumed = false;
    if (dispatchPluginCanvasInput(pluginEvent, &consumed) && consumed)
    {
        event->accept();
        return;
    }

    if (m_pluginToolModeActive && event->matches(QKeySequence::Undo))
    {
        bool handled = false;
        if (m_chartController && m_chartController->canUndo())
        {
            const QString actionText = m_chartController->nextUndoActionText();
            m_chartController->undo();
            if (PluginManager *pm = activePluginManager())
                pm->notifyHostUndo(actionText);
            handled = true;
        }
        if (handled)
        {
            event->accept();
            return;
        }
    }
    if (m_pluginToolModeActive && event->matches(QKeySequence::Redo))
    {
        bool handled = false;
        if (m_chartController && m_chartController->canRedo())
        {
            const QString actionText = m_chartController->nextRedoActionText();
            m_chartController->redo();
            if (PluginManager *pm = activePluginManager())
                pm->notifyHostRedo(actionText);
            handled = true;
        }
        if (handled)
        {
            event->accept();
            return;
        }
    }

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
            const auto &notes = chart()->notes();
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

void ChartCanvas::keyReleaseEvent(QKeyEvent *event)
{
    PluginInterface::CanvasInputEvent pluginEvent;
    pluginEvent.type = "key_up";
    pluginEvent.key = event->key();
    pluginEvent.modifiers = static_cast<int>(event->modifiers());
    pluginEvent.timestampMs = QDateTime::currentMSecsSinceEpoch();
    bool consumed = false;
    if (dispatchPluginCanvasInput(pluginEvent, &consumed) && consumed)
    {
        event->accept();
        return;
    }

    QWidget::keyReleaseEvent(event);
}

int ChartCanvas::leftMargin() const
{
    return width() / kSideMarginDivisor;
}

int ChartCanvas::rightMargin() const
{
    return width() / kSideMarginDivisor;
}

void ChartCanvas::snapPlayheadToGrid()
{
    if (!chart() || !m_snapToGrid)
    {
        return;
    }

    if (m_playbackController && m_playbackController->state() == PlaybackController::Playing)
        return;

    double currentTime = m_currentPlayTime;
    const auto &bpmList = chart()->bpmList();
    int offset = chart()->meta().offset;

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




