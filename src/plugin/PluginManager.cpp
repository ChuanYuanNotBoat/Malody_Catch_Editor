#include "PluginManager.h"
#include "file/PluginLoader.h"
#include "utils/Logger.h"
#include <QDebug>
#include <QLocale>
#include <exception>

PluginManager::PluginManager(QObject *parent) : QObject(parent) {}

PluginManager::~PluginManager()
{
    unloadPlugins();
}

void PluginManager::loadPlugins(const QString &pluginsDir, QWidget *parent)
{
    unloadPlugins();

    try
    {
        QVector<PluginInterface *> loaded = PluginLoader::loadPlugins(pluginsDir);
        Logger::info(QString("Discovered %1 plugin binaries.").arg(loaded.size()));

        QVector<PluginInterface *> rejected;
        const QString locale = QLocale::system().name();

        for (int i = 0; i < loaded.size(); ++i)
        {
            PluginInterface *p = loaded[i];
            if (!p)
            {
                Logger::warn(QString("Plugin %1 is null!").arg(i));
                continue;
            }

            if (p->pluginApiVersion() != PluginInterface::kHostApiVersion)
            {
                Logger::warn(QString("Plugin '%1' skipped: API version mismatch (plugin=%2, host=%3)")
                                 .arg(p->displayName())
                                 .arg(p->pluginApiVersion())
                                 .arg(PluginInterface::kHostApiVersion));
                rejected.append(p);
                continue;
            }

            try
            {
                const bool ok = p->initialize(parent);
                if (!ok)
                {
                    Logger::warn(QString("Plugin '%1' initialize returned false. Skipped.").arg(p->displayName()));
                    rejected.append(p);
                    continue;
                }

                m_plugins.append(p);
                Logger::info(QString("Plugin initialized: id='%1', name='%2', version='%3', author='%4'")
                                 .arg(p->pluginId())
                                 .arg(p->localizedDisplayName(locale))
                                 .arg(p->version())
                                 .arg(p->author()));
            }
            catch (const std::exception &e)
            {
                Logger::error(QString("Error initializing plugin %1: %2").arg(i).arg(QString::fromStdString(std::string(e.what()))));
                rejected.append(p);
            }
            catch (...)
            {
                Logger::error(QString("Unknown error initializing plugin %1").arg(i));
                rejected.append(p);
            }
        }

        if (!rejected.isEmpty())
            PluginLoader::unloadPlugins(rejected);

        Logger::info(QString("PluginManager ready: %1 active plugins.").arg(m_plugins.size()));
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

void PluginManager::unloadPlugins()
{
    if (m_plugins.isEmpty())
        return;

    for (PluginInterface *p : m_plugins)
    {
        if (!p)
            continue;
        try
        {
            p->shutdown();
        }
        catch (...)
        {
            Logger::warn(QString("Error in plugin '%1' shutdown").arg(p->displayName()));
        }
    }

    PluginLoader::unloadPlugins(m_plugins);
    Logger::info("All plugins unloaded.");
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

void PluginManager::notifyChartLoaded(const QString &chartPath)
{
    for (PluginInterface *p : m_plugins)
    {
        if (!p)
            continue;
        try
        {
            p->onChartLoaded(chartPath);
        }
        catch (...)
        {
            Logger::warn(QString("Error in plugin '%1' onChartLoaded").arg(p->displayName()));
        }
    }
}

void PluginManager::notifyChartSaved(const QString &chartPath)
{
    for (PluginInterface *p : m_plugins)
    {
        if (!p)
            continue;
        try
        {
            p->onChartSaved(chartPath);
        }
        catch (...)
        {
            Logger::warn(QString("Error in plugin '%1' onChartSaved").arg(p->displayName()));
        }
    }
}

bool PluginManager::tryOpenAdvancedColorEditor(const QVariantMap &context)
{
    for (PluginInterface *p : m_plugins)
    {
        if (!p || !p->hasCapability(PluginInterface::kCapabilityAdvancedColorEditor))
            continue;

        try
        {
            if (p->openAdvancedColorEditor(context))
            {
                Logger::info(QString("Advanced color editor handled by plugin '%1'.").arg(p->displayName()));
                return true;
            }
        }
        catch (...)
        {
            Logger::warn(QString("Error in plugin '%1' openAdvancedColorEditor").arg(p->displayName()));
        }
    }
    return false;
}
