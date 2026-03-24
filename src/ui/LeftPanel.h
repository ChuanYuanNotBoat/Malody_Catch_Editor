#pragma once

#include <QWidget>

class ChartController;
class PlaybackController;
class DensityCurve;
class QPushButton;

class LeftPanel : public QWidget {
    Q_OBJECT
public:
    explicit LeftPanel(QWidget* parent = nullptr);
    void setChartController(ChartController* controller);
    void setPlaybackController(PlaybackController* controller);

private slots:
    void onPlayPauseClicked();

private:
    void setupUi();

    ChartController* m_chartController;
    PlaybackController* m_playbackController;
    DensityCurve* m_densityCurve;
    QPushButton* m_playPauseBtn;
};