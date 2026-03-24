#pragma once

#include <QPainter>
#include <QPixmap>

class BackgroundRenderer {
public:
    void setBackground(const QPixmap& pixmap);
    void drawBackground(QPainter& painter, const QRect& rect);

private:
    QPixmap m_background;
};