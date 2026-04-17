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
#include <QLabel>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QDebug>

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
    m_noteRadio->setChecked(true);
    m_modeGroup->addButton(m_noteRadio, 0);
    m_modeGroup->addButton(m_rainRadio, 1);
    m_modeGroup->addButton(m_deleteRadio, 2);
    connect(m_modeGroup, &QButtonGroup::buttonClicked, this, [this](QAbstractButton *button)
            { setMode(m_modeGroup->id(button)); });

    mainLayout->addWidget(m_noteRadio);
    mainLayout->addWidget(m_rainRadio);
    mainLayout->addWidget(m_deleteRadio);

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

void NoteEditPanel::setChartController(ChartController *controller)
{
    m_chartController = controller;
}

void NoteEditPanel::setSelectionController(SelectionController *controller)
{
    m_selectionController = controller;
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
    if (m_copyButton)
        m_copyButton->setText(tr("Copy"));
    if (m_timeDivisionLabel)
        m_timeDivisionLabel->setText(tr("Time Division:"));
    if (m_gridSnapCheck)
        m_gridSnapCheck->setText(tr("Grid Snap"));
    if (m_gridSettingsBtn)
        m_gridSettingsBtn->setText(tr("Grid Settings..."));
}

