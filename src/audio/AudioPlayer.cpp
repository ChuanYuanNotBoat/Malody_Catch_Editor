#include "AudioPlayer.h"
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QDebug>

AudioPlayer::AudioPlayer(QObject* parent) : QObject(parent)
{
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);

    connect(m_player, &QMediaPlayer::positionChanged, this, &AudioPlayer::positionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &AudioPlayer::durationChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &AudioPlayer::stateChanged);
}

AudioPlayer::~AudioPlayer()
{
}

bool AudioPlayer::load(const QString& filePath)
{
    m_player->setSource(QUrl::fromLocalFile(filePath));
    return m_player->error() == QMediaPlayer::NoError;
}

void AudioPlayer::play()
{
    m_player->play();
}

void AudioPlayer::pause()
{
    m_player->pause();
}

void AudioPlayer::stop()
{
    m_player->stop();
}

void AudioPlayer::setPosition(qint64 positionMs)
{
    m_player->setPosition(positionMs);
}

qint64 AudioPlayer::position() const
{
    return m_player->position();
}

qint64 AudioPlayer::duration() const
{
    return m_player->duration();
}

void AudioPlayer::setSpeed(double speed)
{
    m_player->setPlaybackRate(speed);
}

double AudioPlayer::speed() const
{
    return m_player->playbackRate();
}

bool AudioPlayer::isPlaying() const
{
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}

bool AudioPlayer::isPaused() const
{
    return m_player->playbackState() == QMediaPlayer::PausedState;
}