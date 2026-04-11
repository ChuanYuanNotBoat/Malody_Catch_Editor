#pragma once

#include <QWidget>

class ChartController;
class PlaybackController;
class ChartCanvas;
class DensityCurve;
class QPushButton;
class QDoubleSpinBox;

class LeftPanel : public QWidget {
    Q_OBJECT
public:
    explicit LeftPanel(QWidget* parent = nullptr);
    void setChartController(ChartController* controller);
    void setPlaybackController(PlaybackController* controller);
    void setChartCanvas(ChartCanvas* canvas);   // 新增：用于缩放控制

private slots:
    void onPlayPauseClicked();
    void onZoomInClicked();      // 放大
    void onZoomOutClicked();     // 缩小
    void onTimeScaleChanged(double scale); // 手动输入

private:
    void setupUi();

    ChartController* m_chartController;
    PlaybackController* m_playbackController;
    ChartCanvas* m_chartCanvas;
    DensityCurve* m_densityCurve;
    QPushButton* m_playPauseBtn;
    QPushButton* m_zoomInBtn;
    QPushButton* m_zoomOutBtn;
    QDoubleSpinBox* m_timeScaleSpin;
};