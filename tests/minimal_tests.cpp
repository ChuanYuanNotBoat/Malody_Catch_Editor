#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtGlobal>
#include <cstdio>

#include "file/ProjectIO.h"
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
        {"Chart removeNote by id", &testChartRemoveById},
        {"Chart BPM sorting", &testChartBpmSort},
        {"ProjectIO scan + difficulty", &testProjectIoReadDifficultyAndScan},
        {"ChartController applyBatchEdit valid payload", &testChartControllerApplyBatchEditAcceptsValidPayload},
        {"ChartController applyBatchEdit invalid add", &testChartControllerApplyBatchEditRejectsInvalidAddNote},
        {"ChartController applyBatchEdit conflict remove+move", &testChartControllerApplyBatchEditRejectsConflictingMoveAndRemove},
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
