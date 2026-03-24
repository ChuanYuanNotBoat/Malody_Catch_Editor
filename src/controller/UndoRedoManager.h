#pragma once

#include <QUndoStack>

// 占位符，实际使用 QUndoStack
class UndoRedoManager {
public:
    UndoRedoManager() : m_stack(new QUndoStack()) {}
    ~UndoRedoManager() { delete m_stack; }

    QUndoStack* stack() const { return m_stack; }

private:
    QUndoStack* m_stack;
};