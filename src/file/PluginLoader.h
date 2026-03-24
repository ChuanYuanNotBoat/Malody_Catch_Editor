#pragma once

#include <QString>
#include <QVector>
#include "plugin/PluginInterface.h"

class PluginLoader {
public:
    static QVector<PluginInterface*> loadPlugins(const QString& pluginsDir);
    static void unloadPlugins(QVector<PluginInterface*>& plugins);
};