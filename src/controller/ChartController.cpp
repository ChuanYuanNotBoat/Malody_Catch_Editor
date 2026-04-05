#include "ChartController.h"
#include "file/ChartIO.h"
#include "utils/Logger.h"
#include <QUndoCommand>
#include <QDebug>
#include <QList>
#include <QPair>

// 撤销命令基类
class ChartController::ChartCommand : public QUndoCommand {
public:
    ChartCommand(ChartController* controller, const QString& text) : QUndoCommand(text), m_controller(controller) {}
protected:
    ChartController* m_controller;
};

// 添加音符命令
class ChartController::AddNoteCommand : public ChartController::ChartCommand {
public:
    AddNoteCommand(ChartController* controller, const Note& note) : ChartCommand(controller, "Add Note"), m_note(note) {}
    void undo() override { m_controller->m_chart.removeNote(m_note); m_controller->chartChanged(); }
    void redo() override { m_controller->m_chart.addNote(m_note); m_controller->chartChanged(); }
private:
    Note m_note;
};

// 删除音符命令
class ChartController::RemoveNoteCommand : public ChartController::ChartCommand {
public:
    RemoveNoteCommand(ChartController* controller, const Note& note) : ChartCommand(controller, "Remove Note"), m_note(note) {}
    void undo() override { m_controller->m_chart.addNote(m_note); m_controller->chartChanged(); }
    void redo() override { m_controller->m_chart.removeNote(m_note); m_controller->chartChanged(); }
private:
    Note m_note;
};

// 移动单个音符命令
class ChartController::MoveNoteCommand : public ChartController::ChartCommand {
public:
    MoveNoteCommand(ChartController* controller, const Note& original, const Note& newNote)
        : ChartCommand(controller, "Move Note"), m_original(original), m_new(newNote) {}
    void undo() override { m_controller->m_chart.removeNote(m_new); m_controller->m_chart.addNote(m_original); m_controller->chartChanged(); }
    void redo() override { m_controller->m_chart.removeNote(m_original); m_controller->m_chart.addNote(m_new); m_controller->chartChanged(); }
private:
    Note m_original, m_new;
};

// 复合移动多个音符命令
class ChartController::MoveNotesCommand : public ChartController::ChartCommand {
public:
    MoveNotesCommand(ChartController* controller, const QList<QPair<Note, Note>>& changes)
        : ChartCommand(controller, "Move Notes"), m_changes(changes) {}
    void undo() override {
        for (const auto& change : m_changes) {
            m_controller->m_chart.removeNote(change.second);
        }
        for (const auto& change : m_changes) {
            m_controller->m_chart.addNote(change.first);
        }
        m_controller->chartChanged();
    }
    void redo() override {
        for (const auto& change : m_changes) {
            m_controller->m_chart.removeNote(change.first);
        }
        for (const auto& change : m_changes) {
            m_controller->m_chart.addNote(change.second);
        }
        m_controller->chartChanged();
    }
private:
    QList<QPair<Note, Note>> m_changes;
};

// 添加 BPM 命令
class ChartController::AddBpmCommand : public ChartController::ChartCommand {
public:
    AddBpmCommand(ChartController* controller, const BpmEntry& bpm) : ChartCommand(controller, "Add BPM"), m_bpm(bpm) {}
    void undo() override { m_controller->m_chart.removeBpm(m_controller->m_chart.bpmList().size() - 1); m_controller->chartChanged(); }
    void redo() override { m_controller->m_chart.addBpm(m_bpm); m_controller->chartChanged(); }
private:
    BpmEntry m_bpm;
};

// 删除 BPM 命令
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

// 更新 BPM 命令
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

// 设置元数据命令
class ChartController::SetMetaCommand : public ChartController::ChartCommand {
public:
    SetMetaCommand(ChartController* controller, const MetaData& oldMeta, const MetaData& newMeta)
        : ChartCommand(controller, "Edit Meta"), m_old(oldMeta), m_new(newMeta) {}
    void undo() override { m_controller->m_chart.meta() = m_old; m_controller->chartChanged(); }
    void redo() override { m_controller->m_chart.meta() = m_new; m_controller->chartChanged(); }
private:
    MetaData m_old, m_new;
};

// ---------- ChartController 实现 ----------
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

void ChartController::moveNotes(const QList<QPair<Note, Note>>& changes)
{
    if (changes.isEmpty()) return;
    m_undoStack->push(new MoveNotesCommand(this, changes));
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
    Logger::info(QString("ChartController::loadChart: Loading chart from %1").arg(path));
    try {
        Chart newChart;
        Logger::debug("ChartController::loadChart: Created new Chart object");
        
        if (ChartIO::load(path, newChart)) {
            Logger::debug("ChartController::loadChart: ChartIO::load completed successfully");
            Logger::debug(QString("ChartController::loadChart: Chart has %1 notes").arg(newChart.notes().size()));
            
            m_chart = newChart;
            Logger::debug("ChartController::loadChart: Chart assigned to m_chart");
            
            m_undoStack->clear();
            Logger::debug("ChartController::loadChart: Undo stack cleared");
            
            emit chartChanged();
            emit chartLoaded();
            Logger::debug("ChartController::loadChart: Signals emitted");
            
            Logger::info(QString("ChartController::loadChart: Successfully loaded chart with %1 notes").arg(newChart.notes().size()));
            return true;
        }
        Logger::error(QString("ChartController::loadChart: ChartIO::load failed for %1").arg(path));
        emit errorOccurred("Failed to load chart: " + path);
        return false;
    } catch (const std::exception& e) {
        Logger::error(QString("ChartController::loadChart: Exception - %1").arg(e.what()));
        emit errorOccurred("Exception loading chart: " + QString::fromStdString(std::string(e.what())));
        return false;
    } catch (...) {
        Logger::error("ChartController::loadChart: Unknown exception");
        emit errorOccurred("Unknown exception loading chart");
        return false;
    }
}

bool ChartController::saveChart(const QString& path)
{
    Logger::info(QString("ChartController::saveChart: Saving chart to %1").arg(path));
    Logger::debug(QString("ChartController::saveChart: Chart has %1 notes").arg(m_chart.notes().size()));
    
    try {
        if (ChartIO::save(path, m_chart)) {
            Logger::info(QString("ChartController::saveChart: Successfully saved chart to %1").arg(path));
            return true;
        }
        Logger::error(QString("ChartController::saveChart: ChartIO::save failed for %1").arg(path));
        emit errorOccurred("Failed to save chart: " + path);
        return false;
    } catch (const std::exception& e) {
        Logger::error(QString("ChartController::saveChart: Exception - %1").arg(e.what()));
        emit errorOccurred("Exception saving chart: " + QString::fromStdString(std::string(e.what())));
        return false;
    } catch (...) {
        Logger::error("ChartController::saveChart: Unknown exception");
        emit errorOccurred("Unknown exception saving chart");
        return false;
    }
}