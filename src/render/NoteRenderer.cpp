#include "NoteRenderer.h"
#include "model/Skin.h"
#include "render/BeatDivisionColor.h"
#include "render/HyperfruitDetector.h"
#include "utils/Settings.h"
#include <QPainter>
#include <QPen>
#include "utils/Logger.h"

NoteRenderer::NoteRenderer()
    : m_skin(nullptr), m_showColors(true), m_hyperfruitEnabled(true),
      m_hyperfruitDetector(nullptr), m_noteSize(16)
{
}

void NoteRenderer::setSkin(const Skin *skin)
{
    m_skin = skin;
}

void NoteRenderer::setShowColors(bool show)
{
    m_showColors = show;
}

void NoteRenderer::setHyperfruitEnabled(bool enabled)
{
    m_hyperfruitEnabled = enabled;
}

void NoteRenderer::setHyperfruitDetector(HyperfruitDetector *detector)
{
    m_hyperfruitDetector = detector;
}

void NoteRenderer::setHyperfruitIndices(const QSet<int> &indices)
{
    m_hyperfruitIndices = indices;
}

void NoteRenderer::setNoteSize(int size)
{
    m_noteSize = size;
}

int NoteRenderer::getNoteSize() const
{
    return m_noteSize;
}

void NoteRenderer::drawNote(QPainter &painter, const Note &note, const QPointF &pos, bool selected, int index) const
{
    int noteType = 0;
    if (note.type != NoteType::RAIN)
    {
        if (note.denominator == 2)
            noteType = 1;
        else if (note.denominator == 4)
            noteType = 2;
        else if (note.denominator == 8 || note.denominator == 16 || note.denominator == 32)
            noteType = 3;
        else if (note.denominator == 3 || note.denominator == 6 || note.denominator == 12 || note.denominator == 24)
            noteType = 4;
        else if (note.denominator == 288)
            noteType = 5;
    }
    else
    {
        noteType = 5;
    }

    QPixmap notePix;
    if (m_skin && m_skin->isValid() && note.type != NoteType::RAIN)
    {
        const QPixmap *pix = m_skin->getNotePixmap(noteType);
        if (pix && !pix->isNull())
        {
            double scale = m_skin->getNoteScale(noteType);
            int scaledW = pix->width() * scale;
            int scaledH = pix->height() * scale;
            notePix = pix->scaled(scaledW, scaledH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    int drawW = notePix.isNull() ? m_noteSize : notePix.width();
    int drawH = notePix.isNull() ? m_noteSize : notePix.height();
    QRectF rect(pos.x() - drawW / 2, pos.y() - drawH / 2, drawW, drawH);

    int outlineWidth;
    QColor outlineColor;
    calculateOutline(note, selected, index, outlineWidth, outlineColor);

    if (!notePix.isNull())
    {
        painter.drawPixmap(rect.toRect(), notePix);
    }
    else if (m_showColors && !m_skin)
    {
        painter.setBrush(BeatDivisionColor::noteColorForDivision(note.denominator, note.numerator));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(rect);
    }
    else
    {
        painter.setBrush(Qt::lightGray);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(rect);
    }

    if (outlineWidth > 0)
    {
        painter.setPen(QPen(outlineColor, outlineWidth));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(rect);
    }

    if (selected)
    {
        drawSelectionHighlight(painter, rect);
    }
}

void NoteRenderer::drawRain(QPainter &painter, const Note &note, const QRectF &rect, bool selected) const
{
    if (!validateRect(rect))
    {
        Logger::warn(QString("Invalid rectangle in drawRain: x=%1, y=%2, w=%3, h=%4")
                         .arg(rect.x())
                         .arg(rect.y())
                         .arg(rect.width())
                         .arg(rect.height()));
        return;
    }

    painter.setBrush(QColor(0, 0, 255, 100));
    painter.setPen(Qt::NoPen);
    painter.drawRect(rect);

    int outlineWidth;
    QColor outlineColor;
    calculateOutline(note, selected, -1, outlineWidth, outlineColor);

    if (outlineWidth > 0)
    {
        painter.setPen(QPen(outlineColor, outlineWidth));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect);
    }

    if (selected)
    {
        drawSelectionHighlight(painter, rect);
    }
}

void NoteRenderer::calculateOutline(const Note &note, bool selected, int index, int &outlineWidth, QColor &outlineColor) const
{
    if (selected)
    {
        outlineWidth = 2;
        outlineColor = QColor(255, 165, 0);
    }
    else if (m_hyperfruitEnabled && m_hyperfruitIndices.contains(index))
    {
        outlineWidth = 4;
        outlineColor = Qt::red;
    }
    else
    {
        outlineWidth = Settings::instance().outlineWidth();
        outlineColor = Settings::instance().outlineColor();
    }
}

void NoteRenderer::drawSelectionHighlight(QPainter &painter, const QRectF &rect) const
{
    painter.setBrush(QColor(255, 255, 0, 80));
    painter.setPen(Qt::NoPen);
    painter.drawRect(rect);
}

bool NoteRenderer::validateRect(const QRectF &rect) const
{
    if (rect.width() <= 0 || rect.height() <= 0)
        return false;
    if (!std::isfinite(rect.x()) || !std::isfinite(rect.y()) ||
        !std::isfinite(rect.width()) || !std::isfinite(rect.height()))
        return false;
    return true;
}
