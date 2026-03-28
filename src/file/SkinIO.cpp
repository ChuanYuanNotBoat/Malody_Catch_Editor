#include "SkinIO.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include "utils/Logger.h"

bool SkinIO::loadSkin(const QString& folderPath, Skin& outSkin)
{
    outSkin.clear();
    QDir skinDir(folderPath);
    if (!skinDir.exists()) {
        Logger::warn(QString("Skin directory does not exist: %1").arg(folderPath));
        return false;
    }

    QFile previewFile(skinDir.filePath("preview.json"));
    if (previewFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(previewFile.readAll());
        if (!doc.isNull()) {
            QJsonObject obj = doc.object();
            outSkin.setTitle(obj.value("title").toString());
            outSkin.setDesc(obj.value("desc").toString());
            outSkin.setCoverPath(skinDir.filePath(obj.value("cover").toString()));
            Logger::info(QString("Loaded skin preview: %1").arg(outSkin.title()));
        }
    } else {
        Logger::warn(QString("No preview.json found in %1, using folder name as display name").arg(folderPath));
        outSkin.setTitle(skinDir.dirName());
        outSkin.setDesc("");
    }

    if (!outSkin.loadFromDir(folderPath)) {
        Logger::error("Failed to load skin resources from " + folderPath);
        return false;
    }

    Logger::info(QString("Skin loaded successfully from %1").arg(folderPath));
    return true;
}

QStringList SkinIO::getSkinList(const QString& skinsDir)
{
    QDir dir(skinsDir);
    if (!dir.exists()) {
        Logger::warn("Skins directory not found: " + skinsDir);
        return QStringList();
    }
    QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    Logger::debug(QString("Found %1 skin directories in %2").arg(subdirs.size()).arg(skinsDir));
    return subdirs;
}

QString SkinIO::getSkinDisplayName(const QString& skinPath)
{
    QDir skinDir(skinPath);
    if (!skinDir.exists()) return skinDir.dirName();

    QFile previewFile(skinDir.filePath("preview.json"));
    if (previewFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(previewFile.readAll());
        if (!doc.isNull()) {
            QJsonObject obj = doc.object();
            QString title = obj.value("title").toString();
            if (!title.isEmpty()) return title;
        }
    }
    return skinDir.dirName();
}