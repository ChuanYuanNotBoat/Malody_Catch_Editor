// src/app/MainWindow.h
#pragma once

#include <QMainWindow>

class ChartController;
class SelectionController;
class PlaybackController;
class Skin;
class RightPanel;
class LeftPanel;
class QSplitter;
class QMenu;
class QScrollBar;
class ChartCanvas;
class NoteEditPanel;
class BPMTimePanel;
class MetaEditPanel;
class QToolBar;
class QAction;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ChartController* chartCtrl,
                        SelectionController* selCtrl,
                        PlaybackController* playCtrl,
                        Skin* skin,
                        QWidget* parent = nullptr);
    ~MainWindow();

    void setSkin(Skin* skin);

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
    void toggleVerticalFlip(bool flipped);
    void togglePlayback();
    void changeSkin(const QString& skinName);
    void adjustNoteSize();
    void calibrateSkin();
    void configureOutline();
    void openLogSettings();
    void exportDiagnosticsReport();

private:
    void setupUi();
    void createMenus();
    void createCentralArea();
    void retranslateUi();
    void populateSkinMenu();

    class Private;
    Private* d;
};