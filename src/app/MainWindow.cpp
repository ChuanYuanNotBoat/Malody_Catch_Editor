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
#include <QHBoxLayout>
#include <QAction>
#include <QApplication>
#include <QFileInfo>
#include <QSpinBox>
#include <QPainter> 
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QColorDialog>
#include <QTabWidget>
#include <QDoubleSpinBox>
#include <QDebug>
#include <QSlider>
#include <QPushButton>

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
    QMenu* skinMenu;
    QAction* noteSizeAction;
    QAction* calibrateSkinAction;
    QAction* outlineAction;
};

MainWindow::MainWindow(ChartController* chartCtrl,
                       SelectionController* selCtrl,
                       PlaybackController* playCtrl,
                       Skin* skin,
                       QWidget* parent)
    : QMainWindow(parent), d(new Private)
{
    Logger::info("MainWindow constructor called");

    d->chartController = chartCtrl;
    d->selectionController = selCtrl;
    d->playbackController = playCtrl;
    d->skin = skin;

    setupUi();
    createCentralArea(); // 必须在 createMenus 之前，因为 createMenus 需要 canvas 存在
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

    // 设置菜单
    QMenu* settingsMenu = menuBar()->addMenu(tr("&Settings"));
    d->noteSizeAction = settingsMenu->addAction(tr("Note Size..."));
    connect(d->noteSizeAction, &QAction::triggered, this, &MainWindow::adjustNoteSize);
    d->calibrateSkinAction = settingsMenu->addAction(tr("Calibrate Skin..."));
    connect(d->calibrateSkinAction, &QAction::triggered, this, &MainWindow::calibrateSkin);
    d->outlineAction = settingsMenu->addAction(tr("Outline Settings..."));
    connect(d->outlineAction, &QAction::triggered, this, &MainWindow::configureOutline);

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
        if (d->skin) delete d->skin;
        d->skin = newSkin;
        Settings::instance().setCurrentSkin(skinName);
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

    d->canvas = new ChartCanvas(this);
    d->canvas->setChartController(d->chartController);
    d->canvas->setSelectionController(d->selectionController);
    d->canvas->setColorMode(Settings::instance().colorNoteEnabled());
    d->canvas->setHyperfruitEnabled(Settings::instance().hyperfruitOutlineEnabled());
    if (d->skin) d->canvas->setSkin(d->skin);
    d->canvas->setNoteSize(Settings::instance().noteSize());

    d->rightPanelContainer = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(d->rightPanelContainer);
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

    // 连接模式切换信号
    connect(d->notePanel, &NoteEditPanel::modeChanged, d->canvas, [this](int mode) {
        d->canvas->setMode(static_cast<ChartCanvas::Mode>(mode));
        Logger::debug(QString("Canvas mode set to %1").arg(mode));
    });
    // 连接时间分度变化信号
    connect(d->notePanel, &NoteEditPanel::timeDivisionChanged, d->canvas, &ChartCanvas::setTimeDivision);
    Logger::debug("Connected mode and time division signals");

    d->splitter = new QSplitter(Qt::Horizontal, this);
    d->splitter->addWidget(d->canvas);
    d->splitter->addWidget(d->rightPanelContainer);
    d->splitter->setSizes({800, 300});
    setCentralWidget(d->splitter);

    QToolBar* toolBar = addToolBar(tr("Tools"));
    toolBar->addAction(tr("Note"), [this]() {
        d->notePanel->setVisible(true);
        d->bpmPanel->setVisible(false);
        d->metaPanel->setVisible(false);
        d->currentRightPanel = d->notePanel;
        d->canvas->setMode(ChartCanvas::PlaceNote);
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

void MainWindow::adjustNoteSize()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Note Size"));
    dialog.setMinimumSize(400, 300);
    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    // 大小选择
    QHBoxLayout* sizeLayout = new QHBoxLayout;
    QLabel* label = new QLabel(tr("Size (pixels):"));
    QSpinBox* sizeSpin = new QSpinBox;
    sizeSpin->setRange(8, 64);
    sizeSpin->setValue(Settings::instance().noteSize());
    sizeLayout->addWidget(label);
    sizeLayout->addWidget(sizeSpin);
    layout->addLayout(sizeLayout);

    // 预览区域
    QLabel* previewLabel = new QLabel;
    previewLabel->setFixedSize(128, 128);
    previewLabel->setStyleSheet("border: 1px solid gray; background: white;");
    previewLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(previewLabel, 0, Qt::AlignCenter);

    // 实时更新预览，使用当前皮肤
    auto updatePreview = [previewLabel, sizeSpin, this]() {
        int sz = sizeSpin->value();
        QPixmap pix(sz, sz);
        pix.fill(Qt::white);
        QPainter painter(&pix);
        // 使用当前皮肤绘制一个示例音符（假设分母=4，普通note）
        Note exampleNote(0, 1, 4, 256);
        QPointF center(sz/2, sz/2);
        if (d->skin && d->skin->isValid()) {
            // 获取皮肤图片，缩放至指定大小
            const QPixmap* notePix = d->skin->getNotePixmap(2); // type 2 = 1/4
            if (notePix && !notePix->isNull()) {
                QPixmap scaled = notePix->scaled(sz, sz, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                painter.drawPixmap((sz - scaled.width())/2, (sz - scaled.height())/2, scaled);
            } else {
                painter.setBrush(Qt::lightGray);
                painter.drawEllipse(0, 0, sz, sz);
            }
        } else {
            painter.setBrush(Qt::lightGray);
            painter.drawEllipse(0, 0, sz, sz);
        }
        previewLabel->setPixmap(pix);
    };
    updatePreview();
    connect(sizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), updatePreview);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() == QDialog::Accepted) {
        int newSize = sizeSpin->value();
        Settings::instance().setNoteSize(newSize);
        d->canvas->setNoteSize(newSize);
        Logger::info(QString("Note size set to %1").arg(newSize));
    }
}

void MainWindow::calibrateSkin()
{
    if (!d->skin || !d->skin->isValid()) {
        QMessageBox::information(this, tr("No Skin"), tr("No skin loaded, cannot calibrate."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Calibrate Skin: %1").arg(d->skin->title()));
    dialog.setMinimumSize(600, 400);
    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    // 创建选项卡，每种 note 类型一个
    QTabWidget* tabs = new QTabWidget;
    QStringList typeNames = {"1/1", "1/2", "1/4", "1/8/16/32", "1/3/6/12/24", "Rain"};
    for (int i = 0; i <= 5; ++i) {
        QWidget* page = new QWidget;
        QVBoxLayout* pageLayout = new QVBoxLayout(page);

        // 当前缩放比例
        QHBoxLayout* scaleLayout = new QHBoxLayout;
        QLabel* scaleLabel = new QLabel(tr("Scale Factor:"));
        QDoubleSpinBox* scaleSpin = new QDoubleSpinBox;
        scaleSpin->setRange(0.2, 3.0);
        scaleSpin->setSingleStep(0.05);
        scaleSpin->setValue(d->skin->getNoteScale(i));
        scaleLayout->addWidget(scaleLabel);
        scaleLayout->addWidget(scaleSpin);
        pageLayout->addLayout(scaleLayout);

        // 预览区域
        QLabel* previewLabel = new QLabel;
        previewLabel->setFixedSize(128, 128);
        previewLabel->setStyleSheet("border: 1px solid gray; background: white;");
        previewLabel->setAlignment(Qt::AlignCenter);
        pageLayout->addWidget(previewLabel, 0, Qt::AlignCenter);

        // 实时更新预览
        auto updatePreview = [previewLabel, scaleSpin, this, i]() {
            double scale = scaleSpin->value();
            QPixmap pix(128, 128);
            pix.fill(Qt::white);
            QPainter painter(&pix);
            const QPixmap* notePix = d->skin->getNotePixmap(i);
            if (notePix && !notePix->isNull()) {
                int scaledW = notePix->width() * scale;
                int scaledH = notePix->height() * scale;
                QPixmap scaled = notePix->scaled(scaledW, scaledH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                painter.drawPixmap((128 - scaled.width())/2, (128 - scaled.height())/2, scaled);
            } else {
                painter.setBrush(Qt::lightGray);
                painter.drawEllipse(20, 20, 88, 88);
            }
            previewLabel->setPixmap(pix);
        };
        updatePreview();
        connect(scaleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), updatePreview);

        // 保存控件以便最后获取
        scaleSpin->setProperty("noteType", i);
        page->setProperty("scaleSpin", QVariant::fromValue(scaleSpin));
        pageLayout->addStretch();
        tabs->addTab(page, typeNames[i]);
    }
    layout->addWidget(tabs);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        // 保存所有校准值到皮肤配置
        for (int i = 0; i <= 5; ++i) {
            QWidget* page = tabs->widget(i);
            QDoubleSpinBox* scaleSpin = page->property("scaleSpin").value<QDoubleSpinBox*>();
            if (scaleSpin) {
                d->skin->setNoteScale(i, scaleSpin->value());
            }
        }
        d->skin->saveConfig();  // 保存到 skin_config.json
        d->canvas->update();
        Logger::info("Skin calibration saved");
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::configureOutline()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Outline Settings"));
    QFormLayout form(&dialog);

    QSpinBox* widthSpin = new QSpinBox;
    widthSpin->setRange(1, 8);
    widthSpin->setValue(Settings::instance().outlineWidth());
    form.addRow(tr("Outline Width (px):"), widthSpin);

    // 颜色选择
    QPushButton* colorBtn = new QPushButton;
    QColor outlineColor = Settings::instance().outlineColor();
    colorBtn->setStyleSheet(QString("background-color: %1").arg(outlineColor.name()));
    connect(colorBtn, &QPushButton::clicked, [&]() {
        QColor newColor = QColorDialog::getColor(outlineColor, &dialog);
        if (newColor.isValid()) {
            outlineColor = newColor;
            colorBtn->setStyleSheet(QString("background-color: %1").arg(outlineColor.name()));
        }
    });
    form.addRow(tr("Outline Color:"), colorBtn);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form.addRow(buttons);

    if (dialog.exec() == QDialog::Accepted) {
        Settings::instance().setOutlineWidth(widthSpin->value());
        Settings::instance().setOutlineColor(outlineColor);
        d->canvas->update();
        Logger::info("Outline settings updated");
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
    populateSkinMenu();
    Logger::debug("UI retranslated");
}