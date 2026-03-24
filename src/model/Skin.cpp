#include "Skin.h"
#include <QDir>
#include <QFileInfo>
#include <QImageReader>

Skin::Skin() : m_valid(false) {}

bool Skin::loadFromDir(const QString& skinPath) {
    clear();
    QDir dir(skinPath);
    if (!dir.exists()) return false;

    // 简单加载：尝试加载 catch-note-*.png
    // 这里仅作示例，实际需要根据规则映射
    QImageReader reader;
    // 加载 note 类型映射
    // 0: catch-note-0.png
    QFileInfo note0(dir.filePath("catch-note-0.png"));
    if (note0.exists()) {
        m_notePixmaps[0] = QPixmap(note0.filePath());
    }
    // 1: catch-note-1.png
    QFileInfo note1(dir.filePath("catch-note-1.png"));
    if (note1.exists()) m_notePixmaps[1] = QPixmap(note1.filePath());
    // 2: catch-note-2.png
    QFileInfo note2(dir.filePath("catch-note-2.png"));
    if (note2.exists()) m_notePixmaps[2] = QPixmap(note2.filePath());
    // 3: catch-note-3.png (用于 1/8,1/16,1/32)
    QFileInfo note3(dir.filePath("catch-note-3.png"));
    if (note3.exists()) m_notePixmaps[3] = QPixmap(note3.filePath());
    // 4: catch-note-4.png (用于 1/3,1/6,1/12,1/24)
    QFileInfo note4(dir.filePath("catch-note-4.png"));
    if (note4.exists()) m_notePixmaps[4] = QPixmap(note4.filePath());
    // 5: catch-note-5.png (rain)
    QFileInfo note5(dir.filePath("catch-note-5.png"));
    if (note5.exists()) m_notePixmaps[5] = QPixmap(note5.filePath());

    // 加载 bar
    QFileInfo bar(dir.filePath("catch-bar.png"));
    if (bar.exists()) m_barPixmap = QPixmap(bar.filePath());

    // 加载 light 图片
    for (int i = 0; i <= 16; ++i) {
        QFileInfo light(dir.filePath(QString("catch-light-%1.png").arg(i)));
        if (light.exists()) m_lightPixmaps[i] = QPixmap(light.filePath());
    }
    // 其他 light 变体
    QFileInfo lightArc(dir.filePath("catch-light-arc.png"));
    if (lightArc.exists()) m_lightPixmaps[17] = QPixmap(lightArc.filePath());
    QFileInfo lightBar(dir.filePath("catch-light-bar.png"));
    if (lightBar.exists()) m_lightPixmaps[18] = QPixmap(lightBar.filePath());

    m_valid = true;
    return true;
}

void Skin::clear() {
    m_notePixmaps.clear();
    m_barPixmap = QPixmap();
    m_lightPixmaps.clear();
    m_valid = false;
}

const QPixmap* Skin::getNotePixmap(int noteType) const {
    auto it = m_notePixmaps.constFind(noteType);
    if (it != m_notePixmaps.constEnd())
        return &it.value();
    return nullptr;
}

const QPixmap* Skin::getBarPixmap() const {
    return m_barPixmap.isNull() ? nullptr : &m_barPixmap;
}

const QPixmap* Skin::getLightPixmap(int lightIndex) const {
    auto it = m_lightPixmaps.constFind(lightIndex);
    if (it != m_lightPixmaps.constEnd())
        return &it.value();
    return nullptr;
}