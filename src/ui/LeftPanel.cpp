#include "LeftPanel.h"
#include "DensityCurve.h"
#include "controller/ChartController.h"
#include "controller/PlaybackController.h"
#include "ui/CustomWidgets/ChartCanvas/ChartCanvas.h"
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

    // 瀵嗗害鏇茬嚎
    m_densityCurve = new DensityCurve(this);
    layout->addWidget(m_densityCurve);

    // 鎾斁/鏆傚仠鎸夐挳
    m_playPauseBtn = new QPushButton(tr("Play"), this);
    layout->addWidget(m_playPauseBtn);
    connect(m_playPauseBtn, &QPushButton::clicked, this, &LeftPanel::onPlayPauseClicked);

    // 鏃堕棿杞寸缉鏀炬帶鍒?
    QHBoxLayout *zoomLayout = new QHBoxLayout;
    QLabel *zoomLabel = new QLabel(tr("Zoom:"), this);
    m_zoomOutBtn = new QPushButton("-", this);
    m_zoomOutBtn->setFixedWidth(30);
    m_zoomInBtn = new QPushButton("+", this);
    m_zoomInBtn->setFixedWidth(30);

    m_timeScaleSpin = new QDoubleSpinBox(this);
    m_timeScaleSpin->setRange(0.2, 5.0); // 鎵嬪姩杈撳叆鏃犱弗鏍奸檺鍒?
    m_timeScaleSpin->setSingleStep(0.1);
    m_timeScaleSpin->setDecimals(2);
    m_timeScaleSpin->setValue(2.25);
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
    double newScale = m_chartCanvas->timeScale() * 1.2; // 澧炲姞 20%
    m_chartCanvas->setTimeScale(newScale);
    // 鏇存柊 spinbox 鏄剧ず锛堜俊鍙蜂細瑙﹀彂鏇存柊锛屼絾涓轰簡闃叉寰幆锛屽厛 block锛?
    m_timeScaleSpin->blockSignals(true);
    m_timeScaleSpin->setValue(m_chartCanvas->timeScale());
    m_timeScaleSpin->blockSignals(false);
}

void LeftPanel::onZoomOutClicked()
{
    if (!m_chartCanvas)
        return;
    double newScale = m_chartCanvas->timeScale() / 1.2; // 鍑忓皯 20%
    m_chartCanvas->setTimeScale(newScale);
    m_timeScaleSpin->blockSignals(true);
    m_timeScaleSpin->setValue(m_chartCanvas->timeScale());
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
        // 褰撶敾甯冪缉鏀炬敼鍙樻椂锛屽悓姝ユ洿鏂?spinbox 鏄剧ず
        connect(canvas, &ChartCanvas::timeScaleChanged, this, [this](double scale)
                {
            m_timeScaleSpin->blockSignals(true);
            m_timeScaleSpin->setValue(scale);
            m_timeScaleSpin->blockSignals(false); });
        // 鍒濆鍖?spinbox 涓哄綋鍓嶇缉鏀惧€?
        m_timeScaleSpin->setValue(canvas->timeScale());
    }
}


