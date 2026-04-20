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
#include <QHBoxLayout>
#include <QFileDialog>

MetaEditPanel::MetaEditPanel(QWidget *parent)
    : RightPanel(parent), m_chartController(nullptr)
{
    setupUi();
}

void MetaEditPanel::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    m_formLayout = new QFormLayout;

    m_titleEdit = new QLineEdit(this);
    m_titleLabel = new QLabel(tr("Title:"), this);
    m_formLayout->addRow(m_titleLabel, m_titleEdit);
    m_titleOrgEdit = new QLineEdit(this);
    m_titleOrgLabel = new QLabel(tr("Original Title:"), this);
    m_formLayout->addRow(m_titleOrgLabel, m_titleOrgEdit);
    m_artistEdit = new QLineEdit(this);
    m_artistLabel = new QLabel(tr("Artist:"), this);
    m_formLayout->addRow(m_artistLabel, m_artistEdit);
    m_artistOrgEdit = new QLineEdit(this);
    m_artistOrgLabel = new QLabel(tr("Original Artist:"), this);
    m_formLayout->addRow(m_artistOrgLabel, m_artistOrgEdit);
    m_difficultyEdit = new QLineEdit(this);
    m_difficultyLabel = new QLabel(tr("Difficulty:"), this);
    m_formLayout->addRow(m_difficultyLabel, m_difficultyEdit);
    m_chartAuthorEdit = new QLineEdit(this);
    m_chartAuthorLabel = new QLabel(tr("Chart Author:"), this);
    m_formLayout->addRow(m_chartAuthorLabel, m_chartAuthorEdit);
    m_audioFileEdit = new QLineEdit(this);
    m_audioFileLabel = new QLabel(tr("Audio File:"), this);
    m_formLayout->addRow(m_audioFileLabel, m_audioFileEdit);
    m_audioBrowseBtn = new QPushButton(tr("Browse..."), this);
    QHBoxLayout *audioLayout = new QHBoxLayout;
    audioLayout->addWidget(m_audioFileEdit);
    audioLayout->addWidget(m_audioBrowseBtn);
    m_audioOggLabel = new QLabel(tr("Audio (ogg):"), this);
    m_formLayout->addRow(m_audioOggLabel, audioLayout);
    connect(m_audioBrowseBtn, &QPushButton::clicked, [this]()
            {
        QString fileName = QFileDialog::getOpenFileName(this, tr("Select Audio"), QString(), tr("OGG Files (*.ogg)"));
        if (!fileName.isEmpty()) m_audioFileEdit->setText(fileName); });

    m_backgroundFileEdit = new QLineEdit(this);
    m_bgBrowseBtn = new QPushButton(tr("Browse..."), this);
    QHBoxLayout *bgLayout = new QHBoxLayout;
    bgLayout->addWidget(m_backgroundFileEdit);
    bgLayout->addWidget(m_bgBrowseBtn);
    m_backgroundLabel = new QLabel(tr("Background (jpg):"), this);
    m_formLayout->addRow(m_backgroundLabel, bgLayout);
    connect(m_bgBrowseBtn, &QPushButton::clicked, [this]()
            {
        QString fileName = QFileDialog::getOpenFileName(this, tr("Select Background"), QString(), tr("JPEG Files (*.jpg)"));
        if (!fileName.isEmpty()) m_backgroundFileEdit->setText(fileName); });

    m_previewTimeSpin = new QSpinBox(this);
    m_previewTimeSpin->setRange(0, 999999);
    m_previewTimeSpin->setSuffix(tr(" ms"));
    m_previewTimeLabel = new QLabel(tr("Preview Time:"), this);
    m_formLayout->addRow(m_previewTimeLabel, m_previewTimeSpin);

    m_firstBpmSpin = new QDoubleSpinBox(this);
    m_firstBpmSpin->setRange(1, 999);
    m_firstBpmSpin->setDecimals(3);
    m_firstBpmLabel = new QLabel(tr("First BPM:"), this);
    m_formLayout->addRow(m_firstBpmLabel, m_firstBpmSpin);

    m_offsetSpin = new QSpinBox(this);
    m_offsetSpin->setRange(-9999, 9999);
    m_offsetSpin->setSuffix(tr(" ms"));
    m_offsetLabel = new QLabel(tr("Offset:"), this);
    m_formLayout->addRow(m_offsetLabel, m_offsetSpin);

    m_speedSpin = new QSpinBox(this);
    m_speedSpin->setRange(1, 100);
    m_speedLabel = new QLabel(tr("Fall Speed:"), this);
    m_formLayout->addRow(m_speedLabel, m_speedSpin);

    mainLayout->addLayout(m_formLayout);

    m_saveBtn = new QPushButton(tr("Save"), this);
    connect(m_saveBtn, &QPushButton::clicked, this, &MetaEditPanel::onSaveClicked);
    mainLayout->addWidget(m_saveBtn);
    mainLayout->addStretch();
}

void MetaEditPanel::refreshMeta()
{
    Logger::info("MetaEditPanel::refreshMeta called");
    if (!m_chartController || !m_chartController->chart())
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
    if (m_chartController)
    {
        disconnect(m_chartController, &ChartController::chartChanged, this, &MetaEditPanel::refreshMeta);
        disconnect(m_chartController, &ChartController::chartLoaded, this, &MetaEditPanel::refreshMeta);
    }

    m_chartController = controller;
    if (!m_chartController)
        return;

    connect(m_chartController, &ChartController::chartChanged, this, &MetaEditPanel::refreshMeta, Qt::UniqueConnection);
    connect(m_chartController, &ChartController::chartLoaded, this, &MetaEditPanel::refreshMeta, Qt::UniqueConnection);
    refreshMeta();
}

void MetaEditPanel::setSelectionController(SelectionController *controller)
{
    Q_UNUSED(controller);
}

void MetaEditPanel::retranslateUi()
{
    if (m_titleLabel)
        m_titleLabel->setText(tr("Title:"));
    if (m_titleOrgLabel)
        m_titleOrgLabel->setText(tr("Original Title:"));
    if (m_artistLabel)
        m_artistLabel->setText(tr("Artist:"));
    if (m_artistOrgLabel)
        m_artistOrgLabel->setText(tr("Original Artist:"));
    if (m_difficultyLabel)
        m_difficultyLabel->setText(tr("Difficulty:"));
    if (m_chartAuthorLabel)
        m_chartAuthorLabel->setText(tr("Chart Author:"));
    if (m_audioFileLabel)
        m_audioFileLabel->setText(tr("Audio File:"));
    if (m_audioOggLabel)
        m_audioOggLabel->setText(tr("Audio (ogg):"));
    if (m_backgroundLabel)
        m_backgroundLabel->setText(tr("Background (jpg):"));
    if (m_previewTimeLabel)
        m_previewTimeLabel->setText(tr("Preview Time:"));
    if (m_firstBpmLabel)
        m_firstBpmLabel->setText(tr("First BPM:"));
    if (m_offsetLabel)
        m_offsetLabel->setText(tr("Offset:"));
    if (m_speedLabel)
        m_speedLabel->setText(tr("Fall Speed:"));
    if (m_audioBrowseBtn)
        m_audioBrowseBtn->setText(tr("Browse..."));
    if (m_bgBrowseBtn)
        m_bgBrowseBtn->setText(tr("Browse..."));
    if (m_saveBtn)
        m_saveBtn->setText(tr("Save"));
    if (m_previewTimeSpin)
        m_previewTimeSpin->setSuffix(tr(" ms"));
    if (m_offsetSpin)
        m_offsetSpin->setSuffix(tr(" ms"));
}
