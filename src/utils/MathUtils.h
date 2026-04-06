#pragma once

#include <QVector>
#include "model/Note.h"
#include "model/BpmEntry.h"

class MathUtils {
public:
    static double beatToMs(int beatNum, int numerator, int denominator,
                           const QVector<BpmEntry>& bpmList, int offsetMs);
    static void msToBeat(double ms, const QVector<BpmEntry>& bpmList, int offsetMs,
                         int& outBeatNum, int& outNumerator, int& outDenominator);
    static Note snapNoteToTime(const Note& note, int timeDivision);
    static int snapXToGrid(int x, int gridDivision);
    static bool isSameTime(const Note& a, const Note& b, int timeDivision);
    
    // 边界吸附函数
    static int snapXToBoundary(int x);  // X轴边界吸附到0或512
    static Note snapNoteToTimeWithBoundary(const Note& note, int timeDivision);  // 时间分度吸附带边界检查
};