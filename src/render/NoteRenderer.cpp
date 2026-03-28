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
    default: return QColor(173, 216, 230);   // 默认浅蓝
    }
}

void NoteRenderer::drawNote(QPainter& painter, const Note& note, const QPointF& pos, bool selected) const
{
    // 确定音符类型（用于皮肤）
    int noteType = 0; // 默认
    if (!note.isRain) {
        if (note.denominator == 2) noteType = 1;
        else if (note.denominator == 4) noteType = 2;
        else if (note.denominator == 8 || note.denominator == 16 || note.denominator == 32) noteType = 3;
        else if (note.denominator == 3 || note.denominator == 6 || note.denominator == 12 || note.denominator == 24) noteType = 4;
    } else {
        noteType = 5; // rain 音符
    }

    // 获取皮肤图片（如果有）并缩放
    QPixmap notePix;
    if (m_skin && m_skin->isValid()) {
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

    // 描边设置
    int outlineWidth = selected ? 2 : (m_hyperfruitEnabled && m_hyperfruitSet.contains(note.x) ? 2 : Settings::instance().outlineWidth());
    QColor outlineColor = selected ? QColor(255, 165, 0) :
                          (m_hyperfruitEnabled && m_hyperfruitSet.contains(note.x) ? Qt::red : Settings::instance().outlineColor());

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

    // 如果是选中状态，额外绘制半透明内部填充（黄色，30%）
    if (selected) {
        painter.setBrush(QColor(255, 255, 0, 80));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(rect);
    }
}

void NoteRenderer::drawRain(QPainter& painter, const Note& note, const QRectF& rect, bool selected) const
{
    // Rain 音符的绘制，使用皮肤（如果有）
    int noteType = 5;
    QPixmap rainPix;
    if (m_skin && m_skin->isValid()) {
        const QPixmap* pix = m_skin->getNotePixmap(noteType);
        if (pix && !pix->isNull()) {
            double scale = m_skin->getNoteScale(noteType);
            int scaledW = pix->width() * scale;
            int scaledH = pix->height() * scale;
            rainPix = pix->scaled(scaledW, scaledH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    if (!rainPix.isNull()) {
        // 平铺或拉伸？这里简单拉伸
        painter.drawPixmap(rect.toRect(), rainPix);
    } else {
        painter.setBrush(QColor(0, 0, 255, 100));
        painter.setPen(Qt::NoPen);
        painter.drawRect(rect);
    }

    // 描边
    int outlineWidth = selected ? 2 : (m_hyperfruitEnabled && m_hyperfruitSet.contains(note.x) ? 2 : Settings::instance().outlineWidth());
    QColor outlineColor = selected ? QColor(255, 165, 0) :
                          (m_hyperfruitEnabled && m_hyperfruitSet.contains(note.x) ? Qt::red : Settings::instance().outlineColor());
    if (outlineWidth > 0) {
        painter.setPen(QPen(outlineColor, outlineWidth));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect);
    }

    // 选中内部填充
    if (selected) {
        painter.setBrush(QColor(255, 255, 0, 80));
        painter.setPen(Qt::NoPen);
        painter.drawRect(rect);
    }
}