// src/file/ChartIO.cpp
#include "ChartIO.h"
#include "utils/Logger.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

bool ChartIO::load(const QString& filePath, Chart& outChart)
{
    Logger::info(QString("ChartIO::load - Loading chart from: %1").arg(filePath));
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Logger::error(QString("ChartIO::load - Cannot open file: %1").arg(filePath));
        return false;
    }

    QByteArray data = file.readAll();
    file.close();
    Logger::debug(QString("ChartIO::load - Read %1 bytes from file").arg(data.size()));

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        Logger::error(QString("ChartIO::load - Invalid JSON in file: %1").arg(filePath));
        return false;
    }

    QJsonObject root = doc.object();
    outChart.clear();
    Logger::debug("ChartIO::load - JSON parsed successfully, starting to load data");

    // 读取 time 数组（BPM 表）
    int bpmCount = 0;
    if (root.contains("time") && root["time"].isArray()) {
        QJsonArray timeArray = root["time"].toArray();
        Logger::debug(QString("ChartIO::load - Found 'time' array with %1 entries").arg(timeArray.size()));
        for (const QJsonValue& val : timeArray) {
            QJsonObject obj = val.toObject();
            if (!obj.contains("beat") || !obj.contains("bpm")) continue;
            QJsonArray beatArr = obj["beat"].toArray();
            if (beatArr.size() < 3) continue;
            int beatNum = beatArr[0].toInt();
            int num = beatArr[1].toInt();
            int den = beatArr[2].toInt();
            double bpm = obj["bpm"].toDouble();
            outChart.addBpm(BpmEntry(beatNum, num, den, bpm));
            bpmCount++;
        }
        Logger::info(QString("ChartIO::load - Loaded %1 BPM entries").arg(bpmCount));
    } else {
        Logger::debug("ChartIO::load - No 'time' array found in file");
    }

    // 读取 note 数组
    int normalNoteCount = 0, rainNoteCount = 0, soundNoteCount = 0;
    if (root.contains("note") && root["note"].isArray()) {
        QJsonArray noteArray = root["note"].toArray();
        Logger::debug(QString("ChartIO::load - Found 'note' array with %1 entries").arg(noteArray.size()));
        
        for (int i = 0; i < noteArray.size(); i++) {
            QJsonObject obj = noteArray[i].toObject();
            QJsonArray beatArr = obj["beat"].toArray();
            if (!obj.contains("beat") || beatArr.size() < 3) {
                Logger::warn(QString("ChartIO::load - Skipping note %1: invalid beat").arg(i));
                continue;
            }
            
            int beatNum = beatArr[0].toInt();
            int num = beatArr[1].toInt();
            int den = beatArr[2].toInt();
            int type = obj.value("type").toInt(0);

            // 音效音符（type=1）：有 sound 字段，无 x 字段
            if (type == 1 && obj.contains("sound")) {
                QString sound = obj["sound"].toString();
                int vol = obj.value("vol").toInt(100);
                int offset = obj.value("offset").toInt(0);
                outChart.addNote(Note(beatNum, num, den, sound, vol, offset));
                soundNoteCount++;
                Logger::debug(QString("ChartIO::load - Note %1: Sound note at [%2,%3,%4], sound=%5").arg(i).arg(beatNum).arg(num).arg(den).arg(sound));
            }
            // Rain 音符（type=3）：有 endbeat 字段和 x 字段
            else if (type == 3 && obj.contains("endbeat") && obj.contains("x")) {
                QJsonArray endBeatArr = obj["endbeat"].toArray();
                if (endBeatArr.size() >= 3) {
                    int x = obj["x"].toInt();
                    int endBeatNum = endBeatArr[0].toInt();
                    int endNum = endBeatArr[1].toInt();
                    int endDen = endBeatArr[2].toInt();
                    outChart.addNote(Note(beatNum, num, den, endBeatNum, endNum, endDen, x));
                    rainNoteCount++;
                    Logger::debug(QString("ChartIO::load - Note %1: Rain note at [%2,%3,%4]-[%5,%6,%7], x=%8").arg(i).arg(beatNum).arg(num).arg(den).arg(endBeatNum).arg(endNum).arg(endDen).arg(x));
                } else {
                    Logger::warn(QString("ChartIO::load - Note %1: Invalid Rain note endbeat format").arg(i));
                }
            }
            // 普通音符（type=0）：必须有 x 字段
            else if (obj.contains("x")) {
                int x = obj["x"].toInt();
                outChart.addNote(Note(beatNum, num, den, x));
                normalNoteCount++;
                Logger::debug(QString("ChartIO::load - Note %1: Normal note at [%2,%3,%4], x=%5").arg(i).arg(beatNum).arg(num).arg(den).arg(x));
            } else {
                Logger::warn(QString("ChartIO::load - Note %1: Skipped (type=%2, missing required fields)").arg(i).arg(type));
            }
        }
        Logger::info(QString("ChartIO::load - Loaded %1 normal notes, %2 rain notes, %3 sound notes").arg(normalNoteCount).arg(rainNoteCount).arg(soundNoteCount));
    } else {
        Logger::debug("ChartIO::load - No 'note' array found in file");
    }

    // 读取 meta（如果有）
    if (root.contains("meta") && root["meta"].isObject()) {
        QJsonObject metaObj = root["meta"].toObject();
        MetaData& meta = outChart.meta();
        meta.title = metaObj.value("title").toString();
        meta.titleOrg = metaObj.value("title_org").toString();
        meta.artist = metaObj.value("artist").toString();
        meta.artistOrg = metaObj.value("artist_org").toString();
        meta.difficulty = metaObj.value("difficulty").toString();
        meta.chartAuthor = metaObj.value("chart_author").toString();
        meta.audioFile = metaObj.value("audio").toString();
        meta.backgroundFile = metaObj.value("background").toString();
        meta.previewTime = metaObj.value("preview_time").toInt();
        meta.firstBpm = metaObj.value("bpm").toDouble();
        meta.offset = metaObj.value("offset").toInt();
        meta.speed = metaObj.value("speed").toInt();
        Logger::debug(QString("ChartIO::load - Meta loaded: title=%1, artist=%2, difficulty=%3").arg(meta.title).arg(meta.artist).arg(meta.difficulty));
    } else {
        Logger::debug("ChartIO::load - No 'meta' object found in file");
    }

    outChart.sortNotes();
    Logger::info(QString("ChartIO::load - Chart loaded successfully from: %1").arg(filePath));
    return true;
}

bool ChartIO::save(const QString& filePath, const Chart& chart)
{
    Logger::info(QString("ChartIO::save - Saving chart to: %1").arg(filePath));
    
    QJsonObject root;

    // 保存 meta
    QJsonObject metaObj;
    metaObj["title"] = chart.meta().title;
    metaObj["title_org"] = chart.meta().titleOrg;
    metaObj["artist"] = chart.meta().artist;
    metaObj["artist_org"] = chart.meta().artistOrg;
    metaObj["difficulty"] = chart.meta().difficulty;
    metaObj["chart_author"] = chart.meta().chartAuthor;
    metaObj["audio"] = chart.meta().audioFile;
    metaObj["background"] = chart.meta().backgroundFile;
    metaObj["preview_time"] = chart.meta().previewTime;
    metaObj["bpm"] = chart.meta().firstBpm;
    metaObj["offset"] = chart.meta().offset;
    metaObj["speed"] = chart.meta().speed;
    root["meta"] = metaObj;
    Logger::debug(QString("ChartIO::save - Meta saved: title=%1, difficulty=%2").arg(chart.meta().title).arg(chart.meta().difficulty));

    // 保存 time 数组
    QJsonArray timeArray;
    for (const BpmEntry& bpm : chart.bpmList()) {
        QJsonObject obj;
        QJsonArray beatArr;
        beatArr.append(bpm.beatNum);
        beatArr.append(bpm.numerator);
        beatArr.append(bpm.denominator);
        obj["beat"] = beatArr;
        obj["bpm"] = bpm.bpm;
        timeArray.append(obj);
    }
    root["time"] = timeArray;
    Logger::debug(QString("ChartIO::save - Time array saved with %1 entries").arg(timeArray.size()));

    // 保存 effect 数组（暂时为空，留作扩展）
    QJsonArray effectArray;
    root["effect"] = effectArray;
    Logger::debug("ChartIO::save - Effect array saved (empty)");

    // 保存 note 数组
    QJsonArray noteArray;
    int normalNoteCount = 0, rainNoteCount = 0, soundNoteCount = 0;
    
    for (const Note& note : chart.notes()) {
        QJsonObject obj;
        QJsonArray beatArr;
        beatArr.append(note.beatNum);
        beatArr.append(note.numerator);
        beatArr.append(note.denominator);
        obj["beat"] = beatArr;

        if (note.type == 1) {
            // 音效音符
            obj["type"] = 1;
            obj["sound"] = note.sound;
            obj["vol"] = note.vol;
            obj["offset"] = note.offset;
            soundNoteCount++;
            Logger::debug(QString("ChartIO::save - Sound note: [%1,%2,%3], sound=%4").arg(note.beatNum).arg(note.numerator).arg(note.denominator).arg(note.sound));
        } else if (note.type == 3) {
            // Rain 音符
            obj["type"] = 3;
            obj["x"] = note.x;
            QJsonArray endBeatArr;
            endBeatArr.append(note.endBeatNum);
            endBeatArr.append(note.endNumerator);
            endBeatArr.append(note.endDenominator);
            obj["endbeat"] = endBeatArr;
            rainNoteCount++;
            Logger::debug(QString("ChartIO::save - Rain note: [%1,%2,%3]-[%4,%5,%6], x=%7").arg(note.beatNum).arg(note.numerator).arg(note.denominator).arg(note.endBeatNum).arg(note.endNumerator).arg(note.endDenominator).arg(note.x));
        } else {
            // 普通音符
            obj["x"] = note.x;
            normalNoteCount++;
            Logger::debug(QString("ChartIO::save - Normal note: [%1,%2,%3], x=%4").arg(note.beatNum).arg(note.numerator).arg(note.denominator).arg(note.x));
        }
        noteArray.append(obj);
    }
    root["note"] = noteArray;
    Logger::info(QString("ChartIO::save - Saved %1 normal notes, %2 rain notes, %3 sound notes").arg(normalNoteCount).arg(rainNoteCount).arg(soundNoteCount));

    // 保存 extra（可选，包含测试配置等）
    QJsonObject extraObj;
    QJsonObject testObj;
    testObj["divide"] = 4;
    testObj["speed"] = 100;
    testObj["save"] = 0;
    testObj["lock"] = 0;
    testObj["edit_mode"] = 0;
    extraObj["test"] = testObj;
    root["extra"] = extraObj;
    Logger::debug("ChartIO::save - Extra (test config) saved");

    QJsonDocument doc(root);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Logger::error(QString("ChartIO::save - Cannot open file for writing: %1").arg(filePath));
        return false;
    }
    
    QByteArray jsonData = doc.toJson();
    file.write(jsonData);
    file.close();
    
    Logger::info(QString("ChartIO::save - Chart saved successfully (%1 bytes) to: %2").arg(jsonData.size()).arg(filePath));
    return true;
}