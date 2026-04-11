#include "HyperfruitDetector.h"
#include "utils/MathUtils.h"
#include <cmath>
#include <limits>

HyperfruitDetector::HyperfruitDetector() : m_cs(3.2)
{
}

void HyperfruitDetector::setCS(double cs)
{
    m_cs = cs;
}

double HyperfruitDetector::catcherWidth() const
{
    return 106.75 * (1.0 - 0.14 * (m_cs - 5));
}

QSet<int> HyperfruitDetector::detect(const QVector<Note> &notes,
                                     const QVector<BpmEntry> &bpmList,
                                     int offset) const
{
    QSet<int> hyperSet;
    if (notes.size() < 2 || bpmList.isEmpty())
        return hyperSet;

    double catcherW = catcherWidth() / 2.0;
    const double BASE_SPEED = 1.0;
    const double DASH_MULT = 2.0;

    for (int i = 0; i < notes.size() - 1; ++i)
    {
        const Note &note1 = notes[i];
        if (note1.type != NoteType::NORMAL)
            continue;

        int nextIdx = i + 1;
        while (nextIdx < notes.size() && notes[nextIdx].type != NoteType::NORMAL)
        {
            nextIdx++;
        }
        if (nextIdx >= notes.size())
            break;

        const Note &note2 = notes[nextIdx];

        double t1 = MathUtils::beatToMs(note1.beatNum, note1.numerator, note1.denominator, bpmList, offset);
        double t2 = MathUtils::beatToMs(note2.beatNum, note2.numerator, note2.denominator, bpmList, offset);
        double dt = t2 - t1;
        if (dt <= 0)
            continue;

        double dx = std::abs(note2.x - note1.x);
        double W = catcherW;
        double D = std::max(0.0, dx - W);
        double D_max = BASE_SPEED * DASH_MULT * dt;

        if (D > D_max)
        {
            hyperSet.insert(i);            
        }
    }

    return hyperSet;
}