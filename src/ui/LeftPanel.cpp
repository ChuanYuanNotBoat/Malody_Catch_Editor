#include "LeftPanel.h"
#include "DensityCurve.h"
#include "controller/ChartController.h"
#include "controller/PlaybackController.h"
#include "ui/CustomWidgets/ChartCanvas.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDoubleSpinBox>

LeftPanel::LeftPanel(QWidget *parent)
    : QWidget(parent), m_chartController(nullptr), m_playbackController(nullptr), m_chartCanvas(nullptr)
{
    setupUi();
}

void LeftPanel::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    // 密度曲线
    m_densityCurve = new DensityCurve(this);
    layout->addWidget(m_densityCurve);

    // 播放/暂停按钮
    m_playPauseBtn = new QPushButton(tr("Play"), this);
    layout->addWidget(m_playPauseBtn);
    connect(m_playPauseBtn, &QPushButton::clicked, this, &LeftPanel::onPlayPauseClicked);

    // 时间轴缩放控制
    QHBoxLayout *zoomLayout = new QHBoxLayout;
    QLabel *zoomLabel = new QLabel(tr("Zoom:"), this);
    m_zoomOutBtn = new QPushButton("-", this);
    m_zoomOutBtn->setFixedWidth(30);
    m_zoomInBtn = new QPushButton("+", this);
    m_zoomInBtn->setFixedWidth(30);

    m_timeScaleSpin = new QDoubleSpinBox(this);
    m_timeScaleSpin->setRange(0.1, 10.0); // 手动输入无严格限制
    m_timeScaleSpin->setSingleStep(0.1);
    m_timeScaleSpin->setDecimals(2);
    m_timeScaleSpin->setValue(1.0);
    m_timeScaleSpin->setSuffix("x");

    zoomLayout->addWidget(zoomLabel);
    zoomLayout->addWidget(m_zoomOutBtn);
    zoomLayout->addWidget(m_timeScaleSpin);
    zoomLayout->addWidget(m_zoomInBtn);
    layout->addLayout(zoomLayout);

    connect(m_zoomInBtn, &QPushButton::clicked, this, &LeftPanel::onZoomInClicked);
    connect(m_zoomOutBtn, &QPushButton::clicked, this, &LeftPanel::onZoomOutClicked);
    connect(m_timeScaleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LeftPanel::onTimeScaleChanged);

    layout->addStretch();
}

void LeftPanel::onPlayPauseClicked()
{
    if (!m_playbackController)
        return;
    if (m_playbackController->state() == PlaybackController::Playing)
    {
        m_playbackController->pause();
        m_playPauseBtn->setText(tr("Play"));
    }
    else
    {
        m_playbackController->play();
        m_playPauseBtn->setText(tr("Pause"));
    }
}

void LeftPanel::onZoomInClicked()
{
    if (!m_chartCanvas)
        return;
    double newScale = m_chartCanvas->timeScale() * 1.2; // 增加 20%
    m_chartCanvas->setTimeScale(newScale);
    // 更新 spinbox 显示（信号会触发更新，但为了防止循环，先 block）
    m_timeScaleSpin->blockSignals(true);
    m_timeScaleSpin->setValue(newScale);
    m_timeScaleSpin->blockSignals(false);
}

void LeftPanel::onZoomOutClicked()
{
    if (!m_chartCanvas)
        return;
    double newScale = m_chartCanvas->timeScale() / 1.2; // 减少 20%
    m_chartCanvas->setTimeScale(newScale);
    m_timeScaleSpin->blockSignals(true);
    m_timeScaleSpin->setValue(newScale);
    m_timeScaleSpin->blockSignals(false);
}

void LeftPanel::onTimeScaleChanged(double scale)
{
    if (!m_chartCanvas)
        return;
    m_chartCanvas->setTimeScale(scale);
}

void LeftPanel::setChartController(ChartController *controller)
{
    m_chartController = controller;
    if (m_densityCurve)
        m_densityCurve->setChart(controller->chart());
}

void LeftPanel::setPlaybackController(PlaybackController *controller)
{
    m_playbackController = controller;
    connect(controller, &PlaybackController::stateChanged, this, [this](PlaybackController::State state)
            { m_playPauseBtn->setText(state == PlaybackController::Playing ? tr("Pause") : tr("Play")); });
}

void LeftPanel::setChartCanvas(ChartCanvas *canvas)
{
    m_chartCanvas = canvas;
    if (canvas)
    {
        // 当画布缩放改变时，同步更新 spinbox 显示
        connect(canvas, &ChartCanvas::timeScaleChanged, this, [this](double scale)
                {
            m_timeScaleSpin->blockSignals(true);
            m_timeScaleSpin->setValue(scale);
            m_timeScaleSpin->blockSignals(false); });
        // 初始化 spinbox 为当前缩放值
        m_timeScaleSpin->setValue(canvas->timeScale());
    }
}