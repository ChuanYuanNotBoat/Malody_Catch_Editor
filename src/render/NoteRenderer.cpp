#include "NoteRenderer.h"
#include "model/Skin.h"
#include "render/HyperfruitDetector.h"
#include <QPainter>
#include <QPen>

NoteRenderer::NoteRenderer()
    : m_skin(nullptr), m_showColors(true), m_hyperfruitEnabled(true), m_hyperfruitDetector(nullptr)
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

void NoteRenderer::drawNote(QPainter& painter, const Note& note, const QPointF& pos, bool selected) const
{
    QRectF rect(pos.x() - 8, pos.y() - 8, 16, 16);
    QColor fillColor;
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
        // 使用颜色模式，根据分度确定颜色
        double fraction = static_cast<double>(note.numerator) / note.denominator;
        // 近似分母对应的颜色（简化）
        if (note.denominator == 2) fillColor = QColor(135, 206, 235); // 水色
        else if (note.denominator == 3) fillColor = QColor(0, 255, 0); // 绿
        else if (note.denominator == 4) fillColor = QColor(128, 0, 128); // 紫
        else if (note.denominator == 6) fillColor = QColor(0, 255, 0);
        else if (note.denominator == 8) fillColor = QColor(255, 215, 0); // 金
        else if (note.denominator == 12) fillColor = QColor(0, 255, 0);
        else if (note.denominator == 16) fillColor = QColor(255, 215, 0);
        else if (note.denominator == 24) fillColor = QColor(0, 255, 0);
        else if (note.denominator == 32) fillColor = QColor(255, 215, 0);
        else fillColor = Qt::lightGray;
        painter.setBrush(fillColor);
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
    } else {
        // 默认样式
        painter.setBrush(Qt::lightGray);
    }

    painter.drawEllipse(rect);
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