#include "ChartCanvas.h"
#include "controller/ChartController.h"
#include "controller/SelectionController.h"
#include "controller/PlaybackController.h"
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
#include <QWindow>
#include <QShowEvent>
#include <QDebug>
#include <chrono>
#include <algorithm>

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
      m_isMovingSelection(false),
      m_gridSnapBackup(false),
      m_wasGridSnapEnabled(false),
      m_dragReferenceIndex(-1),
      m_rainFirst(true),
      m_snapToGrid(true),
      m_snapTimerId(0),
      m_isScrolling(false),
      m_repaintTimer(nullptr),
      m_repaintPending(false),
      m_forceRepaint(false),
      m_lastRepaintTime(0),
      m_hyperCacheValid(false),
      m_backgroundCacheDirty(true),
      m_noteDataDirty(true),
      m_timesDirty(true),
      m_frameCount(0),
      m_currentFps(0.0),
      m_isPlaying(false)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(800, 400);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NativeWindow);

    m_noteRenderer->setNoteSize(Settings::instance().noteSize());

    m_repaintTimer = new QTimer(this);
    m_repaintTimer->setSingleShot(true);
    connect(m_repaintTimer, &QTimer::timeout, this, &ChartCanvas::performDelayedRepaint);

    m_fpsTimer.start();
}

ChartCanvas::~ChartCanvas()
{
    delete m_noteRenderer;
    delete m_gridRenderer;
    delete m_hyperfruitDetector;
    delete m_backgroundRenderer;
}

void ChartCanvas::rebuildNoteTimesCache()
{
    if (!m_chartController || !m_chartController->chart())
    {
        m_noteBeatPositions.clear();
        m_noteEndBeatPositions.clear();
        m_noteXPositions.clear();
        m_noteTypes.clear();
        m_timesDirty = false;
        m_noteDataDirty = false;
        return;
    }
    const auto &notes = m_chartController->chart()->notes();
    const auto &bpmList = m_chartController->chart()->bpmList();
    int offset = m_chartController->chart()->meta().offset;

    const int N = notes.size();
    m_noteBeatPositions.resize(N);
    m_noteEndBeatPositions.resize(N);
    m_noteXPositions.resize(N);
    m_noteTypes.resize(N);

    for (int i = 0; i < N; ++i)
    {
        const Note &note = notes[i];
        m_noteTypes[i] = note.type;
        if (note.type == NoteType::SOUND)
        {
            m_noteBeatPositions[i] = 0.0;
            m_noteEndBeatPositions[i] = 0.0;
            m_noteXPositions[i] = 0.0;
            continue;
        }
        double beat = MathUtils::beatToFloat(note.beatNum, note.numerator, note.denominator);
        m_noteBeatPositions[i] = beat;
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
            m_backgroundCacheDirty = true;
            update(); });
        m_hyperfruitDetector->setCS(3.2);
        m_noteRenderer->setHyperfruitDetector(m_hyperfruitDetector);
        updateBackgroundCache();
        m_timesDirty = true;
        m_noteDataDirty = true;
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
                requestNextFrame();
            } else {
                m_isPlaying = false;
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

void ChartCanvas::paste()
{
    if (!m_selectionController)
        return;
    QVector<Note> clipboard = m_selectionController->getClipboard();
    if (clipboard.isEmpty())
        return;

    m_pasteNotes = clipboard;
    m_pasteOffset = QPointF(0, 0);
    m_isPasting = true;
    setFocus();
    update();
}

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
        return;

    if (m_playbackController)
    {
        m_currentPlayTime = m_playbackController->currentTime();
    }

    if (m_autoScrollEnabled && m_chartController && m_chartController->chart())
    {
        const auto &bpmList = m_chartController->chart()->bpmList();
        int offset = m_chartController->chart()->meta().offset;
        int beatNum, numerator, denominator;
        MathUtils::msToBeat(m_currentPlayTime, bpmList, offset, beatNum, numerator, denominator);
        double beat = beatNum + static_cast<double>(numerator) / denominator;

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
        emit scrollPositionChanged(m_scrollBeat);
    }

    if (QWindow *win = windowHandle())
    {
        win->requestUpdate();
    }
    else
    {
        update();
        QTimer::singleShot(16, this, &ChartCanvas::requestNextFrame);
    }

    update();
}

void ChartCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    // FPS 计算
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

    int startBeatNum, startNum, startDen;
    MathUtils::floatToBeat(startBeat, startBeatNum, startNum, startDen);
    double startTime = MathUtils::beatToMs(startBeatNum, startNum, startDen, bpmList, offset);
    int endBeatNum, endNum, endDen;
    MathUtils::floatToBeat(endBeat, endBeatNum, endNum, endDen);
    double endTime = MathUtils::beatToMs(endBeatNum, endNum, endDen, bpmList, offset);
    const double TIME_MARGIN = 100.0;

    if (m_hyperfruitEnabled && !bpmList.isEmpty())
    {
        if (!m_hyperCacheValid)
        {
            m_cachedHyperSet = m_hyperfruitDetector->detect(notes, bpmList, offset);
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

    // ---- 快速剔除优化 ----
    if (type == NoteType::NORMAL) {
        // 普通音符：起始拍不在可见区域附近则跳过
        if (beat < startBeat - 0.5 || beat > endBeat + 0.5)
            continue;
    } else if (type == NoteType::RAIN) {
        // Rain 音符：整个时间区间与可见区域无交集则跳过
        if (endBeatNote <= startBeat || beat >= endBeat)
            continue;
    }

    double y = baseY + sign * ((beat - m_scrollBeat) * invVisibleRange * canvasHeight);

    if (type == NoteType::RAIN)
    {
        // Rain 绘制（区间已确保有交集）
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
    else // NORMAL
    {
        double x = lmargin + m_noteXPositions[i] * availableWidth;
        QPointF pos(x, y);
        bool selected = selectedSet.contains(i);
        m_noteRenderer->drawNote(painter, notes[i], pos, selected, i);
        renderedNotesCount++;
    }
}

    if (m_isPasting && !m_pasteNotes.isEmpty())
    {
        painter.setOpacity(0.5);
        for (const Note &note : m_pasteNotes)
        {
            if (note.type == NoteType::SOUND)
                continue;
            double beat = MathUtils::beatToFloat(note.beatNum, note.numerator, note.denominator);
            double y = baseY + sign * ((beat - m_scrollBeat) * invVisibleRange * canvasHeight);
            double x = lmargin + (note.x / 512.0) * availableWidth;
            m_noteRenderer->drawNote(painter, note, QPointF(x, y), false, -1);
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

    if (m_isPlaying)
    {
        QTimer::singleShot(0, this, &ChartCanvas::requestNextFrame);
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

        const auto &bpmList = m_chartController->chart()->bpmList();
        int offset = m_chartController->chart()->meta().offset;

        int startBeatNum, startNum, startDen;
        MathUtils::floatToBeat(m_scrollBeat, startBeatNum, startNum, startDen);
        double startTime = MathUtils::beatToMs(startBeatNum, startNum, startDen, bpmList, offset);
        int endBeatNum, endNum, endDen;
        MathUtils::floatToBeat(m_scrollBeat + effectiveVisibleBeatRange(), endBeatNum, endNum, endDen);
        double endTime = MathUtils::beatToMs(endBeatNum, endNum, endDen, bpmList, offset);
        m_gridRenderer->drawGrid(painter, rect, m_gridDivision,
                                 startTime, endTime,
                                 m_timeDivision, bpmList, offset,
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

    // 计算相对于移动起点的总偏移（像素）
    QPointF delta = currentPos - m_moveStartPos;
    double deltaY = delta.y();
    if (m_verticalFlip)
        deltaY = -deltaY;

    // 将像素偏移转换为拍号偏移和 X 偏移
    double deltaBeat = (deltaY / height()) * effectiveVisibleBeatRange();
    double deltaX = delta.x() / width() * 512.0;

    // 如果启用了时间吸附，则对偏移量进行吸附（以参考音符为基准）
    double snapOffsetBeat = 0.0;
    if (m_timeDivision > 0 && m_dragReferenceIndex >= 0)
    {
        const Note &refOriginal = m_moveChanges[m_dragReferenceIndex].first;
        double refOriginalBeat = MathUtils::beatToFloat(refOriginal.beatNum, refOriginal.numerator, refOriginal.denominator);
        double refNewBeat = refOriginalBeat + deltaBeat;

        Note refNote = refOriginal;
        MathUtils::floatToBeat(refNewBeat, refNote.beatNum, refNote.numerator, refNote.denominator);
        refNote = MathUtils::snapNoteToTimeWithBoundary(refNote, m_timeDivision);
        double refSnappedBeat = MathUtils::beatToFloat(refNote.beatNum, refNote.numerator, refNote.denominator);
        snapOffsetBeat = refSnappedBeat - refNewBeat;
    }

    // 最终应用的偏移量
    double appliedDeltaBeat = deltaBeat + snapOffsetBeat;
    double appliedDeltaX = deltaX;

    // 获取音符列表，基于原始位置重新计算新位置（避免累积误差）
    QVector<Note> &notes = const_cast<QVector<Note> &>(m_chartController->chart()->notes());
    QList<int> selectedList = m_originalSelectedIndices.values();

    for (int idx : selectedList)
    {
        const Note &original = m_moveChanges[idx].first; // 原始音符快照
        Note newNote = original;

        // 起始时间偏移
        double originalBeat = MathUtils::beatToFloat(original.beatNum, original.numerator, original.denominator);
        double newBeat = originalBeat + appliedDeltaBeat;
        if (newBeat < 0)
            newBeat = 0;
        MathUtils::floatToBeat(newBeat, newNote.beatNum, newNote.numerator, newNote.denominator);

        // X 坐标偏移（边界限制）
        newNote.x = qBound(0, qRound(original.x + appliedDeltaX), 512);

        // Rain 音符结束时间偏移
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

    // 标记缓存脏
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

    QList<QPair<Note, Note>> finalChanges;
    const auto &notes = m_chartController->chart()->notes();
    for (int idx : m_originalSelectedIndices)
    {
        const Note &currentNote = notes[idx];
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
    m_moveChanges.clear();
    m_moveStartPos = QPointF();

    m_noteDataDirty = true;
    m_timesDirty = true;
    update();
}

void ChartCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (m_isPasting)
        {
            if (event->pos().x() >= 10 && event->pos().x() <= 110 && event->pos().y() >= 10 && event->pos().y() <= 40)
            {
                confirmPaste();
                return;
            }
            else if (event->pos().x() >= 120 && event->pos().x() <= 220 && event->pos().y() >= 10 && event->pos().y() <= 40)
            {
                m_isPasting = false;
                update();
                return;
            }
            else
            {
                m_isDragging = true;
                m_dragStart = event->pos();
                m_draggedNotes.clear();
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
        QAction *playFromRefAction = menu.addAction(tr("从参考线时间点播放"));
        QAction *selectedAction = menu.exec(event->globalPos());
        if (selectedAction == playFromRefAction)
        {
            playFromReferenceLine();
        }
    }
}

void ChartCanvas::confirmPaste()
{
    if (!m_chartController || m_pasteNotes.isEmpty())
        return;

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
    int refBeatNum, refNum, refDen;
    MathUtils::floatToBeat(baselineBeat, refBeatNum, refNum, refDen);
    double referenceTime = MathUtils::beatToMs(refBeatNum, refNum, refDen, bpmList, offset);

    Note *baseNote = nullptr;
    double minTime = std::numeric_limits<double>::max();
    for (Note &note : m_pasteNotes)
    {
        if (note.type == NoteType::SOUND)
            continue;
        double t = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator, bpmList, offset);
        if (t < minTime)
        {
            minTime = t;
            baseNote = &note;
        }
    }
    if (!baseNote)
    {
        m_isPasting = false;
        update();
        return;
    }
    double baseTime = minTime;
    double timeShift = referenceTime - baseTime;

    for (const Note &note : m_pasteNotes)
    {
        Note newNote = note;
        newNote.id = Note::generateId();
        newNote.x += static_cast<int>(m_pasteOffset.x() / width() * 512);

        double originalTime = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator, bpmList, offset);
        double newTime = originalTime + timeShift;

        int beat, num, den;
        MathUtils::msToBeat(newTime, bpmList, offset, beat, num, den);
        newNote.beatNum = beat;
        newNote.numerator = num;
        newNote.denominator = den;

        if (note.type == NoteType::RAIN)
        {
            double originalEndTime = MathUtils::beatToMs(note.endBeatNum, note.endNumerator, note.endDenominator, bpmList, offset);
            double newEndTime = originalEndTime + timeShift;
            int endBeat, endNumBeat, endDenBeat;
            MathUtils::msToBeat(newEndTime, bpmList, offset, endBeat, endNumBeat, endDenBeat);
            newNote.endBeatNum = endBeat;
            newNote.endNumerator = endNumBeat;
            newNote.endDenominator = endDenBeat;
        }

        if (Settings::instance().pasteUse288Division())
        {
            double beatFloat = MathUtils::beatToFloat(newNote.beatNum, newNote.numerator, newNote.denominator);
            int newBeatNum, newNum, newDen;
            MathUtils::floatToBeat(beatFloat, newBeatNum, newNum, newDen, 288);
            newNote.beatNum = newBeatNum;
            newNote.numerator = newNum;
            newNote.denominator = newDen;

            if (newNote.type == NoteType::RAIN)
            {
                double endBeatFloat = MathUtils::beatToFloat(newNote.endBeatNum, newNote.endNumerator, newNote.endDenominator);
                int newEndBeatNum, newEndNum, newEndDen;
                MathUtils::floatToBeat(endBeatFloat, newEndBeatNum, newEndNum, newEndDen, 288);
                newNote.endBeatNum = newEndBeatNum;
                newNote.endNumerator = newEndNum;
                newNote.endDenominator = newEndDen;
            }
        }

        if (newNote.x < 0 || newNote.x > 512)
        {
            QMessageBox::StandardButton reply = QMessageBox::question(this, tr("Out of Bounds"),
                                                                      tr("Some notes are outside the playfield (x=0~512). Force fix them to edges?"),
                                                                      QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes)
            {
                newNote.x = qBound(0, newNote.x, 512);
                m_chartController->addNote(newNote);
            }
        }
        else
        {
            m_chartController->addNote(newNote);
        }
    }
    m_isPasting = false;
    update();
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
    else if (m_isDragging)
    {
        if (m_isPasting)
        {
            QPointF delta = event->pos() - m_dragStart;
            m_pasteOffset += delta;
            m_dragStart = event->pos();
            m_repaintPending = true;
            if (!m_repaintTimer->isActive())
                m_repaintTimer->start(16);
        }
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
    m_currentPlayTime = timeMs;

    if (!m_playbackController || m_playbackController->state() != PlaybackController::Playing || !m_autoScrollEnabled)
    {
        update();
        return;
    }
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