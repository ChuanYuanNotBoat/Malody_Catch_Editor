#include "ExternalProcessPlugin.h"
#include "utils/Logger.h"
#include "utils/Settings.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QVariantMap>
#include <QProcessEnvironment>
#include <utility>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace
{
QJsonObject toJsonObject(const QVariantMap &map)
{
    return QJsonObject::fromVariantMap(map);
}

int requestTimeoutMsForMethod(const QString &method)
{
    if (method == "runToolAction")
        return 15000;
    if (method == "openAdvancedColorEditor")
        return 10000;
    if (method == "listToolActions")
        return 5000;
    return 5000;
}

void pumpUiEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
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

QString ExternalProcessPlugin::pluginSourcePath() const
{
    return m_manifest.manifestPath;
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

    QString locale = Settings::instance().language().trimmed();
    if (locale.isEmpty())
        locale = QLocale::system().name();
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

QList<PluginInterface::ToolAction> ExternalProcessPlugin::toolActions() const
{
    QJsonValue result;
    if (!requestJson("listToolActions", QJsonObject(), &result))
        return {};
    if (!result.isArray())
        return {};

    QList<ToolAction> actions;
    const QJsonArray arr = result.toArray();
    for (const QJsonValue &v : arr)
    {
        if (!v.isObject())
            continue;
        const QJsonObject obj = v.toObject();
        ToolAction action;
        action.actionId = obj.value("action_id").toString().trimmed();
        action.title = obj.value("title").toString().trimmed();
        action.description = obj.value("description").toString().trimmed();
        action.confirmMessage = obj.value("confirm_message").toString().trimmed();
        action.placement = obj.value("placement").toString().trimmed();
        if (action.placement.isEmpty())
            action.placement = PluginInterface::kPlacementToolsMenu;
        action.requiresUndoSnapshot = obj.value("requires_undo_snapshot").toBool(true);
        if (action.actionId.isEmpty() || action.title.isEmpty())
            continue;
        actions.append(action);
    }
    return actions;
}

bool ExternalProcessPlugin::runToolAction(const QString &actionId, const QVariantMap &context)
{
    if (actionId.isEmpty())
        return false;

    // Prefer one-shot execution to match standalone script behavior and avoid
    // request/response channel stalls for long-running file operations.
    if (runToolActionOneShot(actionId, context))
        return true;

    Logger::warn(QString("Process plugin '%1' runToolAction(%2) one-shot path failed, trying protocol fallback.")
                     .arg(m_manifest.pluginId)
                     .arg(actionId));
    QJsonObject payload{
        {"action_id", actionId},
        {"context", toJsonObject(context)},
    };
    return requestBool("runToolAction", payload, false);
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
    QJsonValue result;
    if (!requestJson(method, payload, &result))
        return defaultValue;
    if (!result.isBool())
        return defaultValue;
    return result.toBool(defaultValue);
}

bool ExternalProcessPlugin::requestJson(const QString &method, const QJsonObject &payload, QJsonValue *result) const
{
    if (result)
        *result = QJsonValue();
    if (m_process.state() != QProcess::Running)
    {
        Logger::warn(QString("Process plugin '%1' request '%2' skipped: process not running.")
                         .arg(m_manifest.pluginId)
                         .arg(method));
        return false;
    }

    const QString requestId = QString::number(QDateTime::currentMSecsSinceEpoch());
    QJsonObject req{
        {"type", "request"},
        {"id", requestId},
        {"method", method},
        {"payload", payload},
    };
    const QByteArray line = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";
    if (!const_cast<ExternalProcessPlugin *>(this)->writeLine(line))
    {
        Logger::warn(QString("Process plugin '%1' request '%2' failed: writeLine error.")
                         .arg(m_manifest.pluginId)
                         .arg(method));
        return false;
    }

    const int timeoutMs = requestTimeoutMsForMethod(method);
    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + timeoutMs;
    while (QDateTime::currentMSecsSinceEpoch() < deadline)
    {
        const int remaining = static_cast<int>(deadline - QDateTime::currentMSecsSinceEpoch());
        const int waitSlice = qMax(1, qMin(remaining, 50));
        if (!m_process.waitForReadyRead(waitSlice))
        {
            pumpUiEvents();
            continue;
        }
        const QString responseLine = QString::fromUtf8(m_process.readLine()).trimmed();
        if (responseLine.isEmpty())
        {
            pumpUiEvents();
            continue;
        }

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(responseLine.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
        {
            pumpUiEvents();
            continue;
        }

        const QJsonObject obj = doc.object();
        if (obj.value("type").toString() != "response")
        {
            pumpUiEvents();
            continue;
        }
        if (obj.value("id").toString() != requestId)
        {
            pumpUiEvents();
            continue;
        }

        if (result)
            *result = obj.value("result");
        return true;
    }
    const QByteArray stderrMsg = m_process.readAllStandardError();
    if (!stderrMsg.isEmpty())
    {
        Logger::warn(QString("Process plugin '%1' request '%2' timeout, stderr: %3")
                         .arg(m_manifest.pluginId)
                         .arg(method)
                         .arg(QString::fromUtf8(stderrMsg).trimmed()));
    }
    else
    {
        Logger::warn(QString("Process plugin '%1' request '%2' timeout without response.")
                         .arg(m_manifest.pluginId)
                         .arg(method));
    }
    return false;
}

bool ExternalProcessPlugin::runToolActionOneShot(const QString &actionId, const QVariantMap &context) const
{
    const QFileInfo manifestInfo(m_manifest.manifestPath);
    const QString baseDir = manifestInfo.absolutePath();

    QString executable = m_manifest.executable.trimmed();
    QFileInfo execInfo(executable);
    const bool looksLikePath = executable.contains('/') || executable.contains('\\') || executable.startsWith('.');
    if (looksLikePath && execInfo.isRelative())
        executable = QDir(baseDir).filePath(executable);

    QStringList args;
    for (const QString &arg : m_manifest.args)
    {
        const QString trimmed = arg.trimmed();
        if (trimmed == "--plugin")
            continue;
        const bool argLooksLikePath = trimmed.contains('/') || trimmed.contains('\\') || trimmed.startsWith('.');
        QFileInfo argInfo(trimmed);
        if (argLooksLikePath && argInfo.isRelative())
            args.append(QDir(baseDir).filePath(trimmed));
        else
            args.append(trimmed);
    }

    QStringList candidates;
    const QString pNative = context.value("chart_path_native").toString();
    const QString pPath = context.value("chart_path").toString();
    const QString pCanonical = context.value("chart_path_canonical").toString();
    if (!pNative.isEmpty())
        candidates << pNative;
    if (!pPath.isEmpty())
        candidates << pPath;
    if (!pCanonical.isEmpty())
        candidates << pCanonical;

    QString chartPath;
    for (const QString &candidate : candidates)
    {
        if (QFileInfo::exists(candidate))
        {
            chartPath = candidate;
            break;
        }
    }
    if (chartPath.isEmpty() && !candidates.isEmpty())
        chartPath = candidates.first();

    args << "--run-tool-action" << actionId;
    if (!chartPath.isEmpty())
        args << chartPath;

    Logger::info(QString("Process plugin one-shot start (%1): action=%2 path=%3 exists=%4")
                     .arg(m_manifest.pluginId)
                     .arg(actionId)
                     .arg(chartPath)
                     .arg(QFileInfo::exists(chartPath)));

    QProcess oneShot;
    oneShot.setWorkingDirectory(baseDir);
    oneShot.setProgram(executable);
    oneShot.setArguments(args);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString locale = context.value("locale").toString().trimmed();
    const QString language = context.value("language").toString().trimmed();
    if (!locale.isEmpty())
        env.insert("MALODY_LOCALE", locale);
    if (!language.isEmpty())
        env.insert("MALODY_LANGUAGE", language);
    oneShot.setProcessEnvironment(env);
    oneShot.setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
    oneShot.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *p)
                                              { p->flags |= CREATE_NO_WINDOW; });
#endif
    oneShot.start();
    if (!oneShot.waitForStarted(3000))
    {
        Logger::warn(QString("Process plugin '%1' one-shot fallback start failed.").arg(m_manifest.pluginId));
        return false;
    }

    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + 120000;
    while (oneShot.state() != QProcess::NotRunning && QDateTime::currentMSecsSinceEpoch() < deadline)
    {
        oneShot.waitForFinished(50);
        pumpUiEvents();
    }
    if (oneShot.state() != QProcess::NotRunning)
    {
        oneShot.kill();
        oneShot.waitForFinished(1000);
        Logger::warn(QString("Process plugin '%1' one-shot fallback timed out.").arg(m_manifest.pluginId));
        return false;
    }

    const QByteArray out = oneShot.readAll();
    const QString outText = QString::fromUtf8(out).trimmed();
    if (!outText.isEmpty())
    {
        Logger::info(QString("Process plugin one-shot output (%1): %2")
                         .arg(m_manifest.pluginId)
                         .arg(outText));
    }

    const bool ok = (oneShot.exitStatus() == QProcess::NormalExit && oneShot.exitCode() == 0);
    if (!ok)
    {
        Logger::warn(QString("Process plugin one-shot failed (%1): exitStatus=%2 exitCode=%3")
                         .arg(m_manifest.pluginId)
                         .arg(static_cast<int>(oneShot.exitStatus()))
                         .arg(oneShot.exitCode()));
    }
    return ok;
}

bool ExternalProcessPlugin::ensureProcessRunning()
{
    if (m_process.state() == QProcess::Running)
        return true;

    const QFileInfo manifestInfo(m_manifest.manifestPath);
    const QString baseDir = manifestInfo.absolutePath();

    QString executable = m_manifest.executable.trimmed();
    QFileInfo execInfo(executable);
    const bool looksLikePath = executable.contains('/') || executable.contains('\\') || executable.startsWith('.');
    if (looksLikePath && execInfo.isRelative())
    {
        executable = QDir(baseDir).filePath(executable);
    }

    QStringList resolvedArgs;
    resolvedArgs.reserve(m_manifest.args.size());
    for (const QString &arg : m_manifest.args)
    {
        const QString trimmed = arg.trimmed();
        const bool argLooksLikePath = trimmed.contains('/') || trimmed.contains('\\') || trimmed.startsWith('.');
        QFileInfo argInfo(trimmed);
        if (argLooksLikePath && argInfo.isRelative())
            resolvedArgs.append(QDir(baseDir).filePath(trimmed));
        else
            resolvedArgs.append(trimmed);
    }

    m_process.setWorkingDirectory(baseDir);
    m_process.setProgram(executable);
    m_process.setArguments(resolvedArgs);
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
#ifdef Q_OS_WIN
    m_process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args)
                                                { args->flags |= CREATE_NO_WINDOW; });
#endif
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
