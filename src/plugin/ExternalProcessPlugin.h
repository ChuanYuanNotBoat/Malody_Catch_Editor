#pragma once

#include "plugin/PluginInterface.h"
#include <QJsonObject>
#include <QProcess>
#include <QStringList>

class ExternalProcessPlugin final : public PluginInterface
{
public:
    struct Manifest
    {
        QString pluginId;
        QString displayName;
        QString version;
        QString description;
        QString author;
        int apiVersion = 0;
        QString executable;
        QStringList args;
        QStringList capabilities;
        QJsonObject localizedDisplayName;
        QJsonObject localizedDescription;
        QString manifestPath;
    };

    explicit ExternalProcessPlugin(Manifest manifest);
    ~ExternalProcessPlugin() override;

    QString pluginId() const override;
    QString displayName() const override;
    QString version() const override;
    QString description() const override;
    QString author() const override;
    QString pluginSourcePath() const override;
    QString localizedDisplayName(const QString &locale) const override;
    QString localizedDescription(const QString &locale) const override;
    int pluginApiVersion() const override;
    QStringList capabilities() const override;

    bool initialize(QWidget *mainWindow) override;
    void shutdown() override;

    void onChartChanged() override;
    void onChartLoaded(const QString &chartPath) override;
    void onChartSaved(const QString &chartPath) override;
    bool openAdvancedColorEditor(const QVariantMap &context) override;

private:
    bool sendNotification(const QString &event, const QJsonObject &payload = QJsonObject());
    bool requestBool(const QString &method, const QJsonObject &payload, bool defaultValue) const;
    bool ensureProcessRunning();
    QString resolveLocalizedValue(const QJsonObject &table, const QString &locale, const QString &fallback) const;
    QString readSingleLine(int timeoutMs) const;
    bool writeLine(const QByteArray &line);

private:
    Manifest m_manifest;
    mutable QProcess m_process;
    bool m_initialized = false;
};
