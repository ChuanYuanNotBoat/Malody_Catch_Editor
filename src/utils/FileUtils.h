#pragma once

#include <QString>
#include <QStringList>

class FileUtils {
public:
    static bool copyFile(const QString& src, const QString& dest);
    static bool removeFile(const QString& path);
    static bool exists(const QString& path);
    static QStringList getFilesInDir(const QString& dir, const QStringList& filters);
    static bool createDir(const QString& path);
    static QString getTempDir();
};