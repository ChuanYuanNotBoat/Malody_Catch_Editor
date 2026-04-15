// src/ui/CustomWidgets/ChartCanvas.h
#pragma once

#include <QWidget>
#include <QPointF>
#include <QSet>
#include <QTimer>
#include <QDateTime>
#include <QVector>
#include <QElapsedTimer>
#include <QHash>
#include "model/Note.h"

class ChartController;
class SelectionController;
class NoteRenderer;
class GridRenderer;
class HyperfruitDetector;
class BackgroundRenderer;
class Skin;
class PlaybackController;

class ChartCanvas : public QWidget
{
    Q_OBJECT
public:
    enum Mode
    {
        PlaceNote,
        PlaceRain,
        Delete,
        Select
    };
    explicit ChartCanvas(QWidget *parent = nullptr);
    ~ChartCanvas();

    void setChartController(ChartController *controller);
    void setSelectionController(SelectionController *controller);
    void setSkin(Skin *skin);
    void setPlaybackController(PlaybackController *controller);
    void setColorMode(bool enabled);
    void setHyperfruitEnabled(bool enabled);
    void setTimeDivision(int division);
    void setGridDivision(int division);
    void setGridSnap(bool snap);
    void setScrollPos(double timeMs);
    void setNoteSize(int size);
    void setMode(Mode mode);

    // 复制粘贴功能
    void handleCopy();                     // 处理复制（快捷键/菜单/按钮）
    void paste();                          // 普通粘贴（参考线）
    void pasteAtCursor(const QPoint &pos); // 右键粘贴（光标位置）
    void cancelOperation();                // 取消当前操作（区间选择/粘贴预览）
    void beginPastePreview(const QVector<Note> &notes, const QPoint &cursorPos = QPoint());

    bool isVerticalFlip() const;
    void setVerticalFlip(bool flip);
    double currentPlayTime() const;

    void setTimeScale(double scale);
    double timeScale() const { return m_timeScale; }

public slots:
    void showGridSettings();
    void playbackPositionChanged(double timeMs);
    void playFromReferenceLine();

signals:
    void verticalFlipChanged(bool flipped);
    void scrollPositionChanged(double beat);
    void timeScaleChanged(double scale);
    void statusMessage(const QString &msg); // 用于状态栏提示

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void timerEvent(QTimerEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void drawBackground(QPainter &painter);
    void drawGrid(QPainter &painter);
    QPointF noteToPos(const Note &note) const;
    Note posToNote(const QPointF &pos) const;
    double yPosFromTime(double timeMs) const;
    double beatToY(double beat) const;
    double yToBeat(double y) const;
    int hitTestNote(const QPointF &pos) const;
    QRectF getRainNoteRect(const Note &note) const;
    void invalidateCache();
    void updateNotePosCacheIfNeeded(); // 已废弃（保留空实现兼容）
    void updateBackgroundCache();

    void beginMoveSelection(const QPointF &startPos, int referenceIndex = -1);
    void updateMoveSelection(const QPointF &currentPos);
    void endMoveSelection();
    void prepareMoveChanges();

    void snapPlayheadToGrid();
    void startSnapTimer();
    void stopSnapTimer();

    double getNoteTimeMs(const Note &note) const;
    void confirmPaste();

    void rebuildNoteTimesCache();

    double effectiveVisibleBeatRange() const
    {
        return m_baseVisibleBeatRange / m_timeScale;
    }

    // 新增：粘贴预览相关辅助函数
    double calculatePasteReferenceTime() const; // 计算粘贴基准时间（参考线或光标位置）
    double yToTime(double y) const;             // Y坐标转时间（ms）

    // 新增：区间选择
    enum IntervalState
    {
        IntervalNone,
        IntervalWaitingEnd // 已记录起点，等待第二次复制
    };
    IntervalState m_intervalState;
    double m_intervalStartTime;
    QVector<Note> m_intervalNotes;

    void startIntervalSelection();    // 记录起点，进入等待状态
    void completeIntervalSelection(); // 记录终点，完成区间选择并进入预览
    void cancelIntervalSelection();

    // 新增：粘贴预览拖动相关
    bool m_isDraggingPaste;                              // 是否正在拖动粘贴预览
    QPointF m_pasteDragStartPos;                         // 拖动起始位置（屏幕坐标）
    double m_pasteTimeOffset;                            // 当前粘贴预览的时间偏移（拍数）
    double m_pasteXOffset;                               // 当前粘贴预览的 X 偏移（像素）
    double m_pasteRefBeat;                               // 粘贴预览最早音符的原始拍数（用于吸附计算）
    int m_pasteDragReferenceIndex;                       // 拖动时用于吸附的参考音符索引
    void cancelPaste();                                  // 取消粘贴预览
    void beginDragPaste(const QPointF &startPos);        // 开始拖动预览
    void updateDragPaste(const QPointF &currentPos);     // 更新拖动偏移
    void endDragPaste();                                 // 结束拖动
    void updatePastePreviewPosition();                   // 根据当前偏移更新预览位置（重绘）
    double snapPasteTimeOffset(double offsetBeat) const; // 对时间偏移进行吸附

    // 预计算缓存数据
    QVector<double> m_noteBeatPositions;
    QVector<double> m_noteEndBeatPositions;
    QVector<double> m_noteXPositions;
    QVector<double> m_noteTimesMs; // 优化2：预计算音符的毫秒时间（含 offset）
    QVector<NoteType> m_noteTypes;
    bool m_noteDataDirty;
    bool m_timesDirty;

    ChartController *m_chartController;
    SelectionController *m_selectionController;
    PlaybackController *m_playbackController;
    NoteRenderer *m_noteRenderer;
    GridRenderer *m_gridRenderer;
    HyperfruitDetector *m_hyperfruitDetector;
    BackgroundRenderer *m_backgroundRenderer;

    Mode m_currentMode;
    bool m_colorMode;
    bool m_hyperfruitEnabled;
    bool m_verticalFlip;
    int m_timeDivision;
    int m_gridDivision;
    bool m_gridSnap;
    double m_scrollBeat;
    double m_baseVisibleBeatRange;
    double m_timeScale;
    double m_currentPlayTime;
    bool m_autoScrollEnabled;

    bool m_isSelecting;
    QPointF m_selectionStart;
    QPointF m_selectionEnd;
    bool m_isDragging;
    QPointF m_dragStart;
    QSet<int> m_draggedNotes;

    bool m_isPasting;
    bool m_useCursorPaste;   // 是否使用光标位置粘贴（右键触发）
    QPoint m_pasteCursorPos; // 右键粘贴时的光标位置
    QVector<Note> m_pasteNotes;
    QPointF m_pasteOffset;

    bool m_isMovingSelection;
    QPointF m_moveStartPos;
    QHash<int, QPair<Note, Note>> m_moveChanges; // 存储原始音符快照，key=索引
    QSet<int> m_originalSelectedIndices;
    int m_dragReferenceIndex;

    bool m_gridSnapBackup;
    bool m_wasGridSnapEnabled;
    bool m_hyperfruitEnabledBackup;

    bool m_rainFirst;
    QPointF m_rainStartPos;

    bool m_snapToGrid;
    int m_snapTimerId;
    bool m_isScrolling;

    QTimer *m_repaintTimer;
    QTimer *m_playbackTimer; // 优化4：播放驱动定时器（16ms 间隔）
    bool m_repaintPending;
    bool m_forceRepaint;
    qint64 m_lastRepaintTime;

    QSet<int> m_cachedHyperSet;
    bool m_hyperCacheValid;

    QPixmap m_backgroundCache;
    bool m_backgroundCacheDirty;

    QElapsedTimer m_fpsTimer;
    int m_frameCount;
    double m_currentFps;

    bool m_isPlaying;

    int leftMargin() const;
    int rightMargin() const;

private slots:
    void onSelectionChanged();
    void performDelayedRepaint();
    void requestNextFrame();
};