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
#include "utils/MathUtils.h"

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

    // Copy/paste
    void handleCopy();
    void paste();
    void pasteAtCursor(const QPoint &pos);
    void cancelOperation();
    void beginPastePreview(const QVector<Note> &notes, const QPoint &cursorPos = QPoint());

    bool isVerticalFlip() const;
    void setVerticalFlip(bool flip);
    double currentPlayTime() const;
    int timeDivision() const { return m_timeDivision; }

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
    void statusMessage(const QString &msg); // 鐢ㄤ簬鐘舵€佹爮鎻愮ず

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
    void updateNotePosCacheIfNeeded(); // 宸插簾寮冿紙淇濈暀绌哄疄鐜板吋瀹癸級
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

    void rebuildBpmTimeCache();
    const QVector<MathUtils::BpmCacheEntry> &bpmTimeCache();

    void rebuildNoteTimesCache();

    double effectiveVisibleBeatRange() const
    {
        return m_baseVisibleBeatRange / m_timeScale;
    }

    // Paste preview helpers
    double calculatePasteReferenceTime() const;
    double yToTime(double y) const;
    // 鏂板锛氬尯闂撮€夋嫨
    enum IntervalState
    {
        IntervalNone,
        IntervalWaitingEnd // Start point recorded, waiting for second copy click.
    };
    IntervalState m_intervalState;
    double m_intervalStartTime;
    QVector<Note> m_intervalNotes;

    void startIntervalSelection();
    void completeIntervalSelection();
    void cancelIntervalSelection();

    // Paste preview drag state
    bool m_isDraggingPaste;
    QPointF m_pasteDragStartPos;
    double m_pasteTimeOffset;
    double m_pasteXOffset;
    double m_pasteTimeOffsetRaw;                         // 粘贴预览原始时间偏移（未吸附）
    double m_pasteXOffsetRaw;                            // 粘贴预览原始 X 偏移
    double m_pasteAnchorBeat;                            // 进入预览时锁定的参考拍
    double m_pasteRefBeat;
    int m_pasteDragReferenceIndex;
    void cancelPaste();
    void beginDragPaste(const QPointF &startPos);
    void updateDragPaste(const QPointF &currentPos);
    void endDragPaste();
    void updatePastePreviewPosition();
    double snapPasteTimeOffset(double offsetBeat) const;

    // Precomputed cache data
    QVector<double> m_noteBeatPositions;
    QVector<double> m_noteEndBeatPositions;
    QVector<double> m_noteXPositions;
    QVector<double> m_noteTimesMs;
    QVector<NoteType> m_noteTypes;
    bool m_noteDataDirty;
    bool m_timesDirty;
    QVector<MathUtils::BpmCacheEntry> m_bpmTimeCache;
    bool m_bpmCacheDirty;

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
    bool m_useCursorPaste;   // 鏄惁浣跨敤鍏夋爣浣嶇疆绮樿创锛堝彸閿Е鍙戯級
    QPoint m_pasteCursorPos; // 鍙抽敭绮樿创鏃剁殑鍏夋爣浣嶇疆
    QVector<Note> m_pasteNotes;
    QVector<double> m_pasteOriginalTimesMs;
    double m_pasteBaseOriginalTimeMs;
    QPointF m_pasteOffset;

    bool m_isMovingSelection;
    QPointF m_moveStartPos;
    QHash<int, QPair<Note, Note>> m_moveChanges; // 瀛樺偍鍘熷闊崇蹇収锛宬ey=绱㈠紩
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
    QTimer *m_playbackTimer; // Playback tick timer (~16ms).
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
    qint64 m_lastScrollSignalTimeMs;
    bool m_hasPlaybackAnchor;
    double m_playbackAnchorMs;
    qint64 m_playbackAnchorWallMs;

    int leftMargin() const;
    int rightMargin() const;

private slots:
    void onSelectionChanged();
    void performDelayedRepaint();
    void requestNextFrame();
};
