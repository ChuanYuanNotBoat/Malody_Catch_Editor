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
class PlaybackController;

class ChartCanvas : public QWidget {
    Q_OBJECT
public:
    enum Mode { PlaceNote, PlaceRain, Delete, Select };
    explicit ChartCanvas(QWidget* parent = nullptr);
    ~ChartCanvas();

    void setChartController(ChartController* controller);
    void setSelectionController(SelectionController* controller);
    void setSkin(Skin* skin);
    void setPlaybackController(PlaybackController* controller);
    void setColorMode(bool enabled);
    void setHyperfruitEnabled(bool enabled);
    void setTimeDivision(int division);
    void setGridDivision(int division);
    void setGridSnap(bool snap);
    void setScrollPos(double timeMs);
    void setNoteSize(int size);
    void setMode(Mode mode);
    void paste();
    
    bool isVerticalFlip() const;
    void setVerticalFlip(bool flip);

public slots:
    void showGridSettings();
    void playbackPositionChanged(double timeMs);

signals:
    void verticalFlipChanged(bool flipped);
    void scrollPositionChanged(double beat);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    void drawGrid(QPainter& painter);
    QPointF noteToPos(const Note& note) const;
    Note posToNote(const QPointF& pos) const;
    double yPosFromTime(double timeMs) const;
    double beatToY(double beat) const;
    double yToBeat(double y) const;
    int hitTestNote(const QPointF& pos) const;          // 返回音符索引，未命中返回 -1
    QRectF getRainNoteRect(const Note& note) const;     // 计算rain音符的矩形区域

    void beginMoveSelection(const QPointF& startPos, int referenceIndex = -1);   // 开始移动选中音符
    void updateMoveSelection(const QPointF& currentPos); // 更新移动偏移
    void endMoveSelection();                             // 结束移动，压入复合撤销命令
    void prepareMoveChanges();                           // 备份当前选中的音符

    void snapPlayheadToGrid();       // 对齐参考线到网格
    void startSnapTimer();           // 启动对齐定时器
    void stopSnapTimer();            // 停止对齐定时器

    ChartController* m_chartController;
    SelectionController* m_selectionController;
    PlaybackController* m_playbackController;
    NoteRenderer* m_noteRenderer;
    GridRenderer* m_gridRenderer;
    HyperfruitDetector* m_hyperfruitDetector;

    Mode m_currentMode;
    bool m_colorMode;
    bool m_hyperfruitEnabled;
    bool m_verticalFlip;
    int m_timeDivision;
    int m_gridDivision;
    bool m_gridSnap;
    double m_scrollBeat;          // 滚动位置（拍）
    double m_visibleBeatRange;    // 可见范围（拍）
    double m_currentPlayTime;     // 当前播放时间（毫秒）
    bool m_autoScrollEnabled;     // 自动滚动是否启用（用户手动滚动后禁用）

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
    int m_dragReferenceIndex;                  // 拖动参考音符索引（-1表示无）

    // 棚格吸附备份状态
    bool m_gridSnapBackup;                     // 备份的棚格吸附状态
    bool m_wasGridSnapEnabled;                 // 移动前棚格吸附是否启用

    // 超果检测备份状态
    bool m_hyperfruitEnabledBackup;            // 移动前超果检测是否启用

    bool m_rainFirst;
    QPointF m_rainStartPos;

    bool m_snapToGrid;               // 是否启用网格对齐
    int m_snapTimerId;               // 滚动结束对齐定时器ID
    bool m_isScrolling;              // 是否正在滚动

    int leftMargin() const;
    int rightMargin() const;

private slots:
    void onSelectionChanged();                 // 选中状态变化处理

};