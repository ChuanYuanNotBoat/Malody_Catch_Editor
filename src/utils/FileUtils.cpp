#include "FileUtils.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

bool FileUtils::copyFile(const QString &src, const QString &dest)
{
    return QFile::copy(src, dest);
}

bool FileUtils::removeFile(const QString &path)
{
    return QFile::remove(path);
}

bool FileUtils::exists(const QString &path)
{
    return QFile::exists(path);
}

QStringList FileUtils::getFilesInDir(const QString &dir, const QStringList &filters)
{
    QDir d(dir);
    return d.entryList(filters, QDir::Files);
}

bool FileUtils::createDir(const QString &path)
{
    QDir d;
    return d.mkpath(path);
}

QString FileUtils::getTempDir()
{
    static QTemporaryDir tempDir;
    if (!tempDir.isValid())
        return QString();
    return tempDir.path();
}