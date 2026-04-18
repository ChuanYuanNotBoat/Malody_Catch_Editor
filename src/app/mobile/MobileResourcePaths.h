#pragma once

#include <QStringList>

namespace MobileResourcePaths
{
void ensureBundledResourcesReady();
QStringList additionalSkinBaseDirs();
QStringList additionalNoteSoundBaseDirs();
}
