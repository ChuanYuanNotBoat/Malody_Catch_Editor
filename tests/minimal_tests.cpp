#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtGlobal>
#include <algorithm>
#include <cstdio>

#include "file/ProjectIO.h"
#include "file/ChartIO.h"
#include "controller/ChartController.h"
#include "model/Chart.h"
#include "utils/MathUtils.h"

namespace
{
bool nearlyEqual(double a, double b, double eps = 1e-6)
{
    return qAbs(a - b) <= eps;
}

bool testMathUtilsRoundTrip()
{
    const QVector<BpmEntry> bpmList = {BpmEntry(0, 0, 1, 120.0)};

    const double ms = MathUtils::beatToMs(1, 0, 1, bpmList, 0);
    if (!nearlyEqual(ms, 500.0))
        return false;

    int beatNum = 0;
    int num = 0;
    int den = 1;
    MathUtils::msToBeat(750.0, bpmList, 0, beatNum, num, den);

    const double beat = MathUtils::beatToFloat(beatNum, num, den);
    if (!nearlyEqual(beat, 1.5))
        return false;

    MathUtils::floatToBeat(3.125, beatNum, num, den, 1024);
    return beatNum == 3 && num == 1 && den == 8;
}

bool testMathUtilsCacheConsistency()
{
    const QVector<BpmEntry> bpmList = {
        BpmEntry(0, 0, 1, 120.0),
        BpmEntry(4, 0, 1, 180.0),
        BpmEntry(8, 0, 1, 90.0)};

    const auto cache = MathUtils::buildBpmTimeCache(bpmList, 120);
    if (cache.size() != 3)
        return false;

    struct Probe
    {
        int beatNum;
        int num;
        int den;
    };

    const Probe probes[] = {
        {0, 0, 1},
        {3, 1, 2},
        {5, 0, 1},
        {9, 0, 1}};

    for (const Probe &p : probes)
    {
        const double byList = MathUtils::beatToMs(p.beatNum, p.num, p.den, bpmList, 120);
        const double byCache = MathUtils::beatToMs(p.beatNum, p.num, p.den, cache);
        if (!nearlyEqual(byList, byCache))
            return false;
    }
    return true;
}

bool testMathUtilsEmptyBpmBoundary()
{
    const QVector<BpmEntry> bpmList;
    if (!nearlyEqual(MathUtils::beatToMs(2, 0, 1, bpmList, 1234), -1234.0))
        return false;

    int beatNum = -1;
    int num = -1;
    int den = -1;
    MathUtils::msToBeat(500.0, bpmList, 0, beatNum, num, den);
    return beatNum == 0 && num == 1 && den == 1;
}

bool testMathUtilsZeroBpmBoundary()
{
    const QVector<BpmEntry> bpmList = {BpmEntry(0, 0, 1, 0.0)};
    if (!nearlyEqual(MathUtils::beatToMs(8, 0, 1, bpmList, 250), -250.0))
        return false;

    int beatNum = -1;
    int num = -1;
    int den = -1;
    MathUtils::msToBeat(2000.0, bpmList, 0, beatNum, num, den);
    return beatNum == 0 && num == 0 && den == 1;
}

bool testMathUtilsExtremeOffsetBoundary()
{
    const QVector<BpmEntry> bpmList = {BpmEntry(0, 0, 1, 120.0)};
    const int positiveOffset = 1000000;
    const int negativeOffset = -1000000;

    if (!nearlyEqual(MathUtils::beatToMs(0, 0, 1, bpmList, positiveOffset), -1000000.0))
        return false;
    if (!nearlyEqual(MathUtils::beatToMs(0, 0, 1, bpmList, negativeOffset), 1000000.0))
        return false;

    int beatNum = -1;
    int num = -1;
    int den = -1;
    MathUtils::msToBeat(-1000001.0, bpmList, positiveOffset, beatNum, num, den);
    return beatNum == 0 && num == 0 && den == 1;
}

bool testMathUtilsCrossSegmentRoundTripBoundary()
{
    const QVector<BpmEntry> bpmList = {
        BpmEntry(0, 0, 1, 120.0),
        BpmEntry(4, 0, 1, 240.0)};

    const struct BeatProbe
    {
        int beatNum;
        int num;
        int den;
    } probes[] = {
        {3, 999, 1000},
        {4, 1, 1000},
        {7, 1, 2},
    };

    for (const BeatProbe &probe : probes)
    {
        const double beatIn = MathUtils::beatToFloat(probe.beatNum, probe.num, probe.den);
        const double ms = MathUtils::beatToMs(probe.beatNum, probe.num, probe.den, bpmList, 0);

        int outBeatNum = 0;
        int outNum = 0;
        int outDen = 1;
        MathUtils::msToBeat(ms, bpmList, 0, outBeatNum, outNum, outDen);
        const double beatOut = MathUtils::beatToFloat(outBeatNum, outNum, outDen);
        if (!nearlyEqual(beatIn, beatOut, 1e-4))
            return false;
    }
    return true;
}

bool testChartRemoveById()
{
    Chart chart;
    chart.clearNotes();

    Note first(1, 0, 1, 128);
    first.id = "n1";
    Note second(1, 0, 1, 128);
    second.id = "n2";

    chart.addNote(first);
    chart.addNote(second);

    Note removeTarget = second;
    chart.removeNote(removeTarget);
    if (chart.notes().size() != 1)
        return false;
    return chart.notes().first().id == "n1";
}

bool testChartBpmSort()
{
    Chart chart;
    chart.bpmList().clear();
    chart.addBpm(BpmEntry(8, 0, 1, 180.0));
    chart.addBpm(BpmEntry(0, 0, 1, 120.0));
    chart.addBpm(BpmEntry(4, 1, 2, 150.0));

    if (chart.bpmList().size() != 3)
        return false;
    if (chart.bpmList()[0].beatNum != 0)
        return false;
    if (chart.bpmList()[1].beatNum != 4 || chart.bpmList()[1].numerator != 1)
        return false;
    return chart.bpmList()[2].beatNum == 8;
}

bool writeTextFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(content) == content.size();
}

bool testProjectIoReadDifficultyAndScan()
{
    QTemporaryDir tempDir;
    if (!tempDir.isValid())
        return false;

    const QString root = tempDir.path();
    const QString nestedDir = root + "/nested";
    if (!QDir().mkpath(nestedDir))
        return false;

    const QString hardMc = root + "/hard.mc";
    const QByteArray hardJson = R"({"meta":{"version":"Hard"}})";
    if (!writeTextFile(hardMc, hardJson))
        return false;

    const QString noVersionMc = nestedDir + "/fallback.mc";
    const QByteArray fallbackJson = R"({"meta":{}})";
    if (!writeTextFile(noVersionMc, fallbackJson))
        return false;

    if (ProjectIO::getDifficultyFromMc(hardMc) != "Hard")
        return false;

    const auto charts = ProjectIO::findChartsInDirectory(root);
    bool foundHard = false;
    bool foundFallback = false;
    for (const auto &entry : charts)
    {
        if (entry.first == hardMc && entry.second == "Hard")
            foundHard = true;
        if (entry.first == noVersionMc && entry.second == "fallback")
            foundFallback = true;
    }
    return foundHard && foundFallback;
}

Note makeNormalNote(int beatNum, int num, int den, int x, const QString &id = QString())
{
    Note note(beatNum, num, den, x);
    if (!id.isEmpty())
        note.id = id;
    return note;
}

bool testChartControllerApplyBatchEditAcceptsValidPayload()
{
    ChartController controller;
    const Note base = makeNormalNote(1, 0, 1, 64, "seed-a");
    controller.addNote(base);

    const Note moved = makeNormalNote(2, 0, 1, 128, "seed-a");
    const Note added = makeNormalNote(3, 0, 1, 256, "seed-b");

    const bool ok = controller.applyBatchEdit(
        "batch edit valid",
        QVector<Note>{added},
        QVector<Note>{},
        QList<QPair<Note, Note>>{qMakePair(base, moved)});
    if (!ok)
        return false;

    const QVector<Note> &notes = controller.chart()->notes();
    if (notes.size() != 2)
        return false;

    bool foundMoved = false;
    bool foundAdded = false;
    for (const Note &n : notes)
    {
        if (n.id == "seed-a" && n.beatNum == 2 && n.x == 128)
            foundMoved = true;
        if (n.id == "seed-b" && n.beatNum == 3 && n.x == 256)
            foundAdded = true;
    }
    return foundMoved && foundAdded;
}

bool testChartControllerApplyBatchEditRejectsInvalidAddNote()
{
    ChartController controller;
    const Note base = makeNormalNote(1, 0, 1, 64, "seed-a");
    controller.addNote(base);

    Note invalidAdd = makeNormalNote(2, 0, 1, 900, "bad-add");
    invalidAdd.type = NoteType::NORMAL;

    const bool ok = controller.applyBatchEdit(
        "batch edit invalid add",
        QVector<Note>{invalidAdd},
        QVector<Note>{},
        QList<QPair<Note, Note>>{});
    if (ok)
        return false;

    const QVector<Note> &notes = controller.chart()->notes();
    return notes.size() == 1 && notes.first().id == "seed-a";
}

bool testChartControllerApplyBatchEditRejectsConflictingMoveAndRemove()
{
    ChartController controller;
    const Note base = makeNormalNote(1, 0, 1, 64, "seed-a");
    controller.addNote(base);

    const Note moved = makeNormalNote(2, 0, 1, 128, "seed-a");
    const bool ok = controller.applyBatchEdit(
        "batch edit conflict",
        QVector<Note>{},
        QVector<Note>{base},
        QList<QPair<Note, Note>>{qMakePair(base, moved)});
    if (ok)
        return false;

    const QVector<Note> &notes = controller.chart()->notes();
    return notes.size() == 1 && notes.first().id == "seed-a";
}

bool testChartControllerApplyBatchEditRejectsMissingMoveSource()
{
    ChartController controller;
    const Note existing = makeNormalNote(1, 0, 1, 64, "seed-a");
    controller.addNote(existing);

    const Note missingSource = makeNormalNote(5, 0, 1, 300, "not-in-chart");
    const Note movedTarget = makeNormalNote(6, 0, 1, 200, "not-in-chart");
    const bool ok = controller.applyBatchEdit(
        "batch edit missing source",
        QVector<Note>{},
        QVector<Note>{},
        QList<QPair<Note, Note>>{qMakePair(missingSource, movedTarget)});
    if (ok)
        return false;

    const QVector<Note> &notes = controller.chart()->notes();
    return notes.size() == 1 && notes.first().id == "seed-a";
}

struct RenderBenchmarkResult
{
    int notesTotal = 0;
    int sampleCount = 0;
    qint64 elapsedNs = 0;
    double avgVisibleNotes = 0.0;
    int maxVisibleNotes = 0;
};

RenderBenchmarkResult runRenderVisibilityBenchmark(const QVector<Note> &inputNotes, int sampleCount)
{
    RenderBenchmarkResult result;
    result.notesTotal = inputNotes.size();
    result.sampleCount = qMax(1, sampleCount);
    if (inputNotes.isEmpty())
        return result;

    struct NoteCache
    {
        NoteType type = NoteType::NORMAL;
        double beat = 0.0;
        double endBeat = 0.0;
    };

    QVector<NoteCache> cache(inputNotes.size());
    QVector<int> normalIndices;
    QVector<int> rainIndices;
    normalIndices.reserve(inputNotes.size());
    rainIndices.reserve(inputNotes.size());

    double maxBeat = 0.0;
    for (int i = 0; i < inputNotes.size(); ++i)
    {
        const Note &note = inputNotes[i];
        const double beat = MathUtils::beatToFloat(note.beatNum, note.numerator, note.denominator);
        double endBeat = beat;
        if (note.type == NoteType::RAIN)
            endBeat = MathUtils::beatToFloat(note.endBeatNum, note.endNumerator, note.endDenominator);

        cache[i].type = note.type;
        cache[i].beat = beat;
        cache[i].endBeat = endBeat;
        maxBeat = qMax(maxBeat, qMax(beat, endBeat));

        if (note.type == NoteType::RAIN)
            rainIndices.append(i);
        else if (note.type == NoteType::NORMAL)
            normalIndices.append(i);
    }

    auto byBeat = [&cache](int lhs, int rhs) {
        if (cache[lhs].beat == cache[rhs].beat)
            return lhs < rhs;
        return cache[lhs].beat < cache[rhs].beat;
    };
    std::sort(normalIndices.begin(), normalIndices.end(), byBeat);
    std::sort(rainIndices.begin(), rainIndices.end(), byBeat);

    constexpr double visibleRange = 8.0;
    QElapsedTimer timer;
    timer.start();
    qint64 totalVisible = 0;

    for (int s = 0; s < result.sampleCount; ++s)
    {
        const double t = (result.sampleCount == 1) ? 0.0 : static_cast<double>(s) / (result.sampleCount - 1);
        const double scrollBeat = qMax(0.0, (maxBeat + 1.0) * t - (visibleRange * 0.5));
        const double startBeat = scrollBeat;
        const double endBeat = scrollBeat + visibleRange;

        int visibleCount = 0;

        auto rainBegin = std::lower_bound(
            rainIndices.begin(),
            rainIndices.end(),
            startBeat,
            [&cache](int idx, double beatValue) {
                return cache[idx].beat < beatValue;
            });
        auto rainStartIt = rainBegin;
        while (rainStartIt != rainIndices.begin())
        {
            auto prev = rainStartIt - 1;
            const int idx = *prev;
            if (cache[idx].endBeat <= startBeat)
                break;
            rainStartIt = prev;
        }
        for (auto it = rainStartIt; it != rainIndices.end(); ++it)
        {
            const int idx = *it;
            if (cache[idx].beat >= endBeat)
                break;
            if (cache[idx].endBeat > startBeat)
                ++visibleCount;
        }

        auto normalStart = std::lower_bound(
            normalIndices.begin(),
            normalIndices.end(),
            startBeat - 0.5,
            [&cache](int idx, double beatValue) {
                return cache[idx].beat < beatValue;
            });
        for (auto it = normalStart; it != normalIndices.end(); ++it)
        {
            const int idx = *it;
            if (cache[idx].beat > endBeat + 0.5)
                break;
            ++visibleCount;
        }

        totalVisible += visibleCount;
        if (visibleCount > result.maxVisibleNotes)
            result.maxVisibleNotes = visibleCount;
    }

    result.elapsedNs = timer.nsecsElapsed();
    result.avgVisibleNotes = static_cast<double>(totalVisible) / result.sampleCount;
    return result;
}

bool testKedamonoRenderBaseline()
{
    const QString chartPath =
        QStringLiteral("C:/Users/boatnotcy/AppData/Local/CatchEditor/Malody Catch Chart Editor/beatmap/KEDAMONO Drop-out/0/1737904376.mc");
    if (!QFileInfo::exists(chartPath))
    {
        std::fprintf(stdout, "SKIPPED: KEDAMONO baseline (chart not found at %s)\n", chartPath.toUtf8().constData());
        return true;
    }

    Chart chart;
    if (!ChartIO::load(chartPath, chart, false))
    {
        std::fprintf(stderr, "FAILED: KEDAMONO baseline (load failed)\n");
        return false;
    }

    QVector<Note> notes = chart.notes();
    if (notes.isEmpty())
    {
        std::fprintf(stderr, "FAILED: KEDAMONO baseline (no notes)\n");
        return false;
    }

    std::sort(notes.begin(), notes.end(), [](const Note &a, const Note &b) {
        const double beatA = MathUtils::beatToFloat(a.beatNum, a.numerator, a.denominator);
        const double beatB = MathUtils::beatToFloat(b.beatNum, b.numerator, b.denominator);
        if (beatA == beatB)
            return a.x < b.x;
        return beatA < beatB;
    });

    const int total = notes.size();
    const int sample5k = qMin(5000, total);
    const int sample10k = qMin(10000, total);

    const RenderBenchmarkResult baseline5k = runRenderVisibilityBenchmark(notes.mid(0, sample5k), 200000);
    const RenderBenchmarkResult baseline10k = runRenderVisibilityBenchmark(notes.mid(0, sample10k), 200000);

    std::fprintf(stdout,
                 "KEDAMONO_BASELINE total=%d notes, sample5k=%d, sample10k=%d\n",
                 total,
                 sample5k,
                 sample10k);
    std::fprintf(stdout,
                 "KEDAMONO_BASELINE 5k elapsed_ms=%.3f avg_visible=%.2f max_visible=%d\n",
                 baseline5k.elapsedNs / 1000000.0,
                 baseline5k.avgVisibleNotes,
                 baseline5k.maxVisibleNotes);
    std::fprintf(stdout,
                 "KEDAMONO_BASELINE 10k elapsed_ms=%.3f avg_visible=%.2f max_visible=%d\n",
                 baseline10k.elapsedNs / 1000000.0,
                 baseline10k.avgVisibleNotes,
                 baseline10k.maxVisibleNotes);

    return true;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    struct Case
    {
        const char *name;
        bool (*fn)();
    };

    const Case cases[] = {
        {"MathUtils round-trip", &testMathUtilsRoundTrip},
        {"MathUtils cache consistency", &testMathUtilsCacheConsistency},
        {"MathUtils empty BPM boundary", &testMathUtilsEmptyBpmBoundary},
        {"MathUtils zero BPM boundary", &testMathUtilsZeroBpmBoundary},
        {"MathUtils extreme offset boundary", &testMathUtilsExtremeOffsetBoundary},
        {"MathUtils cross-segment round-trip boundary", &testMathUtilsCrossSegmentRoundTripBoundary},
        {"Chart removeNote by id", &testChartRemoveById},
        {"Chart BPM sorting", &testChartBpmSort},
        {"ProjectIO scan + difficulty", &testProjectIoReadDifficultyAndScan},
        {"ChartController applyBatchEdit valid payload", &testChartControllerApplyBatchEditAcceptsValidPayload},
        {"ChartController applyBatchEdit invalid add", &testChartControllerApplyBatchEditRejectsInvalidAddNote},
        {"ChartController applyBatchEdit conflict remove+move", &testChartControllerApplyBatchEditRejectsConflictingMoveAndRemove},
        {"ChartController applyBatchEdit missing move source", &testChartControllerApplyBatchEditRejectsMissingMoveSource},
        {"KEDAMONO render baseline", &testKedamonoRenderBaseline},
    };

    int failed = 0;
    for (const Case &c : cases)
    {
        const bool ok = c.fn();
        if (!ok)
        {
            std::fprintf(stderr, "FAILED: %s\n", c.name);
            ++failed;
        }
        else
        {
            std::fprintf(stdout, "PASSED: %s\n", c.name);
        }
    }

    return failed == 0 ? 0 : 1;
}
