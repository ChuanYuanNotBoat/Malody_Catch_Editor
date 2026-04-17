#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QVariantMap>
#include <QWidget>
#include <QtGlobal>

class PluginInterface
{
public:
    struct ToolAction
    {
        QString actionId;
        QString title;
        QString description;
        QString confirmMessage;
        QString placement = "tools_menu"; // tools_menu | top_toolbar | left_sidebar
        bool requiresUndoSnapshot = true;
    };

    // Host ABI/API version for runtime compatibility checks.
    static constexpr int kHostApiVersion = 2;

    // Capability keys.
    static constexpr const char *kCapabilityChartObserver = "chart_observer";
    static constexpr const char *kCapabilityAdvancedColorEditor = "advanced_color_editor";
    static constexpr const char *kCapabilityToolActions = "tool_actions";
    static constexpr const char *kPlacementToolsMenu = "tools_menu";
    static constexpr const char *kPlacementTopToolbar = "top_toolbar";
    static constexpr const char *kPlacementLeftSidebar = "left_sidebar";

    virtual ~PluginInterface() = default;

    // Metadata (required).
    virtual QString pluginId() const = 0;
    virtual QString displayName() const = 0;
    virtual QString version() const = 0;
    virtual QString description() const = 0;
    virtual QString author() const = 0;
    virtual QString pluginSourcePath() const
    {
        return QString();
    }
    virtual QString localizedDisplayName(const QString &locale) const
    {
        (void)locale;
        return displayName();
    }
    virtual QString localizedDescription(const QString &locale) const
    {
        (void)locale;
        return description();
    }
    virtual int pluginApiVersion() const = 0;
    virtual QStringList capabilities() const = 0;

    // Lifecycle.
    virtual bool initialize(QWidget *mainWindow) = 0;
    virtual void shutdown() = 0;

    // Optional lifecycle events.
    virtual void onChartChanged() {}
    virtual void onChartLoaded(const QString &chartPath)
    {
        (void)chartPath;
    }
    virtual void onChartSaved(const QString &chartPath)
    {
        (void)chartPath;
    }

    // Optional UI extension point.
    virtual bool openAdvancedColorEditor(const QVariantMap &context)
    {
        (void)context;
        return false;
    }

    // Optional tool action extension points.
    virtual QList<ToolAction> toolActions() const
    {
        return {};
    }
    virtual bool runToolAction(const QString &actionId, const QVariantMap &context)
    {
        (void)actionId;
        (void)context;
        return false;
    }

    bool hasCapability(const QString &capability) const
    {
        return capabilities().contains(capability, Qt::CaseInsensitive);
    }
};

// Runtime loader contract (mandatory exports in plugin dynamic library).
using CreatePluginFn = PluginInterface *(*)();
using DestroyPluginFn = void (*)(PluginInterface *);
using PluginApiVersionFn = int (*)();

#define PLUGIN_EXPORT_API extern "C" Q_DECL_EXPORT
#define PLUGIN_EXPORT_CREATE PLUGIN_EXPORT_API PluginInterface *createPlugin()
#define PLUGIN_EXPORT_DESTROY PLUGIN_EXPORT_API void destroyPlugin(PluginInterface *plugin)
#define PLUGIN_EXPORT_VERSION PLUGIN_EXPORT_API int pluginApiVersion()
