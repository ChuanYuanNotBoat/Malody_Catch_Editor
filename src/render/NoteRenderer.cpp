#include "NoteRenderer.h"
#include "model/Skin.h"
#include "render/HyperfruitDetector.h"
#include <QPainter>
#include <QPen>
#include "utils/Logger.h"

NoteRenderer::NoteRenderer()
    : m_skin(nullptr), m_showColors(true), m_hyperfruitEnabled(true),
      m_hyperfruitDetector(nullptr), m_noteSize(16)
{
}

void NoteRenderer::setSkin(const Skin* skin)
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

void NoteRenderer::setHyperfruitDetector(HyperfruitDetector* detector)
{
    m_hyperfruitDetector = detector;
}

void NoteRenderer::setHyperfruitSet(const QSet<int>& hyperSet)
{
    m_hyperfruitSet = hyperSet;
}

void NoteRenderer::setNoteSize(int size)
{
    m_noteSize = size;
}

int NoteRenderer::getNoteSize() const
{
    return m_noteSize;
}

// 根据分母获取颜色，严格按照需求文档
static QColor getNoteColor(int denominator, int numerator)
{
    Q_UNUSED(numerator); // 需求中颜色仅由分母决定，分子为1
    switch (denominator) {
    case 1:  return QColor(255, 0, 0);       // 红色
    case 2:  return QColor(135, 206, 235);   // 水色
    case 3:  return QColor(0, 255, 0);       // 绿色
    case 4:  return QColor(128, 0, 128);     // 紫色
    case 6:  return QColor(0, 255, 0);       // 绿色
    case 8:  return QColor(255, 215, 0);     // 金色
    case 12: return QColor(0, 255, 0);       // 绿色
    case 16: return QColor(255, 215, 0);     // 金色
    case 24: return QColor(0, 255, 0);       // 绿色
    case 32: return QColor(255, 215, 0);     // 金色
    default: return QColor(173, 216, 230);   // 默认浅蓝
    }
}

void NoteRenderer::drawNote(QPainter& painter, const Note& note, const QPointF& pos, bool selected) const
{
    QRectF rect(pos.x() - m_noteSize/2, pos.y() - m_noteSize/2, m_noteSize, m_noteSize);
    QColor outlineColor = Qt::black;

    if (selected) {
        outlineColor = QColor(255, 165, 0);
        painter.setPen(QPen(outlineColor, 2));
    } else if (m_hyperfruitEnabled && m_hyperfruitSet.contains(note.x)) {
        outlineColor = Qt::red;
        painter.setPen(QPen(outlineColor, 2));
    } else {
        painter.setPen(QPen(outlineColor, 1));
    }

    if (m_showColors && !m_skin) {
        // 使用颜色模式，根据分母获取颜色
        QColor fillColor = getNoteColor(note.denominator, note.numerator);
        painter.setBrush(fillColor);
        painter.drawEllipse(rect);
    } else if (m_skin) {
        // 使用皮肤图片
        int noteType = 0; // 默认
        if (note.denominator == 2) noteType = 1;
        else if (note.denominator == 4) noteType = 2;
        else if (note.denominator == 8 || note.denominator == 16 || note.denominator == 32) noteType = 3;
        else if (note.denominator == 3 || note.denominator == 6 || note.denominator == 12 || note.denominator == 24) noteType = 4;
        const QPixmap* pix = m_skin->getNotePixmap(noteType);
        if (pix && !pix->isNull()) {
            painter.drawPixmap(rect.toRect(), *pix);
            return;
        }
        // 皮肤缺失时回退到颜色
        painter.setBrush(Qt::lightGray);
        painter.drawEllipse(rect);
    } else {
        // 默认样式
        painter.setBrush(Qt::lightGray);
        painter.drawEllipse(rect);
    }
}

void NoteRenderer::drawRain(QPainter& painter, const Note& note, const QRectF& rect, bool selected) const
{
    painter.setBrush(QColor(0, 0, 255, 100));
    painter.setPen(Qt::NoPen);
    painter.drawRect(rect);
    if (selected) {
        painter.setPen(QPen(QColor(255, 165, 0), 2));
        painter.drawRect(rect);
    } else if (m_hyperfruitEnabled && m_hyperfruitSet.contains(note.x)) {
        painter.setPen(QPen(Qt::red, 2));
        painter.drawRect(rect);
    }
}