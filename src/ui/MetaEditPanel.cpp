#include "MetaEditPanel.h"
#include "controller/ChartController.h"
#include "model/MetaData.h"
#include "utils/Logger.h"
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QFileDialog>

MetaEditPanel::MetaEditPanel(QWidget *parent)
    : RightPanel(parent), m_chartController(nullptr)
{
    setupUi();
}

void MetaEditPanel::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QFormLayout *form = new QFormLayout;

    m_titleEdit = new QLineEdit(this);
    form->addRow(tr("Title:"), m_titleEdit);
    m_titleOrgEdit = new QLineEdit(this);
    form->addRow(tr("Original Title:"), m_titleOrgEdit);
    m_artistEdit = new QLineEdit(this);
    form->addRow(tr("Artist:"), m_artistEdit);
    m_artistOrgEdit = new QLineEdit(this);
    form->addRow(tr("Original Artist:"), m_artistOrgEdit);
    m_difficultyEdit = new QLineEdit(this);
    form->addRow(tr("Difficulty:"), m_difficultyEdit);
    m_chartAuthorEdit = new QLineEdit(this);
    form->addRow(tr("Chart Author:"), m_chartAuthorEdit);
    m_audioFileEdit = new QLineEdit(this);
    form->addRow(tr("Audio File:"), m_audioFileEdit);
    QPushButton *audioBrowse = new QPushButton(tr("Browse..."), this);
    QHBoxLayout *audioLayout = new QHBoxLayout;
    audioLayout->addWidget(m_audioFileEdit);
    audioLayout->addWidget(audioBrowse);
    form->addRow(tr("Audio (ogg):"), audioLayout);
    connect(audioBrowse, &QPushButton::clicked, [this]()
            {
        QString fileName = QFileDialog::getOpenFileName(this, tr("Select Audio"), QString(), tr("OGG Files (*.ogg)"));
        if (!fileName.isEmpty()) m_audioFileEdit->setText(fileName); });

    m_backgroundFileEdit = new QLineEdit(this);
    QPushButton *bgBrowse = new QPushButton(tr("Browse..."), this);
    QHBoxLayout *bgLayout = new QHBoxLayout;
    bgLayout->addWidget(m_backgroundFileEdit);
    bgLayout->addWidget(bgBrowse);
    form->addRow(tr("Background (jpg):"), bgLayout);
    connect(bgBrowse, &QPushButton::clicked, [this]()
            {
        QString fileName = QFileDialog::getOpenFileName(this, tr("Select Background"), QString(), tr("JPEG Files (*.jpg)"));
        if (!fileName.isEmpty()) m_backgroundFileEdit->setText(fileName); });

    m_previewTimeSpin = new QSpinBox(this);
    m_previewTimeSpin->setRange(0, 999999);
    m_previewTimeSpin->setSuffix(" ms");
    form->addRow(tr("Preview Time:"), m_previewTimeSpin);

    m_firstBpmSpin = new QDoubleSpinBox(this);
    m_firstBpmSpin->setRange(1, 999);
    m_firstBpmSpin->setDecimals(3);
    form->addRow(tr("First BPM:"), m_firstBpmSpin);

    m_offsetSpin = new QSpinBox(this);
    m_offsetSpin->setRange(-9999, 9999);
    m_offsetSpin->setSuffix(" ms");
    form->addRow(tr("Offset:"), m_offsetSpin);

    m_speedSpin = new QSpinBox(this);
    m_speedSpin->setRange(1, 100);
    form->addRow(tr("Fall Speed:"), m_speedSpin);

    mainLayout->addLayout(form);

    m_saveBtn = new QPushButton(tr("Save"), this);
    connect(m_saveBtn, &QPushButton::clicked, this, &MetaEditPanel::onSaveClicked);
    mainLayout->addWidget(m_saveBtn);
    mainLayout->addStretch();
}

void MetaEditPanel::refreshMeta()
{
    Logger::info("MetaEditPanel::refreshMeta called");
    if (!m_chartController)
        return;
    const MetaData &meta = m_chartController->chart()->meta();
    Logger::info(QString("MetaEditPanel::refreshMeta - title='%1', artist='%2', difficulty='%3', speed=%4")
                     .arg(meta.title)
                     .arg(meta.artist)
                     .arg(meta.difficulty)
                     .arg(meta.speed));
    m_titleEdit->setText(meta.title);
    m_titleOrgEdit->setText(meta.titleOrg);
    m_artistEdit->setText(meta.artist);
    m_artistOrgEdit->setText(meta.artistOrg);
    m_difficultyEdit->setText(meta.difficulty);
    m_chartAuthorEdit->setText(meta.chartAuthor);
    m_audioFileEdit->setText(meta.audioFile);
    m_backgroundFileEdit->setText(meta.backgroundFile);
    m_previewTimeSpin->setValue(meta.previewTime);
    m_firstBpmSpin->setValue(meta.firstBpm);
    m_offsetSpin->setValue(meta.offset);
    m_speedSpin->setValue(meta.speed);
}

void MetaEditPanel::onSaveClicked()
{
    if (!m_chartController)
        return;
    MetaData meta;
    meta.title = m_titleEdit->text();
    meta.titleOrg = m_titleOrgEdit->text();
    meta.artist = m_artistEdit->text();
    meta.artistOrg = m_artistOrgEdit->text();
    meta.difficulty = m_difficultyEdit->text();
    meta.chartAuthor = m_chartAuthorEdit->text();
    meta.audioFile = m_audioFileEdit->text();
    meta.backgroundFile = m_backgroundFileEdit->text();
    meta.previewTime = m_previewTimeSpin->value();
    meta.firstBpm = m_firstBpmSpin->value();
    meta.offset = m_offsetSpin->value();
    meta.speed = m_speedSpin->value();
    m_chartController->setMetaData(meta);
}

void MetaEditPanel::setChartController(ChartController *controller)
{
    m_chartController = controller;
    connect(m_chartController, &ChartController::chartChanged, this, &MetaEditPanel::refreshMeta);
    connect(m_chartController, &ChartController::chartLoaded, this, &MetaEditPanel::refreshMeta);
    refreshMeta();
}

void MetaEditPanel::setSelectionController(SelectionController *controller)
{
    Q_UNUSED(controller);
}