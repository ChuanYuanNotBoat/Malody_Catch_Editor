#pragma once

#include "plugin/PluginInterface.h"

class BuiltinBeatNormalizerPlugin final : public PluginInterface
{
public:
    QString pluginId() const override;
    QString displayName() const override;
    QString version() const override;
    QString description() const override;
    QString author() const override;
    QString localizedDisplayName(const QString &locale) const override;
    QString localizedDescription(const QString &locale) const override;
    int pluginApiVersion() const override;
    QStringList capabilities() const override;

    bool initialize(QWidget *mainWindow) override;
    void shutdown() override;

    QList<ToolAction> toolActions() const override;
    bool runToolAction(const QString &actionId, const QVariantMap &context) override;

private:
    static QString resolveChartPath(const QVariantMap &context);
    static bool normalizeMcFile(const QString &path);
};

