#pragma once

#include <QPainter>
#include <QRect>
#include <QVector>
#include "model/BpmEntry.h"

class GridRenderer
{
public:
    void drawGrid(QPainter &painter, const QRect &rect, int xDivisions,
                  double startTime, double endTime, double timeDivision,
                  const QVector<BpmEntry> &bpmList, int offset,
                  bool verticalFlip = false);
};