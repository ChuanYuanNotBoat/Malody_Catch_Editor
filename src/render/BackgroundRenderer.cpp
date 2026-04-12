#include "BackgroundRenderer.h"
#include "utils/Logger.h"
#include <QFileInfo>
#include <QPainter>

void BackgroundRenderer::setBackgroundImage(const QString &imagePath)
{
    if (imagePath.isEmpty())
    {
        m_background = QPixmap();
        m_cacheDirty = true;
        return;
    }

    QFileInfo info(imagePath);
    if (!info.exists())
    {
        Logger::warn(QString("Background image not found: %1").arg(imagePath));
        m_background = QPixmap();
        m_cacheDirty = true;
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
    m_cacheDirty = true;
}

void BackgroundRenderer::setBackgroundColor(const QColor &color)
{
    if (m_backgroundColor != color)
    {
        m_backgroundColor = color;
        m_cacheDirty = true;
    }
}

void BackgroundRenderer::setImageEnabled(bool enabled)
{
    if (m_imageEnabled != enabled)
    {
        m_imageEnabled = enabled;
        m_cacheDirty = true;
    }
}

QPixmap BackgroundRenderer::generateBackground(const QSize &size)
{
    QPixmap cache(size);
    cache.fill(m_backgroundColor);

    if (m_imageEnabled && !m_background.isNull())
    {
        QPainter painter(&cache);
        int targetWidth = size.width();
        int targetHeight = static_cast<int>(m_background.height() * (static_cast<double>(targetWidth) / m_background.width()));
        QRectF targetRect(0, 0, targetWidth, targetHeight);
        painter.drawPixmap(targetRect.toRect(), m_background, m_background.rect());
    }

    m_cacheDirty = false;
    return cache;
}