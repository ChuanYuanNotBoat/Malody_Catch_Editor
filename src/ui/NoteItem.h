#pragma once

#include <QWidget>
#include "model/Note.h"

class NoteItem : public QWidget {
    Q_OBJECT
public:
    explicit NoteItem(const Note& note, QWidget* parent = nullptr);
    void setSelected(bool selected);
    Note note() const { return m_note; }

signals:
    void clicked(const Note& note);
    void doubleClicked(const Note& note);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    Note m_note;
    bool m_selected;
};