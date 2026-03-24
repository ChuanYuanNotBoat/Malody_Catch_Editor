#pragma once

#include <QString>
#include "model/Chart.h"

class ProjectIO {
public:
    static bool exportToMcz(const QString& outputPath, const Chart& chart,
                            const QString& audioPath, const QString& bgPath);
    static bool importMcz(const QString& mczPath, QString& outChartPath);
};