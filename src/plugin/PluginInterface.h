#pragma once

#include <QString>
#include <QWidget>

class PluginInterface
{
public:
    virtual ~PluginInterface() = default;
    virtual QString name() const = 0;
    virtual QString version() const = 0;
    virtual QString description() const = 0;
    virtual bool initialize(QWidget *mainWindow) = 0;
    virtual void onChartChanged() {}
};

#define PLUGIN_EXPORT extern "C" PluginInterface *createPlugin()