#pragma once

#include <QString>
#include <QWidget>
#include <QVariantMap>

class PluginInterface
{
public:
    virtual ~PluginInterface() = default;
    virtual QString name() const = 0;
    virtual QString version() const = 0;
    virtual QString description() const = 0;
    virtual bool initialize(QWidget *mainWindow) = 0;
    virtual void onChartChanged() {}

    // Reserved interface for future advanced note color editing plugins.
    virtual bool supportsAdvancedColorEditor() const { return false; }
    virtual bool openAdvancedColorEditor(const QVariantMap &context)
    {
        (void)context;
        return false;
    }
};

#define PLUGIN_EXPORT extern "C" PluginInterface *createPlugin()
