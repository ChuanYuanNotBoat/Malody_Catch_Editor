#include "ChartCanvas.h"
#include "controller/ChartController.h"
#include "controller/SelectionController.h"
#include "controller/PlaybackController.h"
#include "render/NoteRenderer.h"
#include "render/GridRenderer.h"
#include "render/HyperfruitDetector.h"
#include "utils/MathUtils.h"
#include "utils/Settings.h"
#include "utils/Logger.h"
#include "utils/DiagnosticCollector.h"
#include "model/Chart.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
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
#include <QDebug>
#include <chrono>

ChartCanvas::ChartCanvas(QWidget* parent)
    : QWidget(parent),
      m_chartController(nullptr),
      m_selectionController(nullptr),
      m_playbackController(nullptr),
      m_noteRenderer(new NoteRenderer),
      m_gridRenderer(new GridRenderer),
      m_hyperfruitDetector(new HyperfruitDetector),
      m_currentMode(PlaceNote),
      m_colorMode(true),
      m_hyperfruitEnabled(true),
      m_verticalFlip(true),
      m_timeDivision(16),
      m_gridDivision(20),
      m_gridSnap(true),
      m_scrollBeat(0),
      m_baseVisibleBeatRange(10),      // 初始可见 10 拍
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
      m_cacheValid(false),
      m_cachedScrollBeat(0),
      m_cachedVisibleBeatRange(0),
      m_cachedWidth(0),
      m_cachedHeight(0),
      m_cachedVerticalFlip(true)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(800, 400);
    m_noteRenderer->setNoteSize(Settings::instance().noteSize());
    
    m_repaintTimer = new QTimer(this);
    m_repaintTimer->setSingleShot(true);
    connect(m_repaintTimer, &QTimer::timeout, this, &ChartCanvas::performDelayedRepaint);
}

ChartCanvas::~ChartCanvas()
{
    delete m_noteRenderer;
    delete m_gridRenderer;
    delete m_hyperfruitDetector;
}

void ChartCanvas::setChartController(ChartController* controller)
{
    m_chartController = controller;
    if (controller) {
        connect(controller, &ChartController::chartChanged, this, QOverload<>::of(&ChartCanvas::update));
        m_hyperfruitDetector->setCS(3.2);
        m_noteRenderer->setHyperfruitDetector(m_hyperfruitDetector);
    }
    update();
}

void ChartCanvas::setPlaybackController(PlaybackController* controller)
{
    if (m_playbackController == controller)
        return;
    
    if (m_playbackController) {
        disconnect(m_playbackController, &PlaybackController::positionChanged, this, &ChartCanvas::playbackPositionChanged);
        disconnect(m_playbackController, &PlaybackController::stateChanged, this, nullptr);
    }
    
    m_playbackController = controller;
    m_currentPlayTime = 0;
    
    if (m_playbackController) {
        connect(m_playbackController, &PlaybackController::positionChanged, this, &ChartCanvas::playbackPositionChanged);
        connect(m_playbackController, &PlaybackController::stateChanged,
                this, [this](PlaybackController::State state) {
            if (state == PlaybackController::Stopped || state == PlaybackController::Paused) {
                snapPlayheadToGrid();
            }
            if (state == PlaybackController::Playing) {
                m_autoScrollEnabled = true;
            }
        });
        m_currentPlayTime = m_playbackController->currentTime();
    }
    
    update();
}

void ChartCanvas::setSelectionController(SelectionController* controller)
{
    m_selectionController = controller;
    connect(controller, &SelectionController::selectionChanged, this, QOverload<>::of(&ChartCanvas::update));
    connect(controller, &SelectionController::selectionChanged, this, &ChartCanvas::onSelectionChanged);
}

void ChartCanvas::setSkin(Skin* skin)
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
    update();
}

bool ChartCanvas::isVerticalFlip() const
{
    return m_verticalFlip;
}

void ChartCanvas::setVerticalFlip(bool flip)
{
    if (m_verticalFlip == flip) return;
    
    m_verticalFlip = flip;
    emit verticalFlipChanged(flip);
    invalidateCache();
    update();
}

void ChartCanvas::setTimeDivision(int division)
{
    if (division != m_timeDivision) {
        m_timeDivision = division;
        snapPlayheadToGrid();
        update();
    }
}

void ChartCanvas::setGridDivision(int division)
{
    Logger::info(QString("[Grid] Division changed from %1 to %2").arg(m_gridDivision).arg(division));
    m_gridDivision = division;
    update();
}

void ChartCanvas::setGridSnap(bool snap)
{
    Logger::info(QString("[Grid] setGridSnap: %1").arg(snap));
    m_gridSnap = snap;
}

void ChartCanvas::setScrollPos(double timeMs)
{
    int beatNum, numerator, denominator;
    MathUtils::msToBeat(timeMs, m_chartController->chart()->bpmList(),
                        m_chartController->chart()->meta().offset,
                        beatNum, numerator, denominator);
    m_scrollBeat = beatNum + static_cast<double>(numerator) / denominator;
    invalidateCache();
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
    if (mode != PlaceRain) {
        m_rainFirst = true;
    }
    m_currentMode = mode;
    Logger::debug(QString("ChartCanvas mode set to %1").arg(mode));
    update();
}

void ChartCanvas::paste()
{
    if (!m_selectionController) return;
    QVector<Note> clipboard = m_selectionController->getClipboard();
    if (clipboard.isEmpty()) return;

    m_pasteNotes = clipboard;
    m_pasteOffset = QPointF(0, 0);
    m_isPasting = true;
    setFocus();
    update();
    Logger::debug("Paste preview started");
}

void ChartCanvas::showGridSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Grid Settings"));
    QFormLayout form(&dialog);
    QCheckBox* snapCheck = new QCheckBox(tr("Enable Grid Snap"));
    snapCheck->setChecked(m_gridSnap);
    QSpinBox* divisionSpin = new QSpinBox;
    divisionSpin->setRange(4, 64);
    divisionSpin->setValue(m_gridDivision);
    form.addRow(tr("Snap to Grid:"), snapCheck);
    form.addRow(tr("Grid Divisions (4-64):"), divisionSpin);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form.addRow(buttons);

    if (dialog.exec() == QDialog::Accepted) {
        setGridSnap(snapCheck->isChecked());
        setGridDivision(divisionSpin->value());
        Logger::info("Grid settings updated");
    }
}

// 设置时间轴缩放因子
void ChartCanvas::setTimeScale(double scale)
{
    if (qFuzzyCompare(m_timeScale, scale))
        return;

    // 保持参考线位置不变（以屏幕 80% 高度处为基准）
    double baselineRatio = 0.8;
    double baselineBeat;
    if (m_verticalFlip) {
        baselineBeat = m_scrollBeat + (1.0 - baselineRatio) * effectiveVisibleBeatRange();
    } else {
        baselineBeat = m_scrollBeat + baselineRatio * effectiveVisibleBeatRange();
    }

    m_timeScale = scale;

    // 重新计算 m_scrollBeat 使基准线拍号不变
    if (m_verticalFlip) {
        m_scrollBeat = baselineBeat - (1.0 - baselineRatio) * effectiveVisibleBeatRange();
    } else {
        m_scrollBeat = baselineBeat - baselineRatio * effectiveVisibleBeatRange();
    }
    if (m_scrollBeat < 0) m_scrollBeat = 0;

    invalidateCache();
    update();
    emit scrollPositionChanged(m_scrollBeat);
    emit timeScaleChanged(m_timeScale);

    Logger::debug(QString("Time scale changed to %1").arg(m_timeScale));
}

void ChartCanvas::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    auto frameStartTime = std::chrono::high_resolution_clock::now();
    
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);

    if (!m_chartController) {
        Logger::warn("ChartCanvas::paintEvent - No chart controller available");
        return;
    }

    try {
        const Chart* chart = m_chartController->chart();
        if (!chart) {
            Logger::error("ChartCanvas::paintEvent - Chart pointer is null");
            return;
        }
        
        drawGrid(painter);
    } catch (const std::exception& e) {
        Logger::error(QString("ChartCanvas::paintEvent - Exception in drawGrid: %1").arg(e.what()));
        return;
    } catch (...) {
        Logger::error("ChartCanvas::paintEvent - Unknown exception in drawGrid");
        return;
    }

    double startBeat = m_scrollBeat;
    double endBeat = startBeat + effectiveVisibleBeatRange();

    const auto& notes = m_chartController->chart()->notes();
    const auto& bpmList = m_chartController->chart()->bpmList();
    int offset = m_chartController->chart()->meta().offset;

    int startBeatNum, startNum, startDen;
    MathUtils::floatToBeat(startBeat, startBeatNum, startNum, startDen);
    double startTime = MathUtils::beatToMs(startBeatNum, startNum, startDen, bpmList, offset);
    int endBeatNum, endNum, endDen;
    MathUtils::floatToBeat(endBeat, endBeatNum, endNum, endDen);
    double endTime = MathUtils::beatToMs(endBeatNum, endNum, endDen, bpmList, offset);

    if (m_hyperfruitEnabled && !bpmList.isEmpty()) {
        QSet<int> hyperSet = m_hyperfruitDetector->detect(notes);
        m_noteRenderer->setHyperfruitSet(hyperSet);
    }

    int renderedNotesCount = 0;
    try {
        for (int i = 0; i < notes.size(); ++i) {
            const Note& note = notes[i];
            double noteTime = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator, bpmList, offset);
            
            if (note.type == NoteType::SOUND) continue;
            
            if (note.type == NoteType::RAIN) {
                double endNoteTime = MathUtils::beatToMs(note.endBeatNum, note.endNumerator, note.endDenominator, bpmList, offset);
                if (endNoteTime < startTime || noteTime > endTime) continue;
                double visibleStart = qMax(noteTime, startTime);
                double visibleEnd = qMin(endNoteTime, endTime);
                if (visibleEnd <= visibleStart) continue;
                double yStart = yPosFromTime(visibleStart);
                double yEnd = yPosFromTime(visibleEnd);
                double rectTop = qMin(yStart, yEnd);
                double rectHeight = qAbs(yEnd - yStart);
                if (rectHeight <= 0) continue;
                int lmargin = leftMargin();
                int rmargin = rightMargin();
                double rainWidth = width() - lmargin - rmargin;
                if (rainWidth <= 0) rainWidth = 1;
                QRectF rainRect(lmargin, rectTop, rainWidth, rectHeight);
                bool selected = m_selectionController && m_selectionController->selectedIndices().contains(i);
                m_noteRenderer->drawRain(painter, note, rainRect, selected);
                renderedNotesCount++;
            } else {
                if (noteTime < startTime - 100 || noteTime > endTime + 100) continue;
                bool selected = m_selectionController && m_selectionController->selectedIndices().contains(i);
                QPointF pos = noteToPos(note);
                m_noteRenderer->drawNote(painter, note, pos, selected);
                renderedNotesCount++;
            }
        }
    } catch (const std::exception& e) {
        Logger::error(QString("ChartCanvas::paintEvent - Exception during note rendering: %1").arg(e.what()));
        return;
    } catch (...) {
        Logger::error("ChartCanvas::paintEvent - Unknown exception during note rendering");
        return;
    }

    if (m_isPasting && !m_pasteNotes.isEmpty()) {
        painter.setOpacity(0.5);
        for (const Note& note : m_pasteNotes) {
            if (note.type == NoteType::SOUND) continue;
            QPointF pos = noteToPos(note) + m_pasteOffset;
            m_noteRenderer->drawNote(painter, note, pos, false);
        }
        painter.setOpacity(1.0);
        painter.fillRect(QRect(10, 10, 100, 30), QColor(200,200,200));
        painter.drawText(QRect(10,10,100,30), Qt::AlignCenter, tr("Confirm"));
        painter.fillRect(QRect(120,10,100,30), QColor(200,200,200));
        painter.drawText(QRect(120,10,100,30), Qt::AlignCenter, tr("Cancel"));
    }

    int canvasHeight = height();
    double baselineY = canvasHeight * 0.8;
    painter.setPen(QPen(QColor(0, 0, 255), 3));
    painter.drawLine(leftMargin(), baselineY, width() - rightMargin(), baselineY);
    
    if (m_playbackController && m_currentPlayTime > 0) {
        painter.setPen(Qt::black);
        painter.drawText(width() - rightMargin() - 50, baselineY - 5,
                       QString::number(m_currentPlayTime, 'f', 0) + "ms");
        QString autoScrollText = m_autoScrollEnabled ? tr("AutoScroll: ON") : tr("AutoScroll: OFF");
        painter.drawText(width() - rightMargin() - 200, baselineY - 5, autoScrollText);
    }

    if (m_isSelecting) {
        QRectF rect = QRectF(m_selectionStart, m_selectionEnd).normalized();
        painter.setPen(Qt::red);
        painter.setBrush(QColor(255, 255, 0, 80));
        painter.drawRect(rect);
    }

    auto frameEndTime = std::chrono::high_resolution_clock::now();
    qint64 frameTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(frameEndTime - frameStartTime).count();
    DiagnosticCollector::instance().recordRenderMetrics(frameTimeMs, renderedNotesCount);
}

void ChartCanvas::drawGrid(QPainter& painter)
{
    try {
        QRect rect = this->rect();
        
        int lmargin = leftMargin();
        int rmargin = rightMargin();
        if (lmargin > 0 || rmargin > 0) {
            rect.adjust(lmargin, 0, -rmargin, 0);
        }
        
        const auto& bpmList = m_chartController->chart()->bpmList();
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
    } catch (const std::exception& e) {
        Logger::error(QString("ChartCanvas::drawGrid - Exception: %1").arg(e.what()));
        throw;
    } catch (...) {
        Logger::error("ChartCanvas::drawGrid - Unknown exception");
        throw;
    }
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
    if (visibleRange <= 0) return 0;
    double y = (beat - m_scrollBeat) / visibleRange * height();
    
    if (m_verticalFlip) {
        y = height() - y;
    }
    
    return y;
}

double ChartCanvas::yToBeat(double y) const
{
    if (height() <= 0) return m_scrollBeat;
    
    if (m_verticalFlip) {
        y = height() - y;
    }
    
    return m_scrollBeat + (y / height()) * effectiveVisibleBeatRange();
}

QPointF ChartCanvas::noteToPos(const Note& note) const
{
    double noteTime = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator,
                                          m_chartController->chart()->bpmList(),
                                          m_chartController->chart()->meta().offset);
    double y = yPosFromTime(noteTime);
    int lmargin = leftMargin();
    int rmargin = rightMargin();
    int availableWidth = width() - lmargin - rmargin;
    if (availableWidth <= 0) availableWidth = 1;
    double x = lmargin + note.x * availableWidth / 512.0;
    return QPointF(x, y);
}

Note ChartCanvas::posToNote(const QPointF& pos) const
{
    double beat = yToBeat(pos.y());
    int beatNum, num, den;
    MathUtils::floatToBeat(beat, beatNum, num, den);
    int lmargin = leftMargin();
    int rmargin = rightMargin();
    int availableWidth = width() - lmargin - rmargin;
    if (availableWidth <= 0) availableWidth = 1;
    int x = static_cast<int>((pos.x() - lmargin) / availableWidth * 512);
    
    Logger::info(QString("[Grid] posToNote: m_gridSnap=%1, gridDivision=%2, raw x=%3")
                 .arg(m_gridSnap).arg(m_gridDivision).arg(x));
    if (m_gridSnap) {
        Logger::info(QString("[grid] before snap x = %1, gridDivision = %2").arg(x).arg(m_gridDivision));
        x = MathUtils::snapXToGrid(x, m_gridDivision);
        Logger::info(QString("[grid] after snap x = %1").arg(x));
    } else {
        Logger::info(QString("[Grid] posToNote: 吸附未启用，跳过吸附"));
    }
    
    x = qBound(0, x, 512);
    
    Note note(beat, num, den, x);
    
    if (m_timeDivision > 0) {
        note = MathUtils::snapNoteToTimeWithBoundary(note, m_timeDivision);
    } else {
        note = MathUtils::snapNoteToTimeWithBoundary(note, 1);
    }
    
    return note;
}

QRectF ChartCanvas::getRainNoteRect(const Note& note) const
{
    if (!m_chartController || !m_chartController->chart()) {
        return QRectF();
    }
    
    const auto& bpmList = m_chartController->chart()->bpmList();
    int offset = m_chartController->chart()->meta().offset;
    
    double startTime = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator, bpmList, offset);
    double endTime = MathUtils::beatToMs(note.endBeatNum, note.endNumerator, note.endDenominator, bpmList, offset);
    
    double yStart = yPosFromTime(startTime);
    double yEnd = yPosFromTime(endTime);
    
    double rectTop = qMin(yStart, yEnd);
    double rectHeight = qAbs(yEnd - yStart);
    
    int lmargin = leftMargin();
    int rmargin = rightMargin();
    double rainWidth = width() - lmargin - rmargin;
    if (rainWidth <= 0) rainWidth = 1;

    return QRectF(lmargin, rectTop, rainWidth, rectHeight);
}

int ChartCanvas::hitTestNote(const QPointF& pos) const
{
    if (!m_chartController || !m_chartController->chart()) {
        return -1;
    }
    
    const auto& notes = m_chartController->chart()->notes();
    int noteSize = m_noteRenderer->getNoteSize();
    double minDist = noteSize * 0.6;
    int hit = -1;
    
    for (int i = 0; i < notes.size(); ++i) {
        const Note& note = notes[i];
        
        if (note.type == NoteType::SOUND) {
            continue;
        }
        
        if (note.type == NoteType::RAIN) {
            QRectF rainRect = getRainNoteRect(note);
            if (rainRect.contains(pos)) {
                return i;
            }
        } else {
            QPointF notePos = noteToPos(note);
            double dist = QLineF(notePos, pos).length();
            if (dist < minDist) {
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
    const auto& notes = m_chartController->chart()->notes();
    for (int idx : m_originalSelectedIndices) {
        if (idx >= 0 && idx < notes.size())
            m_moveChanges.append(qMakePair(notes[idx], notes[idx]));
    }
}

void ChartCanvas::beginMoveSelection(const QPointF& startPos, int referenceIndex)
{
    m_isMovingSelection = true;
    m_moveStartPos = startPos;
    m_originalSelectedIndices = m_selectionController->selectedIndices();
    m_dragReferenceIndex = referenceIndex;
    if (m_dragReferenceIndex == -1 && !m_originalSelectedIndices.isEmpty()) {
        m_dragReferenceIndex = *m_originalSelectedIndices.begin();
    }
    
    Logger::info(QString("[Grid] beginMoveSelection: 原始吸附状态=%1, 备份=%2")
                 .arg(m_gridSnap).arg(m_gridSnapBackup));
    m_gridSnapBackup = m_gridSnap;
    m_wasGridSnapEnabled = m_gridSnap;
    m_gridSnap = false;
    Logger::info(QString("[Grid] beginMoveSelection: 吸附已禁用，新状态=%1").arg(m_gridSnap));
    
    prepareMoveChanges();
}

void ChartCanvas::updateMoveSelection(const QPointF& currentPos)
{
    QPointF delta = currentPos - m_moveStartPos;
    double deltaY = delta.y();
    if (m_verticalFlip) {
        deltaY = -deltaY;
    }
    double deltaBeat = (deltaY / height()) * effectiveVisibleBeatRange();
    double deltaX = delta.x() / width() * 512;

    int refIndex = m_dragReferenceIndex;
    if (refIndex == -1 || !m_originalSelectedIndices.contains(refIndex)) {
        if (!m_originalSelectedIndices.isEmpty()) {
            refIndex = *m_originalSelectedIndices.begin();
        } else {
            return;
        }
    }

    const Note& refOriginal = m_chartController->chart()->notes()[refIndex];
    double refOriginalBeat = MathUtils::beatToFloat(refOriginal.beatNum, refOriginal.numerator, refOriginal.denominator);
    double refNewBeat = refOriginalBeat + deltaBeat;
    
    Note refNote = refOriginal;
    MathUtils::floatToBeat(refNewBeat, refNote.beatNum, refNote.numerator, refNote.denominator);
    
    if (m_timeDivision > 0) {
        refNote = MathUtils::snapNoteToTimeWithBoundary(refNote, m_timeDivision);
    } else {
        refNote = MathUtils::snapNoteToTimeWithBoundary(refNote, 1);
    }
    
    double refSnappedBeat = MathUtils::beatToFloat(refNote.beatNum, refNote.numerator, refNote.denominator);
    double snapOffsetBeat = refSnappedBeat - refNewBeat;

    for (auto& change : m_moveChanges) {
        const Note& original = change.first;
        Note& newNote = change.second;
        
        double originalBeat = MathUtils::beatToFloat(original.beatNum, original.numerator, original.denominator);
        double newBeat = originalBeat + deltaBeat + snapOffsetBeat;
        
        MathUtils::floatToBeat(newBeat, newNote.beatNum, newNote.numerator, newNote.denominator);
        
        if (newBeat < 0) {
            MathUtils::floatToBeat(0, newNote.beatNum, newNote.numerator, newNote.denominator);
        }
        
        newNote.x = qBound(0, qRound(original.x + deltaX), 512);
        
        if (original.type == NoteType::RAIN) {
            double originalEndBeat = MathUtils::beatToFloat(original.endBeatNum, original.endNumerator, original.endDenominator);
            double newEndBeat = originalEndBeat + deltaBeat + snapOffsetBeat;
            MathUtils::floatToBeat(newEndBeat, newNote.endBeatNum, newNote.endNumerator, newNote.endDenominator);
            if (newEndBeat < newBeat) {
                newNote.endBeatNum = newNote.beatNum;
                newNote.endNumerator = newNote.numerator;
                newNote.endDenominator = newNote.denominator;
            }
        }
    }

    QVector<Note>& notes = const_cast<QVector<Note>&>(m_chartController->chart()->notes());
    QList<int> selectedList = m_originalSelectedIndices.values();
    for (int i = 0; i < m_moveChanges.size(); ++i) {
        int idx = selectedList[i];
        if (idx >= 0 && idx < notes.size()) {
            notes[idx] = m_moveChanges[i].second;
        }
    }
    update();
}

void ChartCanvas::endMoveSelection()
{
    if (!m_isMovingSelection) return;
    m_isMovingSelection = false;

    Logger::info(QString("[Grid] endMoveSelection: wasGridSnapEnabled=%1, backup=%2")
                 .arg(m_wasGridSnapEnabled).arg(m_gridSnapBackup));
    if (m_wasGridSnapEnabled) {
        m_gridSnap = m_gridSnapBackup;
        m_wasGridSnapEnabled = false;
        Logger::info(QString("[Grid] endMoveSelection: 吸附已恢复为%1").arg(m_gridSnap));
    } else {
        Logger::info("[Grid] endMoveSelection: 吸附未启用，不恢复");
    }

    QList<QPair<Note, Note>> finalChanges;
    for (const auto& change : m_moveChanges) {
        if (!(change.first == change.second))
            finalChanges.append(change);
    }
    if (!finalChanges.isEmpty()) {
        m_chartController->moveNotes(finalChanges);
    }
    m_moveChanges.clear();
    m_originalSelectedIndices.clear();
    m_dragReferenceIndex = -1;
}

void ChartCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_isPasting) {
            if (event->pos().x() >= 10 && event->pos().x() <= 110 && event->pos().y() >= 10 && event->pos().y() <= 40) {
                const auto& bpmList = m_chartController->chart()->bpmList();
                int offset = m_chartController->chart()->meta().offset;
                double startBeat = m_scrollBeat;
                double endBeat = startBeat + effectiveVisibleBeatRange();
                int startBeatNum, startNum, startDen;
                MathUtils::floatToBeat(startBeat, startBeatNum, startNum, startDen);
                double startTime = MathUtils::beatToMs(startBeatNum, startNum, startDen, bpmList, offset);
                int endBeatNum, endNum, endDen;
                MathUtils::floatToBeat(endBeat, endBeatNum, endNum, endDen);
                double endTime = MathUtils::beatToMs(endBeatNum, endNum, endDen, bpmList, offset);
                double visibleRangeMs = endTime - startTime;
                for (const Note& note : m_pasteNotes) {
                    Note newNote = note;
                    newNote.id = Note::generateId();
                    newNote.x += static_cast<int>(m_pasteOffset.x() / width() * 512);
                    double timeDelta = m_pasteOffset.y() / height() * visibleRangeMs;
                    double newTime = MathUtils::beatToMs(newNote.beatNum, newNote.numerator, newNote.denominator,
                                                         m_chartController->chart()->bpmList(),
                                                         m_chartController->chart()->meta().offset) + timeDelta;
                    int beat, num, den;
                    MathUtils::msToBeat(newTime, m_chartController->chart()->bpmList(),
                                        m_chartController->chart()->meta().offset, beat, num, den);
                    newNote.beatNum = beat;
                    newNote.numerator = num;
                    newNote.denominator = den;
                    if (newNote.x < 0 || newNote.x > 512) {
                        QMessageBox::StandardButton reply = QMessageBox::question(this, tr("Out of Bounds"),
                            tr("Some notes are outside the playfield (x=0~512). Force fix them to edges?"),
                            QMessageBox::Yes | QMessageBox::No);
                        if (reply == QMessageBox::Yes) {
                            newNote.x = qBound(0, newNote.x, 512);
                            m_chartController->addNote(newNote);
                        }
                    } else {
                        m_chartController->addNote(newNote);
                    }
                }
                m_isPasting = false;
                update();
                return;
            } else if (event->pos().x() >= 120 && event->pos().x() <= 220 && event->pos().y() >= 10 && event->pos().y() <= 40) {
                m_isPasting = false;
                update();
                return;
            } else {
                m_isDragging = true;
                m_dragStart = event->pos();
                m_draggedNotes.clear();
                return;
            }
        }

        if (event->modifiers() & Qt::ControlModifier) {
            m_isSelecting = true;
            m_selectionStart = event->pos();
            m_selectionEnd = event->pos();
            return;
        }

        if (m_currentMode == PlaceRain) {
            if (m_rainFirst) {
                m_rainStartPos = event->pos();
                m_rainFirst = false;
                return;
            } else {
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
                if (endTime > startTime) {
                    Note rainNote(startNote.beatNum, startNote.numerator, startNote.denominator,
                                  endNote.beatNum, endNote.numerator, endNote.denominator,
                                  startNote.x);
                    
                    if (rainNote.isValidRain()) {
                        m_chartController->addNote(rainNote);
                    } else {
                        QString errorMsg = tr("Invalid rain note: ");
                        if (rainNote.endBeatNum < rainNote.beatNum) {
                            errorMsg += tr("End time is earlier than start time");
                        } else if (rainNote.endBeatNum == rainNote.beatNum) {
                            double start = static_cast<double>(rainNote.numerator) / rainNote.denominator;
                            double end = static_cast<double>(rainNote.endNumerator) / rainNote.endDenominator;
                            if (end < start) {
                                errorMsg += tr("End fraction is smaller than start fraction");
                            }
                        } else if (rainNote.x < 0 || rainNote.x > 512) {
                            errorMsg += tr("X coordinate out of range (0-512)");
                        } else {
                            errorMsg += tr("Invalid parameters");
                        }
                        
                        QMessageBox::warning(this, tr("Invalid Rain Note"), errorMsg);
                    }
                } else {
                    QMessageBox::warning(this, tr("Invalid Rain Note"),
                                        tr("End time must be later than start time"));
                }
                return;
            }
        }

        int hitIndex = hitTestNote(event->pos());
        if (hitIndex != -1) {
            if (m_currentMode == Delete) {
                const auto& notes = m_chartController->chart()->notes();
                if (hitIndex >= 0 && hitIndex < notes.size()) {
                    QVector<Note> noteToDelete;
                    noteToDelete.append(notes[hitIndex]);
                    m_chartController->removeNotes(noteToDelete);
                }
                return;
            }
            
            if (event->modifiers() & Qt::ControlModifier) {
                if (m_selectionController->selectedIndices().contains(hitIndex))
                    m_selectionController->removeFromSelection(hitIndex);
                else
                    m_selectionController->addToSelection(hitIndex);
                return;
            }
            
            if (!m_selectionController->selectedIndices().contains(hitIndex)) {
                m_selectionController->clearSelection();
                m_selectionController->addToSelection(hitIndex);
            }
            
            beginMoveSelection(event->pos(), hitIndex);
            return;
        }

        m_selectionController->clearSelection();

        if (m_currentMode == PlaceNote) {
            Note note = posToNote(event->pos());
            m_chartController->addNote(note);
        }
    } else if (event->button() == Qt::RightButton) {
        QMenu menu(this);
        QAction* playFromRefAction = menu.addAction(tr("从参考线时间点播放"));
        QAction* selectedAction = menu.exec(event->globalPos());
        if (selectedAction == playFromRefAction) {
            playFromReferenceLine();
        }
    }
}

void ChartCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isSelecting) {
        m_selectionEnd = event->pos();
        update();
    } else if (m_isMovingSelection) {
        updateMoveSelection(event->pos());
    } else if (m_isDragging) {
        if (m_isPasting) {
            QPointF delta = event->pos() - m_dragStart;
            m_pasteOffset += delta;
            m_dragStart = event->pos();
            update();
        }
    }
}

void ChartCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_isSelecting) {
        QRectF rect = QRectF(m_selectionStart, m_selectionEnd).normalized();
        m_selectionController->selectInRect(rect, m_chartController->chart()->notes(),
            [this](const Note& note) { return noteToPos(note); });
        m_isSelecting = false;
        update();
    } else if (m_isMovingSelection) {
        endMoveSelection();
    } else if (m_isDragging) {
        m_isDragging = false;
        m_draggedNotes.clear();
    }
}

void ChartCanvas::wheelEvent(QWheelEvent* event)
{
    // Ctrl + 滚轮：缩放时间轴
    if (event->modifiers() & Qt::ControlModifier) {
        double delta = event->angleDelta().y();
        if (delta != 0) {
            double factor = (delta > 0) ? 1.2 : 1.0 / 1.2;  // 每次 ±20%
            double newScale = m_timeScale * factor;
            // 限制缩放范围（仅通过滚轮时限制，手动输入无限制）
            const double minScale = 0.2;
            const double maxScale = 5.0;
            newScale = qBound(minScale, newScale, maxScale);
            setTimeScale(newScale);
        }
        event->accept();
        return;
    }

    // 普通滚轮：滚动
    m_isScrolling = true;
    stopSnapTimer();
    
    double delta = event->angleDelta().y();
    if (delta != 0) {
        double step = effectiveVisibleBeatRange() * 0.1;
        double newPos = m_scrollBeat - (delta / 120.0) * step;
        if (newPos < 0) newPos = 0;
        m_scrollBeat = newPos;
        m_autoScrollEnabled = false;
        update();
        emit scrollPositionChanged(m_scrollBeat);
        
        // 同步更新参考线时间
        if (m_chartController && m_chartController->chart()) {
            const auto& bpmList = m_chartController->chart()->bpmList();
            int offset = m_chartController->chart()->meta().offset;
            
            double baselineRatio = 0.8;
            double baselineBeat;
            if (m_verticalFlip) {
                baselineBeat = m_scrollBeat + (1.0 - baselineRatio) * effectiveVisibleBeatRange();
            } else {
                baselineBeat = m_scrollBeat + baselineRatio * effectiveVisibleBeatRange();
            }
            
            int beatNum, numerator, denominator;
            MathUtils::floatToBeat(baselineBeat, beatNum, numerator, denominator);
            double timeMs = MathUtils::beatToMs(beatNum, numerator, denominator, bpmList, offset);
            
            m_currentPlayTime = timeMs;
        }
    }
    if (event->angleDelta().x() != 0) {
        double deltaX = event->angleDelta().x();
        double factor = 1.0 + deltaX / 1200.0;
        if (factor < 0.1) factor = 0.1;
        // 水平滚轮调整缩放（保留原行为作为备选）
        setTimeScale(m_timeScale * factor);
    }
    
    startSnapTimer();
}

void ChartCanvas::playbackPositionChanged(double timeMs)
{
    m_currentPlayTime = timeMs;
    
    if (m_playbackController && m_playbackController->state() == PlaybackController::Playing && m_autoScrollEnabled) {
        if (m_chartController && m_chartController->chart()) {
            const auto& bpmList = m_chartController->chart()->bpmList();
            int offset = m_chartController->chart()->meta().offset;
            int beatNum, numerator, denominator;
            MathUtils::msToBeat(timeMs, bpmList, offset, beatNum, numerator, denominator);
            double beat = beatNum + static_cast<double>(numerator) / denominator;
            
            double baselineRatio = 0.8;
            double targetScrollBeat;
            if (m_verticalFlip) {
                targetScrollBeat = beat - (1.0 - baselineRatio) * effectiveVisibleBeatRange();
            } else {
                targetScrollBeat = beat - baselineRatio * effectiveVisibleBeatRange();
            }
            
            m_scrollBeat = targetScrollBeat;
            if (m_scrollBeat < 0) m_scrollBeat = 0;
            m_repaintPending = true;
            emit scrollPositionChanged(m_scrollBeat);
        }
    }
    
    m_repaintPending = true;
    
    int interval = 16;
    if (m_playbackController && m_playbackController->state() == PlaybackController::Paused) {
        interval = 100;
    }
    
    if (!m_repaintTimer->isActive()) {
        m_repaintTimer->start(interval);
    }
}

void ChartCanvas::playFromReferenceLine()
{
    if (!m_playbackController) {
        return;
    }
    
    if (m_currentPlayTime < 0) {
        m_currentPlayTime = 0;
    }
    
    if (m_playbackController->state() == PlaybackController::Playing) {
        m_playbackController->pause();
    }
    
    if (m_chartController && m_chartController->chart()) {
        const auto& bpmList = m_chartController->chart()->bpmList();
        int offset = m_chartController->chart()->meta().offset;
        int beatNum, numerator, denominator;
        MathUtils::msToBeat(m_currentPlayTime, bpmList, offset, beatNum, numerator, denominator);
        double beat = beatNum + static_cast<double>(numerator) / denominator;
        
        double baselineRatio = 0.8;
        double targetScrollBeat;
        if (m_verticalFlip) {
            targetScrollBeat = beat - (1.0 - baselineRatio) * effectiveVisibleBeatRange();
        } else {
            targetScrollBeat = beat - baselineRatio * effectiveVisibleBeatRange();
        }
        
        m_scrollBeat = targetScrollBeat;
        if (m_scrollBeat < 0) m_scrollBeat = 0;
        
        m_repaintPending = true;
        emit scrollPositionChanged(m_scrollBeat);
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
    if (!m_selectionController) return;
    QSet<int> selected = m_selectionController->selectedIndices();
    Logger::info(QString("[Grid] onSelectionChanged: 选中数量=%1, 当前吸附状态=%2, m_isMovingSelection=%3")
                 .arg(selected.size()).arg(m_gridSnap).arg(m_isMovingSelection));
}

void ChartCanvas::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete) {
        if (m_selectionController && !m_selectionController->selectedIndices().isEmpty()) {
            QSet<int> selected = m_selectionController->selectedIndices();
            const auto& notes = m_chartController->chart()->notes();
            QList<int> sorted = selected.values();
            std::sort(sorted.begin(), sorted.end(), std::greater<int>());
            
            QVector<Note> notesToDelete;
            for (int idx : sorted) {
                if (idx >= 0 && idx < notes.size()) {
                    notesToDelete.append(notes[idx]);
                }
            }
            
            if (!notesToDelete.isEmpty()) {
                m_chartController->removeNotes(notesToDelete);
            }
            
            m_selectionController->clearSelection();
        }
    }
    QWidget::keyPressEvent(event);
}

int ChartCanvas::leftMargin() const
{
    // 固定左边距为画布宽度的 1/20
    return width() / 20;
}

int ChartCanvas::rightMargin() const
{
    // 固定右边距为画布宽度的 1/20
    return width() / 20;
}

void ChartCanvas::snapPlayheadToGrid()
{
    if (!m_chartController || !m_chartController->chart() || !m_snapToGrid) {
        return;
    }
    
    double currentTime = m_currentPlayTime;
    const auto& bpmList = m_chartController->chart()->bpmList();
    int offset = m_chartController->chart()->meta().offset;
    
    double snappedTime = MathUtils::snapTimeToGrid(
        currentTime, bpmList, offset, m_timeDivision);
    
    if (std::abs(snappedTime - currentTime) > 1e-6) {
        m_currentPlayTime = snappedTime;
        
        if (m_playbackController) {
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
    if (m_snapTimerId != 0) {
        killTimer(m_snapTimerId);
        m_snapTimerId = 0;
    }
}

void ChartCanvas::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_snapTimerId) {
        m_isScrolling = false;
        snapPlayheadToGrid();
        stopSnapTimer();
    }
    QWidget::timerEvent(event);
}

void ChartCanvas::performDelayedRepaint()
{
    if (m_repaintPending || m_forceRepaint) {
        m_repaintPending = false;
        m_forceRepaint = false;
        m_lastRepaintTime = QDateTime::currentMSecsSinceEpoch();
        update();
    }
}

void ChartCanvas::invalidateCache()
{
    m_cacheValid = false;
}

void ChartCanvas::updateNotePosCacheIfNeeded()
{
    if (!m_chartController || !m_chartController->chart()) {
        m_cacheValid = false;
        return;
    }
    
    const auto& notes = m_chartController->chart()->notes();
    bool sizeChanged = notes.size() != m_notePosCache.size();
    bool paramsChanged = !qFuzzyCompare(m_scrollBeat, m_cachedScrollBeat) ||
                         !qFuzzyCompare(effectiveVisibleBeatRange(), m_cachedVisibleBeatRange) ||
                         (width() != m_cachedWidth) ||
                         (height() != m_cachedHeight) ||
                         (m_verticalFlip != m_cachedVerticalFlip);
    
    if (m_cacheValid && !sizeChanged && !paramsChanged) {
        return;
    }
    
    m_notePosCache.resize(notes.size());
    const auto& bpmList = m_chartController->chart()->bpmList();
    int offset = m_chartController->chart()->meta().offset;
    
    for (int i = 0; i < notes.size(); ++i) {
        const Note& note = notes[i];
        if (note.type == NoteType::SOUND) {
            m_notePosCache[i] = QPointF();
            continue;
        }
        m_notePosCache[i] = noteToPos(note);
    }
    
    m_cachedScrollBeat = m_scrollBeat;
    m_cachedVisibleBeatRange = effectiveVisibleBeatRange();
    m_cachedWidth = width();
    m_cachedHeight = height();
    m_cachedVerticalFlip = m_verticalFlip;
    m_cacheValid = true;
}

void ChartCanvas::resizeEvent(QResizeEvent* event)
{
    invalidateCache();
    QWidget::resizeEvent(event);
}