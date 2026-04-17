#pragma once

#include <QList>
#include <QString>
#include <QWidget>

class ChartController;
class PlaybackController;
class ChartCanvas;
class DensityCurve;
class QPushButton;
class QDoubleSpinBox;
class QLabel;
class QVBoxLayout;

class LeftPanel : public QWidget
{
    Q_OBJECT
public:
    struct PluginQuickAction
    {
        QString pluginId;
        QString actionId;
        QString title;
        QString tooltip;
    };

    explicit LeftPanel(QWidget *parent = nullptr);
    void setChartController(ChartController *controller);
    void setPlaybackController(PlaybackController *controller);
    void setChartCanvas(ChartCanvas *canvas);
    void setPluginQuickActions(const QList<PluginQuickAction> &actions);
    void retranslateUi();

signals:
    void pluginQuickActionTriggered(const QString &pluginId, const QString &actionId);

private slots:
    void onPlayPauseClicked();
    void onZoomInClicked();
    void onZoomOutClicked();
    void onTimeScaleChanged(double scale);

private:
    void setupUi();

    ChartController *m_chartController = nullptr;
    PlaybackController *m_playbackController = nullptr;
    ChartCanvas *m_chartCanvas = nullptr;
    DensityCurve *m_densityCurve = nullptr;
    QPushButton *m_playPauseBtn = nullptr;
    QPushButton *m_zoomInBtn = nullptr;
    QPushButton *m_zoomOutBtn = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QDoubleSpinBox *m_timeScaleSpin = nullptr;
    QLabel *m_pluginSectionLabel = nullptr;
    QWidget *m_pluginSectionContainer = nullptr;
    QVBoxLayout *m_pluginButtonsLayout = nullptr;
};
