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
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFileInfo>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QCheckBox>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QMessageBox>
#include <QMenu>
#include <QAction>
#include <QShowEvent>
#include <QDebug>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

ChartCanvas::ChartCanvas(QWidget *parent)
    : QWidget(parent),
      m_chartController(nullptr),
      m_selectionController(nullptr),
      m_playbackController(nullptr),
      m_noteRenderer(new NoteRenderer),
      m_gridRenderer(new GridRenderer),
      m_hyperfruitDetector(new HyperfruitDetector),
      m_backgroundRenderer(new BackgroundRenderer),
      m_currentMode(PlaceNote),
      m_colorMode(true),
      m_hyperfruitEnabled(true),
      m_verticalFlip(true),
      m_timeDivision(4),
      m_gridDivision(20),
      m_gridSnap(true),
      m_scrollBeat(0),
      m_baseVisibleBeatRange(10),
      m_timeScale(2.25),
      m_currentPlayTime(0),
      m_autoScrollEnabled(true),
      m_isSelecting(false),
      m_isDragging(false),
      m_isPasting(false),
      m_useCursorPaste(false),
      m_pasteCursorPos(0, 0),
      m_intervalState(IntervalNone),
      m_intervalStartTime(0.0),
      m_isMovingSelection(false),
      m_moveDeltaBeatRaw(0.0),
      m_moveDeltaXRaw(0.0),
      m_gridSnapBackup(false),
      m_wasGridSnapEnabled(false),
      m_dragReferenceIndex(-1),
      m_rainFirst(true),
      m_snapToGrid(true),
      m_snapTimerId(0),
      m_isScrolling(false),
      m_playbackTimer(nullptr),
      m_hyperCacheValid(false),
      m_backgroundCacheDirty(true),
      m_noteDataDirty(true),
      m_timesDirty(true),
      m_bpmCacheDirty(true),
      m_frameCount(0),
      m_currentFps(0.0),
      m_isPlaying(false),
      m_isDraggingPaste(false),
      m_pasteTimeOffset(0.0),
      m_pasteXOffset(0.0),
      m_pasteTimeOffsetRaw(0.0),
      m_pasteXOffsetRaw(0.0),
      m_pasteAnchorBeat(0.0),
      m_pasteRefBeat(0.0),
      m_pasteDragReferenceIndex(-1),
      m_pasteBaseOriginalTimeMs(std::numeric_limits<double>::max()),
      m_lastScrollSignalTimeMs(0),
      m_hasPlaybackAnchor(false),
      m_playbackAnchorMs(0.0),
      m_playbackAnchorWallMs(0),
      m_noteSoundPlayer(nullptr),
      m_nextPlayableNoteIndex(0),
      m_lastNoteSoundTimeMs(0.0)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(800, 400);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NativeWindow);

    m_noteRenderer->setNoteSize(Settings::instance().noteSize());
    m_noteSoundPlayer = new NoteSoundPlayer(this);
    m_noteSoundPlayer->setVolumePercent(Settings::instance().noteSoundVolume());
    const QString noteSoundPath = Settings::instance().noteSoundPath();
    m_noteSoundPlayer->setSoundFile(noteSoundPath);
    m_noteSoundPlayer->setEnabled(!noteSoundPath.isEmpty());

    m_playbackTimer = new QTimer(this);
    m_playbackTimer->setInterval(kPlaybackFrameIntervalMs);
    m_playbackTimer->setTimerType(Qt::PreciseTimer);
    connect(m_playbackTimer, &QTimer::timeout, this, &ChartCanvas::requestNextFrame);

    m_fpsTimer.start();
}

ChartCanvas::~ChartCanvas()
{
    delete m_noteRenderer;
    delete m_gridRenderer;
    delete m_hyperfruitDetector;
    delete m_backgroundRenderer;
}

const Chart *ChartCanvas::chart() const
{
    return m_chartController ? m_chartController->chart() : nullptr;
}

Chart *ChartCanvas::chart()
{
    // ChartController currently exposes a const chart() accessor.
    return m_chartController ? const_cast<Chart *>(m_chartController->chart()) : nullptr;
}

QVector<Note> *ChartCanvas::mutableNotes()
{
    Chart *c = chart();
    return c ? &c->notes() : nullptr;
}

void ChartCanvas::rebuildBpmTimeCache()
{
    m_bpmTimeCache.clear();
    if (!chart())
    {
        m_bpmCacheDirty = false;
        return;
    }

    const auto &bpmList = chart()->bpmList();
    if (bpmList.isEmpty())
    {
        m_bpmCacheDirty = false;
        return;
    }

    const int offset = chart()->meta().offset;
    m_bpmTimeCache = MathUtils::buildBpmTimeCache(bpmList, offset);
    m_bpmCacheDirty = false;
}

const QVector<MathUtils::BpmCacheEntry> &ChartCanvas::bpmTimeCache()
{
    if (m_bpmCacheDirty)
        rebuildBpmTimeCache();
    return m_bpmTimeCache;
}

void ChartCanvas::rebuildNoteTimesCache()
{
    if (!chart())
    {
        m_noteBeatPositions.clear();
        m_noteEndBeatPositions.clear();
        m_noteXPositions.clear();
        m_noteTimesMs.clear();
        m_noteTypes.clear();
        m_playableNoteTimesMs.clear();
        m_nextPlayableNoteIndex = 0;
        m_timesDirty = false;
        m_noteDataDirty = false;
        return;
    }
    const auto &notes = chart()->notes();
    const auto &bpmList = chart()->bpmList();

    if (bpmList.isEmpty())
    {
        qWarning() << "ChartCanvas::rebuildNoteTimesCache: BPM list is empty, cannot compute times.";
        m_noteBeatPositions.clear();
        m_noteEndBeatPositions.clear();
        m_noteXPositions.clear();
        m_noteTimesMs.clear();
        m_noteTypes.clear();
        m_playableNoteTimesMs.clear();
        m_nextPlayableNoteIndex = 0;
        m_timesDirty = false;
        m_noteDataDirty = false;
        return;
    }

    const QVector<MathUtils::BpmCacheEntry> &bpmCache = bpmTimeCache();
    if (bpmCache.isEmpty())
    {
        m_noteBeatPositions.clear();
        m_noteEndBeatPositions.clear();
        m_noteXPositions.clear();
        m_noteTimesMs.clear();
        m_noteTypes.clear();
        m_playableNoteTimesMs.clear();
        m_nextPlayableNoteIndex = 0;
        m_timesDirty = false;
        m_noteDataDirty = false;
        return;
    }

    const int N = notes.size();
    m_noteBeatPositions.resize(N);
    m_noteEndBeatPositions.resize(N);
    m_noteXPositions.resize(N);
    m_noteTimesMs.resize(N);
    m_noteTypes.resize(N);
    m_playableNoteTimesMs.clear();
    m_playableNoteTimesMs.reserve(N);

    for (int i = 0; i < N; ++i)
    {
        const Note &note = notes[i];
        m_noteTypes[i] = note.type;
        if (note.type == NoteType::SOUND)
        {
            m_noteBeatPositions[i] = 0.0;
            m_noteEndBeatPositions[i] = 0.0;
            m_noteXPositions[i] = 0.0;
            m_noteTimesMs[i] = 0.0;
            continue;
        }
        double beat = MathUtils::beatToFloat(note.beatNum, note.numerator, note.denominator);
        m_noteBeatPositions[i] = beat;
        m_noteTimesMs[i] = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator, bpmCache);
        m_playableNoteTimesMs.append(m_noteTimesMs[i]);
        if (note.type == NoteType::RAIN)
        {
            double endBeat = MathUtils::beatToFloat(note.endBeatNum, note.endNumerator, note.endDenominator);
            m_noteEndBeatPositions[i] = endBeat;
        }
        else
        {
            m_noteEndBeatPositions[i] = beat;
        }
        m_noteXPositions[i] = static_cast<double>(note.x) / static_cast<double>(kLaneWidth);
    }

    std::sort(m_playableNoteTimesMs.begin(), m_playableNoteTimesMs.end());
    m_nextPlayableNoteIndex = static_cast<int>(std::lower_bound(
        m_playableNoteTimesMs.begin(),
        m_playableNoteTimesMs.end(),
        m_lastNoteSoundTimeMs) - m_playableNoteTimesMs.begin());

    m_timesDirty = false;
    m_noteDataDirty = false;
}

void ChartCanvas::setChartController(ChartController *controller)
{
    if (m_chartController)
    {
        disconnect(m_chartController, &ChartController::chartChanged, this, nullptr);
    }
    m_chartController = controller;
    if (controller)
    {
        connect(controller, &ChartController::chartChanged, this, [this]()
                {
            m_hyperCacheValid = false;
            m_noteDataDirty = true;
            m_timesDirty = true;
            m_bpmCacheDirty = true;
            m_backgroundCacheDirty = true;
            update(); });
        m_hyperfruitDetector->setCS(3.2);
        m_noteRenderer->setHyperfruitDetector(m_hyperfruitDetector);
        updateBackgroundCache();
        m_timesDirty = true;
        m_noteDataDirty = true;
        m_bpmCacheDirty = true;
    }
    update();
}

void ChartCanvas::setPlaybackController(PlaybackController *controller)
{
    if (m_playbackController == controller)
        return;

    if (m_playbackController)
    {
        disconnect(m_playbackController, &PlaybackController::positionChanged, this, &ChartCanvas::playbackPositionChanged);
        disconnect(m_playbackController, &PlaybackController::stateChanged, this, nullptr);
    }

    m_playbackController = controller;
    m_currentPlayTime = 0;

    if (m_playbackController)
    {
        connect(m_playbackController, &PlaybackController::positionChanged, this, &ChartCanvas::playbackPositionChanged);
        connect(m_playbackController, &PlaybackController::stateChanged,
                this, [this](PlaybackController::State state)
                {
            if (state == PlaybackController::Playing) {
                m_autoScrollEnabled = true;
                m_isPlaying = true;
                m_lastScrollSignalTimeMs = 0;
                m_hasPlaybackAnchor = false;
                m_playbackAnchorMs = m_playbackController ? m_playbackController->currentTime() : m_currentPlayTime;
                m_playbackAnchorWallMs = QDateTime::currentMSecsSinceEpoch();
                m_lastNoteSoundTimeMs = m_playbackAnchorMs;
                m_nextPlayableNoteIndex = static_cast<int>(std::lower_bound(
                    m_playableNoteTimesMs.begin(),
                    m_playableNoteTimesMs.end(),
                    m_lastNoteSoundTimeMs) - m_playableNoteTimesMs.begin());
                if (m_playbackTimer)
                    m_playbackTimer->start();
                requestNextFrame();
            } else {
                m_isPlaying = false;
                m_hasPlaybackAnchor = false;
                m_lastNoteSoundTimeMs = m_currentPlayTime;
                if (m_playbackTimer)
                    m_playbackTimer->stop();
                snapPlayheadToGrid();
                update();
            } });
        m_currentPlayTime = m_playbackController->currentTime();
    }

    update();
}

void ChartCanvas::setSelectionController(SelectionController *controller)
{
    m_selectionController = controller;
    connect(controller, &SelectionController::selectionChanged, this, QOverload<>::of(&ChartCanvas::update));
    connect(controller, &SelectionController::selectionChanged, this, &ChartCanvas::onSelectionChanged);
}

void ChartCanvas::setSkin(Skin *skin)
{
    m_noteRenderer->setSkin(skin);
    update();
}

void ChartCanvas::setColorMode(bool enabled)
{
    m_colorMode = enabled;
    m_noteRenderer->setShowColors(enabled);
    update();
}

void ChartCanvas::setHyperfruitEnabled(bool enabled)
{
    m_hyperfruitEnabled = enabled;
    m_noteRenderer->setHyperfruitEnabled(enabled);
    m_hyperCacheValid = false;
    update();
}

bool ChartCanvas::isVerticalFlip() const
{
    return m_verticalFlip;
}

void ChartCanvas::setVerticalFlip(bool flip)
{
    if (m_verticalFlip == flip)
        return;
    m_verticalFlip = flip;
    emit verticalFlipChanged(flip);
    update();
}

void ChartCanvas::setTimeDivision(int division)
{
    if (division != m_timeDivision)
    {
        m_timeDivision = division;
        snapPlayheadToGrid();
        update();
    }
}

void ChartCanvas::setGridDivision(int division)
{
    if (m_gridDivision != division)
    {
        m_gridDivision = division;
        update();
    }
}

void ChartCanvas::setGridSnap(bool snap)
{
    m_gridSnap = snap;
}

void ChartCanvas::setScrollPos(double timeMs)
{
    if (!chart())
        return;

    int beatNum, numerator, denominator;
    MathUtils::msToBeat(timeMs, chart()->bpmList(),
                        chart()->meta().offset,
                        beatNum, numerator, denominator);
    m_scrollBeat = beatNum + static_cast<double>(numerator) / denominator;
    update();
    emit scrollPositionChanged(m_scrollBeat);
}

void ChartCanvas::setNoteSize(int size)
{
    m_noteRenderer->setNoteSize(size);
    update();
}

void ChartCanvas::setMode(Mode mode)
{
    if (mode != PlaceRain)
    {
        m_rainFirst = true;
    }
    m_currentMode = mode;
    update();
}

void ChartCanvas::setNoteSoundFile(const QString &filePath)
{
    if (!m_noteSoundPlayer)
        return;
    m_noteSoundPlayer->setSoundFile(filePath);
}

void ChartCanvas::setNoteSoundEnabled(bool enabled)
{
    if (!m_noteSoundPlayer)
        return;
    m_noteSoundPlayer->setEnabled(enabled);
}

void ChartCanvas::setNoteSoundVolume(int volumePercent)
{
    if (!m_noteSoundPlayer)
        return;
    m_noteSoundPlayer->setVolumePercent(volumePercent);
}




