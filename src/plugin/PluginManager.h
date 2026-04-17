#pragma once

#include <QObject>
#include <QVector>
#include "plugin/PluginInterface.h"

class PluginManager : public QObject
{
    Q_OBJECT
public:
    explicit PluginManager(QObject *parent = nullptr);
    ~PluginManager();

    void loadPlugins(const QString &pluginsDir, QWidget *parent = nullptr);
    void unloadPlugins();
    QVector<PluginInterface *> plugins() const;

    void notifyChartChanged();
    void notifyChartLoaded(const QString &chartPath);
    void notifyChartSaved(const QString &chartPath);
    bool tryOpenAdvancedColorEditor(const QVariantMap &context);

private:
    QVector<PluginInterface *> m_plugins;
};
