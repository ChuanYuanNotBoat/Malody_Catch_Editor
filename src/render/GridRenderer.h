#pragma once

#include <QPainter>
#include <QRect>
#include <QVector>
#include <QList>
#include <QString>
#include "utils/MathUtils.h"

class GridRenderer
{
public:
    void drawGrid(QPainter &painter, const QRect &rect, int xDivisions,
                  double startTime, double endTime, double timeDivision,
                  const QVector<MathUtils::BpmCacheEntry> &bpmCache,
                  bool verticalFlip = false,
                  bool colorizeTimeDivisions = false,
                  const QString &colorPreset = QString(),
                  const QList<int> &customDivisions = QList<int>());
};
