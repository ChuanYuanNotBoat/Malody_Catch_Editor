#include "MainWindow.h"
#include "MainWindowPrivate.h"
#include "ui/CustomWidgets/ChartCanvas/ChartCanvas.h"
#include "ui/LeftPanel.h"
#include "utils/Logger.h"

#include <QAction>
#include <QDockWidget>
#include <QMenuBar>
#include <QScreen>
#include <QTimer>
#include <QToolBar>
#include <QWindow>

bool MainWindow::useCompactMobileLayout() const
{
#if defined(Q_OS_ANDROID)
    return true;
#else
    return false;
#endif
}

void MainWindow::setupMobileFloatingPanels(QWidget *canvasContainer)
{
    if (!canvasContainer)
        return;

    // Keep desktop splitter behavior untouched. Mobile uses floating side panels.
    d->compactUiMode = true;
    setCentralWidget(canvasContainer);

    if (!d->leftDock)
    {
        d->leftDock = new QDockWidget(tr("Left Panel"), this);
        d->leftDock->setObjectName("mobileLeftDock");
        d->leftDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
        d->leftDock->setAllowedAreas(Qt::AllDockWidgetAreas);
        d->leftDock->setWidget(d->leftPanel);
        addDockWidget(Qt::LeftDockWidgetArea, d->leftDock);
    }

    if (!d->rightDock)
    {
        d->rightDock = new QDockWidget(tr("Edit Panel"), this);
        d->rightDock->setObjectName("mobileRightDock");
        d->rightDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
        d->rightDock->setAllowedAreas(Qt::AllDockWidgetAreas);
        d->rightDock->setWidget(d->rightPanelContainer);
        addDockWidget(Qt::RightDockWidgetArea, d->rightDock);
    }

    d->leftDock->setFloating(true);
    d->rightDock->setFloating(true);

    QTimer::singleShot(0, this, [this]() {
        if (!d->leftDock || !d->rightDock)
            return;

        QRect availableRect = geometry();
        if (windowHandle() && windowHandle()->screen())
            availableRect = windowHandle()->screen()->availableGeometry();

        const int maxPanelWidth = qMax(220, availableRect.width() - 48);
        const int maxPanelHeight = qMax(260, availableRect.height() - 120);
        const int sideWidth = qBound(220, availableRect.width() / 2, maxPanelWidth);
        const int sideHeight = qBound(260, availableRect.height() / 2, maxPanelHeight);

        d->leftDock->resize(sideWidth, sideHeight);
        d->rightDock->resize(sideWidth, sideHeight);

        const int leftX = availableRect.left() + 16;
        const int rightX = qMax(availableRect.left() + 16, availableRect.right() - sideWidth - 16);
        const int topY = availableRect.top() + 56;

        d->leftDock->move(leftX, topY);
        d->rightDock->move(rightX, topY);
    });

    Logger::info("Compact mobile layout enabled: using floating side panels.");
}

void MainWindow::populateMobilePrimaryToolbar()
{
    if (!useCompactMobileLayout() || !d->mainToolBar)
        return;

    d->mainToolBar->setMovable(false);
    d->mainToolBar->setFloatable(false);
    d->mainToolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    d->mainToolBar->setVisible(true);

    QAction *anchor = d->mainToolBar->actions().isEmpty() ? nullptr : d->mainToolBar->actions().first();
    auto insertPrimaryAction = [this, anchor](const QString &text, auto slot) {
        QAction *action = new QAction(text, d->mainToolBar);
        connect(action, &QAction::triggered, this, slot);
        if (anchor)
            d->mainToolBar->insertAction(anchor, action);
        else
            d->mainToolBar->addAction(action);
        return action;
    };

    insertPrimaryAction(tr("Open"), &MainWindow::openChart);
    insertPrimaryAction(tr("Save"), &MainWindow::saveChart);
    insertPrimaryAction(tr("Play"), &MainWindow::togglePlayback);

    QAction *leftPanelAction = insertPrimaryAction(tr("Left"), [this]() {
        if (!d->leftDock)
            return;
        d->leftDock->setVisible(true);
        d->leftDock->raise();
    });
    Q_UNUSED(leftPanelAction);

    QAction *rightPanelAction = insertPrimaryAction(tr("Right"), [this]() {
        if (!d->rightDock)
            return;
        d->rightDock->setVisible(true);
        d->rightDock->raise();
    });
    Q_UNUSED(rightPanelAction);

    if (menuBar())
        menuBar()->setVisible(false);
}
