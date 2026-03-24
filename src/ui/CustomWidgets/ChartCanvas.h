#pragma once

#include <QWidget>
#include <QPointF>
#include <QSet>
#include "model/Note.h"

class ChartController;
class SelectionController;
class NoteRenderer;
class GridRenderer;
class HyperfruitDetector;

class ChartCanvas : public QWidget {
    Q_OBJECT
public:
    enum Mode { PlaceNote, PlaceRain, Select, Delete };
    explicit ChartCanvas(QWidget* parent = nullptr);
    ~ChartCanvas();

    void setChartController(ChartController* controller);
    void setSelectionController(SelectionController* controller);
    void setColorMode(bool enabled);
    void setHyperfruitEnabled(bool enabled);
    void setTimeDivision(int division);
    void setGridDivision(int division);
    void setGridSnap(bool snap);
    void setScrollPos(double timeMs);
    void paste(); // 粘贴预览

public slots:
    void showGridSettings();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void drawGrid(QPainter& painter);
    QPointF noteToPos(const Note& note) const;
    Note posToNote(const QPointF& pos) const;

    ChartController* m_chartController;
    SelectionController* m_selectionController;
    NoteRenderer* m_noteRenderer;
    GridRenderer* m_gridRenderer;
    HyperfruitDetector* m_hyperfruitDetector;

    Mode m_currentMode;
    bool m_colorMode;
    bool m_hyperfruitEnabled;
    int m_timeDivision;
    int m_gridDivision;
    bool m_gridSnap;
    double m_scrollPos;
    double m_visibleRange;

    // 交互状态
    bool m_isSelecting;
    QPointF m_selectionStart;
    QPointF m_selectionEnd;
    bool m_isDragging;
    QPointF m_dragStart;
    QSet<int> m_draggedNotes;

    // 粘贴预览
    bool m_isPasting;
    QVector<Note> m_pasteNotes;
    QPointF m_pasteOffset;
};