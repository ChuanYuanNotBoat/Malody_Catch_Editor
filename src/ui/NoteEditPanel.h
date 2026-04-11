#pragma once

#include "CustomWidgets/RightPanel.h"
#include <QButtonGroup>

class QComboBox;
class QCheckBox;
class QSpinBox;
class QPushButton;
class QVBoxLayout;

class NoteEditPanel : public RightPanel
{
    Q_OBJECT
public:
    explicit NoteEditPanel(QWidget *parent = nullptr);
    void setChartController(ChartController *controller) override;
    void setSelectionController(SelectionController *controller) override;

signals:
    void modeChanged(int mode);             // 0: place note, 1: place rain, 2: delete mode
    void timeDivisionChanged(int division); // 新增信号
    void gridDivisionChanged(int division); // 新增网格分度信号
    void gridSnapChanged(bool enabled);     // 新增网格吸附信号

private slots:
    void onNoteModeClicked();
    void onRainModeClicked();
    void onDeleteModeClicked();
    void onGridSettingsClicked();
    void onGridDivisionChanged(int value);
    void onGridSnapToggled(bool on);
    void onTimeDivisionChanged(int index);

private:
    void setupUi();
    void setMode(int mode);

    ChartController *m_chartController;
    SelectionController *m_selectionController;
    QButtonGroup *m_modeGroup;
    QComboBox *m_timeDivisionCombo;
    QCheckBox *m_gridSnapCheck;
    QSpinBox *m_gridDivisionSpin;
    QPushButton *m_gridSettingsBtn;
    int m_currentMode;
};