// MainWindow.cpp - Main window implementation.
#include "MainWindow.h"
#include "MainWindowPrivate.h"
#include "app/Application.h"
#include "plugin/PluginManager.h"
#include "ui/CustomWidgets/ChartCanvas/ChartCanvas.h"
#include "ui/NoteEditPanel.h"
#include "ui/BPMTimePanel.h"
#include "ui/MetaEditPanel.h"
#include "ui/LeftPanel.h"
#include "ui/dialogs/LogSettingsDialog.h"
#include "controller/ChartController.h"
#include "controller/SelectionController.h"
#include "controller/PlaybackController.h"
#include "audio/AudioPlayer.h"
#include "utils/Settings.h"
#include "utils/Translator.h"
#include "utils/DiagnosticCollector.h"
#include "file/SkinIO.h"
#include "file/ProjectIO.h"
#include "file/ChartIO.h"
#include "model/Skin.h"
#include "utils/Logger.h"
#include "utils/MathUtils.h"
#include <QMenuBar>
#include <QToolBar>
#include <QSplitter>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QSlider>
#include <QSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QColorDialog>
#include <QPainter>
#include <QRadioButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QDesktopServices>
#include <QUrl>
#include <QSysInfo>
#include <QGroupBox>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QInputDialog>
#include <QListWidget>
#include <QScrollBar>
#include <QSet>
#include <QTimer>
#include <algorithm>

namespace
{
PluginManager *activePluginManager()
{
    auto *app = qobject_cast<Application *>(QCoreApplication::instance());
    return app ? app->pluginManager() : nullptr;
}

QColor sidebarTextColorFor(const QColor &bg)
{
    const double r = bg.redF();
    const double g = bg.greenF();
    const double b = bg.blueF();
    const double luminance = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    return (luminance >= 0.5) ? QColor(20, 20, 20) : QColor(245, 245, 245);
}
}

MainWindow::MainWindow(ChartController *chartCtrl,
                       SelectionController *selCtrl,
                       PlaybackController *playCtrl,
                       Skin *skin,
                       QWidget *parent)
    : QMainWindow(parent), d(new Private)
{
    Logger::info("MainWindow constructor called");

    d->chartController = chartCtrl;
    d->selectionController = selCtrl;
    d->playbackController = playCtrl;
    d->skin = skin;
    d->canvas = nullptr;
    d->verticalScrollBar = nullptr;
    d->splitter = nullptr;
    d->rightPanelContainer = nullptr;
    d->currentRightPanel = nullptr;
    d->notePanel = nullptr;
    d->bpmPanel = nullptr;
    d->metaPanel = nullptr;
    d->leftPanel = nullptr;
    d->undoAction = nullptr;
    d->redoAction = nullptr;
    d->colorAction = nullptr;
    d->hyperfruitAction = nullptr;
    d->verticalFlipAction = nullptr;
    d->playAction = nullptr;
    d->speedActionGroup = nullptr;
    d->skinMenu = nullptr;
    d->noteSoundMenu = nullptr;
    d->pluginToolsMenu = nullptr;
    d->pluginPanelsMenu = nullptr;
    d->mainToolBar = nullptr;
    d->languageMenu = nullptr;
    d->languageActionGroup = nullptr;
    d->noteSoundVolumeAction = nullptr;
    d->notePanelAction = nullptr;
    d->bpmPanelAction = nullptr;
    d->metaPanelAction = nullptr;
    d->currentChartPath.clear();
    d->isModified = false;

    setupUi();
    createCentralArea();
    createMenus();

    connect(d->chartController, &ChartController::chartChanged, this, [this]()
            {
        d->isModified = true;
        d->canvas->update();
        d->undoAction->setEnabled(d->chartController->canUndo());
        d->redoAction->setEnabled(d->chartController->canRedo());
        if (d->selectionController) {
            d->selectionController->setNotes(&(d->chartController->chart()->notes()));
            d->selectionController->updateSelectionFromNotes();
        } });
    connect(d->chartController, &ChartController::errorOccurred, this, [this](const QString &msg)
            {
        statusBar()->showMessage(msg, 3000);
        Logger::error("ChartController error: " + msg); });
    connect(d->playbackController, &PlaybackController::errorOccurred, this, [this](const QString &msg)
            {
        statusBar()->showMessage(msg, 3000);
        Logger::error("PlaybackController error: " + msg); });
    connect(d->playbackController, &PlaybackController::speedChanged, this, [this](double speed)
            {
        if (!d->speedActionGroup)
            return;
        for (QAction *action : d->speedActionGroup->actions())
        {
            const double actionSpeed = action->data().toDouble();
            action->setChecked(qFuzzyCompare(actionSpeed, speed));
        } });
    connect(d->leftPanel, &LeftPanel::pluginQuickActionTriggered, this, &MainWindow::triggerPluginQuickAction);
    QTimer::singleShot(0, this, [this]()
                       {
        if (PluginManager *pm = activePluginManager())
        {
            connect(pm, &PluginManager::pluginsChanged, this, &MainWindow::refreshPluginUiExtensions);
            refreshPluginUiExtensions();
        } });

    Logger::info("MainWindow constructor finished");
}

MainWindow::~MainWindow()
{
    delete d->skin;
    delete d;
    Logger::info("MainWindow destroyed");
}

void MainWindow::setupUi()
{
    setWindowTitle(tr("Catch Chart Editor"));
    resize(1200, 800);
    Logger::debug("MainWindow UI setup completed");
}

void MainWindow::createMenus()
{
    Logger::debug("Creating menus...");
    menuBar()->clear();
    if (d->languageActionGroup)
    {
        delete d->languageActionGroup;
        d->languageActionGroup = nullptr;
    }

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *openAction = fileMenu->addAction(tr("&Open Chart..."), this, &MainWindow::openChart);
    openAction->setShortcut(QKeySequence::Open);
    QAction *openFolderAction = fileMenu->addAction(tr("Open &Folder..."), this, &MainWindow::openFolder);
    QAction *saveAction = fileMenu->addAction(tr("&Save"), this, &MainWindow::saveChart);
    saveAction->setShortcut(QKeySequence::Save);
    QAction *saveAsAction = fileMenu->addAction(tr("Save &As..."), this, &MainWindow::saveChartAs);
    fileMenu->addSeparator();
    QAction *exportAction = fileMenu->addAction(tr("&Export .mcz..."), this, &MainWindow::exportMcz);
    fileMenu->addSeparator();
    QAction *switchDifficultyAction = fileMenu->addAction(tr("Switch &Difficulty..."), this, &MainWindow::switchDifficulty);
    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    d->undoAction = editMenu->addAction(tr("&Undo"), this, &MainWindow::undo);
    d->undoAction->setShortcut(QKeySequence::Undo);
    d->redoAction = editMenu->addAction(tr("&Redo"), this, &MainWindow::redo);
    d->redoAction->setShortcut(QKeySequence::Redo);
    editMenu->addSeparator();
    QAction *copyAction = editMenu->addAction(tr("&Copy"));
    connect(copyAction, &QAction::triggered, d->canvas, &ChartCanvas::handleCopy);
    copyAction->setShortcut(QKeySequence::Copy);
    QAction *pasteAction = editMenu->addAction(tr("&Paste"), d->canvas, &ChartCanvas::paste);
    pasteAction->setShortcut(QKeySequence::Paste);
    editMenu->addSeparator();
    QAction *deleteAction = editMenu->addAction(tr("&Delete"));
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this, [this]()
            {
        if (d->selectionController && !d->selectionController->selectedIndices().isEmpty()) {
            QSet<int> selected = d->selectionController->selectedIndices();
            const auto& notes = d->chartController->chart()->notes();
            QList<int> sorted = selected.values();
            std::sort(sorted.begin(), sorted.end(), std::greater<int>());
            QVector<Note> notesToDelete;
            for (int idx : sorted) {
                if (idx >= 0 && idx < notes.size()) {
                    notesToDelete.append(notes[idx]);
                }
            }
            if (!notesToDelete.isEmpty()) {
                d->chartController->removeNotes(notesToDelete);
            }
            d->selectionController->clearSelection();
            Logger::debug("Deleted selected notes via menu");
        } });

    // Paste option: use 288-division conversion.
    editMenu->addSeparator();
    QAction *paste288Action = editMenu->addAction(tr("Paste with 288 Division"));
    paste288Action->setCheckable(true);
    paste288Action->setChecked(Settings::instance().pasteUse288Division());
    connect(paste288Action, &QAction::toggled, this, &MainWindow::togglePaste288Division);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    d->colorAction = viewMenu->addAction(tr("&Color Notes"));
    d->colorAction->setCheckable(true);
    d->colorAction->setChecked(Settings::instance().colorNoteEnabled());
    connect(d->colorAction, &QAction::toggled, this, &MainWindow::toggleColorMode);
    d->hyperfruitAction = viewMenu->addAction(tr("&Hyperfruit Outline"));
    d->hyperfruitAction->setCheckable(true);
    d->hyperfruitAction->setChecked(Settings::instance().hyperfruitOutlineEnabled());
    connect(d->hyperfruitAction, &QAction::toggled, this, &MainWindow::toggleHyperfruitMode);
    d->verticalFlipAction = viewMenu->addAction(tr("&Vertical Flip"));
    d->verticalFlipAction->setCheckable(true);
    d->verticalFlipAction->setChecked(Settings::instance().verticalFlip());
    connect(d->verticalFlipAction, &QAction::toggled, this, &MainWindow::toggleVerticalFlip);

    // Background image toggle.
    QAction *bgImageAction = viewMenu->addAction(tr("Show Background Image"));
    bgImageAction->setCheckable(true);
    bgImageAction->setChecked(Settings::instance().backgroundImageEnabled());
    connect(bgImageAction, &QAction::toggled, this, [this](bool on)
            {
    Settings::instance().setBackgroundImageEnabled(on);
    if (d->canvas) d->canvas->refreshBackground(); });

    QMenu *bgColorMenu = viewMenu->addMenu(tr("Background Color"));
    bgColorMenu->addAction(tr("Black"), [this]()
                           {
    // Use a softer dark tone to keep UI readable and avoid pure-black harshness.
    Settings::instance().setBackgroundColor(QColor(24, 26, 30));
    applySidebarTheme();
    if (d->canvas) d->canvas->refreshBackground(); });
    bgColorMenu->addAction(tr("White"), [this]()
                           {
    Settings::instance().setBackgroundColor(Qt::white);
    applySidebarTheme();
    if (d->canvas) d->canvas->refreshBackground(); });
    bgColorMenu->addAction(tr("Gray"), [this]()
                           {
    Settings::instance().setBackgroundColor(QColor(40, 40, 40));
    applySidebarTheme();
    if (d->canvas) d->canvas->refreshBackground(); });
    bgColorMenu->addAction(tr("Custom..."), [this]()
                           {
    const QColor current = Settings::instance().backgroundColor();
    const QColor picked = QColorDialog::getColor(current, this, tr("Select Background Color"));
    if (!picked.isValid())
        return;
    Settings::instance().setBackgroundColor(picked);
    applySidebarTheme();
    if (d->canvas) d->canvas->refreshBackground(); });

    QMenu *settingsMenu = menuBar()->addMenu(tr("&Settings"));
    d->noteSizeAction = settingsMenu->addAction(tr("Note Size..."));
    connect(d->noteSizeAction, &QAction::triggered, this, &MainWindow::adjustNoteSize);
    d->calibrateSkinAction = settingsMenu->addAction(tr("Calibrate Skin..."));
    connect(d->calibrateSkinAction, &QAction::triggered, this, &MainWindow::calibrateSkin);
    d->outlineAction = settingsMenu->addAction(tr("Outline Settings..."));
    connect(d->outlineAction, &QAction::triggered, this, &MainWindow::configureOutline);
    d->noteSoundVolumeAction = settingsMenu->addAction(tr("Note Sound Volume..."));
    connect(d->noteSoundVolumeAction, &QAction::triggered, this, &MainWindow::adjustNoteSoundVolume);
    settingsMenu->addSeparator();
    d->languageMenu = settingsMenu->addMenu(tr("Language"));
    d->languageActionGroup = new QActionGroup(this);
    d->languageActionGroup->setExclusive(true);

    const QString currentLanguage = Settings::instance().language();
    const auto langs = Translator::instance().availableLanguages();
    for (auto it = langs.cbegin(); it != langs.cend(); ++it)
    {
        QAction *act = d->languageMenu->addAction(it.value());
        act->setCheckable(true);
        act->setData(it.key());
        act->setActionGroup(d->languageActionGroup);
        act->setChecked(it.key() == currentLanguage);
        connect(act, &QAction::triggered, this, &MainWindow::changeLanguage);
    }

    QMenu *playMenu = menuBar()->addMenu(tr("&Playback"));
    d->playAction = playMenu->addAction(tr("&Play/Pause"), this, &MainWindow::togglePlayback);
    d->playAction->setShortcut(Qt::Key_Space);
    playMenu->addSeparator();
    QMenu *speedMenu = playMenu->addMenu(tr("&Speed"));
    d->speedActionGroup = new QActionGroup(this);
    d->speedActionGroup->setExclusive(true);
    for (double sp : {0.25, 0.5, 0.75, 1.0})
    {
        QAction *act = speedMenu->addAction(tr("%1x").arg(sp), [this, sp]()
                                            {
            d->playbackController->setSpeed(sp);
            Settings::instance().setPlaybackSpeed(sp);
            Logger::info(QString("Playback speed set to %1x").arg(sp)); });
        act->setCheckable(true);
        act->setData(sp);
        act->setActionGroup(d->speedActionGroup);
        act->setChecked(qFuzzyCompare(sp, Settings::instance().playbackSpeed()));
    }
    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    QAction *pluginManagerAction = toolsMenu->addAction(tr("&Plugin Manager..."));
    connect(pluginManagerAction, &QAction::triggered, this, &MainWindow::openPluginManager);
    d->pluginToolsMenu = toolsMenu->addMenu(tr("Plugin &Actions"));
    connect(d->pluginToolsMenu, &QMenu::aboutToShow, this, &MainWindow::populatePluginToolsMenu);
    d->pluginPanelsMenu = toolsMenu->addMenu(tr("Plugin &Panels"));
    connect(d->pluginPanelsMenu, &QMenu::aboutToShow, this, &MainWindow::populatePluginPanelsMenu);
    toolsMenu->addSeparator();
    QAction *gridAction = toolsMenu->addAction(tr("&Grid Settings..."), d->canvas, &ChartCanvas::showGridSettings);
    toolsMenu->addSeparator();
    QAction *logSettingsAction = toolsMenu->addAction(tr("&Log Settings..."));
    connect(logSettingsAction, &QAction::triggered, this, &MainWindow::openLogSettings);
    QAction *exportDiagAction = toolsMenu->addAction(tr("&Export Diagnostics Report..."));
    connect(exportDiagAction, &QAction::triggered, this, &MainWindow::exportDiagnosticsReport);

    d->skinMenu = menuBar()->addMenu(tr("&Skin"));
    populateSkinMenu();
    d->noteSoundMenu = menuBar()->addMenu(tr("Note &Sound"));
    populateNoteSoundMenu();
    applySidebarTheme();

    Logger::debug("Menus created");
}

void MainWindow::createCentralArea()
{
    Logger::debug("Creating central area...");

    d->leftPanel = new LeftPanel(this);
    d->leftPanel->setObjectName("leftPanelRoot");
    d->leftPanel->setAttribute(Qt::WA_StyledBackground, true);
    d->leftPanel->setChartController(d->chartController);
    d->leftPanel->setPlaybackController(d->playbackController);

    d->canvas = new ChartCanvas(this);
    d->canvas->setChartController(d->chartController);
    d->canvas->setSelectionController(d->selectionController);
    d->canvas->setPlaybackController(d->playbackController);
    d->canvas->setColorMode(Settings::instance().colorNoteEnabled());
    d->canvas->setHyperfruitEnabled(Settings::instance().hyperfruitOutlineEnabled());
    if (d->skin)
        d->canvas->setSkin(d->skin);
    d->canvas->setNoteSize(Settings::instance().noteSize());
    d->canvas->setNoteSoundVolume(Settings::instance().noteSoundVolume());
    QString noteSoundPath = Settings::instance().noteSoundPath();
    if (!noteSoundPath.isEmpty() && !QFile::exists(noteSoundPath))
    {
        noteSoundPath.clear();
        Settings::instance().setNoteSoundPath(QString());
    }
    d->canvas->setNoteSoundFile(noteSoundPath);
    d->canvas->setNoteSoundEnabled(!noteSoundPath.isEmpty());

    connect(d->canvas, &ChartCanvas::statusMessage, this, [this](const QString &msg)
            { statusBar()->showMessage(msg, 2000); });

    d->leftPanel->setChartCanvas(d->canvas);

    d->verticalScrollBar = new QScrollBar(Qt::Vertical);
    d->verticalScrollBar->setRange(0, 100000);
    d->verticalScrollBar->setValue(0);
    d->verticalScrollBar->setSingleStep(100);
    d->verticalScrollBar->setPageStep(5000);

    QWidget *canvasContainer = new QWidget(this);
    QHBoxLayout *canvasLayout = new QHBoxLayout(canvasContainer);
    canvasLayout->setContentsMargins(0, 0, 0, 0);
    canvasLayout->setSpacing(0);
    canvasLayout->addWidget(d->canvas, 1);
    canvasLayout->addWidget(d->verticalScrollBar, 0);

    auto updateScrollPos = [this](int value)
    {
        if (d->canvas && d->chartController && d->chartController->chart())
        {
            qint64 duration = 300000;
            if (d->playbackController && d->playbackController->audioPlayer())
            {
                duration = d->playbackController->audioPlayer()->duration();
                if (duration <= 0)
                    duration = 300000;
            }
            double scrollPos = (value / 100000.0) * duration;
            d->canvas->setScrollPos(scrollPos);
        }
    };
    connect(d->verticalScrollBar, QOverload<int>::of(&QScrollBar::valueChanged), this, updateScrollPos);

    connect(d->canvas, &ChartCanvas::scrollPositionChanged, this, [this](double beat)
            {
        if (d->playbackController && d->playbackController->audioPlayer()) {
            qint64 duration = d->playbackController->audioPlayer()->duration();
            if (duration > 0 && d->chartController && d->chartController->chart()) {
                const auto& bpmList = d->chartController->chart()->bpmList();
                int offset = d->chartController->chart()->meta().offset;
                int beatNum, numerator, denominator;
                MathUtils::floatToBeat(beat, beatNum, numerator, denominator);
                double timeMs = MathUtils::beatToMs(beatNum, numerator, denominator, bpmList, offset);
                int value = static_cast<int>((timeMs / duration) * 100000);
                d->verticalScrollBar->blockSignals(true);
                d->verticalScrollBar->setValue(value);
                d->verticalScrollBar->blockSignals(false);
            }
        } });

    if (d->playbackController && d->playbackController->audioPlayer())
    {
        connect(d->playbackController->audioPlayer(), &AudioPlayer::durationChanged, this, [this](qint64 duration)
                {
            if (duration > 0) {
                Logger::debug(QString("Audio duration changed: %1 ms").arg(duration));
                d->verticalScrollBar->setValue(0);
            } });
    }

    d->rightPanelContainer = new QWidget(this);
    d->rightPanelContainer->setObjectName("rightPanelRoot");
    d->rightPanelContainer->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout *rightLayout = new QVBoxLayout(d->rightPanelContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    d->notePanel = new NoteEditPanel(d->rightPanelContainer);
    d->bpmPanel = new BPMTimePanel(d->rightPanelContainer);
    d->metaPanel = new MetaEditPanel(d->rightPanelContainer);
    d->currentRightPanel = d->notePanel;
    rightLayout->addWidget(d->notePanel);
    rightLayout->addWidget(d->bpmPanel);
    rightLayout->addWidget(d->metaPanel);
    d->bpmPanel->setVisible(false);
    d->metaPanel->setVisible(false);

    d->notePanel->setChartController(d->chartController);
    d->notePanel->setSelectionController(d->selectionController);
    d->bpmPanel->setChartController(d->chartController);
    d->metaPanel->setChartController(d->chartController);

    // Connect NoteEditPanel signals.
    connect(d->notePanel, &NoteEditPanel::timeDivisionChanged, d->canvas, &ChartCanvas::setTimeDivision);
    connect(d->notePanel, &NoteEditPanel::gridDivisionChanged, d->canvas, &ChartCanvas::setGridDivision);
    connect(d->notePanel, &NoteEditPanel::gridSnapChanged, d->canvas, [this](bool on)
            {
        Logger::info(QString("[Grid] MainWindow::gridSnapChanged signal received: %1").arg(on));
        d->canvas->setGridSnap(on); });
    connect(d->notePanel, &NoteEditPanel::modeChanged, d->canvas, [this](int mode)
            { d->canvas->setMode(static_cast<ChartCanvas::Mode>(mode)); });
    connect(d->notePanel, &NoteEditPanel::copyRequested, d->canvas, &ChartCanvas::handleCopy);

    d->splitter = new QSplitter(Qt::Horizontal, this);
    d->splitter->addWidget(d->leftPanel);
    d->splitter->addWidget(canvasContainer);
    d->splitter->addWidget(d->rightPanelContainer);
    d->splitter->setSizes({150, 800, 300});
    setCentralWidget(d->splitter);

    d->mainToolBar = addToolBar(tr("Tools"));
    d->notePanelAction = d->mainToolBar->addAction(tr("Note"), [this]()
                                                   {
        d->notePanel->setVisible(true);
        d->bpmPanel->setVisible(false);
        d->metaPanel->setVisible(false);
        d->currentRightPanel = d->notePanel; });
    d->bpmPanelAction = d->mainToolBar->addAction(tr("BPM"), [this]()
                                                  {
        d->notePanel->setVisible(false);
        d->bpmPanel->setVisible(true);
        d->metaPanel->setVisible(false);
        d->currentRightPanel = d->bpmPanel; });
    d->metaPanelAction = d->mainToolBar->addAction(tr("Meta"), [this]()
                                                   {
        d->notePanel->setVisible(false);
        d->bpmPanel->setVisible(false);
        d->metaPanel->setVisible(true);
        d->currentRightPanel = d->metaPanel; });
    applySidebarTheme();

    Logger::debug("Central area created with LeftPanel.");
}

// ==================== beatmap root path ====================
QString MainWindow::beatmapRootPath() const
{
    return QCoreApplication::applicationDirPath() + "/beatmap";
}

bool MainWindow::confirmSaveIfModified(const QString &reasonText)
{
    if (!d->isModified)
        return true;

    QMessageBox::StandardButton choice = QMessageBox::warning(
        this,
        tr("Unsaved Changes"),
        tr("Current chart has unsaved changes.\n%1\nDo you want to save before continuing?")
            .arg(reasonText),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (choice == QMessageBox::Cancel)
        return false;

    if (choice == QMessageBox::Discard)
        return true;

    QString savePath = d->currentChartPath;
    if (savePath.isEmpty())
    {
        savePath = QFileDialog::getSaveFileName(
            this,
            tr("Save Chart As"),
            Settings::instance().lastOpenPath(),
            tr("Malody Catch Chart (*.mc);;All Files (*.*)"));
        if (savePath.isEmpty())
            return false;
    }

    if (!d->chartController->saveChart(savePath))
    {
        QMessageBox::critical(this, tr("Error"), tr("Failed to save chart."));
        return false;
    }

    d->currentChartPath = savePath;
    Settings::instance().setLastOpenPath(QFileInfo(savePath).absolutePath());
    d->isModified = false;
    statusBar()->showMessage(tr("Saved: %1").arg(savePath), 2000);
    if (PluginManager *pm = activePluginManager())
        pm->notifyChartSaved(savePath);
    return true;
}

// ==================== Open chart file (.mc/.mcz) ====================
void MainWindow::openChart()
{
    QString startDir = Settings::instance().lastProjectPath();
    if (startDir.isEmpty() || !QDir(startDir).exists())
    {
        startDir = beatmapRootPath();
    }

    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Chart"), startDir,
                                                    tr("Malody Catch Chart (*.mc *.mcz);;All Files (*.*)"));
    if (fileName.isEmpty())
        return;

    loadChartFile(fileName);
}

// ==================== Open folder ====================
void MainWindow::openFolder()
{
    QString startDir = Settings::instance().lastProjectPath();
    if (startDir.isEmpty() || !QDir(startDir).exists())
    {
        startDir = beatmapRootPath();
    }

    QString dirPath = QFileDialog::getExistingDirectory(this, tr("Open Folder"), startDir);
    if (dirPath.isEmpty())
        return;

    Settings::instance().setLastProjectPath(dirPath);
    Logger::info(QString("Opening folder: %1").arg(dirPath));

    QList<QPair<QString, QString>> charts = ProjectIO::findChartsInDirectory(dirPath);
    if (charts.isEmpty())
    {
        QMessageBox::information(this, tr("No Charts"), tr("No .mc files found in the selected folder."));
        return;
    }

    QString selectedPath = selectChartFromList(charts, tr("Select Chart in Folder"));
    if (selectedPath.isEmpty())
        return;

    loadChartFile(selectedPath);
}

// ==================== Load chart core logic ====================
void MainWindow::loadChartFile(const QString &filePath)
{
    Logger::info(QString("Loading chart file: %1").arg(filePath));

    QString actualChartPath = filePath;
    QFileInfo fi(filePath);
    if (fi.suffix().toLower() == "mcz")
    {
        QString beatmapDir = beatmapRootPath();
        QString targetDir = beatmapDir + "/" + fi.completeBaseName();

        QDir().mkpath(beatmapDir);

        QString extractedDir;
        if (!ProjectIO::extractMcz(filePath, targetDir, extractedDir))
        {
            QMessageBox::critical(this, tr("Error"), tr("Failed to extract MCZ file."));
            return;
        }

        QList<QPair<QString, QString>> charts = ProjectIO::findChartsInDirectory(extractedDir);
        if (charts.isEmpty())
        {
            QMessageBox::critical(this, tr("Error"), tr("No .mc files found in the extracted content."));
            return;
        }

        actualChartPath = selectChartFromList(charts, tr("Select Chart from MCZ"));
        if (actualChartPath.isEmpty())
            return;

        Settings::instance().setLastProjectPath(beatmapDir);
    }

    if (!d->chartController->loadChart(actualChartPath))
    {
        QMessageBox::critical(this, tr("Error"), tr("Failed to load chart."));
        return;
    }

    d->currentChartPath = actualChartPath;
    Settings::instance().setLastOpenPath(QFileInfo(actualChartPath).absolutePath());

    if (QFileInfo(filePath).suffix().toLower() != "mcz")
    {
        Settings::instance().setLastProjectPath(QFileInfo(actualChartPath).absolutePath());
    }

    QString chartDir = QFileInfo(actualChartPath).absolutePath();
    QString audioFile = d->chartController->chart()->meta().audioFile;
    if (!audioFile.isEmpty())
    {
        QString audioPath = chartDir + "/" + audioFile;
        if (QFile::exists(audioPath))
        {
            d->playbackController->audioPlayer()->load(audioPath);
        }
    }

    d->canvas->update();
    d->isModified = false;
    statusBar()->showMessage(tr("Loaded: %1").arg(QFileInfo(actualChartPath).fileName()), 3000);
}

QString MainWindow::selectChartFromList(const QList<QPair<QString, QString>> &charts, const QString &title)
{
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setMinimumWidth(350);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("Select a chart:")));

    QListWidget *list = new QListWidget();
    for (const auto &chart : charts)
    {
        QString display = chart.second;
        QListWidgetItem *item = new QListWidgetItem(display, list);
        item->setData(Qt::UserRole, chart.first);
        item->setToolTip(chart.first);
    }
    layout->addWidget(list);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted || list->currentItem() == nullptr)
        return QString();

    return list->currentItem()->data(Qt::UserRole).toString();
}

void MainWindow::switchDifficulty()
{
    if (!d->chartController || !d->chartController->chart())
    {
        QMessageBox::information(this, tr("No Chart"), tr("No chart is currently open."));
        return;
    }

    if (!confirmSaveIfModified(tr("Switching difficulty will replace the current chart in editor.")))
        return;

    QString currentDir = QFileInfo(d->currentChartPath).absolutePath();
    QList<QPair<QString, QString>> charts = ProjectIO::findChartsInDirectory(currentDir);
    if (charts.size() <= 1)
    {
        QMessageBox::information(this, tr("No Other Charts"), tr("No other difficulties found in this directory."));
        return;
    }

    QList<QPair<QString, QString>> otherCharts;
    for (const auto &chart : charts)
    {
        if (chart.first != d->currentChartPath)
            otherCharts.append(chart);
    }
    if (otherCharts.isEmpty())
    {
        QMessageBox::information(this, tr("No Other Charts"), tr("No other difficulties found."));
        return;
    }

    QString newPath = selectChartFromList(otherCharts, tr("Switch Difficulty"));
    if (newPath.isEmpty())
        return;

    loadChartFile(newPath);
}

void MainWindow::saveChart()
{
    Logger::info("Save chart requested");
    QString currentPath = Settings::instance().lastOpenPath() + "/" + d->chartController->chart()->meta().difficulty + ".mc";
    if (d->chartController->saveChart(currentPath))
    {
        d->currentChartPath = currentPath;
        d->isModified = false;
        statusBar()->showMessage(tr("Saved: %1").arg(currentPath), 2000);
        Logger::info("Chart saved: " + currentPath);
        if (PluginManager *pm = activePluginManager())
            pm->notifyChartSaved(currentPath);
    }
    else
    {
        Logger::error("Failed to save chart: " + currentPath);
        QMessageBox::critical(this, tr("Error"), tr("Failed to save chart."));
    }
}

void MainWindow::saveChartAs()
{
    Logger::info("Save chart as requested");
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Chart As"), Settings::instance().lastOpenPath(),
                                                    tr("Malody Catch Chart (*.mc);;All Files (*.*)"));
    if (fileName.isEmpty())
    {
        Logger::debug("Save as cancelled");
        return;
    }
    if (d->chartController->saveChart(fileName))
    {
        d->currentChartPath = fileName;
        d->isModified = false;
        statusBar()->showMessage(tr("Saved: %1").arg(fileName), 2000);
        Logger::info("Chart saved as: " + fileName);
        if (PluginManager *pm = activePluginManager())
            pm->notifyChartSaved(fileName);
    }
    else
    {
        Logger::error("Failed to save chart as: " + fileName);
        QMessageBox::critical(this, tr("Error"), tr("Failed to save chart."));
    }
}

void MainWindow::exportMcz()
{
    Logger::info("Export .mcz requested");

    if (d->currentChartPath.isEmpty())
    {
        QMessageBox::information(this, tr("No Chart"), tr("Please open a chart first before exporting."));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, tr("Export .mcz"), Settings::instance().lastOpenPath(),
                                                    tr("Malody Catch Pack (*.mcz);;All Files (*.*)"));
    if (fileName.isEmpty())
    {
        Logger::debug("Export .mcz cancelled");
        return;
    }

    try
    {
        Logger::info(QString("MainWindow::exportMcz - Exporting to: %1").arg(fileName));

        if (ProjectIO::exportToMcz(fileName, d->currentChartPath))
        {
            statusBar()->showMessage(tr("Exported: %1").arg(fileName), 3000);
            Logger::info(QString("MainWindow::exportMcz - Successfully exported to: %1").arg(fileName));
            QMessageBox::information(this, tr("Success"), tr("Chart exported successfully to:\n%1").arg(fileName));
        }
        else
        {
            Logger::error(QString("MainWindow::exportMcz - Failed to export to: %1").arg(fileName));
            QMessageBox::critical(this, tr("Error"), tr("Failed to export chart to MCZ format."));
        }
    }
    catch (const std::exception &e)
    {
        Logger::error(QString("MainWindow::exportMcz - Exception: %1").arg(e.what()));
        QMessageBox::critical(this, tr("Error"), tr("Exception during export: %1").arg(e.what()));
    }
    catch (...)
    {
        Logger::error("MainWindow::exportMcz - Unknown exception");
        QMessageBox::critical(this, tr("Error"), tr("Unknown exception during export."));
    }
}

// ==================== Undo / Redo ====================
void MainWindow::undo()
{
    if (d->chartController)
    {
        Logger::debug("Undo triggered");
        d->chartController->undo();
    }
}

void MainWindow::redo()
{
    if (d->chartController)
    {
        Logger::debug("Redo triggered");
        d->chartController->redo();
    }
}

void MainWindow::toggleColorMode(bool on)
{
    Logger::info(QString("Color mode toggled to %1").arg(on));
    Settings::instance().setColorNoteEnabled(on);
    d->canvas->setColorMode(on);
}

void MainWindow::toggleHyperfruitMode(bool on)
{
    Logger::info(QString("Hyperfruit mode toggled to %1").arg(on));
    Settings::instance().setHyperfruitOutlineEnabled(on);
    d->canvas->setHyperfruitEnabled(on);
}

void MainWindow::toggleVerticalFlip(bool flipped)
{
    Logger::info(QString("Vertical flip toggled to %1").arg(flipped));
    Settings::instance().setVerticalFlip(flipped);
    d->canvas->setVerticalFlip(flipped);
}

void MainWindow::togglePlayback()
{
    if (d->playbackController->state() == PlaybackController::Playing)
    {
        Logger::debug("Playback paused");
        d->playbackController->pause();
    }
    else
    {
        Logger::debug("Playback started");
        double startTime = d->canvas->currentPlayTime();
        const Chart *chart = d->chartController->chart();
        if (chart)
        {
            const QVector<BpmEntry> &bpmList = chart->bpmList();
            int offset = chart->meta().offset;
            int timeDivision = d->canvas ? d->canvas->timeDivision() : 4;
            startTime = MathUtils::snapTimeToGrid(startTime, bpmList, offset, timeDivision);
        }
        d->playbackController->playFromTime(startTime);
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::retranslateUi()
{
    setWindowTitle(tr("Catch Chart Editor"));
    if (d->mainToolBar)
    {
        d->mainToolBar->setWindowTitle(tr("Tools"));
    }
    if (d->notePanelAction)
        d->notePanelAction->setText(tr("Note"));
    if (d->bpmPanelAction)
        d->bpmPanelAction->setText(tr("BPM"));
    if (d->metaPanelAction)
        d->metaPanelAction->setText(tr("Meta"));
    if (d->leftPanel)
        d->leftPanel->retranslateUi();
    if (d->notePanel)
        d->notePanel->retranslateUi();
    if (d->bpmPanel)
        d->bpmPanel->retranslateUi();
    if (d->metaPanel)
        d->metaPanel->retranslateUi();
    if (d->skinMenu)
        d->skinMenu->setTitle(tr("&Skin"));
    if (d->noteSoundMenu)
        d->noteSoundMenu->setTitle(tr("Note &Sound"));
    populateSkinMenu();
    populateNoteSoundMenu();
    populatePluginToolsMenu();
    applySidebarTheme();
    Logger::debug("UI retranslated");
}

// ==================== Paste 288 division option slot ====================
void MainWindow::togglePaste288Division(bool enabled)
{
    Settings::instance().setPasteUse288Division(enabled);
    Logger::info(QString("Paste 288 division: %1").arg(enabled ? "enabled" : "disabled"));
}

void MainWindow::changeLanguage()
{
    QAction *act = qobject_cast<QAction *>(sender());
    if (!act)
        return;

    const QString languageCode = act->data().toString();
    const QString languageName = act->text();
    if (languageCode.isEmpty() || languageCode == Settings::instance().language())
        return;

    if (!Translator::instance().setLanguage(languageCode))
    {
        QMessageBox::warning(this, tr("Language"), tr("Failed to load language pack: %1").arg(languageCode));
        return;
    }

    Settings::instance().setLanguage(languageCode);

    QTimer::singleShot(0, this, [this]()
                       {
        if (PluginManager *pm = activePluginManager())
        {
            pm->reloadPlugins();
            refreshPluginUiExtensions();
        } });

    statusBar()->showMessage(tr("Language changed to %1").arg(languageName), 2000);
}

void MainWindow::applySidebarTheme()
{
    const QColor bg = Settings::instance().backgroundColor();
    const QColor fg = sidebarTextColorFor(bg);
    const bool darkTheme = (fg.lightness() > 128);
    const QColor panelBg = darkTheme ? bg.lighter(108) : bg.darker(103);
    const QColor panelInputBg = darkTheme ? panelBg.lighter(120) : panelBg.darker(105);
    const QColor panelButtonBg = darkTheme ? panelBg.lighter(132) : panelBg.darker(112);
    const QColor panelBorder = darkTheme ? panelBg.lighter(165) : panelBg.darker(145);
    const QColor panelDisabledText = darkTheme ? QColor("#9A9A9A") : QColor("#707070");

    auto applyPanelStyle = [&](QWidget *panel, const QString &rootName)
    {
        if (!panel)
            return;

        const QString css = QString(
                                "QWidget#%7 { background-color: %1; color: %2; border: 1px solid %4; }"
                                "QLabel, QCheckBox, QRadioButton, QGroupBox { color: %2; }"
                                "QLineEdit, QAbstractSpinBox, QComboBox, QListWidget, QTextEdit, QPlainTextEdit {"
                                "  background-color: %3; color: %2; border: 1px solid %4; }"
                                "QAbstractItemView { background-color: %3; color: %2; border: 1px solid %4; selection-background-color: %5; selection-color: %6; }"
                                "QPushButton { background-color: %5; color: %2; border: 1px solid %4; padding: 3px 6px; }"
                                "QPushButton:disabled { color: %6; }"
                                "QScrollBar:vertical, QScrollBar:horizontal { background-color: %1; }")
                                .arg(panelBg.name(), fg.name(), panelInputBg.name(), panelBorder.name(), panelButtonBg.name(),
                                     panelDisabledText.name(), rootName);

        panel->setStyleSheet(css);
    };

    applyPanelStyle(d->leftPanel, "leftPanelRoot");
    applyPanelStyle(d->rightPanelContainer, "rightPanelRoot");

    if (menuBar())
    {
        const QString menuCss = QString(
                                    "QMenuBar { background-color: %1; color: %2; }"
                                    "QMenuBar::item { background: transparent; color: %2; padding: 4px 8px; }"
                                    "QMenuBar::item:selected { background: %3; }"
                                    "QMenu { background-color: %1; color: %2; border: 1px solid %4; }"
                                    "QMenu::item:selected { background-color: %3; }")
                                    .arg(bg.name(), fg.name(), panelButtonBg.name(), panelBorder.name());
        menuBar()->setStyleSheet(menuCss);
    }

    if (d->mainToolBar)
    {
        const QString toolbarCss = QString(
                                       "QToolBar { background-color: %1; color: %2; border-bottom: 1px solid %4; border-top: 1px solid %4; spacing: 6px; padding: 2px 4px; }"
                                       "QToolButton { background-color: %5; color: %2; border: 1px solid %4; padding: 3px 8px; }"
                                       "QToolButton:hover { background-color: %3; }")
                                       .arg(panelBg.name(), fg.name(), panelInputBg.name(), panelBorder.name(), panelButtonBg.name());
        d->mainToolBar->setStyleSheet(toolbarCss);
    }

    if (statusBar())
    {
        statusBar()->setStyleSheet(QString("QStatusBar { background-color: %1; color: %2; border-top: 1px solid %3; }")
                                       .arg(panelBg.name(), fg.name(), panelBorder.name()));
    }

    if (d->splitter)
    {
        d->splitter->setStyleSheet(QString("QSplitter::handle { background-color: %1; }").arg(panelBorder.name()));
    }

    if (d->verticalScrollBar)
    {
        const QString scrollCss = QString(
                                      "QScrollBar:vertical { background: %1; width: 12px; margin: 0; }"
                                      "QScrollBar::handle:vertical { background: %2; min-height: 24px; border-radius: 5px; }"
                                      "QScrollBar::handle:vertical:hover { background: %3; }"
                                      "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
                                      "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
                                      .arg(panelBg.name(), panelButtonBg.name(), panelInputBg.name());
        d->verticalScrollBar->setStyleSheet(scrollCss);
    }
}



