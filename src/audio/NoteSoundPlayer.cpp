#include "NoteSoundPlayer.h"

#include <QFile>
#include <QUrl>
#include <QtGlobal>

NoteSoundPlayer::NoteSoundPlayer(QObject *parent)
    : QObject(parent),
      m_effect(new QSoundEffect(this)),
      m_enabled(false)
{
    m_effect->setLoopCount(1);
    setVolumePercent(100);
}

void NoteSoundPlayer::playHitSound()
{
    if (!m_enabled || !hasValidSound())
        return;

    // Restart to make sure repeated hits are audible during dense playback.
    if (m_effect->isPlaying())
        m_effect->stop();
    m_effect->play();
}

void NoteSoundPlayer::setSoundFile(const QString &filePath)
{
    if (filePath.isEmpty() || !QFile::exists(filePath))
    {
        m_effect->setSource(QUrl());
        return;
    }

    m_effect->setSource(QUrl::fromLocalFile(filePath));
}

QString NoteSoundPlayer::soundFile() const
{
    return m_effect->source().toLocalFile();
}

void NoteSoundPlayer::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

void NoteSoundPlayer::setVolumePercent(int volume)
{
    const int clamped = qBound(0, volume, 200);
    m_effect->setVolume(static_cast<qreal>(clamped) / 100.0);
}

int NoteSoundPlayer::volumePercent() const
{
    return qRound(m_effect->volume() * 100.0);
}

bool NoteSoundPlayer::hasValidSound() const
{
    return !m_effect->source().isEmpty();
}
