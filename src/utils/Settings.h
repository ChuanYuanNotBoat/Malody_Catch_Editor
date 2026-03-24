#pragma once

#include <QSettings>
#include <QKeySequence>

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

    QString currentSkin() const;
    void setCurrentSkin(const QString& skinName);

    QString language() const;
    void setLanguage(const QString& languageCode);

    QKeySequence shortcut(const QString& action) const;
    void setShortcut(const QString& action, const QKeySequence& seq);

private:
    Settings();
    QSettings m_settings;
};