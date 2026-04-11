#pragma once

#include <QPainter>
#include <QRect>
#include <QVector>
#include "model/BpmEntry.h"

class GridRenderer
{
public:
    // 绘制网格，xDivisions 为 X 轴分度，timeDivision 为时间轴分度（每 division 拍一条线）
    void drawGrid(QPainter &painter, const QRect &rect, int xDivisions,
                  double startTime, double endTime, double timeDivision,
                  const QVector<BpmEntry> &bpmList, int offset,
                  bool verticalFlip = false);
};