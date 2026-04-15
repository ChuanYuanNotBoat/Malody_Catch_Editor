#pragma once

#include <QPainter>
#include <QRect>
#include <QVector>
#include "utils/MathUtils.h"   // 新增：提供 MathUtils::BpmCacheEntry

class GridRenderer
{
public:
    // 优化3：使用 BPM 缓存参数，提高网格绘制性能
    void drawGrid(QPainter &painter, const QRect &rect, int xDivisions,
                  double startTime, double endTime, double timeDivision,
                  const QVector<MathUtils::BpmCacheEntry> &bpmCache,
                  bool verticalFlip = false);
};