#include "PlaybackController.h"
#include "utils/MathUtils.h"
#include "utils/Logger.h"
#include "model/Chart.h"
#include <QTimer>

PlaybackController::PlaybackController(AudioPlayer* audioPlayer, QObject* parent)
    : QObject(parent), m_audioPlayer(audioPlayer), m_state(Stopped), m_speed(1.0), m_noteSoundEnabled(true)
{
    connect(m_audioPlayer, &AudioPlayer::positionChanged, this, &PlaybackController::onAudioPositionChanged);
    connect(m_audioPlayer, &AudioPlayer::errorOccurred, this, &PlaybackController::onAudioError);
}

PlaybackController::State PlaybackController::state() const
{
    return m_state;
}

void PlaybackController::play()
{
    if (m_state == Stopped || m_state == Paused) {
        // 检查音频是否可播放
        if (!m_audioPlayer->canPlay()) {
            QString errorMsg = QString("Cannot play audio: %1").arg(m_audioPlayer->lastError());
            Logger::error(QString("PlaybackController::play - %1").arg(errorMsg));
            // 发出错误信号以便UI显示用户提示
            emit errorOccurred(errorMsg);
            return;
        }
        m_audioPlayer->play();
        m_state = Playing;
        Logger::debug(QString("PlaybackController::play - Playing from position %1ms").arg(m_audioPlayer->position()));
        emit stateChanged(m_state);
    } else {
        Logger::debug(QString("PlaybackController::play - Ignored, already in state %1").arg(m_state));
    }
}

void PlaybackController::pause()
{
    if (m_state == Playing) {
        m_audioPlayer->pause();
        m_state = Paused;
        Logger::debug(QString("PlaybackController::pause - Paused at position %1ms").arg(m_audioPlayer->position()));
        emit stateChanged(m_state);
    }
}

void PlaybackController::stop()
{
    if (m_state != Stopped) {
        m_audioPlayer->stop();
        m_state = Stopped;
        Logger::debug("PlaybackController::stop - Playback stopped");
        emit stateChanged(m_state);
    }
}

void PlaybackController::setSpeed(double speed)
{
    if (speed < 0.25) speed = 0.25;
    if (speed > 1.0) speed = 1.0;
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
    Logger::debug(QString("PlaybackController::seekTo - Seeking to %1ms").arg(timeMs));
    m_audioPlayer->setPosition(static_cast<qint64>(timeMs));
}

void PlaybackController::seekToBeat(int beat, int num, int den)
{
    // 需要谱面 BPM 表，暂时无法实现，留空
    // 实际使用时需传入 Chart 引用
}

double PlaybackController::currentTime() const
{
    return static_cast<double>(m_audioPlayer->position());
}

void PlaybackController::setNoteSoundEnabled(bool enabled)
{
    m_noteSoundEnabled = enabled;
}

void PlaybackController::onAudioPositionChanged(qint64 position)
{
    emit positionChanged(static_cast<double>(position));
}

void PlaybackController::onAudioError(const QString& error)
{
    Logger::error(QString("PlaybackController::onAudioError - Audio error: %1").arg(error));
    // 可以选择停止播放并更新状态
    if (m_state != Stopped) {
        m_state = Stopped;
        emit stateChanged(m_state);
    }
    // 将错误信号转发给UI
    emit errorOccurred(error);
}