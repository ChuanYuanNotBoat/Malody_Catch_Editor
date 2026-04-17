#pragma once

#include "MainWindow.h"
#include <QString>

class ChartController;
class SelectionController;
class PlaybackController;
class Skin;
class RightPanel;
class LeftPanel;
class QScrollBar;
class QSplitter;
class QWidget;
class NoteEditPanel;
class BPMTimePanel;
class MetaEditPanel;
class QAction;
class QActionGroup;
class QMenu;
class ChartCanvas;

class MainWindow::Private
{
public:
    ChartController *chartController;
    SelectionController *selectionController;
    PlaybackController *playbackController;
    Skin *skin;
    ChartCanvas *canvas;
    QScrollBar *verticalScrollBar;
    QSplitter *splitter;
    QWidget *rightPanelContainer;
    RightPanel *currentRightPanel;
    NoteEditPanel *notePanel;
    BPMTimePanel *bpmPanel;
    MetaEditPanel *metaPanel;
    LeftPanel *leftPanel;
    QAction *undoAction;
    QAction *redoAction;
    QAction *colorAction;
    QAction *hyperfruitAction;
    QAction *verticalFlipAction;
    QAction *playAction;
    QActionGroup *speedActionGroup;
    QMenu *skinMenu;
    QMenu *noteSoundMenu;
    QAction *noteSizeAction;
    QAction *noteSoundVolumeAction;
    QAction *calibrateSkinAction;
    QAction *outlineAction;

    QString currentChartPath;
    bool isModified = false;
};
