#pragma once

#include <QString>
#include <QStringList>
#include <QPair>
#include "model/Chart.h"

class ProjectIO {
public:
    /**
     * @brief 导入MCZ文件 - 解压到指定目录，返回主.mc文件路径
     * @param mczPath MCZ文件路径
     * @param outputDir 输出目录（不存在则创建）
     * @param outChartPath 输出：解压后的主.mc文件路径
     * @return 成功返回true
     */
    static bool importMcz(const QString& mczPath, const QString& outputDir, QString& outChartPath);
    
    /**
     * @brief 导出为MCZ文件 - 将.mc及其依赖打包为MCZ
     * @param outputMczPath 输出的MCZ文件路径
     * @param sourceChartPath 源.mc文件路径
     * @return 成功返回true
     */
    static bool exportToMcz(const QString& outputMczPath, const QString& sourceChartPath);
    
    /**
     * @brief 自动检测并加载文件 - 根据扩展名决定导入方式
     * @param filePath .mc或.mcz文件路径
     * @param outChartPath 输出：加载后的.mc文件路径
     * @return 成功返回true
     */
    static bool loadChartFile(const QString& filePath, QString& outChartPath);
    
    /**
     * @brief 在MCZ中查找所有.mc文件及其难度
     * @param extractDir 解压后的目录
     * @return 返回(文件路径, 难度名)的列表
     */
    static QList<QPair<QString, QString>> findChartsInMcz(const QString& extractDir);
    
    /**
     * @brief 收集文件依赖 - 从.mc文件所在目录收集所有依赖文件
     * @param chartDir .mc文件所在目录
     * @return 依赖文件的绝对路径列表
     */
    static QStringList collectDependencies(const QString& chartDir);

private:
    /**
     * @brief 递归复制目录
     */
    static bool copyDir(const QString& srcDir, const QString& destDir);
    
    /**
     * @brief 递归删除目录
     */
    static bool removeDir(const QString& dirPath);
    
    /**
     * @brief 从.mc文件读取难度名
     */
    static QString getDifficultyFromMc(const QString& mcPath);
};