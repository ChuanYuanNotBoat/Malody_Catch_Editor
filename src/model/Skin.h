#pragma once

#include <QString>
#include <QMap>
#include <QPixmap>

class Skin {
public:
    Skin();
    bool loadFromDir(const QString& skinPath);
    void clear();

    const QPixmap* getNotePixmap(int noteType) const; // 0=1/1,1=1/2,2=1/4,3=1/8/16/32,4=1/3/6/12/24,5=rain
    const QPixmap* getBarPixmap() const;
    const QPixmap* getLightPixmap(int lightIndex) const;

    QString title() const { return m_title; }
    void setTitle(const QString& t) { m_title = t; }
    QString desc() const { return m_desc; }
    void setDesc(const QString& d) { m_desc = d; }
    QString coverPath() const { return m_coverPath; }
    void setCoverPath(const QString& path) { m_coverPath = path; }

private:
    QMap<int, QPixmap> m_notePixmaps;
    QPixmap m_barPixmap;
    QMap<int, QPixmap> m_lightPixmaps;
    QString m_title, m_desc, m_coverPath;
    bool m_valid;
};