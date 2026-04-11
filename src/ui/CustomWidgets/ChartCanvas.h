#pragma once

#include <QWidget>
#include <QPointF>
#include <QSet>
#include <QTimer>
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
    double currentPlayTime() const;

    // 时间轴缩放
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

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void drawGrid(QPainter& painter);
    QPointF noteToPos(const Note& note) const;
    Note posToNote(const QPointF& pos) const;
    double yPosFromTime(double timeMs) const;
    double beatToY(double beat) const;
    double yToBeat(double y) const;
    int hitTestNote(const QPointF& pos) const;
    QRectF getRainNoteRect(const Note& note) const;
    void updateNotePosCacheIfNeeded();
    void invalidateCache();

    void beginMoveSelection(const QPointF& startPos, int referenceIndex = -1);
    void updateMoveSelection(const QPointF& currentPos);
    void endMoveSelection();
    void prepareMoveChanges();

    void snapPlayheadToGrid();
    void startSnapTimer();
    void stopSnapTimer();

    // 计算实际可见拍数（应用缩放因子）
    double effectiveVisibleBeatRange() const {
        return m_baseVisibleBeatRange / m_timeScale;
    }

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
    double m_scrollBeat;               // 滚动位置（拍）
    double m_baseVisibleBeatRange;     // 基准可见拍数（缩放因子=1.0时的值）
    double m_timeScale;                // 时间轴缩放因子
    double m_currentPlayTime;          // 当前播放时间（毫秒）
    bool m_autoScrollEnabled;

    bool m_isSelecting;
    QPointF m_selectionStart;
    QPointF m_selectionEnd;
    bool m_isDragging;
    QPointF m_dragStart;
    QSet<int> m_draggedNotes;

    bool m_isPasting;
    QVector<Note> m_pasteNotes;
    QPointF m_pasteOffset;

    bool m_isMovingSelection = false;
    QPointF m_moveStartPos;
    QList<QPair<Note, Note>> m_moveChanges;
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

    QTimer* m_repaintTimer;
    bool m_repaintPending;
    bool m_forceRepaint;
    qint64 m_lastRepaintTime;

    QVector<QPointF> m_notePosCache;
    bool m_cacheValid;
    double m_cachedScrollBeat;
    double m_cachedVisibleBeatRange;
    int m_cachedWidth;
    int m_cachedHeight;
    bool m_cachedVerticalFlip;

    int leftMargin() const;
    int rightMargin() const;

private slots:
    void onSelectionChanged();
    void performDelayedRepaint();
};