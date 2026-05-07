#include "PlaybackController.h"
#include "utils/MathUtils.h"
#include "utils/Logger.h"
#include "model/Chart.h"
#include <QTimer>

PlaybackController::PlaybackController(AudioPlayer *audioPlayer, QObject *parent)
    : QObject(parent), m_audioPlayer(audioPlayer), m_state(Stopped), m_speed(1.0), m_noteSoundEnabled(true), m_autoPausedAtEnd(false)
{
    connect(m_audioPlayer, &AudioPlayer::positionChanged, this, &PlaybackController::onAudioPositionChanged);
    connect(m_audioPlayer, &AudioPlayer::stateChanged, this, &PlaybackController::onAudioStateChanged);
    connect(m_audioPlayer, &AudioPlayer::errorOccurred, this, &PlaybackController::onAudioError);
}

PlaybackController::State PlaybackController::state() const
{
    return m_state;
}

void PlaybackController::play()
{
    if (m_state == Stopped || m_state == Paused)
    {
        if (m_autoPausedAtEnd)
        {
            m_audioPlayer->setAdjustedPosition(0);
            m_autoPausedAtEnd = false;
            emit positionChanged(static_cast<double>(m_audioPlayer->adjustedPosition()));
            Logger::info("PlaybackController::play - Restarting from beginning after end-of-media auto pause");
        }

        if (!m_audioPlayer->canPlay())
        {
            const QString playerError = m_audioPlayer->lastError().trimmed();
            QString errorMsg = playerError.isEmpty()
                                   ? QString("Cannot play audio")
                                   : QString("Cannot play audio: %1").arg(playerError);
            Logger::error(QString("PlaybackController::play - %1").arg(errorMsg));
            emit errorOccurred(errorMsg);
            return;
        }
        m_audioPlayer->play();
        m_state = Playing;
        Logger::debug(QString("PlaybackController::play - Playing from position %1ms").arg(m_audioPlayer->position()));
        emit stateChanged(m_state);
    }
    else
    {
        Logger::debug(QString("PlaybackController::play - Ignored, already in state %1").arg(m_state));
    }
}

void PlaybackController::playFromTime(double timeMs)
{
    if (m_state == Playing)
    {
        pause();
    }

    if (m_autoPausedAtEnd)
    {
        timeMs = 0.0;
        m_autoPausedAtEnd = false;
        Logger::info("PlaybackController::playFromTime - End-of-media replay requested, forcing restart from 0ms");
    }

    seekTo(timeMs);
    play();
}

void PlaybackController::pause()
{
    if (m_state == Playing)
    {
        m_autoPausedAtEnd = false;
        m_audioPlayer->pause();
        m_state = Paused;
        Logger::debug(QString("PlaybackController::pause - Paused at position %1ms").arg(m_audioPlayer->position()));
        emit stateChanged(m_state);
    }
}

void PlaybackController::stop()
{
    if (m_state != Stopped)
    {
        m_autoPausedAtEnd = false;
        m_audioPlayer->stop();
        m_state = Stopped;
        Logger::debug("PlaybackController::stop - Playback stopped");
        emit stateChanged(m_state);
    }
}

void PlaybackController::setSpeed(double speed)
{
    if (speed < 0.25)
        speed = 0.25;
    if (speed > 1.0)
        speed = 1.0;
    m_speed = speed;
    m_audioPlayer->setSpeed(speed);
    Logger::debug(QString("PlaybackController::setSpeed - Speed changed to %1x").arg(speed));
    emit speedChanged(speed);
}

double PlaybackController::speed() const
{
    return m_speed;
}

void PlaybackController::seekTo(double timeMs)
{
    m_autoPausedAtEnd = false;
    Logger::debug(QString("PlaybackController::seekTo - Seeking to %1ms (adjusted)").arg(timeMs));
    m_audioPlayer->setAdjustedPosition(static_cast<qint64>(timeMs));
}

void PlaybackController::seekToBeat(int beat, int num, int den)
{
    Q_UNUSED(beat);
    Q_UNUSED(num);
    Q_UNUSED(den);

    const QString msg = "PlaybackController::seekToBeat is not supported without chart timing context; use seekTo(ms).";
    Logger::warn(msg);
    emit errorOccurred(msg);
}

double PlaybackController::currentTime() const
{
    return static_cast<double>(m_audioPlayer->adjustedPosition());
}

void PlaybackController::setNoteSoundEnabled(bool enabled)
{
    m_noteSoundEnabled = enabled;
}

bool PlaybackController::autoPausedAtEnd() const
{
    return m_autoPausedAtEnd;
}

void PlaybackController::onAudioPositionChanged(qint64 position)
{
    Q_UNUSED(position);
    emit positionChanged(static_cast<double>(m_audioPlayer->adjustedPosition()));
}

void PlaybackController::onAudioStateChanged(QMediaPlayer::PlaybackState state)
{
    if (m_state == Playing && state == QMediaPlayer::StoppedState)
    {
        m_autoPausedAtEnd = true;
        m_audioPlayer->setAdjustedPosition(0);
        m_state = Paused;
        Logger::info("PlaybackController::onAudioStateChanged - Reached end of media, auto-paused at beginning");
        emit positionChanged(static_cast<double>(m_audioPlayer->adjustedPosition()));
        emit stateChanged(m_state);
    }
}

void PlaybackController::onAudioError(const QString &error)
{
    Logger::error(QString("PlaybackController::onAudioError - Audio error: %1").arg(error));
    m_autoPausedAtEnd = false;
    if (m_state != Stopped)
    {
        m_state = Stopped;
        emit stateChanged(m_state);
    }
    emit errorOccurred(error);
}

