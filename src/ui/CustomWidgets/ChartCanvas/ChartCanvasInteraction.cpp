#include "ChartCanvas.h"
#include "controller/ChartController.h"
#include "controller/SelectionController.h"
#include "controller/PlaybackController.h"
#include "audio/NoteSoundPlayer.h"
#include "render/NoteRenderer.h"
#include "render/GridRenderer.h"
#include "render/BackgroundRenderer.h"
#include "render/HyperfruitDetector.h"
#include "utils/MathUtils.h"
#include "utils/Settings.h"
#include "utils/Logger.h"
#include "utils/DiagnosticCollector.h"
#include "model/Chart.h"
#include <QPainter>
#include <QPen>
#include <QFileInfo>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QMessageBox>
#include <QMenu>
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QCheckBox>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>


void ChartCanvas::showGridSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Grid Settings"));
    QFormLayout form(&dialog);
    QCheckBox *snapCheck = new QCheckBox(tr("Enable Grid Snap"));
    snapCheck->setChecked(m_gridSnap);
    QSpinBox *divisionSpin = new QSpinBox;
    divisionSpin->setRange(4, 64);
    divisionSpin->setValue(m_gridDivision);
    form.addRow(tr("Snap to Grid:"), snapCheck);
    form.addRow(tr("Grid Divisions (4-64):"), divisionSpin);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form.addRow(buttons);

    if (dialog.exec() == QDialog::Accepted)
    {
        setGridSnap(snapCheck->isChecked());
        setGridDivision(divisionSpin->value());
    }
}

void ChartCanvas::setTimeScale(double scale)
{
    const double clampedScale = qBound(0.2, scale, 5.0);
    if (qFuzzyCompare(m_timeScale, clampedScale))
        return;

    const double baselineRatio = kReferenceLineRatio;
    double baselineBeat;
    if (m_verticalFlip)
    {
        baselineBeat = m_scrollBeat + (1.0 - baselineRatio) * effectiveVisibleBeatRange();
    }
    else
    {
        baselineBeat = m_scrollBeat + baselineRatio * effectiveVisibleBeatRange();
    }

    m_timeScale = clampedScale;

    if (m_verticalFlip)
    {
        m_scrollBeat = baselineBeat - (1.0 - baselineRatio) * effectiveVisibleBeatRange();
    }
    else
    {
        m_scrollBeat = baselineBeat - baselineRatio * effectiveVisibleBeatRange();
    }
    if (m_scrollBeat < 0)
        m_scrollBeat = 0;

    update();
    emit scrollPositionChanged(m_scrollBeat);
    emit timeScaleChanged(m_timeScale);
}

void ChartCanvas::setMirrorAxisX(int axisX)
{
    const int clamped = clampMirrorAxisX(axisX);
    if (m_mirrorAxisX == clamped)
        return;

    m_mirrorAxisX = clamped;
    emit mirrorAxisChanged(m_mirrorAxisX);
    update();
}

void ChartCanvas::setMirrorGuideVisible(bool visible)
{
    if (m_mirrorGuideVisible == visible)
        return;
    m_mirrorGuideVisible = visible;
    update();
}

void ChartCanvas::setMirrorPreviewVisible(bool visible)
{
    if (m_mirrorPreviewVisible == visible)
        return;
    m_mirrorPreviewVisible = visible;
    update();
}

bool ChartCanvas::flipSelectedNotes()
{
    return performMirrorFlip(collectMirrorTargetIndices(QPoint()), m_mirrorAxisX, tr("Mirror Flip Notes"));
}

bool ChartCanvas::flipSelectedNotesAroundCenter()
{
    return performMirrorFlip(collectMirrorTargetIndices(QPoint()), kLaneWidth / 2, tr("Mirror Flip Notes"));
}

int ChartCanvas::clampMirrorAxisX(int axisX) const
{
    return qBound(0, axisX, kLaneWidth);
}

int ChartCanvas::canvasXToLaneX(double canvasX) const
{
    const int lmargin = leftMargin();
    const int rmargin = rightMargin();
    const int availableWidth = qMax(1, width() - lmargin - rmargin);
    const double normalized = (canvasX - lmargin) / static_cast<double>(availableWidth);
    return clampMirrorAxisX(qRound(normalized * kLaneWidth));
}

double ChartCanvas::laneXToCanvasX(int laneX) const
{
    const int lmargin = leftMargin();
    const int rmargin = rightMargin();
    const int availableWidth = qMax(1, width() - lmargin - rmargin);
    return lmargin + (clampMirrorAxisX(laneX) / static_cast<double>(kLaneWidth)) * availableWidth;
}

bool ChartCanvas::isMirrorGuideHandleHit(const QPointF &pos) const
{
    if (!m_mirrorGuideVisible)
        return false;

    const double axisCanvasX = laneXToCanvasX(m_mirrorAxisX);
    const QPointF topHandle(axisCanvasX, 14.0);
    const QPointF bottomHandle(axisCanvasX, height() - 14.0);
    constexpr double kHandleRadius = 10.0;

    return QLineF(pos, topHandle).length() <= kHandleRadius + 2.0 ||
           QLineF(pos, bottomHandle).length() <= kHandleRadius + 2.0;
}

void ChartCanvas::updateBackgroundCache()
{
    m_backgroundCacheDirty = true;
}

void ChartCanvas::refreshBackground()
{
    updateBackgroundCache();
    // When auto-scroll is disabled during playback, playbackPositionChanged()
    // already drives repaint; avoid duplicate frame updates from both paths.
    if (!m_autoScrollEnabled && m_playbackController &&
        m_playbackController->state() == PlaybackController::Playing)
        return;

    update();
}

void ChartCanvas::requestNextFrame()
{
    constexpr double kScrollSignalEpsilonBeat = 1e-6;

    if (!m_isPlaying)
    {
        if (m_playbackTimer && m_playbackTimer->isActive())
            m_playbackTimer->stop();
        return;
    }

    if (m_autoScrollEnabled && chart())
    {
        if (m_playbackController)
        {
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            const double speed = m_playbackController->speed();
            if (!m_hasPlaybackAnchor)
            {
                m_playbackAnchorMs = m_playbackController->currentTime();
                m_playbackAnchorWallMs = nowMs;
                m_hasPlaybackAnchor = true;
            }
            double predicted = m_playbackAnchorMs +
                               (nowMs - m_playbackAnchorWallMs) * speed;
            // Guard against tiny backwards jitter in source timestamps.
            if (predicted < m_currentPlayTime)
                predicted = m_currentPlayTime;
            m_currentPlayTime = predicted;
        }

        const QVector<MathUtils::BpmCacheEntry> &cache = bpmTimeCache();
        if (cache.isEmpty())
            return;

        auto beatFromTimeMs = [&cache](double timeMs) -> double
        {
            int lo = 0;
            int hi = cache.size() - 1;
            while (lo < hi)
            {
                const int mid = (lo + hi + 1) / 2;
                if (cache[mid].accumulatedMs <= timeMs)
                    lo = mid;
                else
                    hi = mid - 1;
            }
            const auto &seg = cache[lo];
            if (seg.bpm <= 0.0)
                return seg.beatPos;
            return seg.beatPos + (timeMs - seg.accumulatedMs) * (seg.bpm / 60000.0);
        };
        const double beat = beatFromTimeMs(m_currentPlayTime);

        const double baselineRatio = kReferenceLineRatio;
        double targetScrollBeat;
        if (m_verticalFlip)
        {
            targetScrollBeat = beat - (1.0 - baselineRatio) * effectiveVisibleBeatRange();
        }
        else
        {
            targetScrollBeat = beat - baselineRatio * effectiveVisibleBeatRange();
        }

        const double previousScrollBeat = m_scrollBeat;
        m_scrollBeat = targetScrollBeat;
        if (m_scrollBeat < 0)
            m_scrollBeat = 0;
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const bool scrollChanged = std::abs(m_scrollBeat - previousScrollBeat) > kScrollSignalEpsilonBeat;
        if (scrollChanged && (m_lastScrollSignalTimeMs == 0 || nowMs - m_lastScrollSignalTimeMs >= kScrollSignalIntervalMs))
        {
            emit scrollPositionChanged(m_scrollBeat);
            m_lastScrollSignalTimeMs = nowMs;
        }
    }

    update();
}



