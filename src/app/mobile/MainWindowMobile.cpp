#include "app/MainWindow.h"
#include "app/MainWindowPrivate.h"
#include "ui/LeftPanel.h"
#include "utils/Logger.h"

#include <QAction>
#include <QBoxLayout>
#include <QDockWidget>
#include <QMenuBar>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>

bool MainWindow::useCompactMobileLayout() const
{
#if defined(Q_OS_ANDROID)
    return true;
#else
    return false;
#endif
}

void MainWindow::setupMobileCentralArea(QWidget *canvasContainer)
{
    if (!canvasContainer)
        return;

    d->compactUiMode = true;
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

    QWidget *mobileRoot = new QWidget(this);
    QVBoxLayout *mobileLayout = new QVBoxLayout(mobileRoot);
    mobileLayout->setContentsMargins(0, 0, 0, 0);
    mobileLayout->setSpacing(0);
    mobileLayout->addWidget(canvasContainer, 1);

    d->mobileTabs = new QTabWidget(mobileRoot);
    d->mobileTabs->setObjectName("mobileMainTabs");
    d->mobileTabs->setTabPosition(QTabWidget::South);
    d->mobileTabs->setDocumentMode(true);
    d->mobileTabs->setElideMode(Qt::ElideRight);
    d->mobileTabs->addTab(d->leftPanel, tr("Library"));
    d->mobileTabs->addTab(d->rightPanelContainer, tr("Editor"));
    d->mobileTabs->setCurrentWidget(d->rightPanelContainer);
    mobileLayout->addWidget(d->mobileTabs, 0);

    setCentralWidget(mobileRoot);
    Logger::info("Compact mobile layout enabled: canvas + bottom tabbed panels.");
}

void MainWindow::populateMobilePrimaryToolbar()
{
    if (!useCompactMobileLayout() || !d->mainToolBar)
        return;

    d->mainToolBar->setMovable(false);
    d->mainToolBar->setFloatable(false);
    d->mainToolBar->setAllowedAreas(Qt::TopToolBarArea | Qt::BottomToolBarArea);
    addToolBar(Qt::BottomToolBarArea, d->mainToolBar);
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

    d->mobileLibraryAction = insertPrimaryAction(tr("Library"), [this]() {
        if (!d->mobileTabs)
            return;
        d->mobileTabs->setCurrentWidget(d->leftPanel);
    });

    d->mobileEditorAction = insertPrimaryAction(tr("Editor"), [this]() {
        if (!d->mobileTabs)
            return;
        d->mobileTabs->setCurrentWidget(d->rightPanelContainer);
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
    if (d->mobileLibraryAction)
        d->mobileLibraryAction->setText(tr("Library"));
    if (d->mobileEditorAction)
        d->mobileEditorAction->setText(tr("Editor"));

    if (d->mobileTabs)
    {
        const int libraryIndex = d->mobileTabs->indexOf(d->leftPanel);
        if (libraryIndex >= 0)
            d->mobileTabs->setTabText(libraryIndex, tr("Library"));
        const int editorIndex = d->mobileTabs->indexOf(d->rightPanelContainer);
        if (editorIndex >= 0)
            d->mobileTabs->setTabText(editorIndex, tr("Editor"));
    }
}
