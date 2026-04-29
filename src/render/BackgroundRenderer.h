#pragma once

#include <QPainter>
#include <QPixmap>
#include <QColor>

class BackgroundRenderer
{
public:
    BackgroundRenderer() = default;
    ~BackgroundRenderer() = default;

    void setBackgroundImage(const QString &imagePath);
    void setBackgroundColor(const QColor &color);
    void setImageEnabled(bool enabled);
    void setImageBrightness(int brightness);

    // 生成背景缓存（由画布在需要时调用）
    QPixmap generateBackground(const QSize &size);

    // 标记缓存需要更新
    bool isCacheDirty() const { return m_cacheDirty; }
    void markCacheDirty() { m_cacheDirty = true; }

private:
    QPixmap m_background;
    QColor m_backgroundColor = QColor(40, 40, 40);
    bool m_imageEnabled = true;
    int m_imageBrightness = 100;
    bool m_cacheDirty = true;
};
