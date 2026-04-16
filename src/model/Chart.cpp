#include "Chart.h"
#include <algorithm>
#include <QDebug>

Chart::Chart() { clear(); }

void Chart::addNote(const Note &note)
{
    m_notes.append(note);
    sortNotes();
}

void Chart::removeNote(int index)
{
    if (index >= 0 && index < m_notes.size())
        m_notes.removeAt(index);
}

void Chart::removeNote(const Note &note)
{
    // Prefer id match to avoid deleting a different note with same content.
    if (!note.id.isEmpty())
    {
        for (int i = 0; i < m_notes.size(); ++i)
        {
            if (m_notes[i].id == note.id)
            {
                m_notes.removeAt(i);
                return;
            }
        }
    }

    // Fallback for notes without id (legacy data).
    int idx = m_notes.indexOf(note);
    if (idx != -1)
    {
        m_notes.removeAt(idx);
        return;
    }

    qDebug() << "[Chart] removeNote: failed to find note with id" << note.id
             << "for removal, beat" << note.getStartBeat();
}

void Chart::clearNotes() { m_notes.clear(); }

const QVector<Note> &Chart::notes() const { return m_notes; }
QVector<Note> &Chart::notes() { return m_notes; }

void Chart::addBpm(const BpmEntry &bpm)
{
    m_bpmList.append(bpm);
    std::sort(m_bpmList.begin(), m_bpmList.end(),
              [](const BpmEntry &a, const BpmEntry &b)
              {
                  if (a.beatNum != b.beatNum)
                      return a.beatNum < b.beatNum;
                  double aPos = static_cast<double>(a.numerator) / a.denominator;
                  double bPos = static_cast<double>(b.numerator) / b.denominator;
                  return aPos < bPos;
              });
}

void Chart::removeBpm(int index)
{
    if (index >= 0 && index < m_bpmList.size())
        m_bpmList.removeAt(index);
}

void Chart::updateBpm(int index, const BpmEntry &bpm)
{
    if (index >= 0 && index < m_bpmList.size())
    {
        m_bpmList[index] = bpm;
        // 重新排序
        std::sort(m_bpmList.begin(), m_bpmList.end(),
                  [](const BpmEntry &a, const BpmEntry &b)
                  {
                      if (a.beatNum != b.beatNum)
                          return a.beatNum < b.beatNum;
                      double aPos = static_cast<double>(a.numerator) / a.denominator;
                      double bPos = static_cast<double>(b.numerator) / b.denominator;
                      return aPos < bPos;
                  });
    }
}

const QVector<BpmEntry> &Chart::bpmList() const { return m_bpmList; }
QVector<BpmEntry> &Chart::bpmList() { return m_bpmList; }

MetaData &Chart::meta() { return m_meta; }
const MetaData &Chart::meta() const { return m_meta; }

void Chart::sortNotes()
{
    std::sort(m_notes.begin(), m_notes.end(),
              [](const Note &a, const Note &b)
              {
                  // 首先按拍号排序
                  if (a.beatNum != b.beatNum)
                      return a.beatNum < b.beatNum;

                  // 同一拍内，按分数位置排序
                  double aPos = static_cast<double>(a.numerator) / a.denominator;
                  double bPos = static_cast<double>(b.numerator) / b.denominator;
                  if (aPos != bPos)
                      return aPos < bPos;

                  // 同一时间位置，按类型排序：普通/rain < 音效
                  if (a.type == NoteType::SOUND && b.type != NoteType::SOUND)
                      return false; // 音效排在后面
                  if (a.type != NoteType::SOUND && b.type == NoteType::SOUND)
                      return true; // 非音效排在前面

                  // 都是音效或都不是音效，则按x坐标排序（音效的x=-1被自动排在最后）
                  return a.x < b.x;
              });
}

bool Chart::isValid() const { return !m_notes.isEmpty() || m_bpmList.size() >= 1; }

void Chart::clear()
{
    m_notes.clear();
    m_bpmList.clear();
    m_meta = MetaData();
    m_bpmList.append(BpmEntry(0, 1, 1, 120.0));
}
