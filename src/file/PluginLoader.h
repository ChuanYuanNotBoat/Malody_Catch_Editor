#pragma once

#include <QString>
#include <QVector>
#include "plugin/PluginInterface.h"

class PluginLoader
{
public:
    // Loads both native binary plugins (*.dll/*.so/*.dylib)
    // and process plugins defined by *.plugin.json manifests.
    static QVector<PluginInterface *> loadPlugins(const QString &pluginsDir);
    static void unloadPlugins(QVector<PluginInterface *> &plugins);
};
