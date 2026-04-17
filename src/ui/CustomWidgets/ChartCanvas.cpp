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
      m_timeDivision(16),
      m_gridDivision(20),
      m_gridSnap(true),
      m_scrollBeat(0),
      m_baseVisibleBeatRange(10),
      m_timeScale(1.0),
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
      m_repaintTimer(nullptr),
      m_playbackTimer(nullptr),
      m_repaintPending(false),
      m_forceRepaint(false),
      m_lastRepaintTime(0),
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

    m_repaintTimer = new QTimer(this);
    m_repaintTimer->setSingleShot(true);
    connect(m_repaintTimer, &QTimer::timeout, this, &ChartCanvas::performDelayedRepaint);

    // 浼樺寲4锛氬垱寤烘挱鏀鹃┍鍔ㄥ畾鏃跺櫒锟?16ms 锟?60fps锟?
    m_playbackTimer = new QTimer(this);
    m_playbackTimer->setInterval(16);
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

void ChartCanvas::rebuildBpmTimeCache()
{
    m_bpmTimeCache.clear();
    if (!m_chartController || !m_chartController->chart())
    {
        m_bpmCacheDirty = false;
        return;
    }

    const auto &bpmList = m_chartController->chart()->bpmList();
    if (bpmList.isEmpty())
    {
        m_bpmCacheDirty = false;
        return;
    }

    const int offset = m_chartController->chart()->meta().offset;
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
    if (!m_chartController || !m_chartController->chart())
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
    const auto &notes = m_chartController->chart()->notes();
    const auto &bpmList = m_chartController->chart()->bpmList();

    // 闃插尽鎬ф鏌ワ細锟?BPM 鍒楄〃涓虹┖锛屾棤娉曡绠楁椂闂达紝娓呯┖缂撳瓨骞惰繑锟?
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

    // 浼樺寲2锛氭瀯锟?BPM 鏃堕棿缂撳瓨锛岀敤浜庡揩閫熻绠楅煶绗︽绉掓椂锟?    
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
        // 浣跨敤缂撳瓨蹇€熻绠楁绉掓椂锟?
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
        m_noteXPositions[i] = static_cast<double>(note.x) / 512.0;
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
                // 浼樺寲4锛氬惎鍔ㄦ挱鏀惧畾鏃跺櫒
                if (m_playbackTimer)
                    m_playbackTimer->start();
                requestNextFrame();
            } else {
                m_isPlaying = false;
                m_hasPlaybackAnchor = false;
                m_lastNoteSoundTimeMs = m_currentPlayTime;
                // 浼樺寲4锛氬仠姝㈡挱鏀惧畾鏃跺櫒
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
    int beatNum, numerator, denominator;
    MathUtils::msToBeat(timeMs, m_chartController->chart()->bpmList(),
                        m_chartController->chart()->meta().offset,
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

// ==================== 澶嶅埗绮樿创鏍稿績鍔熻兘 ====================

void ChartCanvas::handleCopy()
{
    if (!m_selectionController)
        return;

    QSet<int> selected = m_selectionController->selectedIndices();
    if (!selected.isEmpty())
    {
        m_selectionController->copySelected(m_chartController->chart()->notes());
        emit statusMessage(tr("Copied %1 notes").arg(selected.size()));
        if (m_intervalState != IntervalNone)
            cancelIntervalSelection();
        return;
    }

    if (m_intervalState == IntervalNone)
    {
        startIntervalSelection();
    }
    else if (m_intervalState == IntervalWaitingEnd)
    {
        completeIntervalSelection();
    }
}

void ChartCanvas::startIntervalSelection()
{
    double refTime = calculatePasteReferenceTime();
    m_intervalStartTime = refTime;
    m_intervalState = IntervalWaitingEnd;

    emit statusMessage(tr("Interval start at %1 ms. Adjust view and press Copy again to set end (Esc to cancel).").arg(refTime, 0, 'f', 0));
    setCursor(Qt::CrossCursor);
    update();
}

void ChartCanvas::completeIntervalSelection()
{
    if (m_intervalState != IntervalWaitingEnd)
        return;

    double endTime = calculatePasteReferenceTime();
    double startTime = m_intervalStartTime;
    if (startTime > endTime)
        std::swap(startTime, endTime);

    const auto &notes = m_chartController->chart()->notes();
    const auto &bpmList = m_chartController->chart()->bpmList();
    int offset = m_chartController->chart()->meta().offset;

    m_intervalNotes.clear();
    for (const Note &note : notes)
    {
        if (note.type == NoteType::SOUND)
            continue;
        double t = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator, bpmList, offset);
        if (t >= startTime && t <= endTime)
            m_intervalNotes.append(note);
    }

    m_intervalState = IntervalNone;
    setCursor(Qt::ArrowCursor);

    if (m_intervalNotes.isEmpty())
    {
        emit statusMessage(tr("No notes found in interval."));
        return;
    }

    m_selectionController->clearClipboard();
    for (const Note &note : m_intervalNotes)
        m_selectionController->getClipboard().append(note);

    beginPastePreview(m_intervalNotes);
    emit statusMessage(tr("Interval copied (%1 notes). Drag preview to adjust position, then click Confirm.").arg(m_intervalNotes.size()));
}

void ChartCanvas::cancelIntervalSelection()
{
    if (m_intervalState != IntervalNone)
    {
        m_intervalState = IntervalNone;
        setCursor(Qt::ArrowCursor);
        emit statusMessage(tr("Interval selection cancelled."));
        update();
    }
}

void ChartCanvas::cancelOperation()
{
    if (m_isPasting)
        cancelPaste();
    if (m_intervalState != IntervalNone)
        cancelIntervalSelection();
}

void ChartCanvas::paste()
{
    if (!m_selectionController)
        return;
    QVector<Note> clipboard = m_selectionController->getClipboard();
    if (clipboard.isEmpty())
    {
        emit statusMessage(tr("Clipboard is empty."));
        return;
    }
    beginPastePreview(clipboard);
}

void ChartCanvas::pasteAtCursor(const QPoint &pos)
{
    Q_ASSERT_X(!pos.isNull(), "ChartCanvas::pasteAtCursor", "Cursor position must be valid");
    if (!m_selectionController)
        return;
    QVector<Note> clipboard = m_selectionController->getClipboard();
    if (clipboard.isEmpty())
    {
        emit statusMessage(tr("Clipboard is empty."));
        return;
    }
    m_pasteCursorPos = pos;
    m_useCursorPaste = true;
    beginPastePreview(clipboard);
}

void ChartCanvas::beginPastePreview(const QVector<Note> &notes, const QPoint &cursorPos)
{
    m_pasteNotes = notes;
    m_pasteOriginalTimesMs.clear();
    m_pasteBaseOriginalTimeMs = std::numeric_limits<double>::max();
    m_pasteAnchorBeat = 0.0;
    m_isPasting = true;
    m_pasteTimeOffset = 0.0;
    m_pasteXOffset = 0.0;
    m_pasteTimeOffsetRaw = 0.0;
    m_pasteXOffsetRaw = 0.0;
    m_isDraggingPaste = false;

    if (!cursorPos.isNull())
    {
        m_pasteCursorPos = cursorPos;
        m_useCursorPaste = true;
    }
    else
    {
        m_useCursorPaste = false;
    }

    if (m_useCursorPaste)
    {
        m_pasteAnchorBeat = yToBeat(m_pasteCursorPos.y());
    }
    else
    {
        const double baselineRatio = 0.8;
        if (m_verticalFlip)
            m_pasteAnchorBeat = m_scrollBeat + (1.0 - baselineRatio) * effectiveVisibleBeatRange();
        else
            m_pasteAnchorBeat = m_scrollBeat + baselineRatio * effectiveVisibleBeatRange();
    }

    // 璁＄畻鏈€鏃╅煶绗︾殑鍘熷鎷嶆暟锛堢敤浜庡惛闄勶級
    if (!m_pasteNotes.isEmpty())
    {
        const bool canBuildTimeCache =
            m_chartController && m_chartController->chart() &&
            !m_chartController->chart()->bpmList().isEmpty();
        if (canBuildTimeCache)
        {
            const auto &bpmList = m_chartController->chart()->bpmList();
            int offset = m_chartController->chart()->meta().offset;
            m_pasteOriginalTimesMs.resize(m_pasteNotes.size());
            for (int i = 0; i < m_pasteOriginalTimesMs.size(); ++i)
                m_pasteOriginalTimesMs[i] = std::numeric_limits<double>::quiet_NaN();

            for (int i = 0; i < m_pasteNotes.size(); ++i)
            {
                const Note &note = m_pasteNotes[i];
                if (note.type == NoteType::SOUND)
                    continue;
                const double t = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator, bpmList, offset);
                m_pasteOriginalTimesMs[i] = t;
                if (t < m_pasteBaseOriginalTimeMs)
                    m_pasteBaseOriginalTimeMs = t;
            }
        }
        double minBeat = std::numeric_limits<double>::max();
        int refIdx = 0;
        for (int i = 0; i < m_pasteNotes.size(); ++i)
        {
            const Note &note = m_pasteNotes[i];
            if (note.type == NoteType::SOUND)
                continue;
            double beat = MathUtils::beatToFloat(note.beatNum, note.numerator, note.denominator);
            if (beat < minBeat)
            {
                minBeat = beat;
                refIdx = i;
            }
        }
        m_pasteRefBeat = minBeat;
        m_pasteDragReferenceIndex = refIdx;
    }

    setFocus();
    update();
}

double ChartCanvas::calculatePasteReferenceTime() const
{
    if (!m_chartController || !m_chartController->chart())
        return 0.0;

    const auto &bpmList = m_chartController->chart()->bpmList();
    int offset = m_chartController->chart()->meta().offset;

    if (m_useCursorPaste)
    {
        double y = m_pasteCursorPos.y();
        double beat = yToBeat(y);
        int beatNum, num, den;
        MathUtils::floatToBeat(beat, beatNum, num, den);
        return MathUtils::beatToMs(beatNum, num, den, bpmList, offset);
    }
    else
    {
        double baselineRatio = 0.8;
        double baselineBeat;
        if (m_verticalFlip)
            baselineBeat = m_scrollBeat + (1.0 - baselineRatio) * effectiveVisibleBeatRange();
        else
            baselineBeat = m_scrollBeat + baselineRatio * effectiveVisibleBeatRange();
        int beatNum, num, den;
        MathUtils::floatToBeat(baselineBeat, beatNum, num, den);
        return MathUtils::beatToMs(beatNum, num, den, bpmList, offset);
    }
}

double ChartCanvas::snapPasteTimeOffset(double offsetBeat) const
{
    if (m_timeDivision <= 0 || m_pasteDragReferenceIndex < 0 || m_pasteDragReferenceIndex >= m_pasteNotes.size())
        return offsetBeat;

    const Note &refNote = m_pasteNotes[m_pasteDragReferenceIndex];
    double refOriginalBeat = MathUtils::beatToFloat(refNote.beatNum, refNote.numerator, refNote.denominator);
    double refNewBeat = refOriginalBeat + offsetBeat;

    Note tempNote = refNote;
    MathUtils::floatToBeat(refNewBeat, tempNote.beatNum, tempNote.numerator, tempNote.denominator);
    tempNote = MathUtils::snapNoteToTimeWithBoundary(tempNote, m_timeDivision);
    double refSnappedBeat = MathUtils::beatToFloat(tempNote.beatNum, tempNote.numerator, tempNote.denominator);
    return refSnappedBeat - refOriginalBeat;
}

void ChartCanvas::beginDragPaste(const QPointF &startPos)
{
    if (!m_isPasting)
        return;
    m_isDraggingPaste = true;
    m_pasteDragStartPos = startPos;
    m_pasteTimeOffsetRaw = m_pasteTimeOffset;
    m_pasteXOffsetRaw = m_pasteXOffset;
}

void ChartCanvas::updateDragPaste(const QPointF &currentPos)
{
    if (!m_isDraggingPaste)
        return;

    QPointF delta = currentPos - m_pasteDragStartPos;
    double deltaY = delta.y();
    if (m_verticalFlip)
        deltaY = -deltaY;

    // 鏃堕棿鍋忕Щ锛堟媿鏁帮級
    double deltaBeat = (deltaY / height()) * effectiveVisibleBeatRange();
    // X 鍋忕Щ锛堝儚绱犺浆 X 鍧愭爣锟?    
    const int availableWidth = qMax(1, width() - leftMargin() - rightMargin());
    double deltaX = delta.x() / static_cast<double>(availableWidth) * 512.0;

    m_pasteTimeOffsetRaw += deltaBeat;
    m_pasteXOffsetRaw += deltaX;

    // Keep unsnapped offsets to preserve sub-grid remainder while dragging.
    m_pasteTimeOffset = snapPasteTimeOffset(m_pasteTimeOffsetRaw);
    m_pasteXOffset = m_pasteXOffsetRaw;

    m_pasteDragStartPos = currentPos;
    update();
}

void ChartCanvas::endDragPaste()
{
    m_isDraggingPaste = false;
}

void ChartCanvas::confirmPaste()
{
    if (!m_chartController || m_pasteNotes.isEmpty())
    {
        m_isPasting = false;
        m_pasteNotes.clear();
        m_pasteOriginalTimesMs.clear();
        m_pasteBaseOriginalTimeMs = std::numeric_limits<double>::max();
        m_pasteAnchorBeat = 0.0;
        return;
    }

    const auto &bpmList = m_chartController->chart()->bpmList();
    int offset = m_chartController->chart()->meta().offset;
    const QVector<MathUtils::BpmCacheEntry> &bpmCache = bpmTimeCache();
    auto beatFromTimeMs = [&bpmCache, &bpmList, offset](double ms) -> double
    {
        if (!bpmCache.isEmpty())
        {
            int lo = 0;
            int hi = bpmCache.size() - 1;
            while (lo < hi)
            {
                int mid = (lo + hi + 1) / 2;
                if (bpmCache[mid].accumulatedMs <= ms)
                    lo = mid;
                else
                    hi = mid - 1;
            }
            const auto &seg = bpmCache[lo];
            if (seg.bpm <= 0.0)
                return seg.beatPos;
            return seg.beatPos + (ms - seg.accumulatedMs) * (seg.bpm / 60000.0);
        }
        int b = 0, n = 0, d = 1;
        MathUtils::msToBeat(ms, bpmList, offset, b, n, d);
        return MathUtils::beatToFloat(b, n, d);
    };
    auto assignBeatWithDen = [](double beatFloat, int targetDen, int &outBeatNum, int &outNum, int &outDen)
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

    QVector<double> fallbackOriginalTimes;
    bool usingCachedOriginalTimes =
        (m_pasteOriginalTimesMs.size() == m_pasteNotes.size()) &&
        std::isfinite(m_pasteBaseOriginalTimeMs);
    if (!usingCachedOriginalTimes)
    {
        fallbackOriginalTimes.resize(m_pasteNotes.size());
        for (int i = 0; i < fallbackOriginalTimes.size(); ++i)
            fallbackOriginalTimes[i] = std::numeric_limits<double>::quiet_NaN();

        m_pasteBaseOriginalTimeMs = std::numeric_limits<double>::max();
        for (int i = 0; i < m_pasteNotes.size(); ++i)
        {
            const Note &note = m_pasteNotes[i];
            if (note.type == NoteType::SOUND)
                continue;
            const double t = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator, bpmList, offset);
            fallbackOriginalTimes[i] = t;
            if (t < m_pasteBaseOriginalTimeMs)
                m_pasteBaseOriginalTimeMs = t;
        }
        usingCachedOriginalTimes = std::isfinite(m_pasteBaseOriginalTimeMs);
    }

    const QVector<double> &sourceOriginalTimes = usingCachedOriginalTimes ? m_pasteOriginalTimesMs : fallbackOriginalTimes;
    const double baseOriginalTime = m_pasteBaseOriginalTimeMs;
    if (baseOriginalTime == std::numeric_limits<double>::max())
    {
        m_isPasting = false;
        m_pasteNotes.clear();
        m_pasteOriginalTimesMs.clear();
        m_pasteBaseOriginalTimeMs = std::numeric_limits<double>::max();
        m_pasteTimeOffsetRaw = 0.0;
        m_pasteXOffsetRaw = 0.0;
        m_pasteAnchorBeat = 0.0;
        update();
        return;
    }

    const double baseOriginalBeat = beatFromTimeMs(baseOriginalTime);
    const double referenceBeat = m_pasteAnchorBeat;
    const double baseBeatShift = referenceBeat - baseOriginalBeat;
    const double snappedTotalBeatShift = snapPasteTimeOffset(baseBeatShift + m_pasteTimeOffset);

    double finalXShift = m_pasteXOffset;

    QVector<Note> newNotes;
    for (int i = 0; i < m_pasteNotes.size(); ++i)
    {
        const Note &originalNote = m_pasteNotes[i];
        if (originalNote.type == NoteType::SOUND)
            continue;
        if (i >= sourceOriginalTimes.size())
            continue;
        const double originalTime = sourceOriginalTimes[i];
        if (!std::isfinite(originalTime))
            continue;
        Note newNote = originalNote;
        newNote.id = Note::generateId();

        // 时间偏移（拍域）：避免 duration/absolute time 混用带来的 offset 误差
        int b = 0, n = 0, d = 1;
        const int targetStartDen = Settings::instance().pasteUse288Division() ? 288 : qMax(1, originalNote.denominator);
        const double originalBeatFloat = beatFromTimeMs(originalTime);
        const double newBeatFloat = originalBeatFloat + snappedTotalBeatShift;
        assignBeatWithDen(newBeatFloat, targetStartDen, b, n, d);
        newNote.beatNum = b;
        newNote.numerator = n;
        newNote.denominator = d;

        // Rain 缁撴潫鏃堕棿澶勭悊
        if (originalNote.type == NoteType::RAIN)
        {
            int eb = 0, en = 0, ed = 1;
            const int targetEndDen = Settings::instance().pasteUse288Division() ? 288 : qMax(1, originalNote.endDenominator);
            const double originalEndBeatFloat = MathUtils::beatToFloat(originalNote.endBeatNum, originalNote.endNumerator, originalNote.endDenominator);
            const double newEndBeatFloat = originalEndBeatFloat + snappedTotalBeatShift;
            assignBeatWithDen(newEndBeatFloat, targetEndDen, eb, en, ed);
            newNote.endBeatNum = eb;
            newNote.endNumerator = en;
            newNote.endDenominator = ed;
        }

        // X 坐标偏移并边界约束
        newNote.x = originalNote.x + qRound(finalXShift);
        newNote.x = qBound(0, newNote.x, 512);

        newNotes.append(newNote);
    }

    if (!newNotes.isEmpty())
    {
        m_chartController->addNotes(newNotes);
        emit statusMessage(tr("Pasted %1 notes").arg(newNotes.size()));
    }

    m_isPasting = false;
    m_pasteNotes.clear();
    m_pasteOriginalTimesMs.clear();
    m_pasteBaseOriginalTimeMs = std::numeric_limits<double>::max();
    m_pasteTimeOffsetRaw = 0.0;
    m_pasteXOffsetRaw = 0.0;
    m_pasteAnchorBeat = 0.0;
    update();
}

// ==================== 鍏朵粬鍘熸湁鏂规硶锛堜繚鐣欏師鏈夊疄鐜帮級 ====================

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
    if (qFuzzyCompare(m_timeScale, scale))
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

    m_timeScale = scale;

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

void ChartCanvas::requestNextFrame()
{
    if (!m_isPlaying)
    {
        // 浼樺寲4锛氬仠姝㈡挱鏀惧畾鏃跺櫒
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
    if (!m_chartController || !m_chartController->chart())
    {
        painter.fillRect(rect(), Settings::instance().backgroundColor());
        return;
    }

    if (m_timesDirty || m_noteDataDirty)
        rebuildNoteTimesCache();

    const Chart *chart = m_chartController->chart();
    const auto &bpmList = chart->bpmList();
    int offset = chart->meta().offset;
    const auto &notes = chart->notes();

    drawBackground(painter);
    drawGrid(painter);

    double startBeat = m_scrollBeat;
    double visibleRange = effectiveVisibleBeatRange();
    double endBeat = startBeat + visibleRange;

    // 浼樺寲2锛氳绠楀彲瑙佽寖鍥寸殑姣鏃堕棿锛堢敤锟?Y 鍧愭爣绾挎€ф槧灏勶級
    

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

        // 浼樺寲2锛氬熀浜庢绉掓椂闂寸嚎鎬ф槧灏勫埌 Y 鍧愭爣锛岄伩鍏嶆瘡甯ц皟锟?beatToMs
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

    // 缁樺埗绮樿创棰勮锛堟敮鎸佹暣浣撴嫋鍔ㄥ亸绉伙級
    if (m_isPasting && !m_pasteNotes.isEmpty())
    {
        painter.setOpacity(0.5);

        // 璁＄畻褰撳墠棰勮鍩哄噯鏃堕棿锛堝弬鑰冪嚎/鍏夋爣鏃堕棿锟?
        // 鎵惧埌鏈€鏃╅煶绗︾殑鍘熷鏃堕棿
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
                const int previewShiftedX = qBound(0, note.x + qRound(m_pasteXOffset), 512);
                const double x = lmargin + (previewShiftedX / 512.0) * availableWidth;
                // 棰勮鏃朵笉杩涜杈圭晫瑁佸壀
                m_noteRenderer->drawNote(painter, note, QPointF(x, y), false, -1);
            }
        }

        painter.setOpacity(1.0);
        painter.fillRect(QRect(10, 10, 100, 30), QColor(200, 200, 200));
        painter.drawText(QRect(10, 10, 100, 30), Qt::AlignCenter, tr("Confirm"));
        painter.fillRect(QRect(120, 10, 100, 30), QColor(200, 200, 200));
        painter.drawText(QRect(120, 10, 100, 30), Qt::AlignCenter, tr("Cancel"));
    }

    double baselineY = canvasHeight * 0.8;
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

void ChartCanvas::drawBackground(QPainter &painter)
{
    QSize sz = size();
    if (m_backgroundCacheDirty || m_backgroundCache.size() != sz)
    {
        if (!m_chartController || !m_chartController->chart())
        {
            m_backgroundCache = m_backgroundRenderer->generateBackground(sz);
        }
        else
        {
            const MetaData &meta = m_chartController->chart()->meta();
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


        // 浼樺寲3锛氭瀯锟?BPM 鏃堕棿缂撳瓨骞朵紶閫掔粰缃戞牸娓叉煋鍣紝鍑忓皯閲嶅杞崲
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
                                 m_verticalFlip);
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
                               m_chartController->chart()->bpmList(),
                               m_chartController->chart()->meta().offset);
}

double ChartCanvas::yPosFromTime(double timeMs) const
{
    int beatNum, numerator, denominator;
    MathUtils::msToBeat(timeMs, m_chartController->chart()->bpmList(),
                        m_chartController->chart()->meta().offset,
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
                               m_chartController->chart()->bpmList(),
                               m_chartController->chart()->meta().offset);
}

QPointF ChartCanvas::noteToPos(const Note &note) const
{
    double beat = MathUtils::beatToFloat(note.beatNum, note.numerator, note.denominator);
    double y = beatToY(beat);
    int lmargin = leftMargin();
    int rmargin = rightMargin();
    int availableWidth = qMax(1, width() - lmargin - rmargin);
    double x = lmargin + (note.x / 512.0) * availableWidth;
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
    int x = static_cast<int>((pos.x() - lmargin) / availableWidth * 512);

    if (m_gridSnap)
    {
        x = MathUtils::snapXToGrid(x, m_gridDivision);
    }

    x = qBound(0, x, 512);

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
    if (!m_chartController || !m_chartController->chart())
        return QRectF();

    const auto &bpmList = m_chartController->chart()->bpmList();
    int offset = m_chartController->chart()->meta().offset;

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
    if (!m_chartController || !m_chartController->chart())
        return -1;

    const auto &notes = m_chartController->chart()->notes();
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

void ChartCanvas::prepareMoveChanges()
{
    m_moveChanges.clear();
    const auto &notes = m_chartController->chart()->notes();
    for (int idx : m_originalSelectedIndices)
    {
        if (idx >= 0 && idx < notes.size())
            m_moveChanges.insert(idx, qMakePair(notes[idx], notes[idx]));
    }
}

void ChartCanvas::beginMoveSelection(const QPointF &startPos, int referenceIndex)
{
    m_isMovingSelection = true;
    m_moveStartPos = startPos;
    m_moveDeltaBeatRaw = 0.0;
    m_moveDeltaXRaw = 0.0;
    m_originalSelectedIndices = m_selectionController->selectedIndices();
    m_dragReferenceIndex = referenceIndex;
    if (m_dragReferenceIndex == -1 && !m_originalSelectedIndices.isEmpty())
    {
        m_dragReferenceIndex = *m_originalSelectedIndices.begin();
    }

    m_gridSnapBackup = m_gridSnap;
    m_wasGridSnapEnabled = m_gridSnap;
    m_gridSnap = false;

    prepareMoveChanges();
    update();
}

void ChartCanvas::updateMoveSelection(const QPointF &currentPos)
{
    if (!m_isMovingSelection)
        return;

    QPointF delta = currentPos - m_moveStartPos;
    double deltaY = delta.y();
    if (m_verticalFlip)
        deltaY = -deltaY;

    const int availableWidth = qMax(1, width() - leftMargin() - rightMargin());
    const double deltaBeat = (deltaY / height()) * effectiveVisibleBeatRange();
    const double deltaX = delta.x() / static_cast<double>(availableWidth) * 512.0;

    m_moveDeltaBeatRaw += deltaBeat;
    m_moveDeltaXRaw += deltaX;
    m_moveStartPos = currentPos;

    double appliedDeltaBeat = m_moveDeltaBeatRaw;
    if (m_timeDivision > 0 && m_dragReferenceIndex >= 0 && m_moveChanges.contains(m_dragReferenceIndex))
    {
        const Note &refOriginal = m_moveChanges[m_dragReferenceIndex].first;
        const double refOriginalBeat = MathUtils::beatToFloat(refOriginal.beatNum, refOriginal.numerator, refOriginal.denominator);
        const double refNewBeat = refOriginalBeat + m_moveDeltaBeatRaw;

        Note refSnapped = refOriginal;
        MathUtils::floatToBeat(refNewBeat, refSnapped.beatNum, refSnapped.numerator, refSnapped.denominator);
        refSnapped = MathUtils::snapNoteToTimeWithBoundary(refSnapped, m_timeDivision);
        const double refSnappedBeat = MathUtils::beatToFloat(refSnapped.beatNum, refSnapped.numerator, refSnapped.denominator);
        appliedDeltaBeat = refSnappedBeat - refOriginalBeat;
    }
    const double appliedDeltaX = m_moveDeltaXRaw;

    QVector<Note> &notes = const_cast<QVector<Note> &>(m_chartController->chart()->notes());
    QList<int> selectedList = m_originalSelectedIndices.values();

    for (int idx : selectedList)
    {
        const Note &original = m_moveChanges[idx].first;
        Note newNote = original;

        double originalBeat = MathUtils::beatToFloat(original.beatNum, original.numerator, original.denominator);
        double newBeat = originalBeat + appliedDeltaBeat;
        if (newBeat < 0)
            newBeat = 0;
        MathUtils::floatToBeat(newBeat, newNote.beatNum, newNote.numerator, newNote.denominator);

        newNote.x = qBound(0, qRound(original.x + appliedDeltaX), 512);

        if (original.type == NoteType::RAIN)
        {
            double originalEndBeat = MathUtils::beatToFloat(original.endBeatNum, original.endNumerator, original.endDenominator);
            double newEndBeat = originalEndBeat + appliedDeltaBeat;
            if (newEndBeat < newBeat)
                newEndBeat = newBeat;
            MathUtils::floatToBeat(newEndBeat, newNote.endBeatNum, newNote.endNumerator, newNote.endDenominator);
        }

        notes[idx] = newNote;
    }

    m_noteDataDirty = true;
    m_timesDirty = true;
    m_hyperCacheValid = false;

    update();
}

void ChartCanvas::endMoveSelection()
{
    if (!m_isMovingSelection)
        return;

    m_isMovingSelection = false;

    if (m_wasGridSnapEnabled)
    {
        m_gridSnap = m_gridSnapBackup;
        m_wasGridSnapEnabled = false;
    }

    double finalAppliedDeltaBeat = m_moveDeltaBeatRaw;
    if (m_timeDivision > 0 && m_dragReferenceIndex >= 0 && m_moveChanges.contains(m_dragReferenceIndex))
    {
        const Note &refOriginal = m_moveChanges[m_dragReferenceIndex].first;
        const double refOriginalBeat = MathUtils::beatToFloat(refOriginal.beatNum, refOriginal.numerator, refOriginal.denominator);
        const double refNewBeat = refOriginalBeat + m_moveDeltaBeatRaw;

        Note refSnapped = refOriginal;
        MathUtils::floatToBeat(refNewBeat, refSnapped.beatNum, refSnapped.numerator, refSnapped.denominator);
        refSnapped = MathUtils::snapNoteToTimeWithBoundary(refSnapped, m_timeDivision);
        const double refSnappedBeat = MathUtils::beatToFloat(refSnapped.beatNum, refSnapped.numerator, refSnapped.denominator);
        finalAppliedDeltaBeat = refSnappedBeat - refOriginalBeat;
    }
    const double finalAppliedDeltaX = m_moveDeltaXRaw;

    QVector<Note> &notes = const_cast<QVector<Note> &>(m_chartController->chart()->notes());
    QList<int> selectedList = m_originalSelectedIndices.values();
    for (int idx : selectedList)
    {
        if (!m_moveChanges.contains(idx))
            continue;

        const Note &original = m_moveChanges[idx].first;
        Note snappedNote = original;

        double newBeat = MathUtils::beatToFloat(original.beatNum, original.numerator, original.denominator) + finalAppliedDeltaBeat;
        if (newBeat < 0.0)
            newBeat = 0.0;
        MathUtils::floatToBeat(newBeat, snappedNote.beatNum, snappedNote.numerator, snappedNote.denominator);

        snappedNote.x = qBound(0, qRound(original.x + finalAppliedDeltaX), 512);

        if (original.type == NoteType::RAIN)
        {
            double newEndBeat = MathUtils::beatToFloat(original.endBeatNum, original.endNumerator, original.endDenominator) + finalAppliedDeltaBeat;
            if (newEndBeat < newBeat)
                newEndBeat = newBeat;
            MathUtils::floatToBeat(newEndBeat, snappedNote.endBeatNum, snappedNote.endNumerator, snappedNote.endDenominator);
        }

        notes[idx] = snappedNote;
    }

    QList<QPair<Note, Note>> finalChanges;
    const auto &notesNow = m_chartController->chart()->notes();
    for (int idx : m_originalSelectedIndices)
    {
        const Note &currentNote = notesNow[idx];
        auto it = m_moveChanges.find(idx);
        if (it != m_moveChanges.end())
        {
            const Note &originalNote = it->first;
            if (!(originalNote == currentNote))
                finalChanges.append(qMakePair(originalNote, currentNote));
        }
    }

    if (!finalChanges.isEmpty())
        m_chartController->moveNotes(finalChanges);

    m_originalSelectedIndices.clear();
    m_dragReferenceIndex = -1;
    m_moveDeltaBeatRaw = 0.0;
    m_moveDeltaXRaw = 0.0;
    m_moveChanges.clear();
    m_moveStartPos = QPointF();

    m_noteDataDirty = true;
    m_timesDirty = true;
    update();
}

void ChartCanvas::mousePressEvent(QMouseEvent *event)
{
    if (m_intervalState != IntervalNone)
    {
        if (event->button() == Qt::RightButton)
            cancelIntervalSelection();
        return;
    }

    if (event->button() == Qt::LeftButton)
    {
        // 绮樿创棰勮鎷栧姩妫€锟?
        if (m_isPasting)
        {
            // 妫€鏌ユ槸鍚︾偣鍑诲埌棰勮鎸夐挳鍖哄煙
            if (event->pos().x() >= 10 && event->pos().x() <= 110 && event->pos().y() >= 10 && event->pos().y() <= 40)
            {
                confirmPaste();
                return;
            }
            else if (event->pos().x() >= 120 && event->pos().x() <= 220 && event->pos().y() >= 10 && event->pos().y() <= 40)
            {
                cancelPaste();
                return;
            }
            else
            {
                // 寮€濮嬫嫋鍔ㄩ锟?
                beginDragPaste(event->pos());
                return;
            }
        }

        if (event->modifiers() & Qt::ControlModifier)
        {
            m_isSelecting = true;
            m_selectionStart = event->pos();
            m_selectionEnd = event->pos();
            return;
        }

        if (m_currentMode == PlaceRain)
        {
            if (m_rainFirst)
            {
                m_rainStartPos = event->pos();
                m_rainFirst = false;
                return;
            }
            else
            {
                QPointF endPos = event->pos();
                m_rainFirst = true;
                Note startNote = posToNote(m_rainStartPos);
                Note endNote = posToNote(endPos);
                double startTime = MathUtils::beatToMs(startNote.beatNum, startNote.numerator, startNote.denominator,
                                                       m_chartController->chart()->bpmList(),
                                                       m_chartController->chart()->meta().offset);
                double endTime = MathUtils::beatToMs(endNote.beatNum, endNote.numerator, endNote.denominator,
                                                     m_chartController->chart()->bpmList(),
                                                     m_chartController->chart()->meta().offset);
                if (endTime > startTime)
                {
                    Note rainNote(startNote.beatNum, startNote.numerator, startNote.denominator,
                                  endNote.beatNum, endNote.numerator, endNote.denominator,
                                  startNote.x);
                    if (rainNote.isValidRain())
                    {
                        m_chartController->addNote(rainNote);
                    }
                    else
                    {
                        QMessageBox::warning(this, tr("Invalid Rain Note"), tr("Invalid rain note parameters."));
                    }
                }
                else
                {
                    QMessageBox::warning(this, tr("Invalid Rain Note"), tr("End time must be later than start time"));
                }
                return;
            }
        }

        int hitIndex = hitTestNote(event->pos());
        if (hitIndex != -1)
        {
            if (m_currentMode == Delete)
            {
                const auto &notes = m_chartController->chart()->notes();
                if (hitIndex >= 0 && hitIndex < notes.size())
                {
                    QVector<Note> noteToDelete;
                    noteToDelete.append(notes[hitIndex]);
                    m_chartController->removeNotes(noteToDelete);
                }
                return;
            }

            if (event->modifiers() & Qt::ControlModifier)
            {
                if (m_selectionController->selectedIndices().contains(hitIndex))
                    m_selectionController->removeFromSelection(hitIndex);
                else
                    m_selectionController->addToSelection(hitIndex);
                return;
            }

            if (!m_selectionController->selectedIndices().contains(hitIndex))
            {
                m_selectionController->clearSelection();
                m_selectionController->addToSelection(hitIndex);
            }

            beginMoveSelection(event->pos(), hitIndex);
            return;
        }

        m_selectionController->clearSelection();

        if (m_currentMode == PlaceNote)
        {
            Note note = posToNote(event->pos());
            m_chartController->addNote(note);
        }
    }
    else if (event->button() == Qt::RightButton)
    {
        QMenu menu(this);
        QAction *playFromRefAction = menu.addAction(tr("Play from Reference Time"));
        QAction *pasteAction = menu.addAction(tr("Paste"));
        pasteAction->setEnabled(m_selectionController && !m_selectionController->getClipboard().isEmpty());
        QAction *selectedAction = menu.exec(event->globalPos());
        if (selectedAction == playFromRefAction)
        {
            playFromReferenceLine();
        }
        else if (selectedAction == pasteAction)
        {
            pasteAtCursor(event->pos());
        }
    }
}

void ChartCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isSelecting)
    {
        m_selectionEnd = event->pos();
        update();
    }
    else if (m_isMovingSelection)
    {
        updateMoveSelection(event->pos());
    }
    else if (m_isDraggingPaste)
    {
        updateDragPaste(event->pos());
    }
}

void ChartCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_isSelecting)
    {
        QRectF rect = QRectF(m_selectionStart, m_selectionEnd).normalized();
        m_selectionController->selectInRect(rect, m_chartController->chart()->notes(),
                                            [this](const Note &note)
                                            { return noteToPos(note); });
        m_isSelecting = false;
        update();
    }
    else if (m_isMovingSelection)
    {
        endMoveSelection();
    }
    else if (m_isDraggingPaste)
    {
        endDragPaste();
    }
    else if (m_isDragging)
    {
        m_isDragging = false;
        m_draggedNotes.clear();
    }
}

void ChartCanvas::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier)
    {
        double delta = event->angleDelta().y();
        if (delta != 0)
        {
            double factor = (delta > 0) ? 1.2 : 1.0 / 1.2;
            double newScale = m_timeScale * factor;
            newScale = qBound(0.2, newScale, 5.0);
            setTimeScale(newScale);
        }
        event->accept();
        return;
    }

    m_isScrolling = true;
    stopSnapTimer();

    double delta = event->angleDelta().y();
    if (delta != 0)
    {
        double step = effectiveVisibleBeatRange() * 0.1;
        double newPos = m_scrollBeat - (delta / 120.0) * step;
        if (newPos < 0)
            newPos = 0;
        m_scrollBeat = newPos;
        m_autoScrollEnabled = false;
        update();
        emit scrollPositionChanged(m_scrollBeat);

        if (m_chartController && m_chartController->chart())
        {
            const auto &bpmList = m_chartController->chart()->bpmList();
            int offset = m_chartController->chart()->meta().offset;

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

            int beatNum, numerator, denominator;
            MathUtils::floatToBeat(baselineBeat, beatNum, numerator, denominator);
            double timeMs = MathUtils::beatToMs(beatNum, numerator, denominator, bpmList, offset);
            m_currentPlayTime = timeMs;
        }
    }

    startSnapTimer();
}

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

void ChartCanvas::performDelayedRepaint()
{
    if (m_repaintPending || m_forceRepaint)
    {
        m_repaintPending = false;
        m_forceRepaint = false;
        m_lastRepaintTime = QDateTime::currentMSecsSinceEpoch();
        update();
    }
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


