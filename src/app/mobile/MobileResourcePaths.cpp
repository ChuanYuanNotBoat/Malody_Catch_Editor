#include "MobileResourcePaths.h"

#include <QDir>
#include <QStandardPaths>

namespace MobileResourcePaths
{
namespace
{
QStringList dedupeExisting(const QStringList &candidates)
{
    QStringList result;
    for (const QString &path : candidates)
    {
        if (path.isEmpty() || result.contains(path))
            continue;
        if (QDir(path).exists())
            result.append(path);
    }
    return result;
}
} // namespace

QStringList additionalSkinBaseDirs()
{
#if defined(Q_OS_ANDROID)
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString appLocal = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    QStringList candidates;
    candidates << (appData + "/skins")
               << (appData + "/resources/default_skin")
               << (appLocal + "/skins")
               << (appLocal + "/resources/default_skin")
               << "assets:/skins"
               << "assets:/resources/default_skin";
    return dedupeExisting(candidates);
#else
    return {};
#endif
}

QStringList additionalNoteSoundBaseDirs()
{
#if defined(Q_OS_ANDROID)
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString appLocal = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    QStringList candidates;
    candidates << (appData + "/note_sounds")
               << (appData + "/resources/note_sounds")
               << (appLocal + "/note_sounds")
               << (appLocal + "/resources/note_sounds")
               << "assets:/note_sounds"
               << "assets:/resources/note_sounds";
    return dedupeExisting(candidates);
#else
    return {};
#endif
}

} // namespace MobileResourcePaths

