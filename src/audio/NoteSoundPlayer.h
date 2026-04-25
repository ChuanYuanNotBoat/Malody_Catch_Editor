#pragma once

#include <QObject>
#include <QSoundEffect>
#include <QString>
#include <QElapsedTimer>

class NoteSoundPlayer : public QObject
{
    Q_OBJECT
public:
    explicit NoteSoundPlayer(QObject *parent = nullptr);

    void playHitSound();
    void setSoundFile(const QString &filePath);
    QString soundFile() const;
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }
    void setVolumePercent(int volume);
    int volumePercent() const;
    bool hasValidSound() const;

private:
    static constexpr qint64 kMinRetriggerIntervalMs = 12;
    QSoundEffect *m_effect;
    bool m_enabled;
    QElapsedTimer m_retriggerTimer;
};
