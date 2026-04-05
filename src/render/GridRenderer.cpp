#include "GridRenderer.h"
#include "utils/MathUtils.h"
#include "model/BpmEntry.h"
#include <QPainter>
#include <QDebug>

void GridRenderer::drawGrid(QPainter& painter, const QRect& rect, int xDivisions,
                            double startTime, double endTime, double timeDivision,
                            const QVector<BpmEntry>& bpmList, int offset)
{
    // X 轴网格
    int stepX = rect.width() / xDivisions;
    painter.setPen(QPen(Qt::lightGray, 1, Qt::DotLine));
    for (int i = 1; i < xDivisions; ++i) {
        int x = rect.left() + i * stepX;
        painter.drawLine(x, rect.top(), x, rect.bottom());
    }

    // Y 轴网格（时间轴）- 根据 BPM 表绘制精确的节拍线
    if (bpmList.isEmpty()) return;

    double totalDuration = endTime - startTime;
    if (totalDuration <= 0) return;

    // 获取总拍数范围
    int startBeat, startNum, startDen;
    int endBeat, endNum, endDen;
    MathUtils::msToBeat(startTime, bpmList, offset, startBeat, startNum, startDen);
    MathUtils::msToBeat(endTime, bpmList, offset, endBeat, endNum, endDen);
    double startBeatPos = startBeat + static_cast<double>(startNum)/startDen;
    double endBeatPos = endBeat + static_cast<double>(endNum)/endDen;
    double beatSpan = endBeatPos - startBeatPos;
    if (beatSpan <= 0) return;

    // 每 division 拍绘制一条线
    double beatStep = 1.0 / timeDivision;
    painter.setPen(QPen(Qt::gray, 1));
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    for (double beat = startBeatPos; beat <= endBeatPos; beat += beatStep) {
        int beatNum = static_cast<int>(beat);
        double frac = beat - beatNum;
        int numerator = static_cast<int>(frac * 1000000 + 0.5);
        int denominator = 1000000;
        double ms = MathUtils::beatToMs(beatNum, numerator, denominator, bpmList, offset);
        if (ms < startTime || ms > endTime) continue;
        int y = rect.top() + static_cast<int>((ms - startTime) / totalDuration * rect.height());
        painter.drawLine(rect.left(), y, rect.right(), y);

        // 如果是整数拍（frac < 1e-6），在左端显示拍数
        if (frac < 1e-6) {
            QString text = QString::number(beatNum);
            painter.setPen(Qt::darkGray);
            painter.drawText(rect.left() + 2, y - 2, text);
        }
    }
}