#pragma once

#include "CustomWidgets/RightPanel.h"

class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;
class QTextEdit;
class QLabel;
class QFormLayout;

class MetaEditPanel : public RightPanel
{
    Q_OBJECT
public:
    explicit MetaEditPanel(QWidget *parent = nullptr);
    void setChartController(ChartController *controller) override;
    void setSelectionController(SelectionController *controller) override;
    void retranslateUi();

private slots:
    void refreshMeta();
    void onSaveClicked();

private:
    void setupUi();

    ChartController *m_chartController;
    QFormLayout *m_formLayout;
    QLabel *m_titleLabel;
    QLabel *m_titleOrgLabel;
    QLabel *m_artistLabel;
    QLabel *m_artistOrgLabel;
    QLabel *m_difficultyLabel;
    QLabel *m_chartAuthorLabel;
    QLabel *m_audioFileLabel;
    QLabel *m_audioOggLabel;
    QLabel *m_backgroundLabel;
    QLabel *m_previewTimeLabel;
    QLabel *m_firstBpmLabel;
    QLabel *m_offsetLabel;
    QLabel *m_speedLabel;
    QLineEdit *m_titleEdit;
    QLineEdit *m_titleOrgEdit;
    QLineEdit *m_artistEdit;
    QLineEdit *m_artistOrgEdit;
    QLineEdit *m_difficultyEdit;
    QLineEdit *m_chartAuthorEdit;
    QLineEdit *m_audioFileEdit;
    QLineEdit *m_backgroundFileEdit;
    QSpinBox *m_previewTimeSpin;
    QDoubleSpinBox *m_firstBpmSpin;
    QSpinBox *m_offsetSpin;
    QSpinBox *m_speedSpin;
    QPushButton *m_audioBrowseBtn;
    QPushButton *m_bgBrowseBtn;
    QPushButton *m_saveBtn;
};
