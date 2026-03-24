#include "BpmEntry.h"

BpmEntry::BpmEntry() : beatNum(0), numerator(1), denominator(1), bpm(120.0) {}

BpmEntry::BpmEntry(int beatNum, int numerator, int denominator, double bpm)
    : beatNum(beatNum), numerator(numerator), denominator(denominator), bpm(bpm) {}