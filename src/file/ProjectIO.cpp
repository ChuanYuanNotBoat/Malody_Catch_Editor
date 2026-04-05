#include "ProjectIO.h"
#include "utils/Logger.h"
#include "file/ChartIO.h"
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

bool ProjectIO::importMcz(const QString& mczPath, const QString& outputDir, QString& outChartPath)
{
    Logger::info(QString("ProjectIO::importMcz - Importing MCZ: %1 to %2").arg(mczPath, outputDir));
    
    if (!QFile::exists(mczPath)) {
        Logger::error(QString("ProjectIO::importMcz - MCZ file not found: %1").arg(mczPath));
        return false;
    }

    QDir outDir(outputDir);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        Logger::error(QString("ProjectIO::importMcz - Failed to create output directory: %1").arg(outputDir));
        return false;
    }

    try {
        QProcess process;
        process.setWorkingDirectory(outputDir);

        #ifdef Q_OS_WIN
            QStringList args;
            args << "-NoProfile" << "-NonInteractive" << "-Command"
                 << QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
                    .arg(QFileInfo(mczPath).absoluteFilePath(), outputDir);
            process.start("powershell.exe", args);
        #else
            process.start("unzip", QStringList() << "-o" << mczPath << "-d" << outputDir);
        #endif

        if (!process.waitForFinished(30000)) {
            Logger::error(QString("ProjectIO::importMcz - Unzip process timeout"));
            process.kill();
            return false;
        }

        if (process.exitCode() != 0) {
            QString errMsg = QString::fromLocal8Bit(process.readAllStandardError());
            Logger::error(QString("ProjectIO::importMcz - Unzip failed: %1").arg(errMsg));
            return false;
        }

        Logger::debug("ProjectIO::importMcz - Archive extracted successfully");

        // 查找解压后的.mc文件（支持嵌套目录）
        QStringList mcFiles;
        std::function<void(const QString&)> findMcFiles = [&](const QString& dir) {
            QDir d(dir);
            for (const QString& item : d.entryList(QDir::Files)) {
                if (item.endsWith(".mc", Qt::CaseInsensitive)) {
                    mcFiles << d.absoluteFilePath(item);
                }
            }
            for (const QString& item : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                findMcFiles(d.absoluteFilePath(item));
            }
        };
        findMcFiles(outputDir);

        if (mcFiles.isEmpty()) {
            Logger::error("ProjectIO::importMcz - No .mc files found in archive");
            return false;
        }

        // 返回第一个.mc文件，caller需要处理多个文件的情况
        outChartPath = mcFiles.first();
        Logger::info(QString("ProjectIO::importMcz - Found chart file: %1").arg(outChartPath));
        return true;

    } catch (const std::exception& e) {
        Logger::error(QString("ProjectIO::importMcz - Exception: %1").arg(e.what()));
        return false;
    }
}

bool ProjectIO::exportToMcz(const QString& outputMczPath, const QString& sourceChartPath)
{
    Logger::info(QString("ProjectIO::exportToMcz - Exporting to: %1 from %2").arg(outputMczPath, sourceChartPath));
    
    if (!QFile::exists(sourceChartPath)) {
        Logger::error(QString("ProjectIO::exportToMcz - Chart file not found: %1").arg(sourceChartPath));
        return false;
    }

    QString chartDir = QFileInfo(sourceChartPath).absolutePath();

    try {
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            Logger::error("ProjectIO::exportToMcz - Failed to create temporary directory");
            return false;
        }

        QString packDir = tempDir.path() + "/mczpack";
        QDir d;
        if (!d.mkpath(packDir)) {
            Logger::error(QString("ProjectIO::exportToMcz - Failed to create pack directory"));
            return false;
        }

        // 复制整个目录到临时目录
        if (!copyDir(chartDir, packDir)) {
            Logger::error("ProjectIO::exportToMcz - Failed to copy chart directory");
            return false;
        }

        Logger::debug("ProjectIO::exportToMcz - Copied chart directory to pack directory");

        // 创建MCZ文件（ZIP格式）
        QProcess process;

        #ifdef Q_OS_WIN
            QStringList args;
            args << "-NoProfile" << "-NonInteractive" << "-Command"
                 << QString("Compress-Archive -Path '%1\\*' -DestinationPath '%2' -CompressionLevel Optimal -Force")
                    .arg(packDir, QFileInfo(outputMczPath).absoluteFilePath());
            process.start("powershell.exe", args);
        #else
            process.setWorkingDirectory(packDir);
            process.start("zip", QStringList() << "-r" << "-q" 
                         << QFileInfo(outputMczPath).absoluteFilePath() << ".");
        #endif

        if (!process.waitForFinished(60000)) {
            Logger::error("ProjectIO::exportToMcz - Zip process timeout");
            process.kill();
            return false;
        }

        if (process.exitCode() != 0) {
            QString errMsg = QString::fromLocal8Bit(process.readAllStandardError());
            Logger::error(QString("ProjectIO::exportToMcz - Zip failed: %1").arg(errMsg));
            return false;
        }

        Logger::info(QString("ProjectIO::exportToMcz - Successfully exported to: %1").arg(outputMczPath));
        return true;

    } catch (const std::exception& e) {
        Logger::error(QString("ProjectIO::exportToMcz - Exception: %1").arg(e.what()));
        return false;
    }
}

bool ProjectIO::loadChartFile(const QString& filePath, QString& outChartPath)
{
    Logger::info(QString("ProjectIO::loadChartFile - Loading: %1").arg(filePath));
    
    if (!QFile::exists(filePath)) {
        Logger::error(QString("ProjectIO::loadChartFile - File not found: %1").arg(filePath));
        return false;
    }

    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();

    if (suffix == "mcz") {
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            Logger::error("ProjectIO::loadChartFile - Failed to create temp directory");
            return false;
        }

        QString projectName = fileInfo.baseName();
        QString importDir = tempDir.path() + "/" + projectName;

        if (!importMcz(filePath, importDir, outChartPath)) {
            Logger::error(QString("ProjectIO::loadChartFile - Failed to import MCZ: %1").arg(filePath));
            return false;
        }
        
        // 检查是否有多个.mc文件
        QList<QPair<QString, QString>> charts = findChartsInMcz(importDir);
        if (charts.size() > 1) {
            // 有多个谱面，返回列表让caller处理选择
            Logger::info(QString("ProjectIO::loadChartFile - Found %1 charts in MCZ").arg(charts.size()));
        }
        
        Logger::info(QString("ProjectIO::loadChartFile - MCZ imported, chart: %1").arg(outChartPath));
        return true;

    } else if (suffix == "mc") {
        outChartPath = filePath;
        Logger::debug("ProjectIO::loadChartFile - Loading .mc file directly");
        return true;

    } else {
        Logger::error(QString("ProjectIO::loadChartFile - Unknown file format: %1").arg(suffix));
        return false;
    }
}

QList<QPair<QString, QString>> ProjectIO::findChartsInMcz(const QString& extractDir)
{
    QList<QPair<QString, QString>> charts;
    
    Logger::debug(QString("ProjectIO::findChartsInMcz - Scanning: %1").arg(extractDir));
    
    std::function<void(const QString&)> findMcFiles = [&](const QString& dir) {
        QDir d(dir);
        for (const QString& item : d.entryList(QDir::Files)) {
            if (item.endsWith(".mc", Qt::CaseInsensitive)) {
                QString mcPath = d.absoluteFilePath(item);
                QString difficulty = getDifficultyFromMc(mcPath);
                charts.append(qMakePair(mcPath, difficulty));
            }
        }
        for (const QString& item : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            findMcFiles(d.absoluteFilePath(item));
        }
    };
    
    findMcFiles(extractDir);
    Logger::info(QString("ProjectIO::findChartsInMcz - Found %1 charts").arg(charts.size()));
    
    return charts;
}

QString ProjectIO::getDifficultyFromMc(const QString& mcPath)
{
    try {
        QFile file(mcPath);
        if (!file.open(QIODevice::ReadOnly)) {
            Logger::warn(QString("ProjectIO::getDifficultyFromMc - Cannot open: %1").arg(mcPath));
            return "Unknown";
        }

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("meta")) {
                QJsonObject meta = obj["meta"].toObject();
                QString difficulty = meta.value("difficulty").toString();
                return difficulty.isEmpty() ? "Unknown" : difficulty;
            }
        }
    } catch (const std::exception& e) {
        Logger::warn(QString("ProjectIO::getDifficultyFromMc - Exception: %1").arg(e.what()));
    }
    
    return "Unknown";
}

QStringList ProjectIO::collectDependencies(const QString& chartDir)
{
    QStringList dependencies;
    QDir dir(chartDir);
    
    if (!dir.exists()) {
        Logger::warn(QString("ProjectIO::collectDependencies - Directory not found: %1").arg(chartDir));
        return dependencies;
    }

    for (const QString& file : dir.entryList(QDir::Files)) {
        if (!file.endsWith(".mc", Qt::CaseInsensitive)) {
            dependencies << dir.absoluteFilePath(file);
        }
    }

    Logger::debug(QString("ProjectIO::collectDependencies - Found %1 dependencies").arg(dependencies.size()));
    return dependencies;
}

bool ProjectIO::copyDir(const QString& srcDir, const QString& destDir)
{
    QDir src(srcDir);
    QDir dest(destDir);

    if (!dest.exists()) {
        dest.mkpath(".");
    }

    for (const QString& file : src.entryList(QDir::Files)) {
        QString srcFile = src.absoluteFilePath(file);
        QString destFile = dest.absoluteFilePath(file);
        if (!QFile::copy(srcFile, destFile)) {
            Logger::warn(QString("ProjectIO::copyDir - Failed to copy: %1").arg(file));
            return false;
        }
    }

    for (const QString& subDir : src.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString srcSubDir = src.absoluteFilePath(subDir);
        QString destSubDir = dest.absoluteFilePath(subDir);
        if (!copyDir(srcSubDir, destSubDir)) {
            return false;
        }
    }

    return true;
}

bool ProjectIO::removeDir(const QString& dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        return true;
    }

    for (const QString& file : dir.entryList(QDir::Files)) {
        if (!dir.remove(file)) {
            Logger::warn(QString("ProjectIO::removeDir - Failed to remove file: %1").arg(file));
            return false;
        }
    }

    for (const QString& subDir : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!removeDir(dir.absoluteFilePath(subDir))) {
            return false;
        }
    }

    return dir.rmdir(dirPath);
}