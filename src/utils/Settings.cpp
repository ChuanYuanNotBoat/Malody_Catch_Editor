#include "Settings.h"

Settings::Settings() : m_settings("CatchEditor", "CatchChartEditor") {}

Settings& Settings::instance() {
    static Settings inst;
    return inst;
}

QString Settings::lastOpenPath() const {
    return m_settings.value("lastOpenPath", "").toString();
}
void Settings::setLastOpenPath(const QString& path) {
    m_settings.setValue("lastOpenPath", path);
}

bool Settings::colorNoteEnabled() const {
    return m_settings.value("colorNoteEnabled", true).toBool();
}
void Settings::setColorNoteEnabled(bool enabled) {
    m_settings.setValue("colorNoteEnabled", enabled);
}

bool Settings::hyperfruitOutlineEnabled() const {
    return m_settings.value("hyperfruitOutlineEnabled", true).toBool();
}
void Settings::setHyperfruitOutlineEnabled(bool enabled) {
    m_settings.setValue("hyperfruitOutlineEnabled", enabled);
}

double Settings::playbackSpeed() const {
    return m_settings.value("playbackSpeed", 1.0).toDouble();
}
void Settings::setPlaybackSpeed(double speed) {
    m_settings.setValue("playbackSpeed", speed);
}

QString Settings::currentSkin() const {
    return m_settings.value("currentSkin", "default").toString();
}
void Settings::setCurrentSkin(const QString& skinName) {
    m_settings.setValue("currentSkin", skinName);
}

int Settings::noteSize() const {
    return m_settings.value("noteSize", 16).toInt();
}
void Settings::setNoteSize(int size) {
    m_settings.setValue("noteSize", size);
}

int Settings::outlineWidth() const {
    return m_settings.value("outlineWidth", 1).toInt();
}
void Settings::setOutlineWidth(int width) {
    m_settings.setValue("outlineWidth", width);
}

QColor Settings::outlineColor() const {
    return m_settings.value("outlineColor", QColor(Qt::black)).value<QColor>();
}
void Settings::setOutlineColor(const QColor& color) {
    m_settings.setValue("outlineColor", color);
}

QString Settings::language() const {
    return m_settings.value("language", "en_US").toString();
}
void Settings::setLanguage(const QString& languageCode) {
    m_settings.setValue("language", languageCode);
}

QKeySequence Settings::shortcut(const QString& action) const {
    return QKeySequence(m_settings.value("shortcut/" + action).toString());
}
void Settings::setShortcut(const QString& action, const QKeySequence& seq) {
    m_settings.setValue("shortcut/" + action, seq.toString());
}