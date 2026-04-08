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
      m_gridSnap(true),  // 恢复默认值，但拖动过程中不使用棚格吸附
      m_scrollBeat(0),
      m_visibleBeatRange(10), // 初始可见范围 10 拍
      m_currentPlayTime(0),
      m_autoScrollEnabled(true),
      m_isSelecting(false),
      m_isDragging(false),
      m_isPasting(false),
      m_isMovingSelection(false),
      m_gridSnapBackup(false),
      m_wasGridSnapEnabled(false),
      m_dragReferenceIndex(-1),
      m_rainFirst(true)   // 新增：Rain 放置状态
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(800, 400);
    m_noteRenderer->setNoteSize(Settings::instance().noteSize());
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
    
    // 断开旧的连接
    if (m_playbackController) {
        disconnect(m_playbackController, &PlaybackController::positionChanged, this, &ChartCanvas::playbackPositionChanged);
    }
    
    m_playbackController = controller;
    m_currentPlayTime = 0;
    
    // 连接新的信号
    if (m_playbackController) {
        connect(m_playbackController, &PlaybackController::positionChanged, this, &ChartCanvas::playbackPositionChanged);
        // 初始化当前时间
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
    update();
}

void ChartCanvas::setTimeDivision(int division)
{
    m_timeDivision = division;
    update();
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
    // 保留毫秒值用于向后兼容（暂时）
    // 转换为拍号
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
    // 切换模式时重置 Rain 放置状态
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

void ChartCanvas::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    auto frameStartTime = std::chrono::high_resolution_clock::now();
    
    Logger::debug("ChartCanvas::paintEvent - Starting paint event");
    
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);

    if (!m_chartController) {
        Logger::warn("ChartCanvas::paintEvent - No chart controller available");
        return;
    }
    
    Logger::debug("ChartCanvas::paintEvent - Chart controller exists");

    try {
        Logger::debug("ChartCanvas::paintEvent - Checking chart pointer");
        const Chart* chart = m_chartController->chart();
        if (!chart) {
            Logger::error("ChartCanvas::paintEvent - Chart pointer is null");
            return;
        }
        Logger::debug("ChartCanvas::paintEvent - Chart pointer valid, starting drawGrid");
        
        drawGrid(painter);
        Logger::debug("ChartCanvas::paintEvent - drawGrid completed successfully");
    } catch (const std::exception& e) {
        Logger::error(QString("ChartCanvas::paintEvent - Exception in drawGrid: %1").arg(e.what()));
        return;
    } catch (...) {
        Logger::error("ChartCanvas::paintEvent - Unknown exception in drawGrid");
        return;
    }

    double startBeat = m_scrollBeat;
    double endBeat = startBeat + m_visibleBeatRange;

    
    const auto& notes = m_chartController->chart()->notes();
    const auto& bpmList = m_chartController->chart()->bpmList();
    int offset = m_chartController->chart()->meta().offset;

    // 将可见节拍范围转换为毫秒用于渲染
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

    // 绘制音符
    int renderedNotesCount = 0;
    try {
        for (int i = 0; i < notes.size(); ++i) {
            const Note& note = notes[i];
            double noteTime = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator, bpmList, offset);
            
            // 跳过音效音符（type=1），它们不应该在画布上显示
            if (note.type == NoteType::SOUND) continue;
            
            if (note.type == NoteType::RAIN) {
                double endNoteTime = MathUtils::beatToMs(note.endBeatNum, note.endNumerator, note.endDenominator, bpmList, offset);
                if (endNoteTime < startTime || noteTime > endTime) continue;
                double visibleStart = qMax(noteTime, startTime);
                double visibleEnd = qMin(endNoteTime, endTime);
                if (visibleEnd <= visibleStart) continue;
                double yStart = yPosFromTime(visibleStart);
                double yEnd = yPosFromTime(visibleEnd);
                // 确保矩形高度为正（考虑垂直翻转）
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
        Logger::debug(QString("ChartCanvas::paintEvent - Rendered %1 notes successfully").arg(renderedNotesCount));
    } catch (const std::exception& e) {
        Logger::error(QString("ChartCanvas::paintEvent - Exception during note rendering: %1").arg(e.what()));
        return;
    } catch (...) {
        Logger::error("ChartCanvas::paintEvent - Unknown exception during note rendering");
        return;
    }

    // 粘贴预览
    if (m_isPasting && !m_pasteNotes.isEmpty()) {
        painter.setOpacity(0.5);
        for (const Note& note : m_pasteNotes) {
            // 跳过音效音符
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

    // 绘制播放基准线（固定于屏幕80%高度，表示当前时间点）
    int canvasHeight = height();
    double baselineY = canvasHeight * 0.8; // 屏幕80%下侧
    painter.setPen(QPen(QColor(0, 0, 255), 3)); // 加粗深蓝色线
    painter.drawLine(leftMargin(), baselineY, width() - rightMargin(), baselineY);
    
    // 显示当前播放时间（如果有）
    if (m_playbackController && m_currentPlayTime > 0) {
        painter.setPen(Qt::black);
        painter.drawText(width() - rightMargin() - 50, baselineY - 5,
                       QString::number(m_currentPlayTime, 'f', 0) + "ms");
    }

    // 矩形选择区域
    if (m_isSelecting) {
        QRectF rect = QRectF(m_selectionStart, m_selectionEnd).normalized();
        painter.setPen(Qt::red);
        painter.setBrush(QColor(255, 255, 0, 80));
        painter.drawRect(rect);
    }

    // 记录渲染性能指标
    auto frameEndTime = std::chrono::high_resolution_clock::now();
    qint64 frameTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(frameEndTime - frameStartTime).count();
    DiagnosticCollector::instance().recordRenderMetrics(frameTimeMs, renderedNotesCount);
    
    Logger::debug(QString("ChartCanvas::paintEvent - Paint event completed in %1ms").arg(frameTimeMs));
}

void ChartCanvas::drawGrid(QPainter& painter)
{
    try {
        Logger::debug("ChartCanvas::drawGrid - Starting");
        
        QRect rect = this->rect();
        Logger::debug("ChartCanvas::drawGrid - Got rect");
        
        // 调整矩形以考虑左边距和右边距
        int lmargin = leftMargin();
        int rmargin = rightMargin();
        if (lmargin > 0 || rmargin > 0) {
            rect.adjust(lmargin, 0, -rmargin, 0);
        }
        
        const auto& bpmList = m_chartController->chart()->bpmList();
        Logger::debug(QString("ChartCanvas::drawGrid - Got BPM list with %1 entries").arg(bpmList.size()));
        
        int offset = m_chartController->chart()->meta().offset;
        Logger::debug(QString("ChartCanvas::drawGrid - Got offset: %1").arg(offset));
        
        Logger::debug("ChartCanvas::drawGrid - Calling m_gridRenderer->drawGrid");
        // 将节拍转换为毫秒以兼容现有 GridRenderer
        int startBeatNum, startNum, startDen;
        MathUtils::floatToBeat(m_scrollBeat, startBeatNum, startNum, startDen);
        double startTime = MathUtils::beatToMs(startBeatNum, startNum, startDen, bpmList, offset);
        int endBeatNum, endNum, endDen;
        MathUtils::floatToBeat(m_scrollBeat + m_visibleBeatRange, endBeatNum, endNum, endDen);
        double endTime = MathUtils::beatToMs(endBeatNum, endNum, endDen, bpmList, offset);
        m_gridRenderer->drawGrid(painter, rect, m_gridDivision,
                                 startTime, endTime,
                                 m_timeDivision, bpmList, offset,
                                 m_verticalFlip);
        Logger::debug("ChartCanvas::drawGrid - m_gridRenderer->drawGrid completed");
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
    // 将毫秒转换为拍号，然后调用 beatToY
    int beatNum, numerator, denominator;
    MathUtils::msToBeat(timeMs, m_chartController->chart()->bpmList(),
                        m_chartController->chart()->meta().offset,
                        beatNum, numerator, denominator);
    double beat = beatNum + static_cast<double>(numerator) / denominator;
    return beatToY(beat);
}

double ChartCanvas::beatToY(double beat) const
{
    if (m_visibleBeatRange <= 0) return 0;
    double y = (beat - m_scrollBeat) / m_visibleBeatRange * height();
    
    // 应用垂直翻转
    if (m_verticalFlip) {
        y = height() - y;
    }
    
    return y;
}

double ChartCanvas::yToBeat(double y) const
{
    if (height() <= 0) return m_scrollBeat;
    
    // 应用垂直翻转（反向变换）
    if (m_verticalFlip) {
        y = height() - y;
    }
    
    return m_scrollBeat + (y / height()) * m_visibleBeatRange;
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
    
    // 创建新音符时可以使用棚格吸附
    Logger::info(QString("[Grid] posToNote: m_gridSnap=%1, gridDivision=%2, raw x=%3")
                 .arg(m_gridSnap).arg(m_gridDivision).arg(x));
    if (m_gridSnap) {
        Logger::info(QString("[grid] before snap x = %1, gridDivision = %2").arg(x).arg(m_gridDivision));
        x = MathUtils::snapXToGrid(x, m_gridDivision);
        Logger::info(QString("[grid] after snap x = %1").arg(x));
    } else {
        Logger::info(QString("[Grid] posToNote: 吸附未启用，跳过吸附"));
    }
    
    // X轴边界限制（无吸附）
    x = qBound(0, x, 512);
    
    Note note(beat, num, den, x);
    
    // 应用时间分度吸附和边界检查
    if (m_timeDivision > 0) {
        note = MathUtils::snapNoteToTimeWithBoundary(note, m_timeDivision);
    } else {
        // 即使没有时间分度，也需要进行边界检查
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
    
    // 计算起始时间和结束时间
    double startTime = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator, bpmList, offset);
    double endTime = MathUtils::beatToMs(note.endBeatNum, note.endNumerator, note.endDenominator, bpmList, offset);
    
    // 计算Y坐标
    double yStart = yPosFromTime(startTime);
    double yEnd = yPosFromTime(endTime);
    
    // 确保矩形高度为正（考虑垂直翻转）
    double rectTop = qMin(yStart, yEnd);
    double rectHeight = qAbs(yEnd - yStart);
    
    // rain音符覆盖安全区域宽度（从左边界到右边界）
    int lmargin = leftMargin();
    int rmargin = rightMargin();
    double rainWidth = width() - lmargin - rmargin;
    if (rainWidth <= 0) rainWidth = 1;

    // 创建矩形（从左边界开始，覆盖安全区域宽度）
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
        
        // 跳过音效音符
        if (note.type == NoteType::SOUND) {
            continue;
        }
        
        // 对于rain音符，检查点击位置是否在矩形区域内
        if (note.type == NoteType::RAIN) {
            QRectF rainRect = getRainNoteRect(note);
            if (rainRect.contains(pos)) {
                // 对于rain音符，我们直接返回命中（不需要距离比较）
                return i;
            }
        } else {
            // 对于普通音符，使用原有的距离检测逻辑
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
    // 如果参考索引无效，则使用选中集中的第一个索引（如果有）
    if (m_dragReferenceIndex == -1 && !m_originalSelectedIndices.isEmpty()) {
        m_dragReferenceIndex = *m_originalSelectedIndices.begin();
    }
    
    // 保存并禁用棚格吸附
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
    // 直接使用拍号偏移量，避免毫秒转换
    double deltaY = delta.y();
    if (m_verticalFlip) {
        deltaY = -deltaY;
    }
    double deltaBeat = (deltaY / height()) * m_visibleBeatRange;
    double deltaX = delta.x() / width() * 512;

    // 1. 确定参考音符
    int refIndex = m_dragReferenceIndex;
    if (refIndex == -1 || !m_originalSelectedIndices.contains(refIndex)) {
        // 使用选中集中的第一个索引作为参考
        if (!m_originalSelectedIndices.isEmpty()) {
            refIndex = *m_originalSelectedIndices.begin();
        } else {
            // 没有选中音符，直接返回
            return;
        }
    }

    // 2. 获取参考音符的原始拍号
    const Note& refOriginal = m_chartController->chart()->notes()[refIndex];
    double refOriginalBeat = MathUtils::beatToFloat(refOriginal.beatNum, refOriginal.numerator, refOriginal.denominator);
    double refNewBeat = refOriginalBeat + deltaBeat;
    
    // 将新拍号转换回Note以便吸附
    Note refNote = refOriginal;
    MathUtils::floatToBeat(refNewBeat, refNote.beatNum, refNote.numerator, refNote.denominator);
    
    // 吸附参考音符
    if (m_timeDivision > 0) {
        refNote = MathUtils::snapNoteToTimeWithBoundary(refNote, m_timeDivision);
    } else {
        refNote = MathUtils::snapNoteToTimeWithBoundary(refNote, 1);
    }
    
    // 计算吸附后的拍号
    double refSnappedBeat = MathUtils::beatToFloat(refNote.beatNum, refNote.numerator, refNote.denominator);
    // 吸附偏移量（整体偏移）
    double snapOffsetBeat = refSnappedBeat - refNewBeat;

    // 3. 遍历所有选中的音符，应用整体偏移
    for (auto& change : m_moveChanges) {
        const Note& original = change.first;
        Note& newNote = change.second;
        
        // 计算原始拍号
        double originalBeat = MathUtils::beatToFloat(original.beatNum, original.numerator, original.denominator);
        // 应用整体偏移
        double newBeat = originalBeat + deltaBeat + snapOffsetBeat;
        
        // 转换回拍子
        MathUtils::floatToBeat(newBeat, newNote.beatNum, newNote.numerator, newNote.denominator);
        
        // 边界检查（只检查时间是否为负，不进行吸附）
        if (newBeat < 0) {
            // 如果拍号为负，强制设置为0
            MathUtils::floatToBeat(0, newNote.beatNum, newNote.numerator, newNote.denominator);
        }
        
        // X轴边界限制（拖拽时无吸附）
        newNote.x = qBound(0, qRound(original.x + deltaX), 512);
        
        // 处理Rain音符的结束时间
        if (original.type == NoteType::RAIN) {
            double originalEndBeat = MathUtils::beatToFloat(original.endBeatNum, original.endNumerator, original.endDenominator);
            double newEndBeat = originalEndBeat + deltaBeat + snapOffsetBeat;
            MathUtils::floatToBeat(newEndBeat, newNote.endBeatNum, newNote.endNumerator, newNote.endDenominator);
            // 确保结束时间不小于开始时间
            if (newEndBeat < newBeat) {
                newNote.endBeatNum = newNote.beatNum;
                newNote.endNumerator = newNote.numerator;
                newNote.endDenominator = newNote.denominator;
            }
        }
    }

    // 临时更新模型
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

    // 恢复棚格吸附状态
    Logger::info(QString("[Grid] endMoveSelection: wasGridSnapEnabled=%1, backup=%2")
                 .arg(m_wasGridSnapEnabled).arg(m_gridSnapBackup));
    if (m_wasGridSnapEnabled) {
        m_gridSnap = m_gridSnapBackup;
        m_wasGridSnapEnabled = false; // 重置标志，防止onSelectionChanged再次禁用
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
        // 1. 粘贴预览处理
        if (m_isPasting) {
            // 确认粘贴区域
            if (event->pos().x() >= 10 && event->pos().x() <= 110 && event->pos().y() >= 10 && event->pos().y() <= 40) {
                // 将节拍范围转换为毫秒以计算时间偏移
                const auto& bpmList = m_chartController->chart()->bpmList();
                int offset = m_chartController->chart()->meta().offset;
                double startBeat = m_scrollBeat;
                double endBeat = startBeat + m_visibleBeatRange;
                int startBeatNum, startNum, startDen;
                MathUtils::floatToBeat(startBeat, startBeatNum, startNum, startDen);
                double startTime = MathUtils::beatToMs(startBeatNum, startNum, startDen, bpmList, offset);
                int endBeatNum, endNum, endDen;
                MathUtils::floatToBeat(endBeat, endBeatNum, endNum, endDen);
                double endTime = MathUtils::beatToMs(endBeatNum, endNum, endDen, bpmList, offset);
                double visibleRangeMs = endTime - startTime;
                for (const Note& note : m_pasteNotes) {
                    Note newNote = note;
                    newNote.id = Note::generateId();  // 为新粘贴的音符生成唯一ID
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

        // 2. Ctrl+左键：框选（最高优先级）
        if (event->modifiers() & Qt::ControlModifier) {
            m_isSelecting = true;
            m_selectionStart = event->pos();
            m_selectionEnd = event->pos();
            return;
        }

        // 3. Rain 放置：两次点击
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
                    
                    // 加强时间有效性检查：调用完整的Note::isValidRain()验证
                    if (rainNote.isValidRain()) {
                        m_chartController->addNote(rainNote);
                    } else {
                        // 显示错误提示
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
                        
                        // 使用状态栏消息或QMessageBox显示错误
                        QMessageBox::warning(this, tr("Invalid Rain Note"), errorMsg);
                    }
                } else {
                    // 提示用户结束时间不能早于开始时间
                    QMessageBox::warning(this, tr("Invalid Rain Note"),
                                        tr("End time must be later than start time"));
                }
                return;
            }
        }

        // 4. 检查是否点击了音符
        int hitIndex = hitTestNote(event->pos());
        if (hitIndex != -1) {
            // 删除模式：点击音符即删除该音符（最高优先级）
            if (m_currentMode == Delete) {
                const auto& notes = m_chartController->chart()->notes();
                if (hitIndex >= 0 && hitIndex < notes.size()) {
                    QVector<Note> noteToDelete;
                    noteToDelete.append(notes[hitIndex]);
                    m_chartController->removeNotes(noteToDelete);
                }
                return;
            }
            
            // Ctrl+左键：多选
            if (event->modifiers() & Qt::ControlModifier) {
                // 处理选中/取消选中
                if (m_selectionController->selectedIndices().contains(hitIndex))
                    m_selectionController->removeFromSelection(hitIndex);
                else
                    m_selectionController->addToSelection(hitIndex);
                return;
            }
            
            
            // 普通左键：直接拖动
            // 如果点击的音符未被选中，先选中它（并清除其他选中）
            if (!m_selectionController->selectedIndices().contains(hitIndex)) {
                m_selectionController->clearSelection();
                m_selectionController->addToSelection(hitIndex);
            }
            
            // 立即开始拖动
            beginMoveSelection(event->pos(), hitIndex);
            return;
        }

        
        // 6. 点击空白：清除所有选中
        m_selectionController->clearSelection();

        // 7. 根据模式执行操作（仅在未点击音符时）
        if (m_currentMode == PlaceNote) {
            Note note = posToNote(event->pos());
            m_chartController->addNote(note);
        }
        // Select/Delete 模式在空白处无操作

        // 8. 移动选中的音符功能已移除：点击空白区域只清除选中，不开始移动操作
        // 注意：Shift+点击空白区域的功能已在上方实现
    } else if (event->button() == Qt::RightButton) {
        // 右键菜单预留
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
    double delta = event->angleDelta().y();
    if (delta != 0) {
        double step = m_visibleBeatRange * 0.1;
        double newPos = m_scrollBeat - (delta / 120.0) * step;
        if (newPos < 0) newPos = 0;
        m_scrollBeat = newPos;
        m_autoScrollEnabled = false; // 用户手动滚动后禁用自动滚动
        update();
        emit scrollPositionChanged(m_scrollBeat);
    }
    if (event->angleDelta().x() != 0) {
        double deltaX = event->angleDelta().x();
        double factor = 1.0 + deltaX / 1200.0;
        if (factor < 0.1) factor = 0.1;
        m_visibleBeatRange /= factor;
        if (m_visibleBeatRange < 0.1) m_visibleBeatRange = 0.1;
        if (m_visibleBeatRange > 1000) m_visibleBeatRange = 1000;
        update();
    }
}

void ChartCanvas::playbackPositionChanged(double timeMs)
{
    m_currentPlayTime = timeMs;
    
    // 自动滚动到当前播放位置（跟随播放）
    if (m_playbackController && m_playbackController->state() == PlaybackController::Playing && m_autoScrollEnabled) {
        // 将时间转换为拍号
        if (m_chartController && m_chartController->chart()) {
            const auto& bpmList = m_chartController->chart()->bpmList();
            int offset = m_chartController->chart()->meta().offset;
            int beatNum, numerator, denominator;
            MathUtils::msToBeat(timeMs, bpmList, offset, beatNum, numerator, denominator);
            double beat = beatNum + static_cast<double>(numerator) / denominator;
            
            // 计算使当前拍号对齐到基准线（屏幕80%高度）所需的滚动位置
            double baselineRatio = 0.8; // 屏幕80%下侧
            double targetScrollBeat;
            if (m_verticalFlip) {
                // 垂直滚动时，基准线对应画布顶部附近？需要根据实际坐标系调整
                // 从beatToY公式：y = height() - (beat - m_scrollBeat) / m_visibleBeatRange * height()
                // 设y = baselineY = height() * baselineRatio
                // 解得：beat - m_scrollBeat = (height() - baselineY) / height() * m_visibleBeatRange
                // 即：beat - m_scrollBeat = (1 - baselineRatio) * m_visibleBeatRange
                targetScrollBeat = beat - (1.0 - baselineRatio) * m_visibleBeatRange;
            } else {
                // 无垂直翻转：y = (beat - m_scrollBeat) / m_visibleBeatRange * height()
                // 设y = baselineY = height() * baselineRatio
                // 解得：beat - m_scrollBeat = baselineRatio * m_visibleBeatRange
                targetScrollBeat = beat - baselineRatio * m_visibleBeatRange;
            }
            
            // 更新滚动位置
            m_scrollBeat = targetScrollBeat;
            // 确保滚动位置合理
            if (m_scrollBeat < 0) m_scrollBeat = 0;
            // 触发重绘
            update();
            // 发射滚动位置变化信号，用于同步滑条
            emit scrollPositionChanged(m_scrollBeat);
        }
    }
    
    update(); // 重绘画布以更新基准线
}

void ChartCanvas::onSelectionChanged()
{
    if (!m_selectionController) return;
    
    QSet<int> selected = m_selectionController->selectedIndices();
    Logger::info(QString("[Grid] onSelectionChanged: 选中数量=%1, 当前吸附状态=%2, m_isMovingSelection=%3")
                 .arg(selected.size()).arg(m_gridSnap).arg(m_isMovingSelection));
    // 不再禁用或恢复吸附状态，仅用于日志记录
}

void ChartCanvas::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete) {
        if (m_selectionController && !m_selectionController->selectedIndices().isEmpty()) {
            QSet<int> selected = m_selectionController->selectedIndices();
            const auto& notes = m_chartController->chart()->notes();
            QList<int> sorted = selected.values();
            std::sort(sorted.begin(), sorted.end(), std::greater<int>());
            
            // 收集要删除的音符
            QVector<Note> notesToDelete;
            for (int idx : sorted) {
                if (idx >= 0 && idx < notes.size()) {
                    notesToDelete.append(notes[idx]);
                }
            }
            
            // 使用批量删除命令
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
    // 左边距 = 3个标准网格宽度（基于当前gridDivision）
    // 标准网格宽度 = width() / m_gridDivision
    if (m_gridDivision <= 0) return 0;
    return (3 * width()) / m_gridDivision;
}

int ChartCanvas::rightMargin() const
{
    // 右边距 = 0（暂不预留）
    return 0;
}