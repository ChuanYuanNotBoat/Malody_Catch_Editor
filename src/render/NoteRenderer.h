#pragma once

#include <QPainter>
#include <QSet>
#include "model/Note.h"

class Skin;
class HyperfruitDetector;

class NoteRenderer {
public:
    NoteRenderer();

    void setSkin(const Skin* skin);
    void setShowColors(bool show);
    void setHyperfruitEnabled(bool enabled);
    void setHyperfruitDetector(HyperfruitDetector* detector);
    void setHyperfruitSet(const QSet<int>& hyperSet);

    void drawNote(QPainter& painter, const Note& note, const QPointF& pos, bool selected) const;
    void drawRain(QPainter& painter, const Note& note, const QRectF& rect, bool selected) const;

private:
    const Skin* m_skin;
    bool m_showColors;
    bool m_hyperfruitEnabled;
    HyperfruitDetector* m_hyperfruitDetector;
    QSet<int> m_hyperfruitSet;
};