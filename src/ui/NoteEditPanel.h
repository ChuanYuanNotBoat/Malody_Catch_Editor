#pragma once

#include "CustomWidgets/RightPanel.h"
#include <QButtonGroup>

class QComboBox;
class QCheckBox;
class QSpinBox;
class QPushButton;
class QVBoxLayout;
class QLabel;
class QRadioButton;

class NoteEditPanel : public RightPanel
{
    Q_OBJECT
public:
    explicit NoteEditPanel(QWidget *parent = nullptr);
    void setChartController(ChartController *controller) override;
    void setSelectionController(SelectionController *controller) override;
    void retranslateUi();

signals:
    void modeChanged(int mode);
    void timeDivisionChanged(int division);
    void gridDivisionChanged(int division);
    void gridSnapChanged(bool enabled);
    void copyRequested();

private slots:
    void onNoteModeClicked();
    void onRainModeClicked();
    void onDeleteModeClicked();
    void onGridSettingsClicked();
    // void onGridDivisionChanged(int value);
    void onGridSnapToggled(bool on);
    void onTimeDivisionChanged(int index);

private:
    void setupUi();
    void setMode(int mode);

    ChartController *m_chartController;
    SelectionController *m_selectionController;
    QLabel *m_modeLabel;
    QButtonGroup *m_modeGroup;
    QRadioButton *m_noteRadio;
    QRadioButton *m_rainRadio;
    QRadioButton *m_deleteRadio;
    QLabel *m_timeDivisionLabel;
    QComboBox *m_timeDivisionCombo;
    QCheckBox *m_gridSnapCheck;
    QPushButton *m_gridSettingsBtn;
    QPushButton *m_copyButton;
    int m_currentMode;
    int m_gridDivision;
};
