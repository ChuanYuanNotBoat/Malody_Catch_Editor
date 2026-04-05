#include "MainWindow.h"
#include "ui/CustomWidgets/ChartCanvas.h"
#include "ui/NoteEditPanel.h"
#include "ui/BPMTimePanel.h"
#include "ui/MetaEditPanel.h"
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
#include <algorithm>
#include <QDebug>
#include <QSlider>
#include <QPushButton>
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
#include <QTemporaryDir>

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
    
    // MCZ support
    QTemporaryDir* mczTempDir;
    QString currentChartPath;
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
    d->mczTempDir = nullptr;
    d->currentChartPath = "";

    setupUi();
    createCentralArea();
    createMenus();
    retranslateUi();

    connect(d->chartController, &ChartController::chartChanged, this, [this]() {
        d->canvas->update();
        d->undoAction->setEnabled(d->chartController->canUndo());
        d->redoAction->setEnabled(d->chartController->canRedo());
        // 更新选择控制器的音符列表引用并重新计算选中索引
        if (d->selectionController) {
            d->selectionController->setNotes(&(d->chartController->chart()->notes()));
            d->selectionController->updateSelectionFromNotes();
        }
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
    if (d->mczTempDir) {
        delete d->mczTempDir;
        d->mczTempDir = nullptr;
    }
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
    editMenu->addSeparator();
    QAction* deleteAction = editMenu->addAction(tr("&Delete"));
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this, [this]() {
        // 删除选中的音符
        if (d->selectionController && !d->selectionController->selectedIndices().isEmpty()) {
            QSet<int> selected = d->selectionController->selectedIndices();
            const auto& notes = d->chartController->chart()->notes();
            QList<int> sorted = selected.values();
            std::sort(sorted.begin(), sorted.end(), std::greater<int>());
            for (int idx : sorted) {
                if (idx >= 0 && idx < notes.size()) {
                    d->chartController->removeNote(notes[idx]);
                }
            }
            d->selectionController->clearSelection();
            Logger::debug("Deleted selected notes via menu");
        }
    });

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
    toolsMenu->addSeparator();
    QAction* logSettingsAction = toolsMenu->addAction(tr("&Log Settings..."));
    connect(logSettingsAction, &QAction::triggered, this, &MainWindow::openLogSettings);
    QAction* exportDiagAction = toolsMenu->addAction(tr("&Export Diagnostics Report..."));
    connect(exportDiagAction, &QAction::triggered, this, &MainWindow::exportDiagnosticsReport);

    // 皮肤菜单
    d->skinMenu = menuBar()->addMenu(tr("&Skin"));
    populateSkinMenu();

    Logger::debug("Menus created");
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
    if (!d->skin) {
        QMessageBox::information(this, tr("No Skin"), tr("No skin loaded, cannot calibrate."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Calibrate Skin: %1").arg(d->skin->title()));
    dialog.setMinimumSize(600, 400);
    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    QTabWidget* tabs = new QTabWidget;
    QStringList typeNames = {"1/1", "1/2", "1/4", "1/8/16/32", "1/3/6/12/24", "Rain"};
    for (int i = 0; i <= 5; ++i) {
        QWidget* page = new QWidget;
        QVBoxLayout* pageLayout = new QVBoxLayout(page);

        QHBoxLayout* scaleLayout = new QHBoxLayout;
        QLabel* scaleLabel = new QLabel(tr("Scale Factor:"));
        QDoubleSpinBox* scaleSpin = new QDoubleSpinBox;
        scaleSpin->setRange(0.2, 3.0);
        scaleSpin->setSingleStep(0.05);
        scaleSpin->setValue(d->skin->getNoteScale(i));
        scaleLayout->addWidget(scaleLabel);
        scaleLayout->addWidget(scaleSpin);
        pageLayout->addLayout(scaleLayout);

        QLabel* previewLabel = new QLabel;
        previewLabel->setFixedSize(128, 128);
        previewLabel->setStyleSheet("border: 1px solid gray; background: white;");
        previewLabel->setAlignment(Qt::AlignCenter);
        pageLayout->addWidget(previewLabel, 0, Qt::AlignCenter);

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

        pageLayout->addStretch();
        tabs->addTab(page, typeNames[i]);

        scaleSpin->setProperty("noteType", i);
        page->setProperty("scaleSpin", QVariant::fromValue(scaleSpin));
    }
    layout->addWidget(tabs);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
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
    populateSkinMenu();
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

    connect(d->notePanel, &NoteEditPanel::timeDivisionChanged, d->canvas, &ChartCanvas::setTimeDivision);
    connect(d->notePanel, &NoteEditPanel::modeChanged, d->canvas, [this](int mode) {
        d->canvas->setMode(static_cast<ChartCanvas::Mode>(mode));
    });
    Logger::debug("Connected signals");

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
    try {
        QString fileName = QFileDialog::getOpenFileName(this, tr("Open Chart"), Settings::instance().lastOpenPath(),
                                                        tr("Malody Charts (*.mc *.mcz);;Malody Catch Chart (*.mc);;Malody Catch Pack (*.mcz);;All Files (*.*)"));
        if (fileName.isEmpty()) {
            Logger::debug("Open chart cancelled");
            return;
        }
        
        Logger::info(QString("MainWindow::openChart - Opening file: %1").arg(fileName));
        
        QString chartFileToLoad = fileName;
        QFileInfo fileInfo(fileName);
        QString suffix = fileInfo.suffix().toLower();
        
        // Handle MCZ files
        if (suffix == "mcz") {
            Logger::debug("MainWindow::openChart - Detected MCZ file format");
            
            if (d->mczTempDir) {
                Logger::debug("MainWindow::openChart - Cleaning up previous MCZ temp directory");
                delete d->mczTempDir;
                d->mczTempDir = nullptr;
            }
            
            d->mczTempDir = new QTemporaryDir();
            if (!d->mczTempDir->isValid()) {
                Logger::error("MainWindow::openChart - Failed to create temporary directory for MCZ");
                QMessageBox::critical(this, tr("Error"), tr("Failed to create temporary directory for MCZ."));
                return;
            }
            
            Logger::debug(QString("MainWindow::openChart - Created temp directory: %1").arg(d->mczTempDir->path()));
            
            QString importDir = d->mczTempDir->path() + "/" + fileInfo.baseName();
            QString extractedChart;
            
            if (!ProjectIO::importMcz(fileName, importDir, extractedChart)) {
                Logger::error(QString("MainWindow::openChart - Failed to import MCZ: %1").arg(fileName));
                QMessageBox::critical(this, tr("Error"), tr("Failed to import MCZ file."));
                return;
            }
            
            Logger::debug("MainWindow::openChart - MCZ imported successfully");
            
            // Check if there are multiple .mc files
            QList<QPair<QString, QString>> charts = ProjectIO::findChartsInMcz(importDir);
            Logger::debug(QString("MainWindow::openChart - Found %1 charts in MCZ").arg(charts.size()));
            
            if (charts.isEmpty()) {
                Logger::error("MainWindow::openChart - No .mc files found in MCZ");
                QMessageBox::critical(this, tr("Error"), tr("No chart files found in MCZ."));
                return;
            }
            
            if (charts.size() > 1) {
                // Show selection dialog for multiple charts
                Logger::debug("MainWindow::openChart - Multiple charts detected, showing selection dialog");
                QDialog selectDialog(this);
                selectDialog.setWindowTitle(tr("Select Chart"));
                selectDialog.setMinimumWidth(300);
                
                QVBoxLayout* layout = new QVBoxLayout(&selectDialog);
                layout->addWidget(new QLabel(tr("Multiple charts found. Please select one:")));
                
                QListWidget* chartList = new QListWidget();
                int selectedIndex = 0;
                for (int i = 0; i < charts.size(); ++i) {
                    const auto& chart = charts[i];
                    QString displayText = QString("%1 (%2)").arg(QFileInfo(chart.first).baseName(), chart.second);
                    chartList->addItem(displayText);
                    if (i == 0) {
                        chartList->setCurrentRow(0);
                    }
                }
                
                layout->addWidget(chartList);
                
                QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
                connect(buttons, &QDialogButtonBox::accepted, &selectDialog, &QDialog::accept);
                connect(buttons, &QDialogButtonBox::rejected, &selectDialog, &QDialog::reject);
                layout->addWidget(buttons);
                
                if (selectDialog.exec() != QDialog::Accepted) {
                    Logger::debug("MainWindow::openChart - Chart selection cancelled");
                    return;
                }
                
                selectedIndex = chartList->currentRow();
                if (selectedIndex < 0 || selectedIndex >= charts.size()) {
                    selectedIndex = 0;
                }
                
                chartFileToLoad = charts[selectedIndex].first;
                Logger::info(QString("MainWindow::openChart - Selected chart: %1").arg(chartFileToLoad));
            } else {
                chartFileToLoad = charts.first().first;
                Logger::debug(QString("MainWindow::openChart - Only one chart found: %1").arg(chartFileToLoad));
            }
        }
        
        // Load the chart file
        if (d->chartController->loadChart(chartFileToLoad)) {
            Logger::debug("MainWindow::openChart - Chart loaded successfully");
            d->currentChartPath = chartFileToLoad;
            Settings::instance().setLastOpenPath(QFileInfo(fileName).path());
            
            // Try to load audio file
            try {
                QString chartDir = QFileInfo(chartFileToLoad).absolutePath();
                QString audioFile = d->chartController->chart()->meta().audioFile;
                
                Logger::debug(QString("MainWindow::openChart - Audio file from meta: %1").arg(audioFile));
                
                if (!audioFile.isEmpty()) {
                    QString audioPath = chartDir + "/" + audioFile;
                    Logger::debug(QString("MainWindow::openChart - Full audio path: %1").arg(audioPath));
                    
                    if (QFile::exists(audioPath)) {
                        Logger::info(QString("MainWindow::openChart - Found audio file: %1").arg(audioPath));
                        if (d->playbackController->audioPlayer()->load(audioPath)) {
                            Logger::info(QString("MainWindow::openChart - Audio loaded successfully: %1").arg(audioPath));
                        } else {
                            Logger::warn(QString("MainWindow::openChart - Failed to load audio file: %1").arg(audioPath));
                        }
                    } else {
                        Logger::warn(QString("MainWindow::openChart - Audio file not found: %1").arg(audioPath));
                    }
                } else {
                    Logger::debug("MainWindow::openChart - No audio file specified in chart meta");
                }
            } catch (const std::exception& e) {
                Logger::error(QString("MainWindow::openChart - Exception loading audio: %1").arg(e.what()));
            } catch (...) {
                Logger::error("MainWindow::openChart - Unknown exception loading audio");
            }
            
            d->canvas->update();
            Logger::info(QString("MainWindow::openChart - Canvas update called for: %1").arg(chartFileToLoad));
            
            // Force process events to ensure paint happens
            QApplication::processEvents();
            Logger::info(QString("MainWindow::openChart - Events processed successfully"));
            
            Logger::info("Chart loaded: " + chartFileToLoad);
        } else {
            Logger::error("Failed to load chart: " + chartFileToLoad);
            QMessageBox::critical(this, tr("Error"), tr("Failed to load chart."));
            Logger::error(QString("MainWindow::openChart - Exiting with error"));
            return;
        }
    } catch (const std::exception& e) {
        Logger::error(QString("MainWindow::openChart - Exception: %1").arg(e.what()));
        QMessageBox::critical(this, tr("Error"), tr("Exception opening chart: %1").arg(e.what()));
    } catch (...) {
        Logger::error("MainWindow::openChart - Unknown exception");
        QMessageBox::critical(this, tr("Error"), tr("Unknown exception opening chart."));
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
    
    if (d->currentChartPath.isEmpty()) {
        QMessageBox::information(this, tr("No Chart"), tr("Please open a chart first before exporting."));
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export .mcz"), Settings::instance().lastOpenPath(),
                                                    tr("Malody Catch Pack (*.mcz);;All Files (*.*)"));
    if (fileName.isEmpty()) {
        Logger::debug("Export .mcz cancelled");
        return;
    }
    
    try {
        Logger::info(QString("MainWindow::exportMcz - Exporting to: %1").arg(fileName));
        
        if (ProjectIO::exportToMcz(fileName, d->currentChartPath)) {
            statusBar()->showMessage(tr("Exported: %1").arg(fileName), 3000);
            Logger::info(QString("MainWindow::exportMcz - Successfully exported to: %1").arg(fileName));
            QMessageBox::information(this, tr("Success"), tr("Chart exported successfully to:\n%1").arg(fileName));
        } else {
            Logger::error(QString("MainWindow::exportMcz - Failed to export to: %1").arg(fileName));
            QMessageBox::critical(this, tr("Error"), tr("Failed to export chart to MCZ format."));
        }
    } catch (const std::exception& e) {
        Logger::error(QString("MainWindow::exportMcz - Exception: %1").arg(e.what()));
        QMessageBox::critical(this, tr("Error"), tr("Exception during export: %1").arg(e.what()));
    } catch (...) {
        Logger::error("MainWindow::exportMcz - Unknown exception");
        QMessageBox::critical(this, tr("Error"), tr("Unknown exception during export."));
    }
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
    populateSkinMenu();
    Logger::debug("UI retranslated");
}

void MainWindow::openLogSettings()
{
    Logger::info("Log settings dialog opened");
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Log Settings"));
    dialog.setMinimumSize(400, 300);
    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    // JSON logging enable/disable
    QCheckBox* jsonLoggingCheck = new QCheckBox(tr("Enable JSON Logging"));
    jsonLoggingCheck->setChecked(Logger::isJsonLoggingEnabled());
    layout->addWidget(jsonLoggingCheck);

    // Verbose logging enable/disable
    QCheckBox* verboseCheck = new QCheckBox(tr("Enable Verbose Logging"));
    verboseCheck->setChecked(Logger::isVerbose());
    layout->addWidget(verboseCheck);

    // Log file location display
    QHBoxLayout* logPathLayout = new QHBoxLayout;
    QLabel* pathLabel = new QLabel(tr("Log File:"));
    QLineEdit* pathEdit = new QLineEdit;
    pathEdit->setText(Logger::logFilePath());
    pathEdit->setReadOnly(true);
    QPushButton* openLogBtn = new QPushButton(tr("Open Log Folder"));
    connect(openLogBtn, &QPushButton::clicked, [this]() {
        QString logDir = QFileInfo(Logger::logFilePath()).absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(logDir));
        Logger::debug("Opened log folder");
    });
    logPathLayout->addWidget(pathLabel);
    logPathLayout->addWidget(pathEdit);
    logPathLayout->addWidget(openLogBtn);
    layout->addLayout(logPathLayout);

    // JSON log file location display
    QHBoxLayout* jsonPathLayout = new QHBoxLayout;
    QLabel* jsonPathLabel = new QLabel(tr("JSON Log File:"));
    QLineEdit* jsonPathEdit = new QLineEdit;
    jsonPathEdit->setText(Logger::jsonLogFilePath());
    jsonPathEdit->setReadOnly(true);
    jsonPathLayout->addWidget(jsonPathLabel);
    jsonPathLayout->addWidget(jsonPathEdit);
    layout->addLayout(jsonPathLayout);

    layout->addStretch();

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        Logger::setJsonLoggingEnabled(jsonLoggingCheck->isChecked());
        Logger::setVerbose(verboseCheck->isChecked());
        Logger::info(QString("Log settings changed - JSON logging: %1, Verbose: %2")
                     .arg(jsonLoggingCheck->isChecked() ? "enabled" : "disabled")
                     .arg(verboseCheck->isChecked() ? "enabled" : "disabled"));
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::exportDiagnosticsReport()
{
    Logger::info("Diagnostics report export requested");
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export Diagnostics Report"),
                                                    Settings::instance().lastOpenPath(),
                                                    tr("Text Files (*.txt);;JSON Files (*.json);;All Files (*.*)"));
    if (fileName.isEmpty()) {
        Logger::debug("Export diagnostics cancelled");
        return;
    }

    try {
        // Get diagnostics report from DiagnosticCollector
        DiagnosticCollector& collector = DiagnosticCollector::instance();
        DiagnosticCollector::DiagnosticReport report = collector.generateReport();

        // Export based on file extension
        if (fileName.endsWith(".json")) {
            // Export as JSON
            QJsonDocument doc = collector.toJsonDocument();
            QFile file(fileName);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(doc.toJson());
                file.close();
                Logger::info("Diagnostics report exported to JSON: " + fileName);
                QMessageBox::information(this, tr("Export Successful"), 
                                       tr("Diagnostics report exported to:\n%1").arg(fileName));
            } else {
                Logger::error("Failed to open file for writing: " + fileName);
                QMessageBox::warning(this, tr("Export Failed"), tr("Failed to open file for writing."));
            }
        } else {
            // Export as formatted text
            QFile file(fileName);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&file);
                stream << report.toFormattedString();
                file.close();
                Logger::info("Diagnostics report exported to text: " + fileName);
                QMessageBox::information(this, tr("Export Successful"), 
                                       tr("Diagnostics report exported to:\n%1").arg(fileName));
            } else {
                Logger::error("Failed to open file for writing: " + fileName);
                QMessageBox::warning(this, tr("Export Failed"), tr("Failed to open file for writing."));
            }
        }
    } catch (const std::exception& e) {
        Logger::error(QString("Exception during diagnostics export: %1").arg(e.what()));
        QMessageBox::critical(this, tr("Error"), tr("Exception during export:\n%1").arg(e.what()));
    }
}