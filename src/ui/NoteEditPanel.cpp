#include "NoteEditPanel.h"
#include "controller/ChartController.h"
#include "controller/SelectionController.h"
#include "utils/Logger.h"
#include <QtGlobal>
#include <QButtonGroup>
#include <QRadioButton>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QDebug>
#include <QSignalBlocker>

NoteEditPanel::NoteEditPanel(QWidget *parent)
    : RightPanel(parent), m_chartController(nullptr), m_selectionController(nullptr), m_currentMode(0), m_gridDivision(20)
{
    setupUi();
}

void NoteEditPanel::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    m_modeLabel = new QLabel(tr("Mode:"), this);
    mainLayout->addWidget(m_modeLabel);

    m_modeGroup = new QButtonGroup(this);
    m_noteRadio = new QRadioButton(tr("Place Note"), this);
    m_rainRadio = new QRadioButton(tr("Place Rain"), this);
    m_deleteRadio = new QRadioButton(tr("Delete Mode"), this);
    m_selectRadio = new QRadioButton(tr("Select Mode"), this);
    m_noteRadio->setChecked(true);
    m_modeGroup->addButton(m_noteRadio, 0);
    m_modeGroup->addButton(m_rainRadio, 1);
    m_modeGroup->addButton(m_deleteRadio, 2);
    m_modeGroup->addButton(m_selectRadio, 3);
    connect(m_modeGroup, &QButtonGroup::buttonClicked, this, [this](QAbstractButton *button)
            { setMode(m_modeGroup->id(button)); });

    mainLayout->addWidget(m_noteRadio);
    mainLayout->addWidget(m_rainRadio);
    mainLayout->addWidget(m_deleteRadio);
    mainLayout->addWidget(m_selectRadio);

    m_pluginToolsLabel = new QLabel(tr("Note Placement Tools:"), this);
    m_pluginToolsContainer = new QWidget(this);
    m_pluginToolsLayout = new QVBoxLayout(m_pluginToolsContainer);
    m_pluginToolsLayout->setContentsMargins(0, 0, 0, 0);
    m_pluginToolsLayout->setSpacing(6);
    m_pluginToolsLabel->setVisible(false);
    m_pluginToolsContainer->setVisible(false);
    mainLayout->addWidget(m_pluginToolsLabel);
    mainLayout->addWidget(m_pluginToolsContainer);

    // Copy button.
    m_copyButton = new QPushButton(tr("Copy"), this);
    connect(m_copyButton, &QPushButton::clicked, this, &NoteEditPanel::copyRequested);
    mainLayout->addWidget(m_copyButton);

    m_timeDivisionLabel = new QLabel(tr("Time Division:"), this);
    mainLayout->addWidget(m_timeDivisionLabel);
    m_timeDivisionCombo = new QComboBox(this);
    QStringList divisions = {"1", "2", "3", "4", "6", "8", "12", "16", "24", "32"};
    for (const QString &d : divisions)
        m_timeDivisionCombo->addItem(d);
    m_timeDivisionCombo->setEditable(true);
    m_timeDivisionCombo->setCurrentText("4");
    connect(m_timeDivisionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NoteEditPanel::onTimeDivisionChanged);
    mainLayout->addWidget(m_timeDivisionCombo);

    m_gridSnapCheck = new QCheckBox(tr("Grid Snap"), this);
    m_gridSnapCheck->setChecked(true);
    connect(m_gridSnapCheck, &QCheckBox::toggled, this, &NoteEditPanel::onGridSnapToggled);
    mainLayout->addWidget(m_gridSnapCheck);

    m_gridSettingsBtn = new QPushButton(tr("Grid Settings..."), this);
    connect(m_gridSettingsBtn, &QPushButton::clicked, this, &NoteEditPanel::onGridSettingsClicked);
    mainLayout->addWidget(m_gridSettingsBtn);

    m_mirrorGroup = new QGroupBox(tr("Mirror Flip"), this);
    QVBoxLayout *mirrorLayout = new QVBoxLayout(m_mirrorGroup);
    QHBoxLayout *axisLayout = new QHBoxLayout;
    m_mirrorAxisLabel = new QLabel(tr("Axis X:"), m_mirrorGroup);
    m_mirrorAxisSpin = new QSpinBox(m_mirrorGroup);
    m_mirrorAxisSpin->setRange(0, 512);
    m_mirrorAxisSpin->setValue(256);
    axisLayout->addWidget(m_mirrorAxisLabel);
    axisLayout->addWidget(m_mirrorAxisSpin, 1);
    mirrorLayout->addLayout(axisLayout);

    m_mirrorGuideCheck = new QCheckBox(tr("Show Guide"), m_mirrorGroup);
    m_mirrorGuideCheck->setChecked(false);
    mirrorLayout->addWidget(m_mirrorGuideCheck);

    m_mirrorPreviewCheck = new QCheckBox(tr("Show Preview"), m_mirrorGroup);
    m_mirrorPreviewCheck->setChecked(false);
    mirrorLayout->addWidget(m_mirrorPreviewCheck);

    m_mirrorFlipButton = new QPushButton(tr("Flip Selected"), m_mirrorGroup);
    mirrorLayout->addWidget(m_mirrorFlipButton);
    mainLayout->addWidget(m_mirrorGroup);

    connect(m_mirrorAxisSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &NoteEditPanel::onMirrorAxisSpinChanged);
    connect(m_mirrorGuideCheck, &QCheckBox::toggled, this, &NoteEditPanel::mirrorGuideVisibilityChanged);
    connect(m_mirrorPreviewCheck, &QCheckBox::toggled, this, &NoteEditPanel::mirrorPreviewVisibilityChanged);
    connect(m_mirrorFlipButton, &QPushButton::clicked, this, &NoteEditPanel::mirrorFlipRequested);

    mainLayout->addStretch();
}

void NoteEditPanel::setMode(int mode)
{
    m_currentMode = mode;
    emit modeChanged(mode);
}

void NoteEditPanel::onNoteModeClicked() { setMode(0); }
void NoteEditPanel::onRainModeClicked() { setMode(1); }
void NoteEditPanel::onDeleteModeClicked() { setMode(2); }
void NoteEditPanel::onSelectModeClicked() { setMode(3); }

void NoteEditPanel::onGridSettingsClicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Grid Settings"));
    QFormLayout form(&dialog);

    QCheckBox *snapCheck = new QCheckBox(tr("Enable Grid Snap"));
    snapCheck->setChecked(m_gridSnapCheck->isChecked());

    QSpinBox *divisionSpin = new QSpinBox;
    divisionSpin->setRange(4, 64);
    divisionSpin->setValue(m_gridDivision); // Use current grid division instead of hardcoded default.

    form.addRow(tr("Snap to Grid:"), snapCheck);
    form.addRow(tr("Grid Divisions (4-64):"), divisionSpin);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form.addRow(buttons);

    if (dialog.exec() == QDialog::Accepted)
    {
        m_gridSnapCheck->setChecked(snapCheck->isChecked());
        int newDivision = divisionSpin->value();
        m_gridDivision = newDivision;
        emit gridDivisionChanged(newDivision);
        Logger::info(QString("[Grid] NoteEditPanel: grid division changed to %1").arg(newDivision));
    }
}

void NoteEditPanel::onGridSnapToggled(bool on)
{
    Logger::info(QString("[Grid] NoteEditPanel::onGridSnapToggled: %1").arg(on));
    emit gridSnapChanged(on);
}

void NoteEditPanel::onTimeDivisionChanged(int index)
{
    Q_UNUSED(index);
    int division = m_timeDivisionCombo->currentText().toInt();
    if (division < 1)
        division = 1;
    if (division > 96)
        division = 96;
    qDebug() << "NoteEditPanel: Time division changed to" << division;
    emit timeDivisionChanged(division);
}

void NoteEditPanel::onMirrorAxisSpinChanged(int value)
{
    emit mirrorAxisChanged(value);
}

void NoteEditPanel::setChartController(ChartController *controller)
{
    m_chartController = controller;
}

void NoteEditPanel::setSelectionController(SelectionController *controller)
{
    m_selectionController = controller;
}

void NoteEditPanel::setPluginPlacementActions(const QList<PluginPlacementAction> &actions)
{
    if (!m_pluginToolsLayout)
        return;

    while (QLayoutItem *item = m_pluginToolsLayout->takeAt(0))
    {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (const PluginPlacementAction &a : actions)
    {
        if (a.pluginId.isEmpty() || a.actionId.isEmpty() || a.title.isEmpty())
            continue;

        if (a.checkable)
        {
            QCheckBox *cb = new QCheckBox(a.title, m_pluginToolsContainer);
            cb->setChecked(a.checked);
            if (!a.tooltip.isEmpty())
                cb->setToolTip(a.tooltip);
            connect(cb, &QCheckBox::clicked, this, [this, a](bool)
                    { emit pluginPlacementActionTriggered(a.pluginId, a.actionId); });
            m_pluginToolsLayout->addWidget(cb);
            continue;
        }

        QPushButton *btn = new QPushButton(a.title, m_pluginToolsContainer);
        if (!a.tooltip.isEmpty())
            btn->setToolTip(a.tooltip);
        connect(btn, &QPushButton::clicked, this, [this, a](bool)
                { emit pluginPlacementActionTriggered(a.pluginId, a.actionId); });
        m_pluginToolsLayout->addWidget(btn);
    }

    const bool hasActions = (m_pluginToolsLayout->count() > 0);
    if (m_pluginToolsLabel)
        m_pluginToolsLabel->setVisible(hasActions);
    if (m_pluginToolsContainer)
        m_pluginToolsContainer->setVisible(hasActions);
}

void NoteEditPanel::setMirrorAxisValue(int axisX)
{
    if (!m_mirrorAxisSpin)
        return;

    const QSignalBlocker blocker(m_mirrorAxisSpin);
    m_mirrorAxisSpin->setValue(axisX);
}

void NoteEditPanel::retranslateUi()
{
    if (m_modeLabel)
        m_modeLabel->setText(tr("Mode:"));
    if (m_noteRadio)
        m_noteRadio->setText(tr("Place Note"));
    if (m_rainRadio)
        m_rainRadio->setText(tr("Place Rain"));
    if (m_deleteRadio)
        m_deleteRadio->setText(tr("Delete Mode"));
    if (m_selectRadio)
        m_selectRadio->setText(tr("Select Mode"));
    if (m_pluginToolsLabel)
        m_pluginToolsLabel->setText(tr("Note Placement Tools:"));
    if (m_copyButton)
        m_copyButton->setText(tr("Copy"));
    if (m_timeDivisionLabel)
        m_timeDivisionLabel->setText(tr("Time Division:"));
    if (m_gridSnapCheck)
        m_gridSnapCheck->setText(tr("Grid Snap"));
    if (m_gridSettingsBtn)
        m_gridSettingsBtn->setText(tr("Grid Settings..."));
    if (m_mirrorGroup)
        m_mirrorGroup->setTitle(tr("Mirror Flip"));
    if (m_mirrorAxisLabel)
        m_mirrorAxisLabel->setText(tr("Axis X:"));
    if (m_mirrorGuideCheck)
        m_mirrorGuideCheck->setText(tr("Show Guide"));
    if (m_mirrorPreviewCheck)
        m_mirrorPreviewCheck->setText(tr("Show Preview"));
    if (m_mirrorFlipButton)
        m_mirrorFlipButton->setText(tr("Flip Selected"));
}

