// src/utils/MathUtils.cpp
#include "MathUtils.h"
#include <cmath>
#include <numeric>   // for std::gcd
#include <QDebug>

double MathUtils::beatToMs(int beatNum, int numerator, int denominator,
                           const QVector<BpmEntry>& bpmList, int offsetMs)
{
    if (bpmList.isEmpty())
        return offsetMs;

    double targetBeat = beatNum + static_cast<double>(numerator) / denominator;

    // 找到第一个 BPM 变化点之前的 BPM
    int idx = 0;
    while (idx + 1 < bpmList.size()) {
        double nextBeat = bpmList[idx+1].beatNum +
                          static_cast<double>(bpmList[idx+1].numerator) / bpmList[idx+1].denominator;
        if (targetBeat < nextBeat)
            break;
        idx++;
    }

    const BpmEntry& cur = bpmList[idx];
    double curBeat = cur.beatNum + static_cast<double>(cur.numerator) / cur.denominator;

    // 计算从 curBeat 到 targetBeat 的毫秒数
    double beatDelta = targetBeat - curBeat;
    // 每分钟 cur.bpm 拍，所以每拍时长 = 60000 / cur.bpm 毫秒
    double ms = beatDelta * (60000.0 / cur.bpm);

    // 递归计算之前的部分
    if (idx > 0) {
        double prevMs = beatToMs(cur.beatNum, cur.numerator, cur.denominator, bpmList, offsetMs);
        return prevMs + ms;
    } else {
        return offsetMs + ms;
    }
}

void MathUtils::msToBeat(double ms, const QVector<BpmEntry>& bpmList, int offsetMs,
                         int& outBeatNum, int& outNumerator, int& outDenominator)
{
    if (bpmList.isEmpty()) {
        outBeatNum = 0;
        outNumerator = 1;
        outDenominator = 1;
        return;
    }

    double elapsed = ms - offsetMs;
    if (elapsed < 0) {
        outBeatNum = 0;
        outNumerator = 0;
        outDenominator = 1;
        return;
    }

    int idx = 0;
    double accumulatedMs = 0;
    while (idx < bpmList.size()) {
        const BpmEntry& cur = bpmList[idx];
        double curBeat = cur.beatNum + static_cast<double>(cur.numerator) / cur.denominator;

        // 当前段时长
        double nextBeat;
        if (idx + 1 < bpmList.size()) {
            const BpmEntry& next = bpmList[idx+1];
            nextBeat = next.beatNum + static_cast<double>(next.numerator) / next.denominator;
        } else {
            nextBeat = curBeat + 1e9; // 无穷大
        }
        double beatLen = nextBeat - curBeat;
        double segmentMs = beatLen * (60000.0 / cur.bpm);

        if (elapsed < accumulatedMs + segmentMs) {
            // 落在此段
            double remainMs = elapsed - accumulatedMs;
            double beatOffset = remainMs * (cur.bpm / 60000.0);
            double totalBeat = curBeat + beatOffset;

            outBeatNum = static_cast<int>(totalBeat);
            double fraction = totalBeat - outBeatNum;
            // 化简分数？暂时用分母 1
            outNumerator = static_cast<int>(std::round(fraction * 1e6));
            outDenominator = 1000000;
            // 简化分子分母
            int gcd = std::gcd(outNumerator, outDenominator);
            if (gcd > 0) {
                outNumerator /= gcd;
                outDenominator /= gcd;
            }
            // 确保分母为正
            if (outDenominator < 0) { outDenominator = -outDenominator; outNumerator = -outNumerator; }
            // 如果分子为 0，分母设为 1
            if (outNumerator == 0) outDenominator = 1;
            return;
        }

        accumulatedMs += segmentMs;
        idx++;
    }

    // 超出最后一个 BPM 段（理论上不应发生）
    outBeatNum = bpmList.last().beatNum;
    outNumerator = 1;
    outDenominator = 1;
}

Note MathUtils::snapNoteToTime(const Note& note, int timeDivision)
{
    Note snapped = note;
    // 将时间对齐到 division 的整数倍
    // division 表示 1/division 拍，即每拍分成 division 份
    double beatPos = note.beatNum + static_cast<double>(note.numerator) / note.denominator;
    double divisionBeat = 1.0 / timeDivision;
    double rounded = std::round(beatPos / divisionBeat) * divisionBeat;
    snapped.beatNum = static_cast<int>(rounded);
    double frac = rounded - snapped.beatNum;
    // 近似为最接近的分子/分母
    // 简化：将 frac 转为最简分数，分母不超过 256
    int num = static_cast<int>(std::round(frac * timeDivision));
    int den = timeDivision;
    int gcd = std::gcd(num, den);
    if (gcd > 0) {
        num /= gcd;
        den /= gcd;
    }
    snapped.numerator = num;
    snapped.denominator = den;
    if (note.isRain) {
        // 同样处理结束时间
        double endBeatPos = note.endBeatNum + static_cast<double>(note.endNumerator) / note.endDenominator;
        double endRounded = std::round(endBeatPos / divisionBeat) * divisionBeat;
        snapped.endBeatNum = static_cast<int>(endRounded);
        double endFrac = endRounded - snapped.endBeatNum;
        int endNum = static_cast<int>(std::round(endFrac * timeDivision));
        int endDen = timeDivision;
        int endGcd = std::gcd(endNum, endDen);
        if (endGcd > 0) {
            endNum /= endGcd;
            endDen /= endGcd;
        }
        snapped.endNumerator = endNum;
        snapped.endDenominator = endDen;
    }
    return snapped;
}

int MathUtils::snapXToGrid(int x, int gridDivision)
{
    // gridDivision 是 X 轴分度，将 0-512 分成 gridDivision 格
    int step = 512 / gridDivision;
    int rounded = std::round(static_cast<double>(x) / step) * step;
    return std::clamp(rounded, 0, 512);
}

bool MathUtils::isSameTime(const Note& a, const Note& b, int timeDivision)
{
    // 将两个音符的时间对齐到 timeDivision 后比较
    Note sa = snapNoteToTime(a, timeDivision);
    Note sb = snapNoteToTime(b, timeDivision);
    return sa.beatNum == sb.beatNum &&
           sa.numerator == sb.numerator &&
           sa.denominator == sb.denominator;
}