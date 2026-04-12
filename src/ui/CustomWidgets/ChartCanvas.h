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
    void paste();

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
    void updateNotePosCacheIfNeeded(); // 已废弃
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

    // 预计算缓存数据
    QVector<double> m_noteBeatPositions;
    QVector<double> m_noteEndBeatPositions;
    QVector<double> m_noteXPositions;
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