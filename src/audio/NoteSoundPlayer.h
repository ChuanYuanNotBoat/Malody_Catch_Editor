// src/audio/NoteSoundPlayer.h - 音符音效播放器

#pragma once

#include <QObject>
#include <QMediaPlayer>

/**
 * @brief 播放内置的音符击打音效。
 * 
 * 线程安全：主线程调用。
 */
class NoteSoundPlayer : public QObject {
    Q_OBJECT
public:
    explicit NoteSoundPlayer(QObject* parent = nullptr);

    void playHitSound();   // 播放击中音效
    void setEnabled(bool enabled);

private:
    QMediaPlayer* m_player;
    bool m_enabled;
};