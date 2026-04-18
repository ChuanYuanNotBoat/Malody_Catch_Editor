#pragma once

#include "MainWindow.h"
#include <QHash>
#include <QList>
#include <QPointer>
#include <QKeySequence>
#include <QSet>
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
class QDialog;

class MainWindow::Private
{
public:
    ChartController *chartController = nullptr;
    SelectionController *selectionController = nullptr;
    PlaybackController *playbackController = nullptr;
    Skin *skin = nullptr;
    ChartCanvas *canvas = nullptr;
    QScrollBar *verticalScrollBar = nullptr;
    QSplitter *splitter = nullptr;
    QWidget *rightPanelContainer = nullptr;
    RightPanel *currentRightPanel = nullptr;
    NoteEditPanel *notePanel = nullptr;
    BPMTimePanel *bpmPanel = nullptr;
    MetaEditPanel *metaPanel = nullptr;
    LeftPanel *leftPanel = nullptr;
    QAction *undoAction = nullptr;
    QAction *redoAction = nullptr;
    QAction *colorAction = nullptr;
    QAction *hyperfruitAction = nullptr;
    QAction *verticalFlipAction = nullptr;
    QAction *playAction = nullptr;
    QActionGroup *speedActionGroup = nullptr;
    QMenu *skinMenu = nullptr;
    QMenu *noteSoundMenu = nullptr;
    QMenu *pluginToolsMenu = nullptr;
    QMenu *pluginPanelsMenu = nullptr;
    QAction *noteSizeAction = nullptr;
    QAction *noteSoundVolumeAction = nullptr;
    QAction *calibrateSkinAction = nullptr;
    QAction *outlineAction = nullptr;
    QAction *notePanelAction = nullptr;
    QAction *bpmPanelAction = nullptr;
    QAction *metaPanelAction = nullptr;
    QToolBar *mainToolBar = nullptr;
    QMenu *languageMenu = nullptr;
    QActionGroup *languageActionGroup = nullptr;
    QList<QAction *> pluginToolbarActions;
    QHash<QString, QVariantMap> pluginActionMeta;
    QHash<QString, QPointer<QDialog>> pluginPanelDialogs;
    QSet<QString> batchEditDisabledActions;
    QHash<QString, QAction *> shortcutActions;
    QHash<QString, QKeySequence> shortcutDefaults;
    QList<QString> shortcutActionOrder;

    QString currentChartPath;
    bool isModified = false;
};
