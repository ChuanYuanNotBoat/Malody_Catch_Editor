// src/model/Note.cpp
#include "Note.h"

Note::Note()
    : beatNum(0), numerator(1), denominator(1), x(256), isRain(false),
      endBeatNum(0), endNumerator(1), endDenominator(1)
{}

Note::Note(int beatNum, int numerator, int denominator, int x)
    : beatNum(beatNum), numerator(numerator), denominator(denominator), x(x),
      isRain(false), endBeatNum(beatNum), endNumerator(numerator), endDenominator(denominator)
{}

Note::Note(int startBeatNum, int startNumerator, int startDenominator,
           int endBeatNum, int endNumerator, int endDenominator, int x)
    : beatNum(startBeatNum), numerator(startNumerator), denominator(startDenominator), x(x),
      isRain(true), endBeatNum(endBeatNum), endNumerator(endNumerator), endDenominator(endDenominator)
{}

bool Note::isValidRain() const
{
    if (!isRain) return false;
    // 结束时间必须 >= 起始时间
    if (endBeatNum < beatNum) return false;
    if (endBeatNum == beatNum) {
        double start = static_cast<double>(numerator) / denominator;
        double end = static_cast<double>(endNumerator) / endDenominator;
        if (end < start) return false;
    }
    return true;
}

bool Note::operator==(const Note& other) const
{
    return beatNum == other.beatNum &&
           numerator == other.numerator &&
           denominator == other.denominator &&
           x == other.x &&
           isRain == other.isRain &&
           endBeatNum == other.endBeatNum &&
           endNumerator == other.endNumerator &&
           endDenominator == other.endDenominator;
}