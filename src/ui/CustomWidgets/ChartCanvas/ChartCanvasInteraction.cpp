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

    double baselineRatio = 0.8;
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

void ChartCanvas::updateBackgroundCache()
{
    m_backgroundCacheDirty = true;
}

void ChartCanvas::refreshBackground()
{
    updateBackgroundCache();
    update();
}

void ChartCanvas::requestNextFrame()
{
    if (!m_isPlaying)
    {
        if (m_playbackTimer && m_playbackTimer->isActive())
            m_playbackTimer->stop();
        return;
    }

    if (m_autoScrollEnabled && m_chartController && m_chartController->chart())
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

        double baselineRatio = 0.8;
        double targetScrollBeat;
        if (m_verticalFlip)
        {
            targetScrollBeat = beat - (1.0 - baselineRatio) * effectiveVisibleBeatRange();
        }
        else
        {
            targetScrollBeat = beat - baselineRatio * effectiveVisibleBeatRange();
        }

        m_scrollBeat = targetScrollBeat;
        if (m_scrollBeat < 0)
            m_scrollBeat = 0;
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (m_lastScrollSignalTimeMs == 0 || nowMs - m_lastScrollSignalTimeMs >= 33)
        {
            emit scrollPositionChanged(m_scrollBeat);
            m_lastScrollSignalTimeMs = nowMs;
        }
    }

    update();
}


