#pragma once

#include <QPainter>

class GridRenderer {
public:
    void drawGrid(QPainter& painter, const QRect& rect, int xDivisions, double startTime, double endTime, double timeDivision);
};