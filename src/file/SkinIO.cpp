#include "SkinIO.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

bool SkinIO::loadSkin(const QString& folderPath, Skin& outSkin) {
    outSkin.clear();
    QDir skinDir(folderPath);
    if (!skinDir.exists()) return false;

    // 读取 preview.json
    QFile previewFile(skinDir.filePath("preview.json"));
    if (previewFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(previewFile.readAll());
        if (!doc.isNull()) {
            QJsonObject obj = doc.object();
            outSkin.setTitle(obj.value("title").toString());
            outSkin.setDesc(obj.value("desc").toString());
            outSkin.setCoverPath(skinDir.filePath(obj.value("cover").toString()));
        }
    }

    // 加载所有图片，按文件名映射
    // 这里简化：实际需要根据规则将 catch-note-*.png 映射到 note 类型
    // 由于映射逻辑复杂，此处仅作框架，实际由 Skin 类实现细节
    // 我们将在 Skin::loadFromDir 中实现加载逻辑

    return true;
}

QStringList SkinIO::getSkinList(const QString& skinsDir) {
    QDir dir(skinsDir);
    QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    return subdirs;
}