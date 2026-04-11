#pragma once

#include <QPainter>
#include <QPixmap>

class BackgroundRenderer
{
public:
    void setBackgroundImage(const QString &imagePath);
    void setBackgroundColor(const QColor &color);
    void setImageEnabled(bool enabled);
    void drawBackground(QPainter &painter, const QRect &rect);

private:
    QPixmap m_background;
    QColor m_backgroundColor = QColor(40, 40, 40);
    bool m_imageEnabled = true;
};