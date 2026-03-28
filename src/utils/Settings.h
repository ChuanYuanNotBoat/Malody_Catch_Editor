#pragma once

#include <QSettings>
#include <QKeySequence>

class Settings {
public:
    static Settings& instance();

    // 文件路径
    QString lastOpenPath() const;
    void setLastOpenPath(const QString& path);

    // 显示设置
    bool colorNoteEnabled() const;
    void setColorNoteEnabled(bool enabled);

    bool hyperfruitOutlineEnabled() const;
    void setHyperfruitOutlineEnabled(bool enabled);

    double playbackSpeed() const;
    void setPlaybackSpeed(double speed);

    QString currentSkin() const;
    void setCurrentSkin(const QString& skinName);

    // 音符大小（像素）
    int noteSize() const;
    void setNoteSize(int size);

    // 语言
    QString language() const;
    void setLanguage(const QString& languageCode);

    // 快捷键
    QKeySequence shortcut(const QString& action) const;
    void setShortcut(const QString& action, const QKeySequence& seq);

private:
    Settings();
    QSettings m_settings;
};