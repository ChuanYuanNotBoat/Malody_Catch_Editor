#pragma once

#include <QObject>
#include <QVector>
#include "plugin/PluginInterface.h"

class PluginManager : public QObject {
    Q_OBJECT
public:
    explicit PluginManager(QObject* parent = nullptr);
    ~PluginManager();

    void loadPlugins(const QString& pluginsDir, QWidget* parent = nullptr);
    QVector<PluginInterface*> plugins() const;
    void notifyChartChanged();

private:
    QVector<PluginInterface*> m_plugins;
};