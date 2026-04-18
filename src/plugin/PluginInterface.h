#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QVariantMap>
#include <QWidget>
#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QtGlobal>
#include "model/Note.h"

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
    struct FloatingPanelDescriptor
    {
        QString panelId;
        QString title;
        QString description;
    };
    struct CanvasOverlayItem
    {
        enum Kind
        {
            Line,
            Rect,
            Text
        };
        Kind kind = Line;
        QPointF from;
        QPointF to;
        QRectF rect;
        QString text;
        QColor color = QColor(255, 0, 0, 220);
        QColor fillColor = QColor(255, 0, 0, 40);
        double width = 1.5;
        int fontPx = 12;
    };
    struct BatchEdit
    {
        QVector<Note> notesToAdd;
        QVector<Note> notesToRemove;
        QList<QPair<Note, Note>> notesToMove;
    };

    // Host ABI/API version for runtime compatibility checks.
    static constexpr int kHostApiVersion = 2;

    // Capability keys.
    static constexpr const char *kCapabilityChartObserver = "chart_observer";
    static constexpr const char *kCapabilityAdvancedColorEditor = "advanced_color_editor";
    static constexpr const char *kCapabilityToolActions = "tool_actions";
    static constexpr const char *kCapabilityFloatingPanel = "floating_panel";
    static constexpr const char *kCapabilityCanvasOverlay = "canvas_overlay";
    static constexpr const char *kCapabilityHostBatchEdit = "host_batch_edit";
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
    // Optional host-side batch edit path.
    // If true is returned, host applies all edits as ONE undo step.
    virtual bool buildToolActionBatchEdit(const QString &actionId, const QVariantMap &context, BatchEdit *outEdit)
    {
        (void)actionId;
        (void)context;
        if (outEdit)
            *outEdit = BatchEdit{};
        return false;
    }
    // Optional floating panel extension points (native plugin embedding).
    virtual QList<FloatingPanelDescriptor> floatingPanels() const
    {
        return {};
    }
    virtual QWidget *createFloatingPanel(const QString &panelId, QWidget *parent, const QVariantMap &context)
    {
        (void)panelId;
        (void)parent;
        (void)context;
        return nullptr;
    }
    // Optional canvas overlay rendering extension point.
    virtual QList<CanvasOverlayItem> canvasOverlays(const QVariantMap &context) const
    {
        (void)context;
        return {};
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
