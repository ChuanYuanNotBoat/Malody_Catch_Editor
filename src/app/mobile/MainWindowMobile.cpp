#include "app/MainWindow.h"
#include "app/MainWindowPrivate.h"
#include "ui/LeftPanel.h"
#include "utils/Settings.h"
#include "utils/Logger.h"

#include <QAction>
#include <QDockWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QMenuBar>
#include <QMenu>
#include <QPushButton>
#include <QSplitter>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <functional>

bool MainWindow::useCompactMobileLayout() const
{
#if defined(Q_OS_ANDROID)
    return true;
#else
    return Settings::instance().mobileUiTestMode();
#endif
}

void MainWindow::setupMobileCentralArea(QWidget *canvasContainer)
{
    if (!canvasContainer)
        return;

    d->compactUiMode = true;
    d->mobileTabs = nullptr;
    if (d->leftDock)
    {
        d->leftDock->hide();
        d->leftDock->deleteLater();
        d->leftDock = nullptr;
    }
    if (d->rightDock)
    {
        d->rightDock->hide();
        d->rightDock->deleteLater();
        d->rightDock = nullptr;
    }

    if (d->splitter)
    {
        d->splitter->setCollapsible(0, true);
        d->splitter->setCollapsible(1, false);
        d->splitter->setCollapsible(2, true);
        d->splitter->setSizes({150, 800, 300});
        setCentralWidget(d->splitter);
    }

    Logger::info("Compact mobile layout enabled: reuse desktop splitter layout with hideable side panels.");
}

void MainWindow::populateMobilePrimaryToolbar()
{
    if (!useCompactMobileLayout() || !d->mainToolBar)
        return;

    d->mainToolBar->setMovable(false);
    d->mainToolBar->setFloatable(false);
    d->mainToolBar->setAllowedAreas(Qt::TopToolBarArea);
    addToolBar(Qt::TopToolBarArea, d->mainToolBar);
    d->mainToolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    QAction *anchor = d->mainToolBar->actions().isEmpty() ? nullptr : d->mainToolBar->actions().first();
    auto insertPrimaryAction = [this, anchor](const QString &text, auto slot) -> QAction * {
        QAction *action = new QAction(text, d->mainToolBar);
        connect(action, &QAction::triggered, this, slot);
        if (anchor)
            d->mainToolBar->insertAction(anchor, action);
        else
            d->mainToolBar->addAction(action);
        return action;
    };

    d->mobileOpenAction = insertPrimaryAction(tr("Open"), &MainWindow::openChart);
    d->mobileSaveAction = insertPrimaryAction(tr("Save"), &MainWindow::saveChart);
    d->mobilePlayAction = insertPrimaryAction(tr("Play"), &MainWindow::togglePlayback);
    d->mobileToggleLeftPanelAction = insertPrimaryAction(tr("Hide Left"), [this]() {
        if (!d->leftPanel)
            return;
        const bool shouldShow = !d->leftPanel->isVisible();
        d->leftPanel->setVisible(shouldShow);
        if (d->mobileToggleLeftPanelAction)
            d->mobileToggleLeftPanelAction->setText(shouldShow ? tr("Hide Left") : tr("Show Left"));
    });
    d->mobileToggleRightPanelAction = insertPrimaryAction(tr("Hide Right"), [this]() {
        if (!d->rightPanelContainer)
            return;
        const bool shouldShow = !d->rightPanelContainer->isVisible();
        d->rightPanelContainer->setVisible(shouldShow);
        if (d->mobileToggleRightPanelAction)
            d->mobileToggleRightPanelAction->setText(shouldShow ? tr("Hide Right") : tr("Show Right"));
    });
    d->mobileFunctionsAction = insertPrimaryAction(tr("Functions"), &MainWindow::openMobileFunctionHub);

    if (menuBar())
        menuBar()->setVisible(false);
}

void MainWindow::retranslateMobileUi()
{
    if (!useCompactMobileLayout())
        return;

    if (d->mobileOpenAction)
        d->mobileOpenAction->setText(tr("Open"));
    if (d->mobileSaveAction)
        d->mobileSaveAction->setText(tr("Save"));
    if (d->mobilePlayAction)
        d->mobilePlayAction->setText(tr("Play"));
    if (d->mobileToggleLeftPanelAction)
        d->mobileToggleLeftPanelAction->setText((d->leftPanel && d->leftPanel->isVisible()) ? tr("Hide Left") : tr("Show Left"));
    if (d->mobileToggleRightPanelAction)
        d->mobileToggleRightPanelAction->setText((d->rightPanelContainer && d->rightPanelContainer->isVisible()) ? tr("Hide Right") : tr("Show Right"));
    if (d->mobileFunctionsAction)
        d->mobileFunctionsAction->setText(tr("Functions"));
}

void MainWindow::openMobileFunctionHub()
{
    if (!menuBar())
        return;

    struct FunctionItem
    {
        QString path;
        QAction *action = nullptr;
    };

    auto cleanText = [](QString text) {
        text.remove('&');
        text = text.trimmed();
        return text;
    };

    QList<FunctionItem> functions;
    std::function<void(QMenu *, const QString &)> collectFromMenu = [&](QMenu *menu, const QString &prefix) {
        if (!menu)
            return;
        for (QAction *action : menu->actions())
        {
            if (!action || action->isSeparator())
                continue;

            if (QMenu *subMenu = action->menu())
            {
                const QString next = prefix.isEmpty()
                                         ? cleanText(subMenu->title())
                                         : QString("%1 > %2").arg(prefix, cleanText(subMenu->title()));
                collectFromMenu(subMenu, next);
                continue;
            }

            const QString title = cleanText(action->text());
            if (title.isEmpty())
                continue;
            const QString path = prefix.isEmpty() ? title : QString("%1 > %2").arg(prefix, title);
            functions.append({path, action});
        }
    };

    for (QAction *topLevel : menuBar()->actions())
    {
        if (!topLevel || !topLevel->menu())
            continue;
        collectFromMenu(topLevel->menu(), cleanText(topLevel->text()));
    }

    if (functions.isEmpty())
        return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Function Hub (Debug)"));
    dialog.setMinimumSize(620, 520);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *hint = new QLabel(tr("This is a debug-only function list. Double-click an item to run it."), &dialog);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    QLineEdit *search = new QLineEdit(&dialog);
    search->setPlaceholderText(tr("Search functions..."));
    layout->addWidget(search);

    QListWidget *list = new QListWidget(&dialog);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(list, 1);

    for (const FunctionItem &item : std::as_const(functions))
    {
        QListWidgetItem *row = new QListWidgetItem(item.path, list);
        row->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(reinterpret_cast<qulonglong>(item.action)));
        if (!item.action->isEnabled())
        {
            row->setFlags(row->flags() & ~Qt::ItemIsEnabled);
            row->setToolTip(tr("Currently unavailable"));
        }
    }

    auto runSelectedAction = [list]() {
        QListWidgetItem *current = list->currentItem();
        if (!current)
            return;
        QAction *action = reinterpret_cast<QAction *>(current->data(Qt::UserRole).value<qulonglong>());
        if (action && action->isEnabled())
            action->trigger();
    };

    connect(search, &QLineEdit::textChanged, &dialog, [list](const QString &text) {
        const QString keyword = text.trimmed();
        for (int i = 0; i < list->count(); ++i)
        {
            QListWidgetItem *item = list->item(i);
            const bool visible = keyword.isEmpty() || item->text().contains(keyword, Qt::CaseInsensitive);
            item->setHidden(!visible);
        }
    });
    connect(list, &QListWidget::itemDoubleClicked, &dialog, [&dialog, runSelectedAction](QListWidgetItem *) {
        runSelectedAction();
        dialog.accept();
    });

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QPushButton *runBtn = buttons->addButton(tr("Run"), QDialogButtonBox::AcceptRole);
    connect(runBtn, &QPushButton::clicked, &dialog, runSelectedAction);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.exec();
}
