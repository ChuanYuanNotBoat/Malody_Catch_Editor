#include "AudioPlayer.h"
#include "utils/Logger.h"
#include "utils/PerformanceTimer.h"
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QDebug>
#include <QFileInfo>
#include <QEventLoop>
#include <QTimer>
#include <QTemporaryFile>
#include <QDir>

AudioPlayer::AudioPlayer(QObject* parent) : QObject(parent),
    m_loaded(false),
    m_lastError()
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
    
    // 重置状态
    m_loaded = false;
    m_lastError.clear();
    
    // 检查文件是否存在
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        m_lastError = QString("文件不存在: %1").arg(filePath);
        Logger::error(QString("AudioPlayer::load - %1").arg(m_lastError));
        emit errorOccurred(m_lastError);
        return false;
    }
    if (!fileInfo.isReadable()) {
        m_lastError = QString("文件不可读: %1").arg(filePath);
        Logger::error(QString("AudioPlayer::load - %1").arg(m_lastError));
        emit errorOccurred(m_lastError);
        return false;
    }
    
    Logger::debug(QString("AudioPlayer::load - 文件大小: %1 字节, 扩展名: %2").arg(fileInfo.size()).arg(fileInfo.suffix()));
    
    // 路径规范化：如果路径包含非ASCII字符，复制到临时文件
    QString actualPath = normalizeAudioPath(filePath);
    if (actualPath.isEmpty()) {
        m_lastError = QString("路径规范化失败");
        Logger::error(QString("AudioPlayer::load - %1").arg(m_lastError));
        emit errorOccurred(m_lastError);
        return false;
    }
    
    Logger::debug(QString("AudioPlayer::load - 实际加载路径: %1").arg(actualPath));
    
    try {
        QUrl url = QUrl::fromLocalFile(actualPath);
        Logger::debug(QString("AudioPlayer::load - URL: %1").arg(url.toString()));
        
        m_player->setSource(url);
        
        // 异步等待媒体加载完成
        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        
        bool loaded = false;
        bool timedOut = false;
        
        // 媒体状态变化处理
        auto onStatusChanged = [&](QMediaPlayer::MediaStatus status) {
            Logger::debug(QString("AudioPlayer::load - MediaStatus changed: %1").arg(static_cast<int>(status)));
            if (status == QMediaPlayer::LoadedMedia ||
                status == QMediaPlayer::BufferedMedia ||
                status == QMediaPlayer::InvalidMedia) {
                loaded = true;
                loop.quit();
            }
        };
        
        connect(m_player, &QMediaPlayer::mediaStatusChanged, this, onStatusChanged);
        connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
            timedOut = true;
            loop.quit();
        });
        
        timeoutTimer.start(5000); // 5秒超时
        loop.exec();
        
        disconnect(m_player, &QMediaPlayer::mediaStatusChanged, this, nullptr);
        
        // 检查超时
        if (timedOut) {
            m_lastError = QString("音频加载超时 (5秒)");
            Logger::error(QString("AudioPlayer::load - %1").arg(m_lastError));
            emit errorOccurred(m_lastError);
            return false;
        }
        
        // 检查媒体错误
        QMediaPlayer::Error err = m_player->error();
        if (err != QMediaPlayer::NoError) {
            m_lastError = QString("媒体错误 %1: %2").arg(int(err)).arg(m_player->errorString());
            Logger::error(QString("AudioPlayer::load - %1").arg(m_lastError));
            emit errorOccurred(m_lastError);
            return false;
        }
        
        // 验证音频时长是否有效
        qint64 duration = m_player->duration();
        if (duration <= 0) {
            m_lastError = QString("音频时长无效（可能格式不支持），错误信息: %1").arg(m_player->errorString());
            Logger::error(QString("AudioPlayer::load - %1").arg(m_lastError));
            emit errorOccurred(m_lastError);
            return false;
        }
        
        m_loaded = true;
        Logger::info(QString("AudioPlayer::load - 音频加载成功，时长: %1 ms").arg(duration));
        return true;
    } catch (const std::exception& e) {
        m_lastError = QString("异常: %1").arg(e.what());
        Logger::error(QString("AudioPlayer::load - %1").arg(m_lastError));
        emit errorOccurred(m_lastError);
        return false;
    } catch (...) {
        m_lastError = QString("未知异常");
        Logger::error("AudioPlayer::load - Unknown exception");
        emit errorOccurred(m_lastError);
        return false;
    }
}

QString AudioPlayer::normalizeAudioPath(const QString& originalPath)
{
    // 检查路径是否包含非ASCII字符
    bool hasNonAscii = false;
    for (const QChar& ch : originalPath) {
        if (ch.unicode() > 127) {
            hasNonAscii = true;
            break;
        }
    }
    
    if (!hasNonAscii) {
        return originalPath;
    }
    
    // 复制到临时文件
    QTemporaryFile tempFile(QDir::tempPath() + "/audio_XXXXXX.ogg");
    tempFile.setAutoRemove(false); // 程序运行期间保留
    if (tempFile.open()) {
        QString tempPath = tempFile.fileName();
        tempFile.close();
        
        if (QFile::copy(originalPath, tempPath)) {
            Logger::info(QString("AudioPlayer::normalizeAudioPath - Audio file copied to temporary path: %1").arg(tempPath));
            m_tempAudioFiles.append(tempPath); // 记录以便清理
            return tempPath;
        }
    }
    
    Logger::warn(QString("AudioPlayer::normalizeAudioPath - Failed to copy audio file to temp path, using original"));
    return originalPath;
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

bool AudioPlayer::isLoaded() const
{
    return m_loaded;
}

bool AudioPlayer::canPlay() const
{
    return m_loaded && m_player->duration() > 0 && m_player->error() == QMediaPlayer::NoError;
}

QString AudioPlayer::lastError() const
{
    return m_lastError;
}