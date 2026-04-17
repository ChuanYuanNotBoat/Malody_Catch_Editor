#include "MainWindow.h"
#include "MainWindowPrivate.h"
#include "app/Application.h"
#include "controller/ChartController.h"
#include "plugin/PluginManager.h"
#include "ui/dialogs/LogSettingsDialog.h"
#include "ui/dialogs/PluginManagerDialog.h"
#include "ui/CustomWidgets/ChartCanvas/ChartCanvas.h"
#include "ui/LeftPanel.h"
#include "file/ChartIO.h"
#include "utils/Settings.h"
#include "utils/Logger.h"
#include "utils/DiagnosticCollector.h"
#include "model/Skin.h"
#include "model/Note.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QMenu>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDir>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QSpinBox>
#include <QColorDialog>
#include <QPainter>
#include <QTabWidget>
#include <QDoubleSpinBox>
#include <QVariant>
#include <QSlider>
#include <QCoreApplication>
#include <QAction>
#include <QStatusBar>
#include <QToolBar>

void MainWindow::populatePluginToolsMenu()
{
    if (!d->pluginToolsMenu)
        return;

    d->pluginToolsMenu->clear();
    auto *app = qobject_cast<Application *>(QCoreApplication::instance());
    if (!app || !app->pluginManager())
    {
        QAction *none = d->pluginToolsMenu->addAction(tr("(Plugin manager unavailable)"));
        none->setEnabled(false);
        return;
    }

    const QList<PluginManager::ToolActionEntry> entries = app->pluginManager()->toolActions();
    if (entries.isEmpty())
    {
        QAction *none = d->pluginToolsMenu->addAction(tr("(No plugin actions)"));
        none->setEnabled(false);
        return;
    }

    bool added = false;
    for (const PluginManager::ToolActionEntry &entry : entries)
    {
        const QString text = QString("%1  [%2]").arg(entry.action.title, entry.pluginDisplayName);
        QAction *act = d->pluginToolsMenu->addAction(text);
        if (!entry.action.description.isEmpty())
            act->setToolTip(entry.action.description);

        QVariantMap payload;
        payload.insert("plugin_id", entry.pluginId);
        payload.insert("action_id", entry.action.actionId);
        payload.insert("title", entry.action.title);
        payload.insert("confirm_message", entry.action.confirmMessage);
        payload.insert("requires_undo_snapshot", entry.action.requiresUndoSnapshot);
        payload.insert("placement", entry.action.placement);
        act->setData(payload);
        connect(act, &QAction::triggered, this, &MainWindow::triggerPluginToolAction);
        added = true;
    }

    if (!added)
    {
        QAction *none = d->pluginToolsMenu->addAction(tr("(No menu actions)"));
        none->setEnabled(false);
    }
}

void MainWindow::refreshPluginUiExtensions()
{
    auto *app = qobject_cast<Application *>(QCoreApplication::instance());
    if (!app || !app->pluginManager())
    {
        Logger::warn("refreshPluginUiExtensions skipped: plugin manager unavailable.");
        return;
    }

    d->pluginActionMeta.clear();
    for (QAction *a : d->pluginToolbarActions)
    {
        if (d->mainToolBar && a)
            d->mainToolBar->removeAction(a);
        delete a;
    }
    d->pluginToolbarActions.clear();

    QList<LeftPanel::PluginQuickAction> sidebarActions;
    const QList<PluginManager::ToolActionEntry> entries = app->pluginManager()->toolActions();
    Logger::info(QString("refreshPluginUiExtensions: discovered %1 plugin tool actions.").arg(entries.size()));
    for (const PluginManager::ToolActionEntry &entry : entries)
    {
        QVariantMap meta;
        meta.insert("plugin_id", entry.pluginId);
        meta.insert("action_id", entry.action.actionId);
        meta.insert("title", entry.action.title);
        meta.insert("confirm_message", entry.action.confirmMessage);
        meta.insert("requires_undo_snapshot", entry.action.requiresUndoSnapshot);
        meta.insert("placement", entry.action.placement);

        const QString key = entry.pluginId + "::" + entry.action.actionId;
        d->pluginActionMeta.insert(key, meta);

        const QString placement = entry.action.placement.toLower();
        if (placement == QString(PluginInterface::kPlacementTopToolbar) && d->mainToolBar)
        {
            QAction *act = d->mainToolBar->addAction(entry.action.title);
            if (!entry.action.description.isEmpty())
                act->setToolTip(entry.action.description);
            act->setData(meta);
            connect(act, &QAction::triggered, this, &MainWindow::triggerPluginToolAction);
            d->pluginToolbarActions.append(act);
        }
        else if (placement == QString(PluginInterface::kPlacementLeftSidebar))
        {
            LeftPanel::PluginQuickAction qa;
            qa.pluginId = entry.pluginId;
            qa.actionId = entry.action.actionId;
            qa.title = entry.action.title;
            qa.tooltip = entry.action.description;
            sidebarActions.append(qa);
        }
    }

    if (d->leftPanel)
        d->leftPanel->setPluginQuickActions(sidebarActions);
}

void MainWindow::triggerPluginToolAction()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action)
        return;
    runPluginActionWithMeta(action->data().toMap());
}

void MainWindow::triggerPluginQuickAction(const QString &pluginId, const QString &actionId)
{
    const QString key = pluginId + "::" + actionId;
    if (!d->pluginActionMeta.contains(key))
        return;
    runPluginActionWithMeta(d->pluginActionMeta.value(key));
}

bool MainWindow::runPluginActionWithMeta(const QVariantMap &meta)
{
    const QString pluginId = meta.value("plugin_id").toString();
    const QString actionId = meta.value("action_id").toString();
    const QString actionTitle = meta.value("title").toString();
    const QString confirmMessage = meta.value("confirm_message").toString();
    const bool requiresUndo = meta.value("requires_undo_snapshot", true).toBool();
    if (pluginId.isEmpty() || actionId.isEmpty())
        return false;

    if (!confirmMessage.isEmpty())
    {
        QMessageBox::StandardButton confirm = QMessageBox::warning(
            this,
            tr("Confirm Plugin Action"),
            confirmMessage,
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (confirm != QMessageBox::Yes)
            return false;
    }

    auto *app = qobject_cast<Application *>(QCoreApplication::instance());
    if (!app || !app->pluginManager() || !d->chartController)
        return false;

    const QString chartPath = d->chartController->chartFilePath();
    if (chartPath.isEmpty())
    {
        QMessageBox::information(this, tr("No Chart"), tr("Please open a chart first."));
        return false;
    }

    QVariantMap context;
    context.insert("chart_path", chartPath);
    context.insert("chart_path_native", QDir::toNativeSeparators(chartPath));
    context.insert("chart_path_canonical", QFileInfo(chartPath).canonicalFilePath());
    context.insert("action_title", actionTitle);
    Logger::info(QString("Running plugin action: plugin=%1 action=%2 path=%3")
                     .arg(pluginId)
                     .arg(actionId)
                     .arg(chartPath));

    if (!app->pluginManager()->runToolAction(pluginId, actionId, context))
    {
        Logger::warn(QString("Plugin action returned false: plugin=%1 action=%2 path=%3")
                         .arg(pluginId)
                         .arg(actionId)
                         .arg(chartPath));
        QMessageBox::warning(this, tr("Plugin Action"), tr("Plugin action failed: %1").arg(actionTitle));
        return false;
    }

    Chart mutated;
    if (!ChartIO::load(chartPath, mutated, false))
    {
        QMessageBox::warning(this, tr("Plugin Action"), tr("Plugin action finished, but failed to reload chart."));
        return false;
    }

    if (requiresUndo)
        d->chartController->applyExternalChartMutation(tr("Plugin Action: %1").arg(actionTitle), mutated);
    else
        d->chartController->loadChart(chartPath);

    statusBar()->showMessage(tr("Plugin action completed: %1").arg(actionTitle), 2500);
    return true;
}

void MainWindow::openPluginManager()
{
    auto *app = qobject_cast<Application *>(QCoreApplication::instance());
    if (!app || !app->pluginManager())
    {
        QMessageBox::warning(this, tr("Plugin Manager"), tr("Plugin manager is not available."));
        return;
    }

    PluginManagerDialog dialog(app->pluginManager(), this);
    dialog.exec();
    refreshPluginUiExtensions();
}
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

    QLabel *remarkLabel = new QLabel(tr("Note: This setting affects fallback circle notes. When a skin is loaded, note size is controlled by skin calibration."));
    remarkLabel->setWordWrap(true);
    remarkLabel->setStyleSheet("color: #666;");
    layout->addWidget(remarkLabel);

    QLabel *previewLabel = new QLabel;
    previewLabel->setFixedSize(128, 128);
    previewLabel->setStyleSheet("border: 1px solid gray; background: white;");
    previewLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(previewLabel, 0, Qt::AlignCenter);

    auto updatePreview = [previewLabel, sizeSpin, this]()
    {
        int sz = sizeSpin->value();
        QPixmap pix(128, 128);
        pix.fill(Qt::white);
        QPainter painter(&pix);
        if (d->skin && d->skin->isValid())
        {
            const QPixmap *notePix = d->skin->getNotePixmap(2);
            if (notePix && !notePix->isNull())
            {
                double scale = d->skin->getNoteScale(2);
                int scaledW = qMax(1, qRound(notePix->width() * scale));
                int scaledH = qMax(1, qRound(notePix->height() * scale));
                QPixmap scaled = notePix->scaled(scaledW, scaledH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                painter.drawPixmap((128 - scaled.width()) / 2, (128 - scaled.height()) / 2, scaled);
            }
            else
            {
                painter.setBrush(Qt::lightGray);
                painter.setPen(Qt::NoPen);
                painter.drawEllipse((128 - sz) / 2, (128 - sz) / 2, sz, sz);
            }
        }
        else
        {
            painter.setBrush(Qt::lightGray);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse((128 - sz) / 2, (128 - sz) / 2, sz, sz);
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

void MainWindow::adjustNoteSoundVolume()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Note Sound Volume"));
    dialog.setModal(true);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *valueLabel = new QLabel;
    valueLabel->setAlignment(Qt::AlignCenter);
    QSlider *slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, 200);
    slider->setValue(Settings::instance().noteSoundVolume());
    slider->setFixedSize(420, 26);
    slider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "height: 10px;"
        "border: 1px solid #666;"
        "background: #d8d8d8;"
        "border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "background: #444;"
        "width: 18px;"
        "margin: -5px 0;"
        "border-radius: 2px;"
        "}");

    auto refreshLabel = [valueLabel, slider]()
    { valueLabel->setText(QObject::tr("Volume: %1%").arg(slider->value())); };
    refreshLabel();
    connect(slider, &QSlider::valueChanged, this, [this, refreshLabel, slider]()
            {
        refreshLabel();
        if (d->canvas)
            d->canvas->setNoteSoundVolume(slider->value()); });

    layout->addWidget(valueLabel);
    layout->addWidget(slider, 0, Qt::AlignCenter);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    const int originalVolume = Settings::instance().noteSoundVolume();
    if (dialog.exec() == QDialog::Accepted)
    {
        Settings::instance().setNoteSoundVolume(slider->value());
        Logger::info(QString("Note sound volume set to %1%").arg(slider->value()));
    }
    else if (d->canvas)
    {
        d->canvas->setNoteSoundVolume(originalVolume);
    }
}







