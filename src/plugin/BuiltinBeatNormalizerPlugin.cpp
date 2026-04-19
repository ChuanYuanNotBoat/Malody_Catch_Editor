#include "BuiltinBeatNormalizerPlugin.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>
#include <numeric>

QString BuiltinBeatNormalizerPlugin::pluginId() const
{
    return "tool.beat_normalizer.py";
}

QString BuiltinBeatNormalizerPlugin::displayName() const
{
    return "Note Color Formatter";
}

QString BuiltinBeatNormalizerPlugin::version() const
{
    return "1.0.0-builtin";
}

QString BuiltinBeatNormalizerPlugin::description() const
{
    return "Built-in fallback plugin for note beat normalization.";
}

QString BuiltinBeatNormalizerPlugin::author() const
{
    return "Built-in";
}

QString BuiltinBeatNormalizerPlugin::localizedDisplayName(const QString &locale) const
{
    if (locale.startsWith("zh", Qt::CaseInsensitive))
        return QString::fromUtf8("音符颜色格式化");
    return displayName();
}

QString BuiltinBeatNormalizerPlugin::localizedDescription(const QString &locale) const
{
    if (locale.startsWith("zh", Qt::CaseInsensitive))
        return QString::fromUtf8("内置后备插件：用于规整谱面音符拍号分数。");
    return description();
}

int BuiltinBeatNormalizerPlugin::pluginApiVersion() const
{
    return kHostApiVersion;
}

QStringList BuiltinBeatNormalizerPlugin::capabilities() const
{
    return {kCapabilityToolActions};
}

bool BuiltinBeatNormalizerPlugin::initialize(QWidget *mainWindow)
{
    Q_UNUSED(mainWindow);
    return true;
}

void BuiltinBeatNormalizerPlugin::shutdown()
{
}

QList<PluginInterface::ToolAction> BuiltinBeatNormalizerPlugin::toolActions() const
{
    ToolAction action;
    action.actionId = "simplify_note_beats";
    action.title = QString::fromUtf8("格式化音符颜色");
    action.description = QString::fromUtf8("按规则整理并统一谱面中的音符拍号分数。");
    action.confirmMessage = QString::fromUtf8("将对当前谱面的音符分数进行格式化处理，是否继续？");
    action.placement = kPlacementLeftSidebar;
    action.requiresUndoSnapshot = true;
    return {action};
}

bool BuiltinBeatNormalizerPlugin::runToolAction(const QString &actionId, const QVariantMap &context)
{
    if (actionId != "simplify_note_beats")
        return false;

    const QString chartPath = resolveChartPath(context);
    if (chartPath.isEmpty())
        return false;

    if (!QFileInfo::exists(chartPath))
        return false;

    if (!chartPath.endsWith(".mc", Qt::CaseInsensitive))
        return false;

    return normalizeMcFile(chartPath);
}

QString BuiltinBeatNormalizerPlugin::resolveChartPath(const QVariantMap &context)
{
    const QStringList keys = {"chart_path_native", "chart_path", "chart_path_canonical"};
    for (const QString &key : keys)
    {
        const QVariant v = context.value(key);
        if (!v.isValid())
            continue;
        const QString path = v.toString().trimmed();
        if (!path.isEmpty())
            return path;
    }
    return QString();
}

bool BuiltinBeatNormalizerPlugin::normalizeMcFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    QJsonObject root = doc.object();
    const QJsonValue noteValue = root.value("note");
    if (!noteValue.isArray())
        return true;

    QJsonArray notes = noteValue.toArray();
    bool changed = false;
    for (int i = 0; i < notes.size(); ++i)
    {
        if (!notes[i].isObject())
            continue;

        QJsonObject note = notes[i].toObject();
        const QJsonValue beatValue = note.value("beat");
        if (!beatValue.isArray())
            continue;
        QJsonArray beat = beatValue.toArray();
        if (beat.size() != 3)
            continue;

        const int num = beat[1].toInt();
        const int den = beat[2].toInt();
        if (den == 0)
            continue;

        const int g = std::gcd(num, den);
        if (g <= 1)
            continue;

        beat[1] = num / g;
        beat[2] = den / g;
        note.insert("beat", beat);
        notes[i] = note;
        changed = true;
    }

    if (!changed)
        return true;

    root.insert("note", notes);
    const QJsonDocument outDoc(root);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(outDoc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

