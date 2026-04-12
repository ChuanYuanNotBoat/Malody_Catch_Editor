#include "GridRenderer.h"
#include "utils/MathUtils.h"
#include "model/BpmEntry.h"
#include "utils/Logger.h"
#include <QPainter>
#include <QDebug>

void GridRenderer::drawGrid(QPainter &painter, const QRect &rect, int xDivisions,
                            double startTime, double endTime, double timeDivision,
                            const QVector<BpmEntry> &bpmList, int offset,
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

        if (bpmList.isEmpty())
            return;

        double totalDuration = endTime - startTime;
        if (totalDuration <= 0)
            return;

        // 将起止时间转换为拍数（仅两次调用）
        int startBeatNum, startNum, startDen;
        MathUtils::msToBeat(startTime, bpmList, offset, startBeatNum, startNum, startDen);
        double startBeatPos = startBeatNum + static_cast<double>(startNum) / startDen;

        int endBeatNum, endNum, endDen;
        MathUtils::msToBeat(endTime, bpmList, offset, endBeatNum, endNum, endDen);
        double endBeatPos = endBeatNum + static_cast<double>(endNum) / endDen;

        // 以 timeDivision 的倍数作为网格刻度
        int timeDivInt = static_cast<int>(timeDivision);
        if (timeDivInt <= 0)
            timeDivInt = 1;

        int startTick = static_cast<int>(std::ceil(startBeatPos * timeDivInt));
        int endTick = static_cast<int>(std::floor(endBeatPos * timeDivInt));

        painter.setPen(QPen(Qt::gray, 1));
        QFont font = painter.font();
        font.setPointSize(8);
        painter.setFont(font);

        for (int tick = startTick; tick <= endTick; ++tick)
        {
            int beatNum = tick / timeDivInt;
            int numerator = tick % timeDivInt;
            int denominator = timeDivInt;

            double ms = MathUtils::beatToMs(beatNum, numerator, denominator, bpmList, offset);
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