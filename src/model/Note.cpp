// src/model/Note.cpp
#include "Note.h"

Note::Note()
    : beatNum(0), numerator(1), denominator(1), type(0), x(256), isRain(false),
      endBeatNum(0), endNumerator(1), endDenominator(1), vol(100), offset(0)
{
    id = generateId();
}

Note::Note(int beatNum, int numerator, int denominator, int x)
    : beatNum(beatNum), numerator(numerator), denominator(denominator), type(0), x(x),
      isRain(false), endBeatNum(beatNum), endNumerator(numerator), endDenominator(denominator),
      vol(100), offset(0)
{
    id = generateId();
}

Note::Note(int startBeatNum, int startNumerator, int startDenominator,
           int endBeatNum, int endNumerator, int endDenominator, int x)
    : beatNum(startBeatNum), numerator(startNumerator), denominator(startDenominator), 
      type(3), x(x), isRain(true), endBeatNum(endBeatNum), endNumerator(endNumerator), 
      endDenominator(endDenominator), vol(100), offset(0)
{
    id = generateId();
}

Note::Note(int beatNum, int numerator, int denominator,
           const QString& sound, int vol, int offset)
    : beatNum(beatNum), numerator(numerator), denominator(denominator),
      type(1), x(-1), isRain(false), endBeatNum(beatNum), endNumerator(numerator),
      endDenominator(denominator), sound(sound), vol(vol), offset(offset)
{
    id = generateId();
}

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
    // 比较时忽略 id，只比较内容
    if (beatNum != other.beatNum ||
        numerator != other.numerator ||
        denominator != other.denominator ||
        type != other.type ||
        isRain != other.isRain) {
        return false;
    }

    if (type == 1) {
        // 音效音符比较
        return sound == other.sound &&
               vol == other.vol &&
               offset == other.offset;
    } else if (type == 3) {
        // Rain 音符比较
        return x == other.x &&
               endBeatNum == other.endBeatNum &&
               endNumerator == other.endNumerator &&
               endDenominator == other.endDenominator;
    } else {
        // 普通音符比较
        return x == other.x;
    }
}