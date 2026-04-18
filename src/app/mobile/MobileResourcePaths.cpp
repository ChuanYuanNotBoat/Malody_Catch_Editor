#include "MobileResourcePaths.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include "utils/Logger.h"

namespace MobileResourcePaths
{
namespace
{
bool copyResourceTree(const QString &sourceDirPath, const QString &targetDirPath)
{
    QDir sourceDir(sourceDirPath);
    if (!sourceDir.exists())
        return false;

    QDir targetRoot;
    if (!targetRoot.mkpath(targetDirPath))
        return false;

    const QFileInfoList entries = sourceDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);
    for (const QFileInfo &entry : entries)
    {
        const QString sourcePath = entry.filePath();
        const QString targetPath = targetDirPath + "/" + entry.fileName();
        if (entry.isDir())
        {
            if (!copyResourceTree(sourcePath, targetPath))
                return false;
            continue;
        }

        QFile::remove(targetPath);
        if (!QFile::copy(sourcePath, targetPath))
            return false;
    }
    return true;
}

void ensureResourceTree(const QString &sourceDirPath, const QString &targetDirPath)
{
    QDir targetDir(targetDirPath);
    if (targetDir.exists() && !targetDir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty())
        return;

    if (!copyResourceTree(sourceDirPath, targetDirPath))
    {
        Logger::warn(QString("Failed to materialize mobile resource tree: %1 -> %2")
                         .arg(sourceDirPath, targetDirPath));
        return;
    }

    Logger::info(QString("Mobile resource tree prepared: %1").arg(targetDirPath));
}

QStringList dedupeExisting(const QStringList &candidates)
{
    QStringList result;
    for (const QString &path : candidates)
    {
        if (path.isEmpty() || result.contains(path))
            continue;
        if (path.startsWith("assets:/"))
        {
            result.append(path);
            continue;
        }
        if (QDir(path).exists())
            result.append(path);
    }
    return result;
}
} // namespace

void ensureBundledResourcesReady()
{
#if defined(Q_OS_ANDROID)
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appData.isEmpty())
        return;

    ensureResourceTree(":/mobile_assets/resources/default_skin", appData + "/resources/default_skin");
    ensureResourceTree(":/mobile_assets/resources/note_sounds", appData + "/resources/note_sounds");
#endif
}

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
