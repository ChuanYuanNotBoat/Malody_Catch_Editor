#include "ChartCanvas.h"
#include "controller/ChartController.h"
#include "controller/SelectionController.h"
#include "render/NoteRenderer.h"
#include "render/GridRenderer.h"
#include "render/HyperfruitDetector.h"
#include "utils/MathUtils.h"
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

ChartCanvas::ChartCanvas(QWidget* parent)
    : QWidget(parent),
      m_chartController(nullptr),
      m_selectionController(nullptr),
      m_noteRenderer(new NoteRenderer),
      m_gridRenderer(new GridRenderer),
      m_hyperfruitDetector(new HyperfruitDetector),
      m_currentMode(PlaceNote),
      m_colorMode(true),
      m_hyperfruitEnabled(true),
      m_timeDivision(16),
      m_gridDivision(20),
      m_gridSnap(true),
      m_scrollPos(0),
      m_visibleRange(5000),
      m_isSelecting(false),
      m_isDragging(false),
      m_isPasting(false)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(800, 400);
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

void ChartCanvas::setSelectionController(SelectionController* controller)
{
    m_selectionController = controller;
    connect(controller, &SelectionController::selectionChanged, this, QOverload<>::of(&ChartCanvas::update));
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

void ChartCanvas::setTimeDivision(int division)
{
    m_timeDivision = division;
    update();
}

void ChartCanvas::setGridDivision(int division)
{
    m_gridDivision = division;
    update();
}

void ChartCanvas::setGridSnap(bool snap)
{
    m_gridSnap = snap;
}

void ChartCanvas::setScrollPos(double timeMs)
{
    m_scrollPos = timeMs;
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

    // 创建临时悬浮按钮（简化，实际可用 QToolBar 或悬浮窗口）
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
    }
}

void ChartCanvas::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);

    if (!m_chartController) return;

    drawGrid(painter);

    double startTime = m_scrollPos;
    double endTime = startTime + m_visibleRange;

    const auto& notes = m_chartController->chart()->notes();
    const auto& bpmList = m_chartController->chart()->bpmList();
    int offset = m_chartController->chart()->meta().offset;

    if (m_hyperfruitEnabled) {
        QSet<int> hyperSet = m_hyperfruitDetector->detect(notes);
        m_noteRenderer->setHyperfruitSet(hyperSet);
    }

    for (int i = 0; i < notes.size(); ++i) {
        const Note& note = notes[i];
        double noteTime = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator, bpmList, offset);
        if (noteTime < startTime - 100 || noteTime > endTime + 100) continue;

        bool selected = m_selectionController && m_selectionController->selectedIndices().contains(i);
        QPointF pos = noteToPos(note);
        if (note.isRain) {
            double endNoteTime = MathUtils::beatToMs(note.endBeatNum, note.endNumerator, note.endDenominator, bpmList, offset);
            QRectF rainRect(pos.x() - 256, pos.y(), 512, (endNoteTime - noteTime) * (height() / m_visibleRange));
            m_noteRenderer->drawRain(painter, note, rainRect, selected);
        } else {
            m_noteRenderer->drawNote(painter, note, pos, selected);
        }
    }

    if (m_isPasting && !m_pasteNotes.isEmpty()) {
        painter.setOpacity(0.5);
        for (const Note& note : m_pasteNotes) {
            QPointF pos = noteToPos(note) + m_pasteOffset;
            m_noteRenderer->drawNote(painter, note, pos, false);
        }
        painter.setOpacity(1.0);
        painter.fillRect(QRect(10, 10, 100, 30), QColor(200,200,200));
        painter.drawText(QRect(10,10,100,30), Qt::AlignCenter, tr("Confirm"));
        painter.fillRect(QRect(120,10,100,30), QColor(200,200,200));
        painter.drawText(QRect(120,10,100,30), Qt::AlignCenter, tr("Cancel"));
    }

    if (m_isSelecting) {
        QRectF rect = QRectF(m_selectionStart, m_selectionEnd).normalized();
        painter.setPen(Qt::black);
        painter.setBrush(QColor(0,0,0,50));
        painter.drawRect(rect);
    }
}

void ChartCanvas::drawGrid(QPainter& painter)
{
    QRect rect = this->rect();
    m_gridRenderer->drawGrid(painter, rect, m_gridDivision, m_scrollPos, m_scrollPos + m_visibleRange, m_timeDivision);
}

QPointF ChartCanvas::noteToPos(const Note& note) const
{
    double noteTime = MathUtils::beatToMs(note.beatNum, note.numerator, note.denominator,
                                          m_chartController->chart()->bpmList(),
                                          m_chartController->chart()->meta().offset);
    double y = (noteTime - m_scrollPos) / m_visibleRange * height();
    double x = note.x * width() / 512.0;
    return QPointF(x, y);
}

Note ChartCanvas::posToNote(const QPointF& pos) const
{
    double timeMs = m_scrollPos + (pos.y() / height()) * m_visibleRange;
    int beat, num, den;
    MathUtils::msToBeat(timeMs, m_chartController->chart()->bpmList(),
                        m_chartController->chart()->meta().offset, beat, num, den);
    int x = static_cast<int>(pos.x() / width() * 512);
    if (m_gridSnap)
        x = MathUtils::snapXToGrid(x, m_gridDivision);
    Note note(beat, num, den, x);
    if (m_timeDivision > 0)
        note = MathUtils::snapNoteToTime(note, m_timeDivision);
    return note;
}

void ChartCanvas::mousePressEvent(QMouseEvent* event)
{
    qDebug() << "ChartCanvas::mousePressEvent: button=" << event->button() << "pos=" << event->pos();
    try {
        if (event->button() == Qt::LeftButton) {
            if (m_isPasting) {
                qDebug() << "ChartCanvas::mousePressEvent: In paste mode";
                if (event->pos().x() >= 10 && event->pos().x() <= 110 && event->pos().y() >= 10 && event->pos().y() <= 40) {
                    qDebug() << "ChartCanvas::mousePressEvent: Confirming paste";
                    for (const Note& note : m_pasteNotes) {
                        Note newNote = note;
                        newNote.x += static_cast<int>(m_pasteOffset.x() / width() * 512);
                        double timeDelta = m_pasteOffset.y() / height() * m_visibleRange;
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
                } else if (event->pos().x() >= 120 && event->pos().x() <= 220 && event->pos().y() >= 10 && event->pos().y() <= 40) {
                    qDebug() << "ChartCanvas::mousePressEvent: Cancelling paste";
                    m_isPasting = false;
                    update();
                } else {
                    qDebug() << "ChartCanvas::mousePressEvent: Moving paste preview";
                    m_isDragging = true;
                    m_dragStart = event->pos();
                    m_draggedNotes.clear();
                }
                return;
            }

            if (m_currentMode == PlaceNote) {
                qDebug() << "ChartCanvas::mousePressEvent: PlaceNote mode";
                Note note = posToNote(event->pos());
                qDebug() << "ChartCanvas::mousePressEvent: Adding note at" << event->pos() << "-> beat=" << note.beatNum;
                m_chartController->addNote(note);
            } else if (m_currentMode == PlaceRain) {
                qDebug() << "ChartCanvas::mousePressEvent: PlaceRain mode";
                static bool rainFirst = true;
                static QPointF startPos;
                if (rainFirst) {
                    startPos = event->pos();
                    rainFirst = false;
                } else {
                    QPointF endPos = event->pos();
                    rainFirst = true;
                    Note startNote = posToNote(startPos);
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
                        m_chartController->addNote(rainNote);
                    }
                }
            } else if (m_currentMode == Select) {
                if (event->modifiers() & Qt::ControlModifier) {
                    m_isSelecting = true;
                    m_selectionStart = event->pos();
                    m_selectionEnd = event->pos();
                } else {
                    const auto& notes = m_chartController->chart()->notes();
                    double minDist = 10;
                    int hitIndex = -1;
                    for (int i = 0; i < notes.size(); ++i) {
                        QPointF pos = noteToPos(notes[i]);
                        double dist = QLineF(pos, event->pos()).length();
                        if (dist < minDist) {
                            minDist = dist;
                            hitIndex = i;
                        }
                    }
                    if (hitIndex >= 0) {
                        if (m_selectionController->selectedIndices().contains(hitIndex))
                            m_selectionController->removeFromSelection(hitIndex);
                        else
                            m_selectionController->addToSelection(hitIndex);
                    } else {
                        m_selectionController->clearSelection();
                    }
                }
            } else if (m_currentMode == Delete) {
                const auto& notes = m_chartController->chart()->notes();
                for (int i = 0; i < notes.size(); ++i) {
                    QPointF pos = noteToPos(notes[i]);
                    if (QRectF(pos.x()-8, pos.y()-8, 16, 16).contains(event->pos())) {
                        m_chartController->removeNote(notes[i]);
                        break;
                    }
                }
            }
        } else if (event->button() == Qt::RightButton) {
            // 右键菜单（复制/粘贴等），可后续实现
        }
    } catch (const std::exception& e) {
        qCritical() << "ChartCanvas::mousePressEvent exception:" << e.what();
    } catch (...) {
        qCritical() << "ChartCanvas::mousePressEvent unknown exception";
    }
}

void ChartCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isSelecting) {
        m_selectionEnd = event->pos();
        update();
    } else if (m_isDragging) {
        if (m_isPasting) {
            QPointF delta = event->pos() - m_dragStart;
            m_pasteOffset += delta;
            m_dragStart = event->pos();
            update();
        } else {
            QPointF delta = event->pos() - m_dragStart;
            m_dragStart = event->pos();
            QSet<int> selected = m_selectionController->selectedIndices();
            if (selected.isEmpty()) return;
            const auto& notes = m_chartController->chart()->notes();
            for (int idx : selected) {
                if (idx >= 0 && idx < notes.size()) {
                    Note original = notes[idx];
                    Note newNote = original;
                    int deltaX = static_cast<int>(delta.x() / width() * 512);
                    newNote.x += deltaX;
                    if (m_gridSnap)
                        newNote.x = MathUtils::snapXToGrid(newNote.x, m_gridDivision);
                    double deltaTime = delta.y() / height() * m_visibleRange;
                    double oldTime = MathUtils::beatToMs(original.beatNum, original.numerator, original.denominator,
                                                         m_chartController->chart()->bpmList(),
                                                         m_chartController->chart()->meta().offset);
                    double newTime = oldTime + deltaTime;
                    int beat, num, den;
                    MathUtils::msToBeat(newTime, m_chartController->chart()->bpmList(),
                                        m_chartController->chart()->meta().offset, beat, num, den);
                    newNote.beatNum = beat;
                    newNote.numerator = num;
                    newNote.denominator = den;
                    if (m_timeDivision > 0)
                        newNote = MathUtils::snapNoteToTime(newNote, m_timeDivision);
                    if (original.isRain) {
                        double oldEndTime = MathUtils::beatToMs(original.endBeatNum, original.endNumerator, original.endDenominator,
                                                                 m_chartController->chart()->bpmList(),
                                                                 m_chartController->chart()->meta().offset);
                        double newEndTime = oldEndTime + deltaTime;
                        int endBeat, endNum, endDen;
                        MathUtils::msToBeat(newEndTime, m_chartController->chart()->bpmList(),
                                            m_chartController->chart()->meta().offset, endBeat, endNum, endDen);
                        newNote.endBeatNum = endBeat;
                        newNote.endNumerator = endNum;
                        newNote.endDenominator = endDen;
                        if (m_timeDivision > 0) {
                            Note temp = MathUtils::snapNoteToTime(newNote, m_timeDivision);
                            newNote.endBeatNum = temp.endBeatNum;
                            newNote.endNumerator = temp.endNumerator;
                            newNote.endDenominator = temp.endDenominator;
                        }
                    }
                    m_chartController->moveNote(original, newNote);
                }
            }
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
    } else if (m_isDragging) {
        m_isDragging = false;
        m_draggedNotes.clear();
    }
}

void ChartCanvas::wheelEvent(QWheelEvent* event)
{
    double delta = event->angleDelta().y();
    if (delta != 0) {
        double step = m_visibleRange * 0.1;
        double newPos = m_scrollPos - (delta / 120.0) * step;
        if (newPos < 0) newPos = 0;
        m_scrollPos = newPos;
        update();
    }
    if (event->angleDelta().x() != 0) {
        double deltaX = event->angleDelta().x();
        double factor = 1.0 + deltaX / 1200.0;
        if (factor < 0.1) factor = 0.1;
        m_visibleRange /= factor;
        if (m_visibleRange < 100) m_visibleRange = 100;
        if (m_visibleRange > 60000) m_visibleRange = 60000;
        update();
    }
}