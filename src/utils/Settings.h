#pragma once

#include <QSettings>
#include <QKeySequence>
#include <QColor>

class Settings {
public:
    static Settings& instance();

    QString lastOpenPath() const;
    void setLastOpenPath(const QString& path);

    bool colorNoteEnabled() const;
    void setColorNoteEnabled(bool enabled);

    bool hyperfruitOutlineEnabled() const;
    void setHyperfruitOutlineEnabled(bool enabled);

    double playbackSpeed() const;
    void setPlaybackSpeed(double speed);

    int audioLatency() const;
    void setAudioLatency(int latency);
    int globalAudioOffset() const;
    void setGlobalAudioOffset(int offset);

    QString currentSkin() const;
    void setCurrentSkin(const QString& skinName);

    int noteSize() const;
    void setNoteSize(int size);

    // 描边设置
    int outlineWidth() const;
    void setOutlineWidth(int width);
    QColor outlineColor() const;
    void setOutlineColor(const QColor& color);

    QString language() const;
    void setLanguage(const QString& languageCode);

    bool verticalFlip() const;
    void setVerticalFlip(bool flipped);

    QKeySequence shortcut(const QString& action) const;
    void setShortcut(const QString& action, const QKeySequence& seq);

private:
    Settings();
    QSettings m_settings;
};