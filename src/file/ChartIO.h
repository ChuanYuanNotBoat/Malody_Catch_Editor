#pragma once

#include <QString>
#include "model/Chart.h"

class ChartIO {
public:
    static bool load(const QString& filePath, Chart& outChart);
    static bool save(const QString& filePath, const Chart& chart);
};