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

        // Y 轴网格（时间轴）- 根据 BPM 表绘制精确的节拍线
        if (bpmList.isEmpty())
            return;

        double totalDuration = endTime - startTime;
        if (totalDuration <= 0)
            return;

        // 获取总拍数范围
        int startBeat, startNum, startDen;
        MathUtils::msToBeat(startTime, bpmList, offset, startBeat, startNum, startDen);

        int endBeat, endNum, endDen;
        MathUtils::msToBeat(endTime, bpmList, offset, endBeat, endNum, endDen);

        double startBeatPos = startBeat + static_cast<double>(startNum) / startDen;
        double endBeatPos = endBeat + static_cast<double>(endNum) / endDen;
        double beatSpan = endBeatPos - startBeatPos;
        if (beatSpan <= 0)
            return;

        // 每 division 拍绘制一条线
        double beatStep = 1.0 / timeDivision;
        painter.setPen(QPen(Qt::gray, 1));
        QFont font = painter.font();
        font.setPointSize(8);
        painter.setFont(font);

        // 计算第一条网格线的拍号（对齐到beatStep的整数倍）
        double firstBeat = std::ceil(startBeatPos / beatStep) * beatStep;
        if (firstBeat - startBeatPos < -1e-10)
            firstBeat += beatStep;

        // 循环绘制网格线
        for (double beat = firstBeat; beat <= endBeatPos + beatStep * 0.5; beat += beatStep)
        {
            if (beat > endBeatPos)
                break;

            int beatNum = static_cast<int>(beat);
            double frac = beat - beatNum;
            bool isInteger = frac < 1e-4;
            int numerator = static_cast<int>(frac * 1000000 + 0.5);
            int denominator = 1000000;

            try
            {
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
                if (isInteger)
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
            catch (const std::exception &e)
            {
                // 仅保留错误日志，避免刷屏
                Logger::error(QString("GridRenderer::drawGrid - Exception at beat %1: %2").arg(beat).arg(e.what()));
                continue;
            }
            catch (...)
            {
                Logger::error(QString("GridRenderer::drawGrid - Unknown exception at beat %1").arg(beat));
                continue;
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