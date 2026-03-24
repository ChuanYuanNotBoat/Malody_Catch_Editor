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
        // 初始化日志
        Logger::init(QDir::home().filePath(".catch_editor.log"));
        Logger::info("========== Application Starting ==========");

        // 加载设置
        Logger::info("Loading settings...");
        Settings::instance();

        // 加载语言
        Logger::info("Loading language...");
        loadLanguage();

        // 创建控制器
        Logger::info("Creating controllers...");
        m_chartController = new ChartController(this);
        m_selectionController = new SelectionController(this);
        m_playbackController = new PlaybackController(new AudioPlayer(this), this);
        Logger::info("Controllers created successfully.");

        // 加载皮肤
        Logger::info("Loading skin...");
        m_skin = new Skin();
        QString skinPath = QCoreApplication::applicationDirPath() + "/skins/" + Settings::instance().currentSkin();
        qDebug() << "Trying skin path:" << skinPath;
        if (!SkinIO::loadSkin(skinPath, *m_skin)) {
            // 尝试默认皮肤
            skinPath = QCoreApplication::applicationDirPath() + "/skins/default";
            qDebug() << "Trying default skin path:" << skinPath;
            if (!SkinIO::loadSkin(skinPath, *m_skin)) {
                Logger::warn("Failed to load skin, using default colors.");
            }
        }
        Logger::info("Skin loaded successfully.");

        // 创建主窗口
        Logger::info("Creating main window...");
        m_mainWindow = new MainWindow(m_chartController, m_selectionController, m_playbackController);
        m_mainWindow->show();
        Logger::info("Main window created and shown.");

        // 加载插件
        Logger::info("Loading plugins...");
        m_pluginManager = new PluginManager(this);
        QString pluginsDir = QCoreApplication::applicationDirPath() + "/plugins";
        m_pluginManager->loadPlugins(pluginsDir, m_mainWindow);
        Logger::info("Plugins loaded successfully.");

        // 连接信号（示例）
        connect(m_chartController, &ChartController::chartChanged, m_pluginManager, &PluginManager::notifyChartChanged);

        // 自动加载上次项目
        loadLastProject();

        Logger::info("Application initialized successfully.");
        return true;
    } catch (const std::exception& e) {
        Logger::error(QString("Exception during initialization: %1").arg(QString::fromStdString(std::string(e.what()))));
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
        // 可以尝试自动加载，但不必须
    }
}

void Application::loadLanguage()
{
    Translator::instance().setLanguage(Settings::instance().language());
}