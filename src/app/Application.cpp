#include "Application.h"
#include "MainWindow.h"
#include "controller/ChartController.h"
#include "controller/SelectionController.h"
#include "controller/PlaybackController.h"
#include "plugin/PluginManager.h"
#include "model/Skin.h"
#include "file/SkinIO.h"
#include "utils/Settings.h"
#include "utils/Translator.h"
#include "utils/Logger.h"
#include <QDir>
#include <QDebug>

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv), m_mainWindow(nullptr)
{
    setOrganizationName("CatchEditor");
    setApplicationName("Malody Catch Chart Editor");
}

Application::~Application()
{
    delete m_mainWindow;
    delete m_chartController;
    delete m_selectionController;
    delete m_playbackController;
    delete m_pluginManager;
    delete m_skin;
}

bool Application::initialize()
{
    try {
        // 初始化日志系统
        Logger::init("logs");
        Logger::info("========== Application Starting ==========");
        Logger::info(QString("Log file: %1").arg(Logger::logFilePath()));

        Settings::instance();
        Logger::info("Settings loaded.");

        loadLanguage();
        Logger::info("Language loaded.");

        m_chartController = new ChartController(this);
        m_selectionController = new SelectionController(this);
        m_playbackController = new PlaybackController(new AudioPlayer(this), this);
        Logger::info("Controllers created.");

        // 加载皮肤
        m_skin = new Skin();
        QString skinName = Settings::instance().currentSkin();
        QString skinsBaseDir = QCoreApplication::applicationDirPath() + "/resources/default_skin";
        Logger::info(QString("Looking for skins in: %1").arg(skinsBaseDir));

        // 扫描所有皮肤目录
        QStringList skinDirs = SkinIO::getSkinList(skinsBaseDir);
        if (skinDirs.isEmpty()) {
            Logger::warn("No skin directories found, using fallback colors.");
            m_skin = nullptr; // 不使用皮肤
        } else {
            bool loaded = false;
            // 如果当前皮肤名在列表中，则加载它；否则加载第一个
            if (!skinName.isEmpty() && skinDirs.contains(skinName)) {
                QString skinPath = skinsBaseDir + "/" + skinName;
                Logger::info(QString("Trying to load skin '%1' from %2").arg(skinName).arg(skinPath));
                if (SkinIO::loadSkin(skinPath, *m_skin)) {
                    loaded = true;
                    Logger::info(QString("Skin '%1' loaded successfully").arg(skinName));
                } else {
                    Logger::error(QString("Failed to load skin '%1', will try first available").arg(skinName));
                }
            }
            if (!loaded) {
                // 使用第一个可用皮肤
                QString firstSkin = skinDirs.first();
                QString skinPath = skinsBaseDir + "/" + firstSkin;
                Logger::info(QString("Loading first skin: %1 from %2").arg(firstSkin).arg(skinPath));
                if (SkinIO::loadSkin(skinPath, *m_skin)) {
                    loaded = true;
                    Logger::info(QString("Skin '%1' loaded successfully").arg(firstSkin));
                    // 保存为新默认皮肤
                    Settings::instance().setCurrentSkin(firstSkin);
                    Logger::info(QString("Set default skin to %1").arg(firstSkin));
                } else {
                    Logger::error("Failed to load any skin, using fallback colors.");
                    m_skin = nullptr;
                }
            }
        }

        m_mainWindow = new MainWindow(m_chartController, m_selectionController, m_playbackController, m_skin);
        m_mainWindow->show();
        Logger::info("Main window created and shown.");

        m_pluginManager = new PluginManager(this);
        QString pluginsDir = QCoreApplication::applicationDirPath() + "/plugins";
        m_pluginManager->loadPlugins(pluginsDir, m_mainWindow);
        Logger::info("Plugins loaded.");

        connect(m_chartController, &ChartController::chartChanged, m_pluginManager, &PluginManager::notifyChartChanged);

        loadLastProject();

        Logger::info("Application initialized successfully.");
        return true;
    } catch (const std::exception& e) {
        Logger::error(QString("Exception during initialization: %1").arg(e.what()));
        return false;
    } catch (...) {
        Logger::error("Unknown exception during initialization");
        return false;
    }
}

void Application::loadLastProject()
{
    QString lastPath = Settings::instance().lastOpenPath();
    if (!lastPath.isEmpty()) {
        Logger::info(QString("Last project path: %1 (auto-load not implemented)").arg(lastPath));
    }
}

void Application::loadLanguage()
{
    Translator::instance().setLanguage(Settings::instance().language());
}