#include "ExternalProcessPlugin.h"
#include "utils/Logger.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QVariantMap>
#include <utility>

namespace
{
QJsonObject toJsonObject(const QVariantMap &map)
{
    return QJsonObject::fromVariantMap(map);
}
}

ExternalProcessPlugin::ExternalProcessPlugin(Manifest manifest)
    : m_manifest(std::move(manifest))
{
}

ExternalProcessPlugin::~ExternalProcessPlugin()
{
    shutdown();
}

QString ExternalProcessPlugin::pluginId() const
{
    return m_manifest.pluginId;
}

QString ExternalProcessPlugin::displayName() const
{
    return m_manifest.displayName;
}

QString ExternalProcessPlugin::version() const
{
    return m_manifest.version;
}

QString ExternalProcessPlugin::description() const
{
    return m_manifest.description;
}

QString ExternalProcessPlugin::author() const
{
    return m_manifest.author;
}

QString ExternalProcessPlugin::localizedDisplayName(const QString &locale) const
{
    return resolveLocalizedValue(m_manifest.localizedDisplayName, locale, m_manifest.displayName);
}

QString ExternalProcessPlugin::localizedDescription(const QString &locale) const
{
    return resolveLocalizedValue(m_manifest.localizedDescription, locale, m_manifest.description);
}

int ExternalProcessPlugin::pluginApiVersion() const
{
    return m_manifest.apiVersion;
}

QStringList ExternalProcessPlugin::capabilities() const
{
    return m_manifest.capabilities;
}

bool ExternalProcessPlugin::initialize(QWidget *mainWindow)
{
    (void)mainWindow;
    if (m_initialized)
        return true;

    if (!ensureProcessRunning())
        return false;

    const QString locale = QLocale::system().name();
    const QJsonObject payload{
        {"plugin_id", m_manifest.pluginId},
        {"locale", locale},
        {"host_api_version", PluginInterface::kHostApiVersion},
    };
    m_initialized = sendNotification("initialize", payload);
    return m_initialized;
}

void ExternalProcessPlugin::shutdown()
{
    if (m_initialized)
    {
        sendNotification("shutdown");
        m_initialized = false;
    }

    if (m_process.state() != QProcess::NotRunning)
    {
        m_process.terminate();
        if (!m_process.waitForFinished(1000))
        {
            m_process.kill();
            m_process.waitForFinished(1000);
        }
    }
}

void ExternalProcessPlugin::onChartChanged()
{
    sendNotification("onChartChanged");
}

void ExternalProcessPlugin::onChartLoaded(const QString &chartPath)
{
    sendNotification("onChartLoaded", QJsonObject{{"chart_path", chartPath}});
}

void ExternalProcessPlugin::onChartSaved(const QString &chartPath)
{
    sendNotification("onChartSaved", QJsonObject{{"chart_path", chartPath}});
}

bool ExternalProcessPlugin::openAdvancedColorEditor(const QVariantMap &context)
{
    if (!hasCapability(kCapabilityAdvancedColorEditor))
        return false;
    return requestBool("openAdvancedColorEditor", toJsonObject(context), false);
}

bool ExternalProcessPlugin::sendNotification(const QString &event, const QJsonObject &payload)
{
    if (!ensureProcessRunning())
        return false;

    QJsonObject msg{
        {"type", "notify"},
        {"event", event},
    };
    if (!payload.isEmpty())
        msg.insert("payload", payload);

    const QByteArray line = QJsonDocument(msg).toJson(QJsonDocument::Compact) + "\n";
    return writeLine(line);
}

bool ExternalProcessPlugin::requestBool(const QString &method, const QJsonObject &payload, bool defaultValue) const
{
    if (m_process.state() != QProcess::Running)
        return defaultValue;

    const QString requestId = QString::number(QDateTime::currentMSecsSinceEpoch());
    QJsonObject req{
        {"type", "request"},
        {"id", requestId},
        {"method", method},
        {"payload", payload},
    };
    const QByteArray line = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";
    if (!const_cast<ExternalProcessPlugin *>(this)->writeLine(line))
        return defaultValue;

    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + 1500;
    while (QDateTime::currentMSecsSinceEpoch() < deadline)
    {
        const int remaining = static_cast<int>(deadline - QDateTime::currentMSecsSinceEpoch());
        const QString responseLine = readSingleLine(remaining);
        if (responseLine.isEmpty())
            continue;

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(responseLine.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        const QJsonObject obj = doc.object();
        if (obj.value("type").toString() != "response")
            continue;
        if (obj.value("id").toString() != requestId)
            continue;
        return obj.value("result").toBool(defaultValue);
    }

    return defaultValue;
}

bool ExternalProcessPlugin::ensureProcessRunning()
{
    if (m_process.state() == QProcess::Running)
        return true;

    QString executable = m_manifest.executable;
    QFileInfo execInfo(executable);
    if (execInfo.isRelative())
    {
        const QFileInfo manifestInfo(m_manifest.manifestPath);
        const QString baseDir = manifestInfo.absolutePath();
        executable = QDir(baseDir).filePath(executable);
    }

    m_process.setProgram(executable);
    m_process.setArguments(m_manifest.args);
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.start();
    if (!m_process.waitForStarted(2000))
    {
        Logger::warn(QString("Failed to start process plugin '%1' (%2)")
                         .arg(m_manifest.pluginId)
                         .arg(executable));
        return false;
    }
    return true;
}

QString ExternalProcessPlugin::resolveLocalizedValue(const QJsonObject &table,
                                                     const QString &locale,
                                                     const QString &fallback) const
{
    if (table.contains(locale))
        return table.value(locale).toString();

    const int split = locale.indexOf('_');
    if (split > 0)
    {
        const QString languageOnly = locale.left(split);
        if (table.contains(languageOnly))
            return table.value(languageOnly).toString();
    }

    if (table.contains("default"))
        return table.value("default").toString();

    return fallback;
}

QString ExternalProcessPlugin::readSingleLine(int timeoutMs) const
{
    if (!m_process.waitForReadyRead(timeoutMs))
        return QString();
    const QByteArray line = m_process.readLine();
    return QString::fromUtf8(line).trimmed();
}

bool ExternalProcessPlugin::writeLine(const QByteArray &line)
{
    if (m_process.state() != QProcess::Running)
        return false;

    const qint64 written = m_process.write(line);
    if (written <= 0)
        return false;
    return m_process.waitForBytesWritten(1000);
}
