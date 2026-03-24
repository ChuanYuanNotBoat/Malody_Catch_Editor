#include "ProjectIO.h"
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QDebug>

bool ProjectIO::exportToMcz(const QString& outputPath, const Chart& chart,
                            const QString& audioPath, const QString& bgPath) {
    // 使用 Qt 的 zip 支持需要 QuaZIP 或 Qt 的 QZipWriter，这里用简单方法：调用外部 zip 命令
    // 暂未实现，仅占位
    qWarning() << "ProjectIO::exportToMcz not implemented";
    return false;
}

bool ProjectIO::importMcz(const QString& mczPath, QString& outChartPath) {
    // 解压 mcz 到临时目录，并返回 .mc 文件路径
    // 暂未实现
    qWarning() << "ProjectIO::importMcz not implemented";
    return false;
}