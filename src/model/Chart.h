#pragma once

#include <QVector>
#include "Note.h"
#include "BpmEntry.h"
#include "MetaData.h"

class Chart
{
public:
    Chart();

    void addNote(const Note &note);
    void removeNote(int index);
    void removeNote(const Note &note);
    void clearNotes();
    const QVector<Note> &notes() const;
    QVector<Note> &notes();

    void addBpm(const BpmEntry &bpm);
    void removeBpm(int index);
    void updateBpm(int index, const BpmEntry &bpm);
    const QVector<BpmEntry> &bpmList() const;
    QVector<BpmEntry> &bpmList();

    MetaData &meta();
    const MetaData &meta() const;

    void sortNotes();
    bool isValid() const;
    void clear();

private:
    QVector<Note> m_notes;
    QVector<BpmEntry> m_bpmList;
    MetaData m_meta;
};