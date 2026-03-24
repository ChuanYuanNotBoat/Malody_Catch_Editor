#include "NoteItem.h"
#include <QPainter>
#include <QMouseEvent>

NoteItem::NoteItem(const Note& note, QWidget* parent)
    : QWidget(parent), m_note(note), m_selected(false)
{
    setFixedHeight(24);
}

void NoteItem::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

void NoteItem::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), m_selected ? QColor(200, 200, 255) : Qt::white);
    painter.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter,
                     QString("%1:%2/%3 x=%4").arg(m_note.beatNum).arg(m_note.numerator).arg(m_note.denominator).arg(m_note.x));
    if (m_note.isRain) {
        painter.drawText(rect().adjusted(150, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                         QString("~ %1:%2/%3").arg(m_note.endBeatNum).arg(m_note.endNumerator).arg(m_note.endDenominator));
    }
}

void NoteItem::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked(m_note);
    QWidget::mousePressEvent(event);
}

void NoteItem::mouseDoubleClickEvent(QMouseEvent* event)
{
    emit doubleClicked(m_note);
    QWidget::mouseDoubleClickEvent(event);
}