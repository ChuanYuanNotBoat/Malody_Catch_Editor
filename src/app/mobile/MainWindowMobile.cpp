#include "app/MainWindow.h"
#include "app/MainWindowPrivate.h"
#include "ui/LeftPanel.h"
#include "ui/NoteEditPanel.h"
#include "ui/BPMTimePanel.h"
#include "ui/MetaEditPanel.h"
#include "ui/CustomWidgets/ChartCanvas/ChartCanvas.h"
#include "utils/Settings.h"
#include "utils/Logger.h"

#include <QAction>
#include <QQuickItem>
#include <QQuickWidget>
#include <QQmlError>
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
#include <QScrollBar>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVariant>
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
        if (d->canvas)
        {
            d->canvas->setMinimumSize(0, 0);
            d->canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        }
        if (d->verticalScrollBar)
            d->verticalScrollBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

        d->mobileLeftPanelHost = nullptr;
        d->mobileRightPanelHost = nullptr;

        if (d->leftPanel)
        {
            d->leftPanel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            d->leftPanel->setMinimumWidth(0);
        }
        if (d->rightPanelContainer)
        {
            d->rightPanelContainer->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            d->rightPanelContainer->setMinimumWidth(0);
        }

        d->splitter->setCollapsible(0, true);
        d->splitter->setCollapsible(1, false);
        d->splitter->setCollapsible(2, true);
        d->splitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        d->splitter->setMinimumSize(0, 0);
        d->splitter->setStretchFactor(0, 0);
        d->splitter->setStretchFactor(1, 1);
        d->splitter->setStretchFactor(2, 0);

        d->mobileShell = new QWidget(this);
        QVBoxLayout *shellLayout = new QVBoxLayout(d->mobileShell);
        shellLayout->setContentsMargins(0, 0, 0, 0);
        shellLayout->setSpacing(0);
        shellLayout->addWidget(d->splitter, 1);
        setCentralWidget(d->mobileShell);

        auto rebalanceSplitter = [this]() {
            if (!d->splitter)
                return;
            const int fallbackScreenWidth = QGuiApplication::primaryScreen()
                                                ? QGuiApplication::primaryScreen()->availableGeometry().width()
                                                : 1080;
            const int available = qMax(480, d->splitter->width() > 0 ? d->splitter->width() : fallbackScreenWidth);
            const int maxSideWidth = qBound(120, available / 3, 260);
            int sideWidth = qBound(80, available / 6, maxSideWidth);
            int canvasWidth = available - sideWidth * 2;
            if (canvasWidth < 220)
            {
                sideWidth = qMax(80, (available - 220) / 2);
                canvasWidth = qMax(220, available - sideWidth * 2);
            }

            if (d->leftPanel)
                d->leftPanel->setMaximumWidth(maxSideWidth);
            if (d->rightPanelContainer)
                d->rightPanelContainer->setMaximumWidth(maxSideWidth);
            d->splitter->setSizes({sideWidth, canvasWidth, sideWidth});
        };
        rebalanceSplitter();
        QTimer::singleShot(0, this, rebalanceSplitter);
        QTimer::singleShot(180, this, rebalanceSplitter);
    }

    Logger::info("Compact mobile layout enabled: reuse desktop splitter layout with hideable side panels.");
}

void MainWindow::populateMobilePrimaryToolbar()
{
    if (!useCompactMobileLayout())
        return;

    if (!d->mobilePrimaryBar)
    {
        d->mobilePrimaryBar = new QQuickWidget(this);
        d->mobilePrimaryBar->setResizeMode(QQuickWidget::SizeRootObjectToView);
        d->mobilePrimaryBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        d->mobilePrimaryBar->setMinimumHeight(64);
        d->mobilePrimaryBar->setMaximumHeight(64);
        d->mobilePrimaryBar->setAttribute(Qt::WA_TranslucentBackground, false);
        d->mobilePrimaryBar->setClearColor(QColor(31, 35, 41));
        d->mobilePrimaryBar->setObjectName("mobilePrimaryBar");

        connect(d->mobilePrimaryBar, &QQuickWidget::statusChanged, this, [this](QQuickWidget::Status status) {
            if (status == QQuickWidget::Error)
            {
                const auto errors = d->mobilePrimaryBar->errors();
                for (const QQmlError &error : errors)
                    Logger::error(QString("Mobile QML error: %1").arg(error.toString()));
            }
        });

        d->mobilePrimaryBar->setSource(QUrl("qrc:/qml/mobile/MobilePrimaryBar.qml"));
        d->mobilePrimaryBarRoot = d->mobilePrimaryBar->rootObject();
        if (!d->mobilePrimaryBarRoot)
        {
            Logger::error("Failed to create QML mobile primary bar.");
            return;
        }

        connect(d->mobilePrimaryBarRoot, SIGNAL(openRequested()), this, SLOT(openChart()));
        connect(d->mobilePrimaryBarRoot, SIGNAL(saveRequested()), this, SLOT(saveChart()));
        connect(d->mobilePrimaryBarRoot, SIGNAL(playRequested()), this, SLOT(togglePlayback()));
        connect(d->mobilePrimaryBarRoot, SIGNAL(toggleLeftPanelRequested()), this, SLOT(toggleMobileLeftPanel()));
        connect(d->mobilePrimaryBarRoot, SIGNAL(toggleRightPanelRequested()), this, SLOT(toggleMobileRightPanel()));
        connect(d->mobilePrimaryBarRoot, SIGNAL(functionsRequested()), this, SLOT(openMobileFunctionHub()));
        connect(d->mobilePrimaryBarRoot, SIGNAL(notePanelRequested()), this, SLOT(showMobileNotePanel()));
        connect(d->mobilePrimaryBarRoot, SIGNAL(bpmPanelRequested()), this, SLOT(showMobileBpmPanel()));
        connect(d->mobilePrimaryBarRoot, SIGNAL(metaPanelRequested()), this, SLOT(showMobileMetaPanel()));
    }

    if (d->mobileShell && d->mobileShell->layout())
    {
        QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(d->mobileShell->layout());
        if (layout && layout->indexOf(d->mobilePrimaryBar) < 0)
            layout->insertWidget(0, d->mobilePrimaryBar, 0);
    }

    d->mobilePrimaryBar->setVisible(true);
    d->mobilePrimaryBar->update();

    if (menuBar())
        menuBar()->setVisible(false);

    retranslateMobileUi();
}

void MainWindow::retranslateMobileUi()
{
    if (!useCompactMobileLayout())
        return;

    if (!d->mobilePrimaryBarRoot)
        return;

    d->mobilePrimaryBarRoot->setProperty("openText", tr("Open"));
    d->mobilePrimaryBarRoot->setProperty("saveText", tr("Save"));
    d->mobilePrimaryBarRoot->setProperty("playText", tr("Play"));
    d->mobilePrimaryBarRoot->setProperty("leftPanelText", (d->leftPanel && d->leftPanel->isVisible()) ? tr("Hide Left") : tr("Show Left"));
    d->mobilePrimaryBarRoot->setProperty("rightPanelText", (d->rightPanelContainer && d->rightPanelContainer->isVisible()) ? tr("Hide Right") : tr("Show Right"));
    d->mobilePrimaryBarRoot->setProperty("functionsText", tr("Functions"));
    d->mobilePrimaryBarRoot->setProperty("noteText", tr("Note"));
    d->mobilePrimaryBarRoot->setProperty("bpmText", tr("BPM"));
    d->mobilePrimaryBarRoot->setProperty("metaText", tr("Meta"));
    d->mobilePrimaryBarRoot->setProperty("activePanel", d->currentRightPanel == d->bpmPanel   ? "bpm"
                                                 : d->currentRightPanel == d->metaPanel ? "meta"
                                                                                        : "note");
}

void MainWindow::toggleMobileLeftPanel()
{
    if (!d->leftPanel)
        return;
    d->leftPanel->setVisible(!d->leftPanel->isVisible());
    retranslateMobileUi();
}

void MainWindow::toggleMobileRightPanel()
{
    if (!d->rightPanelContainer)
        return;
    d->rightPanelContainer->setVisible(!d->rightPanelContainer->isVisible());
    retranslateMobileUi();
}

void MainWindow::showMobileNotePanel()
{
    showEditorPanel(d->notePanel);
}

void MainWindow::showMobileBpmPanel()
{
    showEditorPanel(d->bpmPanel);
}

void MainWindow::showMobileMetaPanel()
{
    showEditorPanel(d->metaPanel);
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
