// src/audio/AudioPlayer.h - 音频播放器

#pragma once

#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>

/**
 * @brief 封装音频播放，支持播放、暂停、定位、变速。
 *
 * 线程安全：方法调用应在主线程（Qt 信号槽机制）。
 */
class AudioPlayer : public QObject {
    Q_OBJECT
public:
    explicit AudioPlayer(QObject* parent = nullptr);
    ~AudioPlayer();

    bool load(const QString& filePath);
    void play();
    void pause();
    void stop();
    void setPosition(qint64 positionMs);
    qint64 position() const;
    qint64 duration() const;
    void setSpeed(double speed);   // 0.25 ~ 1.0
    double speed() const;

    bool isPlaying() const;
    bool isPaused() const;
    bool isLoaded() const;
    bool canPlay() const;
    QString lastError() const;

signals:
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void stateChanged(QMediaPlayer::PlaybackState state);
    void errorOccurred(const QString& error);

private:
    QMediaPlayer* m_player;
    QAudioOutput* m_audioOutput;
    bool m_loaded;
    QString m_lastError;
    QStringList m_tempAudioFiles; ///< 临时音频文件列表，用于清理

    QString normalizeAudioPath(const QString& originalPath);
};