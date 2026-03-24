#include "ChartController.h"
#include "file/ChartIO.h"
#include <QUndoCommand>
#include <QDebug>

// 撤销命令基类
class ChartController::ChartCommand : public QUndoCommand {
public:
    ChartCommand(ChartController* controller, const QString& text) : QUndoCommand(text), m_controller(controller) {}
protected:
    ChartController* m_controller;
};

class ChartController::AddNoteCommand : public ChartController::ChartCommand {
public:
    AddNoteCommand(ChartController* controller, const Note& note) : ChartCommand(controller, "Add Note"), m_note(note) {}
    void undo() override { m_controller->m_chart.removeNote(m_note); m_controller->chartChanged(); }
    void redo() override { m_controller->m_chart.addNote(m_note); m_controller->chartChanged(); }
private:
    Note m_note;
};

class ChartController::RemoveNoteCommand : public ChartController::ChartCommand {
public:
    RemoveNoteCommand(ChartController* controller, const Note& note) : ChartCommand(controller, "Remove Note"), m_note(note) {}
    void undo() override { m_controller->m_chart.addNote(m_note); m_controller->chartChanged(); }
    void redo() override { m_controller->m_chart.removeNote(m_note); m_controller->chartChanged(); }
private:
    Note m_note;
};

class ChartController::MoveNoteCommand : public ChartController::ChartCommand {
public:
    MoveNoteCommand(ChartController* controller, const Note& original, const Note& newNote)
        : ChartCommand(controller, "Move Note"), m_original(original), m_new(newNote) {}
    void undo() override { m_controller->m_chart.removeNote(m_new); m_controller->m_chart.addNote(m_original); m_controller->chartChanged(); }
    void redo() override { m_controller->m_chart.removeNote(m_original); m_controller->m_chart.addNote(m_new); m_controller->chartChanged(); }
private:
    Note m_original, m_new;
};

class ChartController::AddBpmCommand : public ChartController::ChartCommand {
public:
    AddBpmCommand(ChartController* controller, const BpmEntry& bpm) : ChartCommand(controller, "Add BPM"), m_bpm(bpm) {}
    void undo() override { m_controller->m_chart.removeBpm(m_controller->m_chart.bpmList().size() - 1); m_controller->chartChanged(); }
    void redo() override { m_controller->m_chart.addBpm(m_bpm); m_controller->chartChanged(); }
private:
    BpmEntry m_bpm;
};

class ChartController::RemoveBpmCommand : public ChartController::ChartCommand {
public:
    RemoveBpmCommand(ChartController* controller, int index, const BpmEntry& bpm)
        : ChartCommand(controller, "Remove BPM"), m_index(index), m_bpm(bpm) {}
    void undo() override { m_controller->m_chart.addBpm(m_bpm); m_controller->chartChanged(); }
    void redo() override { m_controller->m_chart.removeBpm(m_index); m_controller->chartChanged(); }
private:
    int m_index;
    BpmEntry m_bpm;
};

class ChartController::UpdateBpmCommand : public ChartController::ChartCommand {
public:
    UpdateBpmCommand(ChartController* controller, int index, const BpmEntry& oldBpm, const BpmEntry& newBpm)
        : ChartCommand(controller, "Update BPM"), m_index(index), m_old(oldBpm), m_new(newBpm) {}
    void undo() override { m_controller->m_chart.updateBpm(m_index, m_old); m_controller->chartChanged(); }
    void redo() override { m_controller->m_chart.updateBpm(m_index, m_new); m_controller->chartChanged(); }
private:
    int m_index;
    BpmEntry m_old, m_new;
};

class ChartController::SetMetaCommand : public ChartController::ChartCommand {
public:
    SetMetaCommand(ChartController* controller, const MetaData& oldMeta, const MetaData& newMeta)
        : ChartCommand(controller, "Edit Meta"), m_old(oldMeta), m_new(newMeta) {}
    void undo() override { m_controller->m_chart.meta() = m_old; m_controller->chartChanged(); }
    void redo() override { m_controller->m_chart.meta() = m_new; m_controller->chartChanged(); }
private:
    MetaData m_old, m_new;
};

ChartController::ChartController(QObject* parent) : QObject(parent)
{
    m_undoStack = new QUndoStack(this);
}

ChartController::~ChartController()
{
}

void ChartController::addNote(const Note& note)
{
    m_undoStack->push(new AddNoteCommand(this, note));
}

void ChartController::removeNote(const Note& note)
{
    // 查找 note 是否存在
    int idx = m_chart.notes().indexOf(note);
    if (idx != -1)
        m_undoStack->push(new RemoveNoteCommand(this, note));
}

void ChartController::moveNote(const Note& original, const Note& newNote)
{
    if (original == newNote) return;
    m_undoStack->push(new MoveNoteCommand(this, original, newNote));
}

void ChartController::addBpm(const BpmEntry& bpm)
{
    m_undoStack->push(new AddBpmCommand(this, bpm));
}

void ChartController::removeBpm(int index)
{
    if (index >= 0 && index < m_chart.bpmList().size()) {
        m_undoStack->push(new RemoveBpmCommand(this, index, m_chart.bpmList()[index]));
    }
}

void ChartController::updateBpm(int index, const BpmEntry& bpm)
{
    if (index >= 0 && index < m_chart.bpmList().size()) {
        m_undoStack->push(new UpdateBpmCommand(this, index, m_chart.bpmList()[index], bpm));
    }
}

void ChartController::setMetaData(const MetaData& meta)
{
    m_undoStack->push(new SetMetaCommand(this, m_chart.meta(), meta));
}

void ChartController::undo()
{
    qDebug() << "ChartController::undo called";
    try {
        m_undoStack->undo();
        qDebug() << "ChartController::undo completed";
    } catch (const std::exception& e) {
        qCritical() << "ChartController::undo exception:" << e.what();
        throw;
    } catch (...) {
        qCritical() << "ChartController::undo unknown exception";
        throw;
    }
}

void ChartController::redo()
{
    qDebug() << "ChartController::redo called";
    try {
        m_undoStack->redo();
        qDebug() << "ChartController::redo completed";
    } catch (const std::exception& e) {
        qCritical() << "ChartController::redo exception:" << e.what();
        throw;
    } catch (...) {
        qCritical() << "ChartController::redo unknown exception";
        throw;
    }
}

bool ChartController::canUndo() const
{
    return m_undoStack->canUndo();
}

bool ChartController::canRedo() const
{
    return m_undoStack->canRedo();
}

bool ChartController::loadChart(const QString& path)
{
    qDebug() << "ChartController::loadChart:" << path;
    try {
        Chart newChart;
        if (ChartIO::load(path, newChart)) {
            qDebug() << "ChartController::loadChart: Chart loaded from file";
            m_chart = newChart;
            m_undoStack->clear();
            qDebug() << "ChartController::loadChart: Undo stack cleared";
            emit chartChanged();
            emit chartLoaded();
            qDebug() << "ChartController::loadChart: Signals emitted";
            return true;
        }
        qWarning() << "ChartController::loadChart: ChartIO::load failed";
        emit errorOccurred("Failed to load chart: " + path);
        return false;
    } catch (const std::exception& e) {
        qCritical() << "ChartController::loadChart exception:" << e.what();
        emit errorOccurred("Exception loading chart: " + QString::fromStdString(std::string(e.what())));
        return false;
    } catch (...) {
        qCritical() << "ChartController::loadChart unknown exception";
        emit errorOccurred("Unknown exception loading chart");
        return false;
    }
}

bool ChartController::saveChart(const QString& path)
{
    qDebug() << "ChartController::saveChart:" << path;
    try {
        if (ChartIO::save(path, m_chart)) {
            qDebug() << "ChartController::saveChart: Chart saved successfully";
            return true;
        }
        qWarning() << "ChartController::saveChart: ChartIO::save failed";
        emit errorOccurred("Failed to save chart: " + path);
        return false;
    } catch (const std::exception& e) {
        qCritical() << "ChartController::saveChart exception:" << e.what();
        emit errorOccurred("Exception saving chart: " + QString::fromStdString(std::string(e.what())));
        return false;
    } catch (...) {
        qCritical() << "ChartController::saveChart unknown exception";
        emit errorOccurred("Unknown exception saving chart");
        return false;
    }
}