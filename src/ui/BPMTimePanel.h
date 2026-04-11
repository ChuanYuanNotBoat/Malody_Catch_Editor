#pragma once

#include "CustomWidgets/RightPanel.h"
#include <QVector>

class QListWidget;
class QLineEdit;
class QDoubleSpinBox;
class QPushButton;

class BPMTimePanel : public RightPanel
{
    Q_OBJECT
public:
    explicit BPMTimePanel(QWidget *parent = nullptr);
    void setChartController(ChartController *controller) override;
    void setSelectionController(SelectionController *controller) override;

private slots:
    void refreshBpmList();
    void onItemSelected(int row);
    void onAddClicked();
    void onRemoveClicked();
    void onBpmChanged(double);

private:
    void setupUi();

    ChartController *m_chartController;
    QListWidget *m_bpmListWidget;
    QLineEdit *m_timeEdit;
    QDoubleSpinBox *m_bpmSpin;
    QPushButton *m_addBtn;
    QPushButton *m_removeBtn;
    int m_selectedIndex;
};