#include "PluginManager.h"
#include "file/PluginLoader.h"
#include "utils/Logger.h"
#include <QDebug>

PluginManager::PluginManager(QObject *parent) : QObject(parent) {}

PluginManager::~PluginManager()
{
    PluginLoader::unloadPlugins(m_plugins);
}

void PluginManager::loadPlugins(const QString &pluginsDir, QWidget *parent)
{
    try
    {
        m_plugins = PluginLoader::loadPlugins(pluginsDir);
        Logger::info(QString("Loaded %1 plugins.").arg(m_plugins.size()));

        for (int i = 0; i < m_plugins.size(); ++i)
        {
            PluginInterface *p = m_plugins[i];
            if (!p)
            {
                Logger::warn(QString("Plugin %1 is null!").arg(i));
                continue;
            }
            try
            {
                p->initialize(parent);
                Logger::info(QString("Plugin %1 initialized successfully.").arg(i));
            }
            catch (const std::exception &e)
            {
                Logger::error(QString("Error initializing plugin %1: %2").arg(i).arg(QString::fromStdString(std::string(e.what()))));
            }
            catch (...)
            {
                Logger::error(QString("Unknown error initializing plugin %1").arg(i));
            }
        }
    }
    catch (const std::exception &e)
    {
        Logger::error(QString("Error loading plugins: %1").arg(QString::fromStdString(std::string(e.what()))));
    }
    catch (...)
    {
        Logger::error("Unknown error loading plugins");
    }
}

QVector<PluginInterface *> PluginManager::plugins() const
{
    return m_plugins;
}

void PluginManager::notifyChartChanged()
{
    for (PluginInterface *p : m_plugins)
    {
        if (p)
        {
            try
            {
                p->onChartChanged();
            }
            catch (...)
            {
                Logger::warn("Error in plugin onChartChanged");
            }
        }
    }
}