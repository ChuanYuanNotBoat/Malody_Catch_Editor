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
class DensityCurve;
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
class QDockWidget;
class QTabWidget;
class QScrollArea;
class QQuickWidget;
class QObject;
class QTimer;
class RealtimePreviewWidget;

class MainWindow::Private
{
public:
    ChartController *chartController = nullptr;
    SelectionController *selectionController = nullptr;
    PlaybackController *playbackController = nullptr;
    Skin *skin = nullptr;
    ChartCanvas *canvas = nullptr;
    RealtimePreviewWidget *previewWidget = nullptr;
    DensityCurve *rightDensityBar = nullptr;
    QSplitter *splitter = nullptr;
    QWidget *rightPanelContainer = nullptr;
    RightPanel *currentRightPanel = nullptr;
    NoteEditPanel *notePanel = nullptr;
    BPMTimePanel *bpmPanel = nullptr;
    MetaEditPanel *metaPanel = nullptr;
    LeftPanel *leftPanel = nullptr;
    QAction *undoAction = nullptr;
    QAction *redoAction = nullptr;
    QAction *deleteAction = nullptr;
    QAction *colorAction = nullptr;
    QAction *timelineDivisionColorAction = nullptr;
    QAction *timelineDivisionColorSettingsAction = nullptr;
    QAction *hyperfruitAction = nullptr;
    QAction *verticalFlipAction = nullptr;
    QAction *playAction = nullptr;
    QActionGroup *speedActionGroup = nullptr;
    QMenu *skinMenu = nullptr;
    QMenu *noteSoundMenu = nullptr;
    QMenu *helpMenu = nullptr;
    QMenu *pluginsMenu = nullptr;
    QMenu *pluginToolsMenu = nullptr;
    QMenu *pluginPanelsMenu = nullptr;
    QAction *pluginToolModeAction = nullptr;
    QAction *pluginToolModeToolbarAction = nullptr;
    QAction *pluginManagerToolbarAction = nullptr;
    QAction *noteSizeAction = nullptr;
    QAction *noteSoundVolumeAction = nullptr;
    QAction *calibrateSkinAction = nullptr;
    QAction *outlineAction = nullptr;
    QAction *notePanelAction = nullptr;
    QAction *bpmPanelAction = nullptr;
    QAction *metaPanelAction = nullptr;
    QAction *mobileUiTestAction = nullptr;
    QAction *checkUpdatesAction = nullptr;
    QAction *helpDocAction = nullptr;
    QAction *aboutAction = nullptr;
    QAction *versionAction = nullptr;
    QAction *logsAction = nullptr;
    QToolBar *mainToolBar = nullptr;
    QToolBar *pluginToolBar = nullptr;
    QWidget *mobileShell = nullptr;
    QQuickWidget *mobilePrimaryBar = nullptr;
    QObject *mobilePrimaryBarRoot = nullptr;
    QDockWidget *leftDock = nullptr;
    QDockWidget *rightDock = nullptr;
    QTabWidget *mobileTabs = nullptr;
    QWidget *mobileCanvasHost = nullptr;
    QScrollArea *mobileLeftPanelHost = nullptr;
    QScrollArea *mobileRightPanelHost = nullptr;
    QTimer *mobileSplitterSettleTimer = nullptr;
    QTimer *autoSaveTimer = nullptr;
    bool mobileSplitterSuppressCanvasUpdates = false;
    bool compactUiMode = false;
    QMenu *languageMenu = nullptr;
    QActionGroup *languageActionGroup = nullptr;
    QList<QAction *> pluginToolbarActions;
    QHash<QString, QVariantMap> pluginActionMeta;
    QHash<QString, QPointer<QDialog>> pluginPanelDialogs;
    QSet<QString> batchEditDisabledActions;
    QString pluginToolModePluginId;
    QHash<QString, QAction *> shortcutActions;
    QHash<QString, QKeySequence> shortcutDefaults;
    QList<QString> shortcutActionOrder;

    QString currentChartPath;
    QString sourceChartPath;
    QString workingChartPath;
    bool isModified = false;
};
