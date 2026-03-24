#include "SelectionController.h"
#include <QDebug>
#include <QMimeData>
#include <QClipboard>
#include <QApplication>

SelectionController::SelectionController(QObject* parent) : QObject(parent)
{
}

QSet<int> SelectionController::selectedIndices() const
{
    return m_selectedIndices;
}

void SelectionController::select(int index)
{
    m_selectedIndices.clear();
    m_selectedIndices.insert(index);
    emit selectionChanged(m_selectedIndices);
}

void SelectionController::select(const QSet<int>& indices)
{
    m_selectedIndices = indices;
    emit selectionChanged(m_selectedIndices);
}

void SelectionController::addToSelection(int index)
{
    m_selectedIndices.insert(index);
    emit selectionChanged(m_selectedIndices);
}

void SelectionController::removeFromSelection(int index)
{
    m_selectedIndices.remove(index);
    emit selectionChanged(m_selectedIndices);
}

void SelectionController::clearSelection()
{
    if (!m_selectedIndices.isEmpty()) {
        m_selectedIndices.clear();
        emit selectionChanged(m_selectedIndices);
    }
}

void SelectionController::selectInRect(const QRectF& rect, const QVector<Note>& notes,
                                       std::function<QPointF(const Note&)> noteToPos)
{
    QSet<int> newSelection;
    for (int i = 0; i < notes.size(); ++i) {
        QPointF pos = noteToPos(notes[i]);
        if (rect.contains(pos)) {
            newSelection.insert(i);
        }
    }
    select(newSelection);
}

void SelectionController::copySelected(const QVector<Note>& notes)
{
    m_clipboard.clear();
    for (int idx : m_selectedIndices) {
        if (idx >= 0 && idx < notes.size())
            m_clipboard.append(notes[idx]);
    }
}

QVector<Note> SelectionController::getClipboard() const
{
    return m_clipboard;
}

void SelectionController::clearClipboard()
{
    m_clipboard.clear();
}