#include "AudioPlayer.h"
#include "utils/Logger.h"
#include "utils/PerformanceTimer.h"
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
    PerformanceTimer loadTimer("AudioPlayer::load", "audio");
    
    Logger::info(QString("AudioPlayer::load - Loading audio from: %1").arg(filePath));
    try {
        QUrl url = QUrl::fromLocalFile(filePath);
        Logger::debug(QString("AudioPlayer::load - URL: %1").arg(url.toString()));
        
        m_player->setSource(url);
        
        QMediaPlayer::Error err = m_player->error();
        if (err != QMediaPlayer::NoError) {
            Logger::error(QString("AudioPlayer::load - Media error: %1 (%2)").arg(int(err)).arg(m_player->errorString()));
            emit errorOccurred(m_player->errorString());
            return false;
        }
        
        Logger::info(QString("AudioPlayer::load - Audio loaded successfully"));
        return true;
    } catch (const std::exception& e) {
        Logger::error(QString("AudioPlayer::load - Exception: %1").arg(e.what()));
        return false;
    } catch (...) {
        Logger::error("AudioPlayer::load - Unknown exception");
        return false;
    }
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