#include "MainWindow.h"
#include "ui/CustomWidgets/ChartCanvas.h"
#include "ui/NoteEditPanel.h"
#include "ui/BPMTimePanel.h"
#include "ui/MetaEditPanel.h"
#include "controller/ChartController.h"
#include "controller/SelectionController.h"
#include "controller/PlaybackController.h"
#include "audio/AudioPlayer.h"
#include "utils/Settings.h"
#include "utils/Translator.h"
#include "file/SkinIO.h"
#include "model/Skin.h"
#include "utils/Logger.h"
#include <QMenuBar>
#include <QToolBar>
#include <QSplitter>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QAction>
#include <QApplication>
#include <QFileInfo>
#include <QDebug>

// Private 实现类
class MainWindow::Private {
public:
    ChartController* chartController;
    SelectionController* selectionController;
    PlaybackController* playbackController;
    Skin* skin;
    ChartCanvas* canvas;
    QSplitter* splitter;
    QWidget* rightPanelContainer;
    RightPanel* currentRightPanel;
    NoteEditPanel* notePanel;
    BPMTimePanel* bpmPanel;
    MetaEditPanel* metaPanel;
    QAction* undoAction;
    QAction* redoAction;
    QAction* colorAction;
    QAction* hyperfruitAction;
    QAction* playAction;
    QMenu* skinMenu; // 皮肤菜单
};

// 构造函数
MainWindow::MainWindow(ChartController* chartCtrl,
                       SelectionController* selCtrl,
                       PlaybackController* playCtrl,
                       Skin* skin,
                       QWidget* parent)
    : QMainWindow(parent), d(new Private)
{
    Logger::info("MainWindow constructor called");

    // 保存控制器指针
    d->chartController = chartCtrl;
    d->selectionController = selCtrl;
    d->playbackController = playCtrl;
    d->skin = skin;

    // 创建 UI 组件（顺序重要：setupUi → createCentralArea → createMenus）
    setupUi();
    createCentralArea(); // 必须先创建 canvas，以便菜单能使用它
    createMenus();
    retranslateUi();

    // 连接控制器信号
    connect(d->chartController, &ChartController::chartChanged, this, [this]() {
        d->canvas->update();
        d->undoAction->setEnabled(d->chartController->canUndo());
        d->redoAction->setEnabled(d->chartController->canRedo());
    });
    connect(d->chartController, &ChartController::errorOccurred, this, [this](const QString& msg) {
        statusBar()->showMessage(msg, 3000);
        Logger::error("ChartController error: " + msg);
    });
    connect(d->playbackController, &PlaybackController::positionChanged, this, [this](double time) {
        d->canvas->setScrollPos(time);
    });

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
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* openAction = fileMenu->addAction(tr("&Open..."), this, &MainWindow::openChart);
    openAction->setShortcut(QKeySequence::Open);
    QAction* saveAction = fileMenu->addAction(tr("&Save"), this, &MainWindow::saveChart);
    saveAction->setShortcut(QKeySequence::Save);
    QAction* saveAsAction = fileMenu->addAction(tr("Save &As..."), this, &MainWindow::saveChartAs);
    fileMenu->addSeparator();
    QAction* exportAction = fileMenu->addAction(tr("&Export .mcz..."), this, &MainWindow::exportMcz);
    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);

    // 编辑菜单
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    d->undoAction = editMenu->addAction(tr("&Undo"), this, &MainWindow::undo);
    d->undoAction->setShortcut(QKeySequence::Undo);
    d->redoAction = editMenu->addAction(tr("&Redo"), this, &MainWindow::redo);
    d->redoAction->setShortcut(QKeySequence::Redo);
    editMenu->addSeparator();
    QAction* copyAction = editMenu->addAction(tr("&Copy"));
    connect(copyAction, &QAction::triggered, this, [this]() {
        d->selectionController->copySelected(d->chartController->chart()->notes());
        Logger::debug("Copy action triggered");
    });
    copyAction->setShortcut(QKeySequence::Copy);
    QAction* pasteAction = editMenu->addAction(tr("&Paste"), d->canvas, &ChartCanvas::paste);
    pasteAction->setShortcut(QKeySequence::Paste);

    // 视图菜单
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    d->colorAction = viewMenu->addAction(tr("&Color Notes"));
    d->colorAction->setCheckable(true);
    d->colorAction->setChecked(Settings::instance().colorNoteEnabled());
    connect(d->colorAction, &QAction::toggled, this, &MainWindow::toggleColorMode);
    d->hyperfruitAction = viewMenu->addAction(tr("&Hyperfruit Outline"));
    d->hyperfruitAction->setCheckable(true);
    d->hyperfruitAction->setChecked(Settings::instance().hyperfruitOutlineEnabled());
    connect(d->hyperfruitAction, &QAction::toggled, this, &MainWindow::toggleHyperfruitMode);

    // 播放菜单
    QMenu* playMenu = menuBar()->addMenu(tr("&Playback"));
    d->playAction = playMenu->addAction(tr("&Play/Pause"), this, &MainWindow::togglePlayback);
    d->playAction->setShortcut(Qt::Key_Space);
    playMenu->addSeparator();
    QMenu* speedMenu = playMenu->addMenu(tr("&Speed"));
    for (double sp : {0.25, 0.5, 0.75, 1.0}) {
        QAction* act = speedMenu->addAction(tr("%1x").arg(sp), [this, sp]() {
            d->playbackController->setSpeed(sp);
            Settings::instance().setPlaybackSpeed(sp);
            Logger::info(QString("Playback speed set to %1x").arg(sp));
        });
        act->setCheckable(true);
        act->setChecked(qFuzzyCompare(sp, Settings::instance().playbackSpeed()));
    }

    // 工具菜单
    QMenu* toolsMenu = menuBar()->addMenu(tr("&Tools"));
    QAction* gridAction = toolsMenu->addAction(tr("&Grid Settings..."), d->canvas, &ChartCanvas::showGridSettings);

    // 皮肤菜单
    d->skinMenu = menuBar()->addMenu(tr("&Skin"));
    populateSkinMenu();

    Logger::debug("Menus created");
}

void MainWindow::populateSkinMenu()
{
    d->skinMenu->clear();
    QString skinsBaseDir = QCoreApplication::applicationDirPath() + "/skins";
    QStringList skinDirs = SkinIO::getSkinList(skinsBaseDir);
    if (skinDirs.isEmpty()) {
        d->skinMenu->addAction(tr("No skins found"))->setEnabled(false);
        Logger::warn("No skin directories found");
        return;
    }

    QString currentSkin = Settings::instance().currentSkin();
    for (const QString& skinDirName : skinDirs) {
        QString skinPath = skinsBaseDir + "/" + skinDirName;
        QString displayName = SkinIO::getSkinDisplayName(skinPath);
        QAction* action = d->skinMenu->addAction(displayName);
        action->setData(skinDirName);
        action->setCheckable(true);
        if (skinDirName == currentSkin) {
            action->setChecked(true);
        }
        connect(action, &QAction::triggered, this, [this, skinDirName]() {
            changeSkin(skinDirName);
        });
    }
    Logger::debug(QString("Populated skin menu with %1 skins").arg(skinDirs.size()));
}

void MainWindow::changeSkin(const QString& skinName)
{
    Logger::info(QString("Changing skin to %1").arg(skinName));

    QString skinsBaseDir = QCoreApplication::applicationDirPath() + "/skins";
    QString skinPath = skinsBaseDir + "/" + skinName;
    Skin* newSkin = new Skin();
    if (SkinIO::loadSkin(skinPath, *newSkin)) {
        // 替换全局皮肤
        if (d->skin) delete d->skin;
        d->skin = newSkin;
        Settings::instance().setCurrentSkin(skinName);
        // 通知画布更新皮肤
        d->canvas->setSkin(d->skin);
        d->canvas->update();
        Logger::info(QString("Skin changed to %1").arg(skinName));
    } else {
        Logger::error(QString("Failed to load skin %1").arg(skinName));
        delete newSkin;
        QMessageBox::warning(this, tr("Skin Error"), tr("Failed to load skin: %1").arg(skinName));
    }
    populateSkinMenu(); // 更新菜单选中状态
}

void MainWindow::setSkin(Skin* skin)
{
    if (d->skin) delete d->skin;
    d->skin = skin;
    if (d->canvas) d->canvas->setSkin(skin);
    Logger::debug("Skin set externally");
}

void MainWindow::createCentralArea()
{
    Logger::debug("Creating central area...");

    // 创建画布
    d->canvas = new ChartCanvas(this);
    d->canvas->setChartController(d->chartController);
    d->canvas->setSelectionController(d->selectionController);
    d->canvas->setColorMode(Settings::instance().colorNoteEnabled());
    d->canvas->setHyperfruitEnabled(Settings::instance().hyperfruitOutlineEnabled());
    if (d->skin) d->canvas->setSkin(d->skin);

    // 右侧面板容器
    d->rightPanelContainer = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(d->rightPanelContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    // 创建各个子面板
    d->notePanel = new NoteEditPanel(d->rightPanelContainer);
    d->bpmPanel = new BPMTimePanel(d->rightPanelContainer);
    d->metaPanel = new MetaEditPanel(d->rightPanelContainer);
    d->currentRightPanel = d->notePanel;
    rightLayout->addWidget(d->notePanel);
    rightLayout->addWidget(d->bpmPanel);
    rightLayout->addWidget(d->metaPanel);
    d->bpmPanel->setVisible(false);
    d->metaPanel->setVisible(false);

    // 设置控制器
    d->notePanel->setChartController(d->chartController);
    d->notePanel->setSelectionController(d->selectionController);
    d->bpmPanel->setChartController(d->chartController);
    d->metaPanel->setChartController(d->chartController);

    // 连接时间分度变化信号
    connect(d->notePanel, &NoteEditPanel::timeDivisionChanged, d->canvas, &ChartCanvas::setTimeDivision);
    Logger::debug("Connected time division signal");

    // 创建分割器
    d->splitter = new QSplitter(Qt::Horizontal, this);
    d->splitter->addWidget(d->canvas);
    d->splitter->addWidget(d->rightPanelContainer);
    d->splitter->setSizes({800, 300});
    setCentralWidget(d->splitter);

    // 工具栏（简单示例）
    QToolBar* toolBar = addToolBar(tr("Tools"));
    toolBar->addAction(tr("Note"), [this]() {
        d->notePanel->setVisible(true);
        d->bpmPanel->setVisible(false);
        d->metaPanel->setVisible(false);
        d->currentRightPanel = d->notePanel;
        Logger::debug("Switched to Note panel");
    });
    toolBar->addAction(tr("BPM"), [this]() {
        d->notePanel->setVisible(false);
        d->bpmPanel->setVisible(true);
        d->metaPanel->setVisible(false);
        d->currentRightPanel = d->bpmPanel;
        Logger::debug("Switched to BPM panel");
    });
    toolBar->addAction(tr("Meta"), [this]() {
        d->notePanel->setVisible(false);
        d->bpmPanel->setVisible(false);
        d->metaPanel->setVisible(true);
        d->currentRightPanel = d->metaPanel;
        Logger::debug("Switched to Meta panel");
    });

    Logger::debug("Central area created");
}

void MainWindow::openChart()
{
    Logger::info("Open chart requested");
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Chart"), Settings::instance().lastOpenPath(),
                                                    tr("Malody Catch Chart (*.mc);;All Files (*.*)"));
    if (fileName.isEmpty()) {
        Logger::debug("Open chart cancelled");
        return;
    }
    if (d->chartController->loadChart(fileName)) {
        Settings::instance().setLastOpenPath(QFileInfo(fileName).path());
        QString audioPath = QFileInfo(fileName).absolutePath() + "/" + d->chartController->chart()->meta().audioFile;
        if (QFile::exists(audioPath)) {
            d->playbackController->audioPlayer()->load(audioPath);
            Logger::info("Audio loaded: " + audioPath);
        } else {
            Logger::warn("Audio file not found: " + audioPath);
        }
        d->canvas->update();
        Logger::info("Chart loaded: " + fileName);
    } else {
        Logger::error("Failed to load chart: " + fileName);
        QMessageBox::critical(this, tr("Error"), tr("Failed to load chart."));
    }
}

void MainWindow::saveChart()
{
    Logger::info("Save chart requested");
    QString currentPath = Settings::instance().lastOpenPath() + "/" + d->chartController->chart()->meta().difficulty + ".mc";
    if (d->chartController->saveChart(currentPath)) {
        statusBar()->showMessage(tr("Saved: %1").arg(currentPath), 2000);
        Logger::info("Chart saved: " + currentPath);
    } else {
        Logger::error("Failed to save chart: " + currentPath);
        QMessageBox::critical(this, tr("Error"), tr("Failed to save chart."));
    }
}

void MainWindow::saveChartAs()
{
    Logger::info("Save chart as requested");
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Chart As"), Settings::instance().lastOpenPath(),
                                                    tr("Malody Catch Chart (*.mc);;All Files (*.*)"));
    if (fileName.isEmpty()) {
        Logger::debug("Save as cancelled");
        return;
    }
    if (d->chartController->saveChart(fileName)) {
        statusBar()->showMessage(tr("Saved: %1").arg(fileName), 2000);
        Logger::info("Chart saved as: " + fileName);
    } else {
        Logger::error("Failed to save chart as: " + fileName);
        QMessageBox::critical(this, tr("Error"), tr("Failed to save chart."));
    }
}

void MainWindow::exportMcz()
{
    Logger::info("Export .mcz requested");
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export .mcz"), Settings::instance().lastOpenPath(),
                                                    tr("Malody Catch Pack (*.mcz);;All Files (*.*)"));
    if (fileName.isEmpty()) {
        Logger::debug("Export cancelled");
        return;
    }
    // TODO: 实现导出
    Logger::warn("Export .mcz not implemented yet");
    QMessageBox::information(this, tr("Not Implemented"), tr("Export .mcz is not yet implemented."));
}

void MainWindow::undo()
{
    if (d->chartController) {
        Logger::debug("Undo triggered");
        d->chartController->undo();
    }
}

void MainWindow::redo()
{
    if (d->chartController) {
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

void MainWindow::togglePlayback()
{
    if (d->playbackController->state() == PlaybackController::Playing) {
        Logger::debug("Playback paused");
        d->playbackController->pause();
    } else {
        Logger::debug("Playback started");
        d->playbackController->play();
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::retranslateUi()
{
    setWindowTitle(tr("Catch Chart Editor"));
    // 菜单文本在 createMenus 中动态创建，此处不重复设置
    // 但可以重新填充皮肤菜单以更新显示名称（如果语言影响皮肤名）
    populateSkinMenu();
    Logger::debug("UI retranslated");
}