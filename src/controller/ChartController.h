// src/controller/ChartController.h - 谱面编辑核心控制器
#pragma once

#include <QObject>
#include <QUndoStack>
#include "model/Chart.h"

/**
 * @brief 谱面编辑控制器，负责所有修改操作，并管理撤销/重做栈。
 *
 * 线程安全：所有方法必须在主线程调用。
 * 修改信号：任何数据变化都会发送 chartChanged() 信号。
 */
class ChartController : public QObject
{
    Q_OBJECT
public:
    explicit ChartController(QObject *parent = nullptr);
    ~ChartController();

    // 获取当前谱面（只读）
    const Chart *chart() const { return &m_chart; }

    // 获取当前谱面文件路径
    QString chartFilePath() const { return m_currentChartPath; }

    // 编辑操作（都会自动压入撤销栈）
    void addNote(const Note &note);
    void removeNote(const Note &note);
    void moveNote(const Note &original, const Note &newNote);
    void moveNotes(const QList<QPair<Note, Note>> &changes); // 批量移动，复合命令
    void removeNotes(const QVector<Note> &notes);            // 批量删除，复合命令
    void addBpm(const BpmEntry &bpm);
    void removeBpm(int index);
    void updateBpm(int index, const BpmEntry &bpm);
    void setMetaData(const MetaData &meta);

    // 撤销/重做
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    // 保存/加载
    bool loadChart(const QString &path);
    bool saveChart(const QString &path);

signals:
    void chartChanged(); // 任何数据变化
    void chartLoaded();  // 加载新谱面
    void errorOccurred(const QString &msg);

private:
    class ChartCommand;
    class AddNoteCommand;
    class RemoveNoteCommand;
    class RemoveNotesCommand;
    class MoveNoteCommand;
    class MoveNotesCommand;
    class AddBpmCommand;
    class RemoveBpmCommand;
    class UpdateBpmCommand;
    class SetMetaCommand;

    Chart m_chart;
    QUndoStack *m_undoStack;
    QString m_currentChartPath; // 当前加载的谱面文件路径
};