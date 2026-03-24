#pragma once

#include <QString>
#include <QStringList>
#include "model/Skin.h"

class SkinIO {
public:
    static bool loadSkin(const QString& folderPath, Skin& outSkin);
    static QStringList getSkinList(const QString& skinsDir);
};