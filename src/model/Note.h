// src/model/Note.h - 音符数据模型，包含普通音符和雨音符

#pragma once

#include <cstdint>

/**
 * @brief 音符结构体
 * 
 * 支持两种类型：
 * - 普通音符：只有起始拍号，isRain = false
 * - Rain音符：有起始和结束拍号，isRain = true，type 字段为 3
 */
struct Note {
    // 起始拍号（整数拍 + 分子/分母）
    int beatNum;      // 从0开始的整数拍
    int numerator;    // 分子（1-...）
    int denominator;  // 分母（1,2,3,4...）

    // 横坐标 0-512（普通音符和 rain 共用）
    int x;

    // 是否为 rain 音符
    bool isRain;

    // rain 专属：结束拍号
    int endBeatNum;
    int endNumerator;
    int endDenominator;

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
     * @brief 判断 rain 音符是否有效（结束时间不早于开始时间）
     */
    bool isValidRain() const;

    /**
     * @brief 判断两个音符是否相等（用于撤销/重做比较）
     */
    bool operator==(const Note &other) const;
};