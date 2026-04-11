#include "BackgroundRenderer.h"
#include "utils/Logger.h"
#include <QFileInfo>

void BackgroundRenderer::setBackgroundImage(const QString &imagePath)
{
    if (imagePath.isEmpty())
    {
        m_background = QPixmap();
        return;
    }

    QFileInfo info(imagePath);
    if (!info.exists())
    {
        Logger::warn(QString("Background image not found: %1").arg(imagePath));
        m_background = QPixmap();
        return;
    }

    QPixmap pix(imagePath);
    if (pix.isNull())
    {
        Logger::warn(QString("Failed to load background image: %1").arg(imagePath));
        m_background = QPixmap();
    }
    else
    {
        m_background = pix;
        Logger::info(QString("Background image loaded: %1 (%2x%3)").arg(imagePath).arg(pix.width()).arg(pix.height()));
    }
}

void BackgroundRenderer::setBackgroundColor(const QColor &color)
{
    m_backgroundColor = color;
}

void BackgroundRenderer::setImageEnabled(bool enabled)
{
    m_imageEnabled = enabled;
}

void BackgroundRenderer::drawBackground(QPainter &painter, const QRect &rect)
{
    painter.fillRect(rect, m_backgroundColor);

    if (m_imageEnabled && !m_background.isNull())
    {
        int targetWidth = rect.width();
        int targetHeight = static_cast<int>(m_background.height() * (static_cast<double>(targetWidth) / m_background.width()));
        QRectF targetRect(rect.left(), rect.top(), targetWidth, targetHeight);
        painter.drawPixmap(targetRect.toRect(), m_background); // 修复点
    }
}