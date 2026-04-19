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
#include <QFrame>
#include <QGuiApplication>
#include <QMenuBar>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QSizePolicy>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabBar>
#include <QTabWidget>
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

    if (!d->mobileShell)
    {
        d->mobileShell = new QWidget(this);
        QVBoxLayout *shellLayout = new QVBoxLayout(d->mobileShell);
        shellLayout->setContentsMargins(0, 0, 0, 0);
        shellLayout->setSpacing(0);
        setCentralWidget(d->mobileShell);
    }

    if (d->canvas)
    {
        d->canvas->setMinimumSize(0, 0);
        d->canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    if (d->verticalScrollBar)
        d->verticalScrollBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    if (d->leftPanel)
    {
        d->leftPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        d->leftPanel->setMinimumWidth(0);
    }
    if (d->rightPanelContainer)
    {
        d->rightPanelContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        d->rightPanelContainer->setMinimumWidth(0);
    }

    if (!d->mobileTabs)
    {
        d->mobileTabs = new QTabWidget(d->mobileShell);
        d->mobileTabs->setDocumentMode(true);
        d->mobileTabs->tabBar()->setVisible(false);
        d->mobileTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        qobject_cast<QVBoxLayout *>(d->mobileShell->layout())->addWidget(d->mobileTabs, 1);
    }

    if (!d->mobileCanvasHost)
    {
        d->mobileCanvasHost = canvasContainer;
        d->mobileTabs->addTab(d->mobileCanvasHost, tr("Editor"));
    }

    if (!d->mobileLeftPanelHost)
    {
        d->mobileLeftPanelHost = new QScrollArea(d->mobileTabs);
        d->mobileLeftPanelHost->setFrameShape(QFrame::NoFrame);
        d->mobileLeftPanelHost->setWidgetResizable(true);
        d->mobileLeftPanelHost->setWidget(d->leftPanel);
        d->mobileTabs->addTab(d->mobileLeftPanelHost, tr("Left"));
    }

    if (!d->mobileRightPanelHost)
    {
        d->mobileRightPanelHost = new QScrollArea(d->mobileTabs);
        d->mobileRightPanelHost->setFrameShape(QFrame::NoFrame);
        d->mobileRightPanelHost->setWidgetResizable(true);
        d->mobileRightPanelHost->setWidget(d->rightPanelContainer);
        d->mobileTabs->addTab(d->mobileRightPanelHost, tr("Right"));
    }

    d->mobileTabs->setCurrentWidget(d->mobileCanvasHost);

    Logger::info("Compact mobile layout enabled: QML top bar + mobile tab shell (editor/left/right).");
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
        connect(d->mobilePrimaryBarRoot, SIGNAL(editorRequested()), this, SLOT(showMobileEditor()));
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
    d->mobilePrimaryBarRoot->setProperty("editorText", tr("Editor"));
    d->mobilePrimaryBarRoot->setProperty("leftPanelText", (d->mobileTabs && d->mobileTabs->currentWidget() == d->mobileLeftPanelHost) ? tr("Back") : tr("Left"));
    d->mobilePrimaryBarRoot->setProperty("rightPanelText", (d->mobileTabs && d->mobileTabs->currentWidget() == d->mobileRightPanelHost) ? tr("Back") : tr("Right"));
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
    if (!d->mobileTabs || !d->mobileLeftPanelHost)
        return;
    if (d->mobileTabs->currentWidget() == d->mobileLeftPanelHost)
        d->mobileTabs->setCurrentWidget(d->mobileCanvasHost);
    else
        d->mobileTabs->setCurrentWidget(d->mobileLeftPanelHost);
    retranslateMobileUi();
}

void MainWindow::toggleMobileRightPanel()
{
    if (!d->mobileTabs || !d->mobileRightPanelHost)
        return;
    if (d->mobileTabs->currentWidget() == d->mobileRightPanelHost)
        d->mobileTabs->setCurrentWidget(d->mobileCanvasHost);
    else
        d->mobileTabs->setCurrentWidget(d->mobileRightPanelHost);
    retranslateMobileUi();
}

void MainWindow::showMobileNotePanel()
{
    showEditorPanel(d->notePanel);
    if (d->mobileTabs && d->mobileRightPanelHost)
        d->mobileTabs->setCurrentWidget(d->mobileRightPanelHost);
    retranslateMobileUi();
}

void MainWindow::showMobileBpmPanel()
{
    showEditorPanel(d->bpmPanel);
    if (d->mobileTabs && d->mobileRightPanelHost)
        d->mobileTabs->setCurrentWidget(d->mobileRightPanelHost);
    retranslateMobileUi();
}

void MainWindow::showMobileMetaPanel()
{
    showEditorPanel(d->metaPanel);
    if (d->mobileTabs && d->mobileRightPanelHost)
        d->mobileTabs->setCurrentWidget(d->mobileRightPanelHost);
    retranslateMobileUi();
}

void MainWindow::showMobileEditor()
{
    if (!d->mobileTabs || !d->mobileCanvasHost)
        return;
    d->mobileTabs->setCurrentWidget(d->mobileCanvasHost);
    retranslateMobileUi();
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
