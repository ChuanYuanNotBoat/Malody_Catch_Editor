#include "NoteRenderer.h"
#include "model/Skin.h"
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
    Q_UNUSED(numerator);
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
    default: return QColor(255, 0, 0);       // 默认红色（非常规分度）
    }
}

void NoteRenderer::drawNote(QPainter& painter, const Note& note, const QPointF& pos, bool selected) const
{
    // 确定音符类型（用于皮肤）
    int noteType = 0; // 默认
    if (note.type != NoteType::RAIN) {
        if (note.denominator == 2) noteType = 1;
        else if (note.denominator == 4) noteType = 2;
        else if (note.denominator == 8 || note.denominator == 16 || note.denominator == 32) noteType = 3;
        else if (note.denominator == 3 || note.denominator == 6 || note.denominator == 12 || note.denominator == 24) noteType = 4;
    } else {
        noteType = 5; // rain 音符（但 rain 不使用皮肤）
    }

    // 获取皮肤图片（如果有）并缩放
    QPixmap notePix;
    if (m_skin && m_skin->isValid() && note.type != NoteType::RAIN) { // rain 不使用皮肤
        const QPixmap* pix = m_skin->getNotePixmap(noteType);
        if (pix && !pix->isNull()) {
            double scale = m_skin->getNoteScale(noteType);
            int scaledW = pix->width() * scale;
            int scaledH = pix->height() * scale;
            notePix = pix->scaled(scaledW, scaledH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    // 计算绘制矩形（以 pos 为中心）
    int drawW = notePix.isNull() ? m_noteSize : notePix.width();
    int drawH = notePix.isNull() ? m_noteSize : notePix.height();
    QRectF rect(pos.x() - drawW/2, pos.y() - drawH/2, drawW, drawH);

    // 描边设置 - 使用统一的辅助方法
    int outlineWidth;
    QColor outlineColor;
    calculateOutline(note, selected, outlineWidth, outlineColor);

    // 绘制填充
    if (!notePix.isNull()) {
        painter.drawPixmap(rect.toRect(), notePix);
    } else if (m_showColors && !m_skin) {
        // 颜色模式
        painter.setBrush(getNoteColor(note.denominator, note.numerator));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(rect);
    } else {
        // 默认灰色
        painter.setBrush(Qt::lightGray);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(rect);
    }

    // 绘制描边
    if (outlineWidth > 0) {
        painter.setPen(QPen(outlineColor, outlineWidth));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(rect);
    }

    // 如果是选中状态，额外绘制半透明内部填充（黄色，30%）- 使用统一的辅助方法
    if (selected) {
        drawSelectionHighlight(painter, rect);
    }
}

void NoteRenderer::drawRain(QPainter& painter, const Note& note, const QRectF& rect, bool selected) const
{
    // 边界验证：检查矩形是否有效
    if (!validateRect(rect)) {
        Logger::warn(QString("Invalid rectangle in drawRain: x=%1, y=%2, w=%3, h=%4")
                     .arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()));
        return;
    }

    // 验证音符大小
    if (m_noteSize <= 0) {
        Logger::warn(QString("Invalid note size: %1, using default 16").arg(m_noteSize));
        const_cast<NoteRenderer*>(this)->m_noteSize = 16;
    }

    // 根据需求：rain 音符渲染为半透明长方形，覆盖 x0-512，不适用皮肤图片
    painter.setBrush(QColor(0, 0, 255, 100));
    painter.setPen(Qt::NoPen);
    painter.drawRect(rect);

    // 描边 - 使用统一的辅助方法
    int outlineWidth;
    QColor outlineColor;
    calculateOutline(note, selected, outlineWidth, outlineColor);
    
    if (outlineWidth > 0) {
        painter.setPen(QPen(outlineColor, outlineWidth));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect);
    }

    // 选中内部填充 - 使用统一的辅助方法
    if (selected) {
        drawSelectionHighlight(painter, rect);
    }
}

// 私有辅助方法实现
void NoteRenderer::calculateOutline(const Note& note, bool selected, int& outlineWidth, QColor& outlineColor) const
{
    if (selected) {
        outlineWidth = 2;
        outlineColor = QColor(255, 165, 0); // 橙色
    } else if (m_hyperfruitEnabled && m_hyperfruitSet.contains(note.x)) {
        outlineWidth = 2;
        outlineColor = Qt::red;
    } else {
        outlineWidth = Settings::instance().outlineWidth();
        outlineColor = Settings::instance().outlineColor();
    }
}

void NoteRenderer::drawSelectionHighlight(QPainter& painter, const QRectF& rect) const
{
    painter.setBrush(QColor(255, 255, 0, 80)); // 黄色，30%透明度
    painter.setPen(Qt::NoPen);
    painter.drawRect(rect);
}

bool NoteRenderer::validateRect(const QRectF& rect) const
{
    // 检查矩形是否有效：宽度和高度应为正数，且坐标不是NaN
    if (rect.width() <= 0 || rect.height() <= 0) {
        return false;
    }
    
    // 检查坐标是否为有限值（不是NaN或无穷大）
    if (!std::isfinite(rect.x()) || !std::isfinite(rect.y()) ||
        !std::isfinite(rect.width()) || !std::isfinite(rect.height())) {
        return false;
    }
    
    return true;
}