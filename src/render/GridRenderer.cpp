#include "GridRenderer.h"
#include "utils/MathUtils.h"
#include "model/BpmEntry.h"
#include "utils/Logger.h"
#include <QPainter>
#include <QDebug>

void GridRenderer::drawGrid(QPainter &painter, const QRect &rect, int xDivisions,
                            double startTime, double endTime, double timeDivision,
                            const QVector<MathUtils::BpmCacheEntry> &bpmCache,
                            bool verticalFlip)
{
    try
    {
        // X 轴网格
        double stepX = static_cast<double>(rect.width()) / xDivisions;
        painter.setPen(QPen(Qt::lightGray, 1, Qt::DotLine));
        for (int i = 1; i < xDivisions; ++i)
        {
            int x = rect.left() + static_cast<int>(std::round(i * stepX));
            painter.drawLine(x, rect.top(), x, rect.bottom());
        }

        if (bpmCache.isEmpty())
            return;

        double totalDuration = endTime - startTime;
        if (totalDuration <= 0)
            return;

        // 1. 根据 startTime 在缓存中找到对应段，计算 startBeatPos
        auto findBeatFromTime = [&](double timeMs) -> double
        {
            if (bpmCache.isEmpty())
                return 0.0;
            // 二分查找最后一个 accumulatedMs <= timeMs 的段
            int lo = 0, hi = bpmCache.size() - 1;
            while (lo < hi)
            {
                int mid = (lo + hi + 1) / 2;
                if (bpmCache[mid].accumulatedMs <= timeMs)
                    lo = mid;
                else
                    hi = mid - 1;
            }
            const auto &seg = bpmCache[lo];
            double beatOffset = (timeMs - seg.accumulatedMs) * (seg.bpm / 60000.0);
            return seg.beatPos + beatOffset;
        };

        double startBeatPos = findBeatFromTime(startTime);
        double endBeatPos = findBeatFromTime(endTime);

        int timeDivInt = static_cast<int>(timeDivision);
        if (timeDivInt <= 0)
            timeDivInt = 1;

        int startTick = static_cast<int>(std::ceil(startBeatPos * timeDivInt));
        int endTick = static_cast<int>(std::floor(endBeatPos * timeDivInt));

        // LOD 优化：若相邻刻度间距小于 5 像素，则跳过中间刻度（仅绘制整数拍）
        double pixelsPerMs = rect.height() / totalDuration;
        double minPixelStep = 5.0;
        double msPerTick = (60000.0 / 120.0) / timeDivInt; // 估算 120 BPM 下每刻度毫秒
        bool skipDenseTicks = (msPerTick * pixelsPerMs < minPixelStep);

        painter.setPen(QPen(Qt::gray, 1));
        QFont font = painter.font();
        font.setPointSize(8);
        painter.setFont(font);

        int lastDrawnY = -9999;
        for (int tick = startTick; tick <= endTick; ++tick)
        {
            int beatNum = tick / timeDivInt;
            int numerator = tick % timeDivInt;
            int denominator = timeDivInt;

            // 使用缓存快速计算毫秒时间
            double ms = MathUtils::beatToMs(beatNum, numerator, denominator, bpmCache);
            if (ms < startTime || ms > endTime)
                continue;

            int y;
            if (!verticalFlip)
            {
                y = rect.top() + static_cast<int>((ms - startTime) / totalDuration * rect.height());
            }
            else
            {
                y = rect.bottom() - static_cast<int>((ms - startTime) / totalDuration * rect.height());
            }

            // LOD 简化：如果密度过高且不是整数拍，跳过
            if (skipDenseTicks && numerator != 0)
            {
                // 仍然绘制整数拍
                continue;
            }

            // 避免完全重叠的线条（当像素间距为0时）
            if (qAbs(y - lastDrawnY) < 1)
                continue;
            lastDrawnY = y;

            painter.drawLine(rect.left(), y, rect.right(), y);

            // 整数拍显示拍号
            if (numerator == 0)
            {
                QString text = QString::number(beatNum);
                painter.setPen(Qt::darkGray);
                int textY = y;
                if (verticalFlip)
                {
                    textY = y + 12;
                    if (textY > rect.bottom())
                        textY = y - 12;
                }
                else
                {
                    textY = y - 2;
                    if (textY < rect.top())
                        textY = y + 12;
                }
                painter.drawText(rect.left() + 2, textY, text);
            }
        }
    }
    catch (const std::exception &e)
    {
        Logger::error(QString("GridRenderer::drawGrid - Exception: %1").arg(e.what()));
    }
    catch (...)
    {
        Logger::error("GridRenderer::drawGrid - Unknown exception");
    }
}