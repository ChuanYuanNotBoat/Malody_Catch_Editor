#include "BackgroundRenderer.h"

void BackgroundRenderer::setBackground(const QPixmap& pixmap) {
    m_background = pixmap;
}

void BackgroundRenderer::drawBackground(QPainter& painter, const QRect& rect) {
    if (m_background.isNull()) {
        painter.fillRect(rect, Qt::white);
    } else {
        painter.drawPixmap(rect, m_background);
    }
}