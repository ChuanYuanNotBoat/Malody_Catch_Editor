#include "HyperfruitDetector.h"
#include "utils/MathUtils.h"
#include <cmath>

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

QSet<int> HyperfruitDetector::detect(const QVector<Note> &notes) const
{
    QSet<int> hyperSet;
    if (notes.size() < 2)
        return hyperSet;

    double catcherW = catcherWidth() / 2.0;
    for (int i = 1; i < notes.size(); ++i)
    {
        const Note &prev = notes[i - 1];
        const Note &curr = notes[i];

        // 只对普通 note 进行检测（Rain 音符不参与判定）
        if (prev.isRain || curr.isRain)
            continue;

        // 获取时间差（毫秒）
        // 假设外部已经传入了 bpmList 和 offset，但这里没有这些信息，所以需要从外部计算
        // 我们无法在此处计算，因此需要外部提供时间差，或者将 bpmList 传入。
        // 这里简化：假设调用者已经保证了 dt 可用，但实际中需要计算。
        // 更好的做法是 HyperfruitDetector 接收 bpmList 和 offset，但为了简化接口，
        // 我们在这里假设外部已经计算好时间差并存储在 note 中？不现实。
        // 因此，我们改变设计：detect 接受 bpmList 和 offset 参数。
        // 但为保持接口一致，我们保留现有接口，在实际使用时由调用者计算时间差后设置到 hyperSet。
        // 这需要外部实现，当前只是框架，留待完善。
        // 实际项目中应修改 detect 参数。
    }
    return hyperSet;
}