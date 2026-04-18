#include "Settings.h"
#include <QDir>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QtGlobal>

Settings::Settings() : m_settings("CatchEditor", "CatchChartEditor") {}

Settings &Settings::instance()
{
    static Settings inst;
    return inst;
}

QString Settings::lastOpenPath() const
{
    return m_settings.value("lastOpenPath", "").toString();
}
void Settings::setLastOpenPath(const QString &path)
{
    m_settings.setValue("lastOpenPath", path);
}

QString Settings::lastProjectPath() const
{
    return m_settings.value("lastProjectPath", defaultBeatmapPath()).toString();
}
void Settings::setLastProjectPath(const QString &path)
{
    m_settings.setValue("lastProjectPath", path);
}

QString Settings::defaultBeatmapPath() const
{
    const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!appDataDir.isEmpty())
        return QDir(appDataDir).filePath("beatmap");

    // Fallback for environments where AppLocalDataLocation is unavailable.
    return QDir::home().filePath("CatchChartEditor/beatmap");
}

bool Settings::colorNoteEnabled() const
{
    return m_settings.value("colorNoteEnabled", true).toBool();
}
void Settings::setColorNoteEnabled(bool enabled)
{
    m_settings.setValue("colorNoteEnabled", enabled);
}

bool Settings::hyperfruitOutlineEnabled() const
{
    return m_settings.value("hyperfruitOutlineEnabled", true).toBool();
}
void Settings::setHyperfruitOutlineEnabled(bool enabled)
{
    m_settings.setValue("hyperfruitOutlineEnabled", enabled);
}

double Settings::playbackSpeed() const
{
    return m_settings.value("playbackSpeed", 1.0).toDouble();
}
void Settings::setPlaybackSpeed(double speed)
{
    m_settings.setValue("playbackSpeed", speed);
}

int Settings::audioLatency() const
{
    return m_settings.value("audio/latency", 0).toInt();
}
void Settings::setAudioLatency(int latency)
{
    m_settings.setValue("audio/latency", latency);
}
int Settings::globalAudioOffset() const
{
    return m_settings.value("audio/globalOffset", 0).toInt();
}
void Settings::setGlobalAudioOffset(int offset)
{
    m_settings.setValue("audio/globalOffset", offset);
}
QString Settings::noteSoundPath() const
{
    return m_settings.value("audio/noteSoundPath", "").toString();
}
void Settings::setNoteSoundPath(const QString &path)
{
    m_settings.setValue("audio/noteSoundPath", path);
}
int Settings::noteSoundVolume() const
{
    return m_settings.value("audio/noteSoundVolume", 100).toInt();
}
void Settings::setNoteSoundVolume(int volume)
{
    m_settings.setValue("audio/noteSoundVolume", qBound(0, volume, 200));
}

QString Settings::currentSkin() const
{
    return m_settings.value("currentSkin", "default").toString();
}
void Settings::setCurrentSkin(const QString &skinName)
{
    m_settings.setValue("currentSkin", skinName);
}

int Settings::noteSize() const
{
    return m_settings.value("noteSize", 16).toInt();
}
void Settings::setNoteSize(int size)
{
    m_settings.setValue("noteSize", size);
}

int Settings::outlineWidth() const
{
    return m_settings.value("outlineWidth", 1).toInt();
}
void Settings::setOutlineWidth(int width)
{
    m_settings.setValue("outlineWidth", width);
}

QColor Settings::outlineColor() const
{
    return m_settings.value("outlineColor", QColor(Qt::black)).value<QColor>();
}
void Settings::setOutlineColor(const QColor &color)
{
    m_settings.setValue("outlineColor", color);
}

QString Settings::language() const
{
    return m_settings.value("language", "en_US").toString();
}
void Settings::setLanguage(const QString &languageCode)
{
    m_settings.setValue("language", languageCode);
}

bool Settings::verticalFlip() const
{
    return m_settings.value("view/verticalFlip", true).toBool();
}
void Settings::setVerticalFlip(bool flipped)
{
    m_settings.setValue("view/verticalFlip", flipped);
}

QKeySequence Settings::shortcut(const QString &action) const
{
    return QKeySequence(m_settings.value("shortcut/" + action).toString());
}
void Settings::setShortcut(const QString &action, const QKeySequence &seq)
{
    m_settings.setValue("shortcut/" + action, seq.toString());
}

bool Settings::pasteUse288Division() const
{
    return m_settings.value("editor/pasteUse288Division", false).toBool();
}
void Settings::setPasteUse288Division(bool enabled)
{
    m_settings.setValue("editor/pasteUse288Division", enabled);
}

bool Settings::backgroundImageEnabled() const
{
    return m_settings.value("view/backgroundImageEnabled", true).toBool();
}
void Settings::setBackgroundImageEnabled(bool enabled)
{
    m_settings.setValue("view/backgroundImageEnabled", enabled);
}

QColor Settings::backgroundColor() const
{
    return m_settings.value("view/backgroundColor", QColor(40, 40, 40)).value<QColor>();
}
void Settings::setBackgroundColor(const QColor &color)
{
    m_settings.setValue("view/backgroundColor", color);
}

QStringList Settings::disabledPluginIds() const
{
    return m_settings.value("plugins/disabledIds", QStringList()).toStringList();
}

void Settings::setDisabledPluginIds(const QStringList &pluginIds)
{
    m_settings.setValue("plugins/disabledIds", pluginIds);
}
