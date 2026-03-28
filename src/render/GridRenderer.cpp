#include "GridRenderer.h"
#include "utils/MathUtils.h"
#include "model/BpmEntry.h"
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

    // Y 轴网格（时间轴） - 根据实际 BPM 表绘制精确的节拍线
    // 如果 timeDivision 大于0，则绘制每个 division 分度的线
    // 但实际需求中，时间网格应该按照时间轴分度来绘制，而不是等分像素。
    // 这里我们根据 BPM 表，计算每个 division 对应的毫秒位置，然后绘制。
    if (bpmList.isEmpty()) return;

    double totalDuration = endTime - startTime;
    if (totalDuration <= 0) return;

    // 简单做法：按时间等分，但为了对齐音符，应该使用 beat 位置。
    // 更精确：遍历所有 BPM 段，在每个段内按 division 生成网格线。
    // 为了简化，我们仍然使用时间等分，但确保使用 MathUtils::msToBeat 计算出对应的拍数，
    // 但实际上绘制网格并不需要精确对齐，因为用户看到的是视觉辅助线。
    // 但如果需要精确对应音符位置，则网格线必须与 beatToMs 计算出的时间点一致。
    // 这里我们改为按时间均匀划分，但时间点使用 beat 位置转换而来，确保与音符渲染一致。

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
    for (double beat = startBeatPos; beat <= endBeatPos; beat += beatStep) {
        int beatNum = static_cast<int>(beat);
        double frac = beat - beatNum;
        int numerator = static_cast<int>(frac * 1000000 + 0.5);
        int denominator = 1000000;
        // 简化分数（可省略）
        double ms = MathUtils::beatToMs(beatNum, numerator, denominator, bpmList, offset);
        if (ms < startTime || ms > endTime) continue;
        int y = rect.top() + static_cast<int>((ms - startTime) / totalDuration * rect.height());
        painter.drawLine(rect.left(), y, rect.right(), y);
    }
}