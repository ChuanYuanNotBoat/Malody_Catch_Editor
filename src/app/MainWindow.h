#pragma once

#include <QMainWindow>
#include <QVariantMap>

class ChartController;
class SelectionController;
class PlaybackController;
class Skin;
class RightPanel;
class LeftPanel;
class QSplitter;
class QMenu;
class QScrollBar;
class QKeySequence;
class ChartCanvas;
class NoteEditPanel;
class BPMTimePanel;
class MetaEditPanel;
class QToolBar;
class QAction;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(ChartController *chartCtrl,
                        SelectionController *selCtrl,
                        PlaybackController *playCtrl,
                        Skin *skin,
                        QWidget *parent = nullptr);
    ~MainWindow();

    void setSkin(Skin *skin);

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void openChart();
    void openFolder();
    void openImportedLibrary();
    void saveChart();
    void saveChartAs();
    void exportMcz();
    void undo();
    void redo();
    void toggleColorMode(bool on);
    void toggleHyperfruitMode(bool on);
    void toggleVerticalFlip(bool flipped);
    void togglePlayback();
    void changeSkin(const QString &skinName);
    void switchDifficulty();
    void adjustNoteSize();
    void adjustNoteSoundVolume();
    void calibrateSkin();
    void configureOutline();
    void openLogSettings();
    void openPluginManager();
    void triggerPluginToolAction();
    void triggerPluginQuickAction(const QString &pluginId, const QString &actionId);
    void triggerPluginPanelAction();
    void exportDiagnosticsReport();
    void togglePaste288Division(bool enabled);
    void changeNoteSound(const QString &soundPath);
    void changeLanguage();
    void configureShortcuts();

private:
    void setupUi();
    void createMenus();
    void createCentralArea();
    void retranslateUi();
    void populateSkinMenu();
    void populateNoteSoundMenu();
    void populatePluginToolsMenu();
    void populatePluginPanelsMenu();
    void refreshPluginUiExtensions();
    bool runPluginActionWithMeta(const QVariantMap &meta);
    void closePluginPanels(const QString &reasonText = QString());
    bool confirmSaveIfModified(const QString &reasonText);
    void loadChartFile(const QString &filePath);
    QString selectChartFromList(const QList<QPair<QString, QString>> &charts, const QString &title);
    QString selectChartFromLibrary(const QString &libraryRoot, const QString &preferredSong = QString());
    void registerShortcutAction(QAction *action, const QString &actionId, const QKeySequence &defaultShortcut);
    QString beatmapRootPath() const; // Return beatmap root directory.
    void applySidebarTheme();

    class Private;
    Private *d;
};
