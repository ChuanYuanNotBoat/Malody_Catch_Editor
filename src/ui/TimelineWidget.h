#pragma once

#include <QWidget>

class ChartController;
class PlaybackController;

class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget* parent = nullptr);
    void setChartController(ChartController* controller);
    void setPlaybackController(PlaybackController* controller);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void updateFromPlayback(double timeMs);

    ChartController* m_chartController;
    PlaybackController* m_playbackController;
    double m_currentTime; // ms (chart time, without offset)
    double m_totalDuration; // audio duration (max chart time + offset)
    double m_offset; // audio offset in ms
};