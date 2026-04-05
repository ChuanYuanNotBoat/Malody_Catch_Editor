// src/model/Note.h - 音符数据模型，包含普通音符和雨音符

#pragma once

#include <QUuid>
#include <QString>
#include <cstdint>

/**
 * @brief 音符结构体
 * 
 * 支持三种类型：
 * - 普通音符：type = 0，只有起始拍号
 * - Rain音符：type = 3，有起始和结束拍号
 * - 音效音符：type = 1，有声音相关属性
 */
struct Note {
    // 起始拍号（整数拍 + 分子/分母）
    int beatNum;      // 从0开始的整数拍
    int numerator;    // 分子（1-...）
    int denominator;  // 分母（1,2,3,4...）

    QString id;       // 唯一标识符

    // 音符类型（0=普通，1=音效，3=rain）
    int type;

    // 横坐标 0-512（普通音符和 rain 共用；音效音符可以为 -1 或其他值）
    int x;

    // 是否为 rain 音符
    bool isRain;

    // rain 专属：结束拍号
    int endBeatNum;
    int endNumerator;
    int endDenominator;

    // 音效音符专属属性
    QString sound;    // 音效文件名
    int vol;          // 音量（0-100）
    int offset;       // 偏移量（毫秒）

    /**
     * @brief 默认构造函数，生成无效音符
     */
    Note();

    /**
     * @brief 普通音符构造函数
     */
    Note(int beatNum, int numerator, int denominator, int x);

    /**
     * @brief rain 音符构造函数
     */
    Note(int startBeatNum, int startNumerator, int startDenominator,
         int endBeatNum, int endNumerator, int endDenominator,
         int x);

    /**
     * @brief 音效音符构造函数
     */
    Note(int beatNum, int numerator, int denominator,
         const QString& sound, int vol, int offset);

    /**
     * @brief 判断 rain 音符是否有效（结束时间不早于开始时间）
     */
    bool isValidRain() const;

    /**
     * @brief 判断两个音符是否相等（用于撤销/重做比较）
     */
    bool operator==(const Note &other) const;

    // 生成新 ID
    static QString generateId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
};