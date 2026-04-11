#include "PluginLoader.h"
#include <QDir>
#include <QLibrary>
#include <QDebug>

QVector<PluginInterface *> PluginLoader::loadPlugins(const QString &pluginsDir)
{
    QVector<PluginInterface *> plugins;
    QDir dir(pluginsDir);
    QStringList files = dir.entryList(QDir::Files);
    for (const QString &file : files)
    {
        if (!file.endsWith(".dll") && !file.endsWith(".so") && !file.endsWith(".dylib"))
            continue;
        QLibrary lib(dir.filePath(file));
        if (!lib.load())
        {
            qWarning() << "Failed to load plugin:" << lib.errorString();
            continue;
        }
        typedef PluginInterface *(*CreatePluginFunc)();
        CreatePluginFunc create = reinterpret_cast<CreatePluginFunc>(lib.resolve("createPlugin"));
        if (!create)
        {
            qWarning() << "No createPlugin function in plugin:" << file;
            lib.unload();
            continue;
        }
        PluginInterface *plugin = create();
        if (plugin)
        {
            plugins.append(plugin);
        }
    }
    return plugins;
}

void PluginLoader::unloadPlugins(QVector<PluginInterface *> &plugins)
{
    for (PluginInterface *plugin : plugins)
    {
        delete plugin;
    }
    plugins.clear();
}