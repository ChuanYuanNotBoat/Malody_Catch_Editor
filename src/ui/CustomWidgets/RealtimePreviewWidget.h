#pragma once

#include <QSet>
#include <QVector>
#include <QWidget>

class ChartController;
class PlaybackController;
class Skin;
class NoteRenderer;
class HyperfruitDetector;

class RealtimePreviewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RealtimePreviewWidget(QWidget *parent = nullptr);
    ~RealtimePreviewWidget() override;

    void setChartController(ChartController *controller);
    void setPlaybackController(PlaybackController *controller);
    void setSkin(Skin *skin);
    void setColorMode(bool enabled);
    void setHyperfruitEnabled(bool enabled);
    void setNoteSize(int size);

public slots:
    void setCurrentBeat(double beat);
    void setCurrentTimeMs(double timeMs);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    static constexpr int kLaneWidth = 512;

    struct BpmSegment
    {
        double beatPos = 0.0;
        double accumulatedMs = 0.0;
        double bpm = 0.0;
    };

    double beatToTimeMs(double beat);
    double timeToY(double timeMs,
                   const QRectF &laneRect,
                   double referenceY,
                   double upperSpanMs,
                   double lowerSpanMs) const;
    void invalidateNoteCache();
    void ensureNoteCache();
    void invalidateHyperCache();
    void ensureHyperCache();

    ChartController *m_chartController = nullptr;
    PlaybackController *m_playbackController = nullptr;
    NoteRenderer *m_noteRenderer = nullptr;
    HyperfruitDetector *m_hyperfruitDetector = nullptr;
    bool m_colorMode = true;
    bool m_hyperfruitEnabled = true;
    double m_currentTimeMs = 0.0;
    QVector<BpmSegment> m_bpmSegments;
    QVector<double> m_noteStartTimesMs;
    QVector<double> m_noteEndTimesMs;
    QVector<int> m_normalIndices;
    QVector<int> m_rainIndices;
    bool m_noteCacheValid = false;
    QSet<int> m_hyperIndices;
    bool m_hyperCacheValid = false;
};
