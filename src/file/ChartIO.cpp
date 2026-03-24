// src/file/ChartIO.cpp
#include "ChartIO.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

bool ChartIO::load(const QString& filePath, Chart& outChart)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        qWarning() << "Invalid JSON in file:" << filePath;
        return false;
    }

    QJsonObject root = doc.object();
    outChart.clear();

    // 读取 time 数组（BPM 表）
    if (root.contains("time") && root["time"].isArray()) {
        QJsonArray timeArray = root["time"].toArray();
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
        }
    }

    // 读取 note 数组
    if (root.contains("note") && root["note"].isArray()) {
        QJsonArray noteArray = root["note"].toArray();
        for (const QJsonValue& val : noteArray) {
            QJsonObject obj = val.toObject();
            if (!obj.contains("beat") || !obj.contains("x")) continue;
            QJsonArray beatArr = obj["beat"].toArray();
            if (beatArr.size() < 3) continue;
            int beatNum = beatArr[0].toInt();
            int num = beatArr[1].toInt();
            int den = beatArr[2].toInt();
            int x = obj["x"].toInt();

            int type = obj.value("type").toInt(0);
            if (type == 3 && obj.contains("endbeat")) {
                // Rain 音符
                QJsonArray endBeatArr = obj["endbeat"].toArray();
                if (endBeatArr.size() >= 3) {
                    int endBeatNum = endBeatArr[0].toInt();
                    int endNum = endBeatArr[1].toInt();
                    int endDen = endBeatArr[2].toInt();
                    outChart.addNote(Note(beatNum, num, den, endBeatNum, endNum, endDen, x));
                } else {
                    // 格式错误，当作普通音符
                    outChart.addNote(Note(beatNum, num, den, x));
                }
            } else {
                outChart.addNote(Note(beatNum, num, den, x));
            }
        }
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
    }

    outChart.sortNotes();
    return true;
}

bool ChartIO::save(const QString& filePath, const Chart& chart)
{
    QJsonObject root;

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

    // 保存 note 数组
    QJsonArray noteArray;
    for (const Note& note : chart.notes()) {
        QJsonObject obj;
        QJsonArray beatArr;
        beatArr.append(note.beatNum);
        beatArr.append(note.numerator);
        beatArr.append(note.denominator);
        obj["beat"] = beatArr;
        obj["x"] = note.x;
        if (note.isRain) {
            obj["type"] = 3;
            QJsonArray endBeatArr;
            endBeatArr.append(note.endBeatNum);
            endBeatArr.append(note.endNumerator);
            endBeatArr.append(note.endDenominator);
            obj["endbeat"] = endBeatArr;
        }
        noteArray.append(obj);
    }
    root["note"] = noteArray;

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

    QJsonDocument doc(root);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file for writing:" << filePath;
        return false;
    }
    file.write(doc.toJson());
    return true;
}