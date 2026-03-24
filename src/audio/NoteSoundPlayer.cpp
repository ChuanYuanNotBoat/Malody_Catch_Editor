#include "NoteSoundPlayer.h"
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QUrl>
#include <QCoreApplication>
#include <QFile>   // 添加这行

NoteSoundPlayer::NoteSoundPlayer(QObject* parent)
    : QObject(parent), m_enabled(true)
{
    m_player = new QMediaPlayer(this);
    QAudioOutput* audioOut = new QAudioOutput(this);
    audioOut->setVolume(0.5);
    m_player->setAudioOutput(audioOut);

    QString soundPath = QCoreApplication::applicationDirPath() + "/sounds/hit.wav";
    if (QFile::exists(soundPath)) {
        m_player->setSource(QUrl::fromLocalFile(soundPath));
    } else {
        // 尝试资源
        m_player->setSource(QUrl("qrc:/sounds/hit.wav"));
    }
}

void NoteSoundPlayer::playHitSound()
{
    if (!m_enabled) return;
    if (m_player->playbackState() == QMediaPlayer::PlayingState)
        m_player->stop();
    m_player->play();
}

void NoteSoundPlayer::setEnabled(bool enabled)
{
    m_enabled = enabled;
}