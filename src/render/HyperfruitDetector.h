#pragma once

#include <QSet>
#include <QVector>
#include "model/Note.h"
#include "model/BpmEntry.h"

class HyperfruitDetector
{
public:
    HyperfruitDetector();
    void setCS(double cs);
    QSet<int> detect(const QVector<Note> &notes,
                     const QVector<BpmEntry> &bpmList,
                     int offset) const;

private:
    double m_cs;
    double catcherWidth() const;
};