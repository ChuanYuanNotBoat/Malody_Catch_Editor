// src/controller/SelectionController.h
#pragma once

#include <QObject>
#include <QSet>
#include <QRectF>
#include <functional>
#include "model/Note.h"

/**
 * @brief 管理音符选中状态。
 * @note 主线程调用。
 */
class SelectionController : public QObject {
    Q_OBJECT
public:
    explicit SelectionController(QObject* parent = nullptr);

    QSet<int> selectedIndices() const;

    void select(int index);
    void select(const QSet<int>& indices);
    void addToSelection(int index);
    void removeFromSelection(int index);
    void clearSelection();

    void selectInRect(const QRectF& rect, const QVector<Note>& notes,
                      std::function<QPointF(const Note&)> noteToPos);

    void copySelected(const QVector<Note>& notes);   // 复制当前选中的音符到剪贴板
    void copySelected(const QVector<Note>& notes, const QVector<int>& indices); // 指定列表复制
    QVector<Note> getClipboard() const;
    void clearClipboard();

signals:
    void selectionChanged(const QSet<int>& selectedIndices);

private:
    QSet<int> m_selectedIndices;
    QVector<Note> m_clipboard;
};