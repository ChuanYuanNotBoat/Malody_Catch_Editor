#pragma once

#include <QMainWindow>

class ChartController;
class SelectionController;
class PlaybackController;
class RightPanel;
class QSplitter;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ChartController* chartCtrl, SelectionController* selCtrl, PlaybackController* playCtrl, QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void openChart();
    void saveChart();
    void saveChartAs();
    void exportMcz();
    void undo();
    void redo();
    void toggleColorMode(bool on);
    void toggleHyperfruitMode(bool on);
    void togglePlayback();

private:
    void setupUi();
    void createMenus();
    void createCentralArea();
    void retranslateUi();

    class Private;
    Private* d;
};