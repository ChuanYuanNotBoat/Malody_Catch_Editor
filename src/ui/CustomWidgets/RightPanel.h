#pragma once

#include <QWidget>

class ChartController;
class SelectionController;

class RightPanel : public QWidget {
    Q_OBJECT
public:
    explicit RightPanel(QWidget* parent = nullptr);
    virtual void setChartController(ChartController* controller) = 0;
    virtual void setSelectionController(SelectionController* controller) = 0;
};