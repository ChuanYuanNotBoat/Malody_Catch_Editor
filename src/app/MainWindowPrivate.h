#pragma once

#include "MainWindow.h"
#include <QHash>
#include <QList>
#include <QString>
#include <QVariantMap>

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
class QToolBar;

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
    QMenu *pluginToolsMenu;
    QAction *noteSizeAction;
    QAction *noteSoundVolumeAction;
    QAction *calibrateSkinAction;
    QAction *outlineAction;
    QToolBar *mainToolBar;
    QList<QAction *> pluginToolbarActions;
    QHash<QString, QVariantMap> pluginActionMeta;

    QString currentChartPath;
    bool isModified = false;
};
