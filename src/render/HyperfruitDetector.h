#pragma once

#include <QSet>
#include <QVector>
#include "model/Note.h"

class HyperfruitDetector
{
public:
    HyperfruitDetector();
    void setCS(double cs);
    QSet<int> detect(const QVector<Note> &notes) const;

private:
    double m_cs;
    double catcherWidth() const;
};