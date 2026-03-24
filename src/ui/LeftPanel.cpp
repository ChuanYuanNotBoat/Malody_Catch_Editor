#include "LeftPanel.h"
#include "DensityCurve.h"
#include "controller/ChartController.h"
#include "controller/PlaybackController.h"
#include <QVBoxLayout>
#include <QPushButton>

LeftPanel::LeftPanel(QWidget* parent)
    : QWidget(parent), m_chartController(nullptr), m_playbackController(nullptr)
{
    setupUi();
}

void LeftPanel::setupUi()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    m_densityCurve = new DensityCurve(this);
    layout->addWidget(m_densityCurve);
    m_playPauseBtn = new QPushButton(tr("Play"), this);
    layout->addWidget(m_playPauseBtn);
    connect(m_playPauseBtn, &QPushButton::clicked, this, &LeftPanel::onPlayPauseClicked);
}

void LeftPanel::onPlayPauseClicked()
{
    if (!m_playbackController) return;
    if (m_playbackController->state() == PlaybackController::Playing) {
        m_playbackController->pause();
        m_playPauseBtn->setText(tr("Play"));
    } else {
        m_playbackController->play();
        m_playPauseBtn->setText(tr("Pause"));
    }
}

void LeftPanel::setChartController(ChartController* controller)
{
    m_chartController = controller;
    if (m_densityCurve)
        m_densityCurve->setChart(controller->chart());
}

void LeftPanel::setPlaybackController(PlaybackController* controller)
{
    m_playbackController = controller;
    connect(controller, &PlaybackController::stateChanged, this, [this](PlaybackController::State state) {
        m_playPauseBtn->setText(state == PlaybackController::Playing ? tr("Pause") : tr("Play"));
    });
}