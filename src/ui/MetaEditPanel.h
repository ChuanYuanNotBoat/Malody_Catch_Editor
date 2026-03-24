#pragma once

#include "CustomWidgets/RightPanel.h"

class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;
class QTextEdit;

class MetaEditPanel : public RightPanel {
    Q_OBJECT
public:
    explicit MetaEditPanel(QWidget* parent = nullptr);
    void setChartController(ChartController* controller) override;
    void setSelectionController(SelectionController* controller) override;

private slots:
    void refreshMeta();
    void onSaveClicked();

private:
    void setupUi();

    ChartController* m_chartController;
    QLineEdit* m_titleEdit;
    QLineEdit* m_titleOrgEdit;
    QLineEdit* m_artistEdit;
    QLineEdit* m_artistOrgEdit;
    QLineEdit* m_difficultyEdit;
    QLineEdit* m_chartAuthorEdit;
    QLineEdit* m_audioFileEdit;
    QLineEdit* m_backgroundFileEdit;
    QSpinBox* m_previewTimeSpin;
    QDoubleSpinBox* m_firstBpmSpin;
    QSpinBox* m_offsetSpin;
    QSpinBox* m_speedSpin;
    QPushButton* m_saveBtn;
};