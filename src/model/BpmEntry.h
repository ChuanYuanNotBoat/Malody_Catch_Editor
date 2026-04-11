#pragma once

struct BpmEntry
{
    int beatNum;
    int numerator;
    int denominator;
    double bpm;

    BpmEntry();
    BpmEntry(int beatNum, int numerator, int denominator, double bpm);
};