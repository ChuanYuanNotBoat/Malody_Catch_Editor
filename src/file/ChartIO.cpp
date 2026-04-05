// src/file/ChartIO.cpp
#include "ChartIO.h"
#include "utils/Logger.h"
#include "utils/DiagnosticCollector.h"
#include "utils/PerformanceTimer.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

bool ChartIO::load(const QString& filePath, Chart& outChart, bool verbose)
{
    PerformanceTimer loadTimer("ChartIO::load", "chart_loading");
    
    Logger::info(QString("ChartIO::load - Loading chart from: %1 (verbose mode: %2)").arg(filePath).arg(verbose ? "on" : "off"));
    
    // 保存原来的 verbose 设置
    bool previousVerbose = Logger::isVerbose();
    // 设置为导入所需的 verbose 模式
    Logger::setVerbose(verbose);
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Logger::error(QString("ChartIO::load - Cannot open file: %1").arg(filePath));
        Logger::setVerbose(previousVerbose);  // 恢复设置
        return false;
    }

    QByteArray data = file.readAll();
    file.close();
    Logger::debug(QString("ChartIO::load - Read %1 bytes from file").arg(data.size()));

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        Logger::error(QString("ChartIO::load - Invalid JSON in file: %1").arg(filePath));
        Logger::setVerbose(previousVerbose);  // 恢复设置
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
    int normalNoteCount = 0, rainNoteCount = 0, soundNoteCount = 0, skippedNoteCount = 0;
    int totalNoteCount = 0;  // 用于统计成功率
    
    if (root.contains("note") && root["note"].isArray()) {
        QJsonArray noteArray = root["note"].toArray();
        totalNoteCount = noteArray.size();
        Logger::debug(QString("ChartIO::load - Found 'note' array with %1 entries").arg(totalNoteCount));
        
        for (int i = 0; i < noteArray.size(); i++) {
            QJsonObject obj = noteArray[i].toObject();
            QJsonArray beatArr = obj["beat"].toArray();
            
            if (!obj.contains("beat") || beatArr.size() < 3) {
                Logger::warn(QString("ChartIO::load - Note %1: Skipped (invalid beat array)").arg(i));
                skippedNoteCount++;
                
                // 记录诊断信息
                QStringList missingFields;
                missingFields << "beat";
                DiagnosticCollector::instance().recordSkippedNote(i, -1, "invalid_beat", missingFields);
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
                if (Logger::isVerbose()) {
                    Logger::debug(QString("ChartIO::load - Note %1: Sound note at [%2,%3,%4], sound=%5").arg(i).arg(beatNum).arg(num).arg(den).arg(sound));
                }
            }
            // Rain 音符（type=3）：有 endbeat 字段，x 字段可选
            else if (type == 3 && obj.contains("endbeat")) {
                QJsonArray endBeatArr = obj["endbeat"].toArray();
                if (endBeatArr.size() >= 3) {
                    int x = obj.value("x").toInt(256);  // 如果没有 x 字段，默认值为 256（屏幕中央）
                    int endBeatNum = endBeatArr[0].toInt();
                    int endNum = endBeatArr[1].toInt();
                    int endDen = endBeatArr[2].toInt();
                    outChart.addNote(Note(beatNum, num, den, endBeatNum, endNum, endDen, x));
                    rainNoteCount++;
                    if (Logger::isVerbose()) {
                        Logger::debug(QString("ChartIO::load - Note %1: Rain note at [%2,%3,%4]-[%5,%6,%7], x=%8").arg(i).arg(beatNum).arg(num).arg(den).arg(endBeatNum).arg(endNum).arg(endDen).arg(x));
                    }
                } else {
                    Logger::warn(QString("ChartIO::load - Note %1: Skipped (type=3, invalid endbeat format)").arg(i));
                    skippedNoteCount++;
                    
                    // 记录诊断信息
                    QStringList missingFields;
                    missingFields << "endbeat";
                    QStringList presentFields;
                    presentFields << "beat" << "type";
                    if (obj.contains("x")) presentFields << "x";
                    DiagnosticCollector::instance().recordSkippedNote(i, 3, "invalid_endbeat_format", missingFields, presentFields);
                }
            }
            // 普通音符（type=0）：必须有 x 字段
            else if (obj.contains("x")) {
                int x = obj["x"].toInt();
                outChart.addNote(Note(beatNum, num, den, x));
                normalNoteCount++;
                if (Logger::isVerbose()) {
                    Logger::debug(QString("ChartIO::load - Note %1: Normal note at [%2,%3,%4], x=%5").arg(i).arg(beatNum).arg(num).arg(den).arg(x));
                }
            } 
            // Sound note检查（type=1但缺少sound字段）
            else if (type == 1) {
                Logger::warn(QString("ChartIO::load - Note %1: Skipped (type=1, missing 'sound' field)").arg(i));
                skippedNoteCount++;
                
                QStringList missingFields;
                missingFields << "sound";
                QStringList presentFields;
                presentFields << "beat" << "type";
                if (obj.contains("vol")) presentFields << "vol";
                if (obj.contains("offset")) presentFields << "offset";
                DiagnosticCollector::instance().recordSkippedNote(i, 1, "missing_sound_field", missingFields, presentFields);
            }
            // Rain note检查（type=3但缺少endbeat字段）
            else if (type == 3) {
                Logger::warn(QString("ChartIO::load - Note %1: Skipped (type=3, missing 'endbeat' field)").arg(i));
                skippedNoteCount++;
                
                QStringList missingFields;
                missingFields << "endbeat";
                QStringList presentFields;
                presentFields << "beat" << "type";
                if (obj.contains("x")) presentFields << "x";
                DiagnosticCollector::instance().recordSkippedNote(i, 3, "missing_endbeat_field", missingFields, presentFields);
            }
            // 普通音符缺少x字段
            else {
                Logger::warn(QString("ChartIO::load - Note %1: Skipped (type=%2, missing 'x' field)").arg(i).arg(type));
                skippedNoteCount++;
                
                QStringList missingFields;
                missingFields << "x";
                QStringList presentFields;
                presentFields << "beat" << "type";
                DiagnosticCollector::instance().recordSkippedNote(i, type, "missing_x_field", missingFields, presentFields);
            }
        }
        
        // 输出详细统计信息
        int totalLoadedNotes = normalNoteCount + rainNoteCount + soundNoteCount;
        double successRate = totalNoteCount > 0 ? (totalLoadedNotes * 100.0 / totalNoteCount) : 0.0;
        
        Logger::info(QString("ChartIO::load - Loaded %1 normal notes, %2 rain notes, %3 sound notes")
                    .arg(normalNoteCount).arg(rainNoteCount).arg(soundNoteCount));
        Logger::info(QString("ChartIO::load - Note Summary: %1 / %2 notes loaded (%3%% success rate)")
                    .arg(totalLoadedNotes).arg(totalNoteCount).arg(QString::number(successRate, 'f', 1)));
        
        if (skippedNoteCount > 0) {
            Logger::warn(QString("ChartIO::load - Skipped %1 notes with missing/invalid fields").arg(skippedNoteCount));
        }
        
        // 记录加载指标
        DiagnosticCollector::instance().recordLoadMetrics(filePath, loadTimer.elapsed(), 
                                                         totalNoteCount, totalLoadedNotes, skippedNoteCount);
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
    Logger::setVerbose(previousVerbose);  // 恢复原来的设置
    return true;
}

bool ChartIO::save(const QString& filePath, const Chart& chart)
{
    PerformanceTimer saveTimer("ChartIO::save", "chart_saving");
    
    Logger::info(QString("ChartIO::save - Saving chart to: %1").arg(filePath));
    Logger::debug(QString("ChartIO::save - Chart has %1 notes").arg(chart.notes().size()));
    
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
    
    // 记录保存指标
    DiagnosticCollector::instance().recordSaveMetrics(filePath, saveTimer.elapsed(), chart.notes().size());
    
    return true;
}