#include "GridRenderer.h"

void GridRenderer::drawGrid(QPainter& painter, const QRect& rect, int xDivisions, double startTime, double endTime, double timeDivision) {
    // X 轴网格
    int stepX = rect.width() / xDivisions;
    painter.setPen(QPen(Qt::lightGray, 1, Qt::DotLine));
    for (int i = 1; i < xDivisions; ++i) {
        int x = rect.left() + i * stepX;
        painter.drawLine(x, rect.top(), x, rect.bottom());
    }

    // Y 轴网格（时间轴）
    double timeSpan = endTime - startTime;
    if (timeSpan <= 0) return;
    // 根据 timeDivision 计算每格代表的毫秒数
    double msPerDivision = timeSpan / timeDivision; // 简单实现
    painter.setPen(QPen(Qt::gray, 1));
    for (double t = startTime; t <= endTime; t += msPerDivision) {
        int y = rect.top() + static_cast<int>((t - startTime) / timeSpan * rect.height());
        painter.drawLine(rect.left(), y, rect.right(), y);
    }
}