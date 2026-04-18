#include "app/MainWindow.h"
#include "app/MainWindowPrivate.h"
#include "ui/LeftPanel.h"
#include "utils/Settings.h"
#include "utils/Logger.h"

#include <QAction>
#include <QDockWidget>
#include <QMenuBar>
#include <QSplitter>
#include <QToolBar>
#include <QToolButton>

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
}
