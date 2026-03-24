#pragma once

#include <QObject>
#include "audio/AudioPlayer.h"

class PlaybackController : public QObject {
    Q_OBJECT
public:
    enum State { Stopped, Playing, Paused };

    explicit PlaybackController(AudioPlayer* audioPlayer, QObject* parent = nullptr);

    State state() const;
    AudioPlayer* audioPlayer() const { return m_audioPlayer; }

    void play();
    void pause();
    void stop();
    void setSpeed(double speed);
    double speed() const;
    void seekTo(double timeMs);
    void seekToBeat(int beat, int num, int den);

    double currentTime() const;

    void setNoteSoundEnabled(bool enabled);

signals:
    void stateChanged(State newState);
    void positionChanged(double timeMs);
    void speedChanged(double speed);
    void beatReached(int beatNum, int num, int den);

private slots:
    void onAudioPositionChanged(qint64 position);

private:
    AudioPlayer* m_audioPlayer;
    State m_state;
    double m_speed;
    bool m_noteSoundEnabled;
};