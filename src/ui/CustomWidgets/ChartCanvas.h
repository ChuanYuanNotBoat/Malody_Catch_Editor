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
class Skin;

class ChartCanvas : public QWidget {
    Q_OBJECT
public:
    enum Mode { PlaceNote, PlaceRain, Delete, Select };
    explicit ChartCanvas(QWidget* parent = nullptr);
    ~ChartCanvas();

    void setChartController(ChartController* controller);
    void setSelectionController(SelectionController* controller);
    void setSkin(Skin* skin);
    void setColorMode(bool enabled);
    void setHyperfruitEnabled(bool enabled);
    void setTimeDivision(int division);
    void setGridDivision(int division);
    void setGridSnap(bool snap);
    void setScrollPos(double timeMs);
    void setNoteSize(int size);
    void setMode(Mode mode);
    void paste();

public slots:
    void showGridSettings();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void drawGrid(QPainter& painter);
    QPointF noteToPos(const Note& note) const;
    Note posToNote(const QPointF& pos) const;
    double yPosFromTime(double timeMs) const;
    int hitTestNote(const QPointF& pos) const;          // 返回音符索引，未命中返回 -1
    QRectF getRainNoteRect(const Note& note) const;     // 计算rain音符的矩形区域

    void beginMoveSelection(const QPointF& startPos);   // 开始移动选中音符
    void updateMoveSelection(const QPointF& currentPos); // 更新移动偏移
    void endMoveSelection();                             // 结束移动，压入复合撤销命令
    void prepareMoveChanges();                           // 备份当前选中的音符

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

    bool m_isSelecting;
    QPointF m_selectionStart;
    QPointF m_selectionEnd;
    bool m_isDragging;
    QPointF m_dragStart;
    QSet<int> m_draggedNotes;

    bool m_isPasting;
    QVector<Note> m_pasteNotes;
    QPointF m_pasteOffset;

    // 移动选中的临时状态
    bool m_isMovingSelection = false;
    QPointF m_moveStartPos;                    // 拖动起始点（全局坐标）
    QList<QPair<Note, Note>> m_moveChanges;    // 原始音符与当前临时音符的映射
    QSet<int> m_originalSelectedIndices;       // 拖动开始时的选中索引集

    bool m_rainFirst;
    QPointF m_rainStartPos;

};