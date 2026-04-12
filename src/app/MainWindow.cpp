// MainWindow.cpp - 支持区间复制和右键粘贴
#include "MainWindow.h"
#include "ui/CustomWidgets/ChartCanvas.h"
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
#include <QApplication>
#include <QFileInfo>
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
#include <algorithm>

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
    QMenu *skinMenu;
    QAction *noteSizeAction;
    QAction *calibrateSkinAction;
    QAction *outlineAction;

    QString currentChartPath;
};

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
    d->leftPanel = nullptr;
    d->currentChartPath.clear();

    setupUi();
    createCentralArea();
    createMenus();
    retranslateUi();

    connect(d->chartController, &ChartController::chartChanged, this, [this]()
            {
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

    Logger::info("MainWindow constructor finished");
}

MainWindow::~MainWindow()
{
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

    // 文件菜单
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

    // 编辑菜单
    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    d->undoAction = editMenu->addAction(tr("&Undo"), this, &MainWindow::undo);
    d->undoAction->setShortcut(QKeySequence::Undo);
    d->redoAction = editMenu->addAction(tr("&Redo"), this, &MainWindow::redo);
    d->redoAction->setShortcut(QKeySequence::Redo);
    editMenu->addSeparator();
    QAction *copyAction = editMenu->addAction(tr("&Copy"));
    // 连接到画布的 handleCopy 方法
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

    // 粘贴时使用288分度选项
    editMenu->addSeparator();
    QAction *paste288Action = editMenu->addAction(tr("Paste with 288 Division"));
    paste288Action->setCheckable(true);
    paste288Action->setChecked(Settings::instance().pasteUse288Division());
    connect(paste288Action, &QAction::toggled, this, &MainWindow::togglePaste288Division);

    // 视图菜单
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

    // 背景图片开关
    QAction *bgImageAction = viewMenu->addAction(tr("Show Background Image"));
    bgImageAction->setCheckable(true);
    bgImageAction->setChecked(Settings::instance().backgroundImageEnabled());
    connect(bgImageAction, &QAction::toggled, this, [this](bool on)
            {
    Settings::instance().setBackgroundImageEnabled(on);
    d->canvas->update(); });

    // 背景色子菜单
    QMenu *bgColorMenu = viewMenu->addMenu(tr("Background Color"));
    bgColorMenu->addAction(tr("Black"), [this]()
                           {
    Settings::instance().setBackgroundColor(Qt::black);
    d->canvas->update(); });
    bgColorMenu->addAction(tr("White"), [this]()
                           {
    Settings::instance().setBackgroundColor(Qt::white);
    d->canvas->update(); });
    bgColorMenu->addAction(tr("Gray"), [this]()
                           {
    Settings::instance().setBackgroundColor(QColor(40, 40, 40));
    d->canvas->update(); });

    // 设置菜单
    QMenu *settingsMenu = menuBar()->addMenu(tr("&Settings"));
    d->noteSizeAction = settingsMenu->addAction(tr("Note Size..."));
    connect(d->noteSizeAction, &QAction::triggered, this, &MainWindow::adjustNoteSize);
    d->calibrateSkinAction = settingsMenu->addAction(tr("Calibrate Skin..."));
    connect(d->calibrateSkinAction, &QAction::triggered, this, &MainWindow::calibrateSkin);
    d->outlineAction = settingsMenu->addAction(tr("Outline Settings..."));
    connect(d->outlineAction, &QAction::triggered, this, &MainWindow::configureOutline);

    // 播放菜单
    QMenu *playMenu = menuBar()->addMenu(tr("&Playback"));
    d->playAction = playMenu->addAction(tr("&Play/Pause"), this, &MainWindow::togglePlayback);
    d->playAction->setShortcut(Qt::Key_Space);
    playMenu->addSeparator();
    QMenu *speedMenu = playMenu->addMenu(tr("&Speed"));
    for (double sp : {0.25, 0.5, 0.75, 1.0})
    {
        QAction *act = speedMenu->addAction(tr("%1x").arg(sp), [this, sp]()
                                            {
            d->playbackController->setSpeed(sp);
            Settings::instance().setPlaybackSpeed(sp);
            Logger::info(QString("Playback speed set to %1x").arg(sp)); });
        act->setCheckable(true);
        act->setChecked(qFuzzyCompare(sp, Settings::instance().playbackSpeed()));
    }

    // 工具菜单
    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    QAction *gridAction = toolsMenu->addAction(tr("&Grid Settings..."), d->canvas, &ChartCanvas::showGridSettings);
    toolsMenu->addSeparator();
    QAction *logSettingsAction = toolsMenu->addAction(tr("&Log Settings..."));
    connect(logSettingsAction, &QAction::triggered, this, &MainWindow::openLogSettings);
    QAction *exportDiagAction = toolsMenu->addAction(tr("&Export Diagnostics Report..."));
    connect(exportDiagAction, &QAction::triggered, this, &MainWindow::exportDiagnosticsReport);

    // 皮肤菜单
    d->skinMenu = menuBar()->addMenu(tr("&Skin"));
    populateSkinMenu();

    Logger::debug("Menus created");
}

void MainWindow::createCentralArea()
{
    Logger::debug("Creating central area...");

    d->leftPanel = new LeftPanel(this);
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

    // 连接画布的状态栏消息信号
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

    // 连接 NoteEditPanel 的信号
    connect(d->notePanel, &NoteEditPanel::timeDivisionChanged, d->canvas, &ChartCanvas::setTimeDivision);
    connect(d->notePanel, &NoteEditPanel::gridDivisionChanged, d->canvas, &ChartCanvas::setGridDivision);
    connect(d->notePanel, &NoteEditPanel::gridSnapChanged, d->canvas, [this](bool on)
            {
        Logger::info(QString("[Grid] MainWindow::gridSnapChanged signal received: %1").arg(on));
        d->canvas->setGridSnap(on); });
    connect(d->notePanel, &NoteEditPanel::modeChanged, d->canvas, [this](int mode)
            { d->canvas->setMode(static_cast<ChartCanvas::Mode>(mode)); });
    // 连接复制请求信号
    connect(d->notePanel, &NoteEditPanel::copyRequested, d->canvas, &ChartCanvas::handleCopy);

    d->splitter = new QSplitter(Qt::Horizontal, this);
    d->splitter->addWidget(d->leftPanel);
    d->splitter->addWidget(canvasContainer);
    d->splitter->addWidget(d->rightPanelContainer);
    d->splitter->setSizes({150, 800, 300});
    setCentralWidget(d->splitter);

    QToolBar *toolBar = addToolBar(tr("Tools"));
    toolBar->addAction(tr("Note"), [this]()
                       {
        d->notePanel->setVisible(true);
        d->bpmPanel->setVisible(false);
        d->metaPanel->setVisible(false);
        d->currentRightPanel = d->notePanel; });
    toolBar->addAction(tr("BPM"), [this]()
                       {
        d->notePanel->setVisible(false);
        d->bpmPanel->setVisible(true);
        d->metaPanel->setVisible(false);
        d->currentRightPanel = d->bpmPanel; });
    toolBar->addAction(tr("Meta"), [this]()
                       {
        d->notePanel->setVisible(false);
        d->bpmPanel->setVisible(false);
        d->metaPanel->setVisible(true);
        d->currentRightPanel = d->metaPanel; });

    Logger::debug("Central area created with LeftPanel.");
}

// ==================== beatmap 根目录 ====================
QString MainWindow::beatmapRootPath() const
{
    return QCoreApplication::applicationDirPath() + "/beatmap";
}

// ==================== 打开文件（单个 .mc 或 .mcz） ====================
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

// ==================== 打开文件夹 ====================
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

// ==================== 加载谱面（核心逻辑） ====================
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
    statusBar()->showMessage(tr("Loaded: %1").arg(QFileInfo(actualChartPath).fileName()), 3000);
}

// ==================== 从列表中选择谱面 ====================
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

// ==================== 切换难度 ====================
void MainWindow::switchDifficulty()
{
    if (!d->chartController || !d->chartController->chart())
    {
        QMessageBox::information(this, tr("No Chart"), tr("No chart is currently open."));
        return;
    }

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

// ==================== 保存谱面 ====================
void MainWindow::saveChart()
{
    Logger::info("Save chart requested");
    QString currentPath = Settings::instance().lastOpenPath() + "/" + d->chartController->chart()->meta().difficulty + ".mc";
    if (d->chartController->saveChart(currentPath))
    {
        statusBar()->showMessage(tr("Saved: %1").arg(currentPath), 2000);
        Logger::info("Chart saved: " + currentPath);
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
        statusBar()->showMessage(tr("Saved: %1").arg(fileName), 2000);
        Logger::info("Chart saved as: " + fileName);
    }
    else
    {
        Logger::error("Failed to save chart as: " + fileName);
        QMessageBox::critical(this, tr("Error"), tr("Failed to save chart."));
    }
}

// ==================== 导出 MCZ ====================
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

// ==================== 视图模式切换 ====================
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

// ==================== 播放控制 ====================
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
            int timeDivision = 4;
            startTime = MathUtils::snapTimeToGrid(startTime, bpmList, offset, timeDivision);
        }
        d->playbackController->playFromTime(startTime);
    }
}

// ==================== 界面翻译 ====================
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
    populateSkinMenu();
    Logger::debug("UI retranslated");
}

// ==================== 日志设置 ====================
void MainWindow::openLogSettings()
{
    Logger::info("Log settings dialog opened");
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Log Settings"));
    dialog.setMinimumSize(400, 300);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QCheckBox *jsonLoggingCheck = new QCheckBox(tr("Enable JSON Logging"));
    jsonLoggingCheck->setChecked(Logger::isJsonLoggingEnabled());
    layout->addWidget(jsonLoggingCheck);

    QCheckBox *verboseCheck = new QCheckBox(tr("Enable Verbose Logging"));
    verboseCheck->setChecked(Logger::isVerbose());
    layout->addWidget(verboseCheck);

    QHBoxLayout *logPathLayout = new QHBoxLayout;
    QLabel *pathLabel = new QLabel(tr("Log File:"));
    QLineEdit *pathEdit = new QLineEdit;
    pathEdit->setText(Logger::logFilePath());
    pathEdit->setReadOnly(true);
    QPushButton *openLogBtn = new QPushButton(tr("Open Log Folder"));
    connect(openLogBtn, &QPushButton::clicked, [this]()
            {
        QString logDir = QFileInfo(Logger::logFilePath()).absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(logDir));
        Logger::debug("Opened log folder"); });
    logPathLayout->addWidget(pathLabel);
    logPathLayout->addWidget(pathEdit);
    logPathLayout->addWidget(openLogBtn);
    layout->addLayout(logPathLayout);

    QHBoxLayout *jsonPathLayout = new QHBoxLayout;
    QLabel *jsonPathLabel = new QLabel(tr("JSON Log File:"));
    QLineEdit *jsonPathEdit = new QLineEdit;
    jsonPathEdit->setText(Logger::jsonLogFilePath());
    jsonPathEdit->setReadOnly(true);
    jsonPathLayout->addWidget(jsonPathLabel);
    jsonPathLayout->addWidget(jsonPathEdit);
    layout->addLayout(jsonPathLayout);

    layout->addStretch();

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]()
            {
        Logger::setJsonLoggingEnabled(jsonLoggingCheck->isChecked());
        Logger::setVerbose(verboseCheck->isChecked());
        Logger::info(QString("Log settings changed - JSON logging: %1, Verbose: %2")
                     .arg(jsonLoggingCheck->isChecked() ? "enabled" : "disabled")
                     .arg(verboseCheck->isChecked() ? "enabled" : "disabled"));
        dialog.accept(); });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.exec();
}

// ==================== 导出诊断报告 ====================
void MainWindow::exportDiagnosticsReport()
{
    Logger::info("Diagnostics report export requested");
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export Diagnostics Report"),
                                                    Settings::instance().lastOpenPath(),
                                                    tr("Text Files (*.txt);;JSON Files (*.json);;All Files (*.*)"));
    if (fileName.isEmpty())
    {
        Logger::debug("Export diagnostics cancelled");
        return;
    }

    try
    {
        DiagnosticCollector &collector = DiagnosticCollector::instance();
        DiagnosticCollector::DiagnosticReport report = collector.generateReport();

        if (fileName.endsWith(".json"))
        {
            QJsonDocument doc = collector.toJsonDocument();
            QFile file(fileName);
            if (file.open(QIODevice::WriteOnly))
            {
                file.write(doc.toJson());
                file.close();
                Logger::info("Diagnostics report exported to JSON: " + fileName);
                QMessageBox::information(this, tr("Export Successful"),
                                         tr("Diagnostics report exported to:\n%1").arg(fileName));
            }
            else
            {
                Logger::error("Failed to open file for writing: " + fileName);
                QMessageBox::warning(this, tr("Export Failed"), tr("Failed to open file for writing."));
            }
        }
        else
        {
            QFile file(fileName);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                QTextStream stream(&file);
                stream << report.toFormattedString();
                file.close();
                Logger::info("Diagnostics report exported to text: " + fileName);
                QMessageBox::information(this, tr("Export Successful"),
                                         tr("Diagnostics report exported to:\n%1").arg(fileName));
            }
            else
            {
                Logger::error("Failed to open file for writing: " + fileName);
                QMessageBox::warning(this, tr("Export Failed"), tr("Failed to open file for writing."));
            }
        }
    }
    catch (const std::exception &e)
    {
        Logger::error(QString("Exception during diagnostics export: %1").arg(e.what()));
        QMessageBox::critical(this, tr("Error"), tr("Exception during export:\n%1").arg(e.what()));
    }
}

// ==================== 音符大小调整 ====================
void MainWindow::adjustNoteSize()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Note Size"));
    dialog.setMinimumSize(400, 300);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QHBoxLayout *sizeLayout = new QHBoxLayout;
    QLabel *label = new QLabel(tr("Size (pixels):"));
    QSpinBox *sizeSpin = new QSpinBox;
    sizeSpin->setRange(8, 64);
    sizeSpin->setValue(Settings::instance().noteSize());
    sizeLayout->addWidget(label);
    sizeLayout->addWidget(sizeSpin);
    layout->addLayout(sizeLayout);

    QLabel *previewLabel = new QLabel;
    previewLabel->setFixedSize(128, 128);
    previewLabel->setStyleSheet("border: 1px solid gray; background: white;");
    previewLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(previewLabel, 0, Qt::AlignCenter);

    auto updatePreview = [previewLabel, sizeSpin, this]()
    {
        int sz = sizeSpin->value();
        QPixmap pix(sz, sz);
        pix.fill(Qt::white);
        QPainter painter(&pix);
        Note exampleNote(0, 1, 4, 256);
        if (d->skin && d->skin->isValid())
        {
            const QPixmap *notePix = d->skin->getNotePixmap(2);
            if (notePix && !notePix->isNull())
            {
                QPixmap scaled = notePix->scaled(sz, sz, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                painter.drawPixmap((sz - scaled.width()) / 2, (sz - scaled.height()) / 2, scaled);
            }
            else
            {
                painter.setBrush(Qt::lightGray);
                painter.drawEllipse(0, 0, sz, sz);
            }
        }
        else
        {
            painter.setBrush(Qt::lightGray);
            painter.drawEllipse(0, 0, sz, sz);
        }
        previewLabel->setPixmap(pix);
    };
    updatePreview();
    connect(sizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), updatePreview);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() == QDialog::Accepted)
    {
        int newSize = sizeSpin->value();
        Settings::instance().setNoteSize(newSize);
        d->canvas->setNoteSize(newSize);
        Logger::info(QString("Note size set to %1").arg(newSize));
    }
}

// ==================== 皮肤校准 ====================
void MainWindow::calibrateSkin()
{
    if (!d->skin)
    {
        QMessageBox::information(this, tr("No Skin"), tr("No skin loaded, cannot calibrate."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Calibrate Skin: %1").arg(d->skin->title()));
    dialog.setMinimumSize(600, 400);
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QTabWidget *tabs = new QTabWidget;
    QStringList typeNames = {"1/1", "1/2", "1/4", "1/8/16/32", "1/3/6/12/24", "Rain"};
    for (int i = 0; i <= 5; ++i)
    {
        QWidget *page = new QWidget;
        QVBoxLayout *pageLayout = new QVBoxLayout(page);

        QHBoxLayout *scaleLayout = new QHBoxLayout;
        QLabel *scaleLabel = new QLabel(tr("Scale Factor:"));
        QDoubleSpinBox *scaleSpin = new QDoubleSpinBox;
        scaleSpin->setRange(0.2, 3.0);
        scaleSpin->setSingleStep(0.05);
        scaleSpin->setValue(d->skin->getNoteScale(i));
        scaleLayout->addWidget(scaleLabel);
        scaleLayout->addWidget(scaleSpin);
        pageLayout->addLayout(scaleLayout);

        QLabel *previewLabel = new QLabel;
        previewLabel->setFixedSize(128, 128);
        previewLabel->setStyleSheet("border: 1px solid gray; background: white;");
        previewLabel->setAlignment(Qt::AlignCenter);
        pageLayout->addWidget(previewLabel, 0, Qt::AlignCenter);

        auto updatePreview = [previewLabel, scaleSpin, this, i]()
        {
            double scale = scaleSpin->value();
            QPixmap pix(128, 128);
            pix.fill(Qt::white);
            QPainter painter(&pix);
            const QPixmap *notePix = d->skin->getNotePixmap(i);
            if (notePix && !notePix->isNull())
            {
                int scaledW = notePix->width() * scale;
                int scaledH = notePix->height() * scale;
                QPixmap scaled = notePix->scaled(scaledW, scaledH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                painter.drawPixmap((128 - scaled.width()) / 2, (128 - scaled.height()) / 2, scaled);
            }
            else
            {
                painter.setBrush(Qt::lightGray);
                painter.drawEllipse(20, 20, 88, 88);
            }
            previewLabel->setPixmap(pix);
        };
        updatePreview();
        connect(scaleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), updatePreview);

        pageLayout->addStretch();
        tabs->addTab(page, typeNames[i]);

        scaleSpin->setProperty("noteType", i);
        page->setProperty("scaleSpin", QVariant::fromValue(scaleSpin));
    }
    layout->addWidget(tabs);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]()
            {
        for (int i = 0; i <= 5; ++i) {
            QWidget* page = tabs->widget(i);
            QDoubleSpinBox* scaleSpin = page->property("scaleSpin").value<QDoubleSpinBox*>();
            if (scaleSpin) {
                d->skin->setNoteScale(i, scaleSpin->value());
            }
        }
        d->skin->saveConfig();
        d->canvas->update();
        Logger::info("Skin calibration saved");
        dialog.accept(); });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.exec();
}

// ==================== 轮廓设置 ====================
void MainWindow::configureOutline()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Outline Settings"));
    QFormLayout form(&dialog);

    QSpinBox *widthSpin = new QSpinBox;
    widthSpin->setRange(1, 8);
    widthSpin->setValue(Settings::instance().outlineWidth());
    form.addRow(tr("Outline Width (px):"), widthSpin);

    QPushButton *colorBtn = new QPushButton;
    QColor outlineColor = Settings::instance().outlineColor();
    colorBtn->setStyleSheet(QString("background-color: %1").arg(outlineColor.name()));
    connect(colorBtn, &QPushButton::clicked, [&]()
            {
        QColor newColor = QColorDialog::getColor(outlineColor, &dialog);
        if (newColor.isValid()) {
            outlineColor = newColor;
            colorBtn->setStyleSheet(QString("background-color: %1").arg(outlineColor.name()));
        } });
    form.addRow(tr("Outline Color:"), colorBtn);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form.addRow(buttons);

    if (dialog.exec() == QDialog::Accepted)
    {
        Settings::instance().setOutlineWidth(widthSpin->value());
        Settings::instance().setOutlineColor(outlineColor);
        d->canvas->update();
        Logger::info("Outline settings updated");
    }
}

// ==================== 皮肤菜单 ====================
void MainWindow::populateSkinMenu()
{
    d->skinMenu->clear();
    QString skinsBaseDir = QCoreApplication::applicationDirPath() + "/skins";
    QStringList skinDirs = SkinIO::getSkinList(skinsBaseDir);
    if (skinDirs.isEmpty())
    {
        d->skinMenu->addAction(tr("No skins found"))->setEnabled(false);
        Logger::warn("No skin directories found");
        return;
    }

    QString currentSkin = Settings::instance().currentSkin();
    for (const QString &skinDirName : skinDirs)
    {
        QString skinPath = skinsBaseDir + "/" + skinDirName;
        QString displayName = SkinIO::getSkinDisplayName(skinPath);
        QAction *action = d->skinMenu->addAction(displayName);
        action->setData(skinDirName);
        action->setCheckable(true);
        if (skinDirName == currentSkin)
        {
            action->setChecked(true);
        }
        connect(action, &QAction::triggered, this, [this, skinDirName]()
                { changeSkin(skinDirName); });
    }
    Logger::debug(QString("Populated skin menu with %1 skins").arg(skinDirs.size()));
}

void MainWindow::changeSkin(const QString &skinName)
{
    Logger::info(QString("Changing skin to %1").arg(skinName));

    QString skinsBaseDir = QCoreApplication::applicationDirPath() + "/skins";
    QString skinPath = skinsBaseDir + "/" + skinName;
    Skin *newSkin = new Skin();
    if (SkinIO::loadSkin(skinPath, *newSkin))
    {
        if (d->skin)
            delete d->skin;
        d->skin = newSkin;
        Settings::instance().setCurrentSkin(skinName);
        d->canvas->setSkin(d->skin);
        d->canvas->update();
        Logger::info(QString("Skin changed to %1").arg(skinName));
    }
    else
    {
        Logger::error(QString("Failed to load skin %1").arg(skinName));
        delete newSkin;
        QMessageBox::warning(this, tr("Skin Error"), tr("Failed to load skin: %1").arg(skinName));
    }
    populateSkinMenu();
}

void MainWindow::setSkin(Skin *skin)
{
    if (d->skin)
        delete d->skin;
    d->skin = skin;
    if (d->canvas)
        d->canvas->setSkin(skin);
    Logger::debug("Skin set externally");
}

// ==================== 粘贴288分度选项槽函数 ====================
void MainWindow::togglePaste288Division(bool enabled)
{
    Settings::instance().setPasteUse288Division(enabled);
    Logger::info(QString("Paste 288 division: %1").arg(enabled ? "enabled" : "disabled"));
}