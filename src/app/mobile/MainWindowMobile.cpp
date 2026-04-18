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
#include <QLabel>
#include <QGuiApplication>
#include <QMenuBar>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QSizePolicy>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
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
        const int windowWidth = width() > 0
                                    ? width()
                                    : (QGuiApplication::primaryScreen()
                                           ? QGuiApplication::primaryScreen()->availableGeometry().width()
                                           : 1080);
        const int boundedWidth = qMax(480, windowWidth);
        const int maxSideWidth = qMax(180, boundedWidth / 3);
        int sideWidth = qBound(140, boundedWidth / 5, maxSideWidth);
        int canvasWidth = boundedWidth - sideWidth * 2;
        if (canvasWidth < 220)
        {
            sideWidth = qMax(120, (boundedWidth - 220) / 2);
            canvasWidth = qMax(220, boundedWidth - sideWidth * 2);
        }

        if (d->leftPanel)
        {
            d->leftPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
            d->leftPanel->setMaximumWidth(maxSideWidth);
        }
        if (d->rightPanelContainer)
        {
            d->rightPanelContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
            d->rightPanelContainer->setMaximumWidth(maxSideWidth);
        }

        d->splitter->setCollapsible(0, true);
        d->splitter->setCollapsible(1, false);
        d->splitter->setCollapsible(2, true);
        d->splitter->setSizes({sideWidth, canvasWidth, sideWidth});
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

    auto cleanText = [](QString text) {
        text.remove('&');
        text = text.trimmed();
        return text;
    };

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Function Hub (Debug)"));
    dialog.setMinimumSize(700, 560);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *hint = new QLabel(tr("This is a debug-only function list. Double-click an item to run it."), &dialog);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    QLineEdit *search = new QLineEdit(&dialog);
    search->setPlaceholderText(tr("Search functions..."));
    layout->addWidget(search);

    QTreeWidget *tree = new QTreeWidget(&dialog);
    tree->setColumnCount(1);
    tree->setHeaderHidden(true);
    tree->setRootIsDecorated(true);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(tree, 1);

    auto appendMenu = [&](auto &self, QMenu *menu, QTreeWidgetItem *parentItem) -> void {
        if (!menu)
            return;
        for (QAction *action : menu->actions())
        {
            if (!action || action->isSeparator())
                continue;

            if (QMenu *subMenu = action->menu())
            {
                const QString subTitle = cleanText(subMenu->title());
                if (subTitle.isEmpty())
                    continue;
                QTreeWidgetItem *subItem = new QTreeWidgetItem(parentItem, QStringList{subTitle});
                self(self, subMenu, subItem);
                continue;
            }

            const QString title = cleanText(action->text());
            if (title.isEmpty())
                continue;

            QTreeWidgetItem *leaf = new QTreeWidgetItem(parentItem, QStringList{title});
            leaf->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(reinterpret_cast<qulonglong>(action)));
            if (!action->isEnabled())
            {
                leaf->setDisabled(true);
                leaf->setToolTip(0, tr("Currently unavailable"));
            }
        }
    };

    for (QAction *topLevel : menuBar()->actions())
    {
        if (!topLevel || !topLevel->menu())
            continue;
        const QString rootTitle = cleanText(topLevel->text());
        if (rootTitle.isEmpty())
            continue;
        QTreeWidgetItem *root = new QTreeWidgetItem(tree, QStringList{rootTitle});
        appendMenu(appendMenu, topLevel->menu(), root);
    }
    tree->expandToDepth(1);

    auto runSelectedAction = [tree]() {
        QTreeWidgetItem *current = tree->currentItem();
        if (!current)
            return;
        QAction *action = reinterpret_cast<QAction *>(current->data(0, Qt::UserRole).value<qulonglong>());
        if (action && action->isEnabled())
            action->trigger();
    };

    auto filterTree = [](QTreeWidgetItem *item, const QString &keyword, auto &self) -> bool {
        const bool selfMatch = keyword.isEmpty() || item->text(0).contains(keyword, Qt::CaseInsensitive);
        bool childMatch = false;
        for (int i = 0; i < item->childCount(); ++i)
        {
            if (self(item->child(i), keyword, self))
                childMatch = true;
        }
        const bool visible = selfMatch || childMatch;
        item->setHidden(!visible);
        if (visible && !keyword.isEmpty() && item->childCount() > 0)
            item->setExpanded(true);
        return visible;
    };

    connect(search, &QLineEdit::textChanged, &dialog, [tree, filterTree](const QString &text) mutable {
        const QString keyword = text.trimmed();
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
        {
            filterTree(tree->topLevelItem(i), keyword, filterTree);
        }
    });
    connect(tree, &QTreeWidget::itemDoubleClicked, &dialog, [&dialog](QTreeWidgetItem *item, int) {
        QAction *action = reinterpret_cast<QAction *>(item->data(0, Qt::UserRole).value<qulonglong>());
        if (action && action->isEnabled())
        {
            action->trigger();
            dialog.accept();
        }
    });

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QPushButton *runBtn = buttons->addButton(tr("Run"), QDialogButtonBox::AcceptRole);
    connect(runBtn, &QPushButton::clicked, &dialog, runSelectedAction);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.exec();
}
