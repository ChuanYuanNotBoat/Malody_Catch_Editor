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
#include <QFileInfo>   // 添加
#include <QDebug>      // 添加

class MainWindow::Private {
public:
    ChartController* chartController;
    SelectionController* selectionController;
    PlaybackController* playbackController;
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
};

MainWindow::MainWindow(ChartController* chartCtrl, SelectionController* selCtrl, PlaybackController* playCtrl, QWidget* parent)
    : QMainWindow(parent), d(new Private)
{
    qDebug() << "MainWindow::ctor: Starting";
    
    // 设置全局控制器
    d->chartController = chartCtrl;
    d->selectionController = selCtrl;
    d->playbackController = playCtrl;
    qDebug() << "MainWindow::ctor: Controllers set";

    qDebug() << "MainWindow::ctor: Calling setupUi";
    setupUi();
    qDebug() << "MainWindow::ctor: setupUi done";
    
    // 必须先创建 UI 中心区域（包括 canvas），再创建菜单
    // 因为菜单可能要访问 canvas
    qDebug() << "MainWindow::ctor: Calling createCentralArea";
    createCentralArea();
    qDebug() << "MainWindow::ctor: createCentralArea done";
    
    qDebug() << "MainWindow::ctor: Calling createMenus";
    createMenus();
    qDebug() << "MainWindow::ctor: createMenus done";
    
    qDebug() << "MainWindow::ctor: Calling retranslateUi";
    retranslateUi();
    qDebug() << "MainWindow::ctor: retranslateUi done";

    qDebug() << "MainWindow::ctor: Connecting signals";
    // 连接控制器信号
    connect(d->chartController, &ChartController::chartChanged, this, [this]() {
        d->canvas->update();
        d->undoAction->setEnabled(d->chartController->canUndo());
        d->redoAction->setEnabled(d->chartController->canRedo());
    });
    connect(d->chartController, &ChartController::errorOccurred, this, [this](const QString& msg) {
        statusBar()->showMessage(msg, 3000);
    });
    connect(d->playbackController, &PlaybackController::positionChanged, this, [this](double time) {
        // 更新画布滚动位置
        d->canvas->setScrollPos(time);
    });
    qDebug() << "MainWindow::ctor: Signals connected";
}

MainWindow::~MainWindow()
{
    delete d;
}

void MainWindow::setupUi()
{
    setWindowTitle(tr("Catch Chart Editor"));
    resize(1200, 800);
}

void MainWindow::createMenus()
{
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

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    d->undoAction = editMenu->addAction(tr("&Undo"), this, &MainWindow::undo);
    d->undoAction->setShortcut(QKeySequence::Undo);
    d->redoAction = editMenu->addAction(tr("&Redo"), this, &MainWindow::redo);
    d->redoAction->setShortcut(QKeySequence::Redo);
    editMenu->addSeparator();
    QAction* copyAction = editMenu->addAction(tr("&Copy"));
    connect(copyAction, &QAction::triggered, this, [this]() { d->selectionController->copySelected(d->chartController->chart()->notes()); });
    copyAction->setShortcut(QKeySequence::Copy);
    QAction* pasteAction = editMenu->addAction(tr("&Paste"), d->canvas, &ChartCanvas::paste);
    pasteAction->setShortcut(QKeySequence::Paste);

    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    d->colorAction = viewMenu->addAction(tr("&Color Notes"));
    d->colorAction->setCheckable(true);
    d->colorAction->setChecked(Settings::instance().colorNoteEnabled());
    connect(d->colorAction, &QAction::toggled, this, &MainWindow::toggleColorMode);
    d->hyperfruitAction = viewMenu->addAction(tr("&Hyperfruit Outline"));
    d->hyperfruitAction->setCheckable(true);
    d->hyperfruitAction->setChecked(Settings::instance().hyperfruitOutlineEnabled());
    connect(d->hyperfruitAction, &QAction::toggled, this, &MainWindow::toggleHyperfruitMode);

    QMenu* playMenu = menuBar()->addMenu(tr("&Playback"));
    d->playAction = playMenu->addAction(tr("&Play/Pause"), this, &MainWindow::togglePlayback);
    d->playAction->setShortcut(Qt::Key_Space);
    playMenu->addSeparator();
    QMenu* speedMenu = playMenu->addMenu(tr("&Speed"));
    for (double sp : {0.25, 0.5, 0.75, 1.0}) {
        QAction* act = speedMenu->addAction(tr("%1x").arg(sp), [this, sp]() {
            d->playbackController->setSpeed(sp);
            Settings::instance().setPlaybackSpeed(sp);
        });
        act->setCheckable(true);
        act->setChecked(qFuzzyCompare(sp, Settings::instance().playbackSpeed()));
    }

    QMenu* toolsMenu = menuBar()->addMenu(tr("&Tools"));
    QAction* gridAction = toolsMenu->addAction(tr("&Grid Settings..."), d->canvas, &ChartCanvas::showGridSettings);
}

void MainWindow::createCentralArea()
{
    d->canvas = new ChartCanvas(this);
    d->canvas->setChartController(d->chartController);
    d->canvas->setSelectionController(d->selectionController);
    d->canvas->setColorMode(Settings::instance().colorNoteEnabled());
    d->canvas->setHyperfruitEnabled(Settings::instance().hyperfruitOutlineEnabled());

    // 右侧面板容器
    d->rightPanelContainer = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(d->rightPanelContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    // 创建各个子面板
    d->notePanel = new NoteEditPanel(d->rightPanelContainer);
    d->bpmPanel = new BPMTimePanel(d->rightPanelContainer);
    d->metaPanel = new MetaEditPanel(d->rightPanelContainer);
    // 默认显示 note 面板
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

    // 创建分屏
    d->splitter = new QSplitter(Qt::Horizontal, this);
    d->splitter->addWidget(d->canvas);
    d->splitter->addWidget(d->rightPanelContainer);
    d->splitter->setSizes({800, 300});

    setCentralWidget(d->splitter);

    // 工具栏切换面板的按钮（简单示例，实际可用 QToolBar）
    QToolBar* toolBar = addToolBar(tr("Tools"));
    toolBar->addAction(tr("Note"), [this]() {
        d->notePanel->setVisible(true);
        d->bpmPanel->setVisible(false);
        d->metaPanel->setVisible(false);
        d->currentRightPanel = d->notePanel;
    });
    toolBar->addAction(tr("BPM"), [this]() {
        d->notePanel->setVisible(false);
        d->bpmPanel->setVisible(true);
        d->metaPanel->setVisible(false);
        d->currentRightPanel = d->bpmPanel;
    });
    toolBar->addAction(tr("Meta"), [this]() {
        d->notePanel->setVisible(false);
        d->bpmPanel->setVisible(false);
        d->metaPanel->setVisible(true);
        d->currentRightPanel = d->metaPanel;
    });
}

void MainWindow::openChart()
{
    qDebug() << "MainWindow::openChart called";
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Chart"), Settings::instance().lastOpenPath(),
                                                    tr("Malody Catch Chart (*.mc);;All Files (*.*)"));
    if (fileName.isEmpty()) {
        qDebug() << "MainWindow::openChart: File dialog cancelled";
        return;
    }
    qDebug() << "MainWindow::openChart: Loading file" << fileName;
    if (d->chartController->loadChart(fileName)) {
        Settings::instance().setLastOpenPath(QFileInfo(fileName).path());
        // 尝试加载音频
        QString audioPath = QFileInfo(fileName).absolutePath() + "/" + d->chartController->chart()->meta().audioFile;
        qDebug() << "MainWindow::openChart: Trying to load audio from" << audioPath;
        if (QFile::exists(audioPath)) {
            d->playbackController->audioPlayer()->load(audioPath);
            qDebug() << "MainWindow::openChart: Audio loaded";
        }
        d->canvas->update();
        qDebug() << "MainWindow::openChart: Chart loaded successfully";
    } else {
        qWarning() << "MainWindow::openChart: Failed to load chart";
        QMessageBox::critical(this, tr("Error"), tr("Failed to load chart."));
    }
}

void MainWindow::saveChart()
{
    qDebug() << "MainWindow::saveChart called";
    // 需要知道当前文件路径，这里简化：使用上次打开路径+难度名.mc
    QString currentPath = Settings::instance().lastOpenPath() + "/" + d->chartController->chart()->meta().difficulty + ".mc";
    qDebug() << "MainWindow::saveChart: Saving to" << currentPath;
    if (d->chartController->saveChart(currentPath)) {
        statusBar()->showMessage(tr("Saved: %1").arg(currentPath), 2000);
        qDebug() << "MainWindow::saveChart: Chart saved successfully";
    } else {
        qWarning() << "MainWindow::saveChart: Failed to save chart";
        QMessageBox::critical(this, tr("Error"), tr("Failed to save chart."));
    }
}

void MainWindow::saveChartAs()
{
    qDebug() << "MainWindow::saveChartAs called";
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Chart As"), Settings::instance().lastOpenPath(),
                                                    tr("Malody Catch Chart (*.mc);;All Files (*.*)"));
    if (fileName.isEmpty()) {
        qDebug() << "MainWindow::saveChartAs: File dialog cancelled";
        return;
    }
    qDebug() << "MainWindow::saveChartAs: Saving to" << fileName;
    if (d->chartController->saveChart(fileName)) {
        statusBar()->showMessage(tr("Saved: %1").arg(fileName), 2000);
        qDebug() << "MainWindow::saveChartAs: Chart saved successfully";
    } else {
        qWarning() << "MainWindow::saveChartAs: Failed to save chart";
        QMessageBox::critical(this, tr("Error"), tr("Failed to save chart."));
    }
}

void MainWindow::exportMcz()
{
    qDebug() << "MainWindow::exportMcz called";
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export .mcz"), Settings::instance().lastOpenPath(),
                                                    tr("Malody Catch Pack (*.mcz);;All Files (*.*)"));
    if (fileName.isEmpty()) {
        qDebug() << "MainWindow::exportMcz: File dialog cancelled";
        return;
    }
    // 实际需要收集音频、背景等文件，此处简化
    // 调用 ProjectIO::exportToMcz
    qDebug() << "MainWindow::exportMcz: Export stub - not implemented yet";
}

void MainWindow::undo()
{
    qDebug() << "MainWindow::undo called";
    if (!d->chartController) {
        qWarning() << "MainWindow::undo: chartController is null";
        return;
    }
    d->chartController->undo();
    qDebug() << "MainWindow::undo: Undo executed";
}

void MainWindow::redo()
{
    qDebug() << "MainWindow::redo called";
    if (!d->chartController) {
        qWarning() << "MainWindow::redo: chartController is null";
        return;
    }
    d->chartController->redo();
    qDebug() << "MainWindow::redo: Redo executed";
}

void MainWindow::toggleColorMode(bool on)
{
    qDebug() << "MainWindow::toggleColorMode: Color mode toggled to" << on;
    Settings::instance().setColorNoteEnabled(on);
    d->canvas->setColorMode(on);
}

void MainWindow::toggleHyperfruitMode(bool on)
{
    qDebug() << "MainWindow::toggleHyperfruitMode: Hyperfruit mode toggled to" << on;
    Settings::instance().setHyperfruitOutlineEnabled(on);
    d->canvas->setHyperfruitEnabled(on);
}

void MainWindow::togglePlayback()
{
    qDebug() << "MainWindow::togglePlayback called";
    if (!d->playbackController) {
        qWarning() << "MainWindow::togglePlayback: playbackController is null";
        return;
    }
    if (d->playbackController->state() == PlaybackController::Playing) {
        qDebug() << "MainWindow::togglePlayback: Pausing playback";
        d->playbackController->pause();
    } else {
        qDebug() << "MainWindow::togglePlayback: Starting playback";
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
    // 重新翻译 UI 文本（这里需更新所有动态文本）
    // 简单起见，可重设窗口标题，菜单文本等
    setWindowTitle(tr("Catch Chart Editor"));
    // 菜单文本需要在 createMenus 中动态生成，这里省略
}