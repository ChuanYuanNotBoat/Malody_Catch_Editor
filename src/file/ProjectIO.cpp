#include "ProjectIO.h"
#include "utils/Logger.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QProcessEnvironment>

// 递归复制目录
static bool copyDir(const QString &srcDir, const QString &destDir)
{
    QDir src(srcDir);
    QDir dest(destDir);

    if (!dest.exists())
    {
        dest.mkpath(".");
    }

    for (const QString &file : src.entryList(QDir::Files))
    {
        QString srcFile = src.absoluteFilePath(file);
        QString destFile = dest.absoluteFilePath(file);
        if (!QFile::copy(srcFile, destFile))
        {
            Logger::warn(QString("copyDir - Failed to copy: %1").arg(file));
            return false;
        }
    }

    for (const QString &subDir : src.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        QString srcSubDir = src.absoluteFilePath(subDir);
        QString destSubDir = dest.absoluteFilePath(subDir);
        if (!copyDir(srcSubDir, destSubDir))
        {
            return false;
        }
    }

    return true;
}

bool ProjectIO::extractMcz(const QString &mczPath, const QString &outputDir, QString &outExtractedDir)
{
    Logger::info(QString("ProjectIO::extractMcz - Extracting %1 to %2").arg(mczPath, outputDir));

    QDir outDir(outputDir);
    if (!outDir.exists() && !outDir.mkpath("."))
    {
        Logger::error(QString("ProjectIO::extractMcz - Failed to create output directory: %1").arg(outputDir));
        return false;
    }

    if (!QFile::exists(mczPath))
    {
        Logger::error(QString("ProjectIO::extractMcz - Source file does not exist: %1").arg(mczPath));
        return false;
    }

    QProcess process;
    process.setWorkingDirectory(outputDir);

#ifdef Q_OS_WIN
    QTemporaryDir tempZipDir;
    if (!tempZipDir.isValid())
    {
        Logger::error("ProjectIO::extractMcz - Failed to create temporary directory for ZIP conversion");
        return false;
    }

    const QString tempZipPath = QDir(tempZipDir.path()).filePath("mcz_extract.zip");

    if (!QFile::copy(mczPath, tempZipPath))
    {
        Logger::error(QString("ProjectIO::extractMcz - Failed to copy MCZ to temporary ZIP: src=%1, dst=%2")
                          .arg(mczPath, tempZipPath));
        return false;
    }

    // 固定命令 + 位置参数，避免命令拼接带来的 PowerShell 注入问题。
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("MALODY_MCZ_TEMP_ZIP", QDir::toNativeSeparators(tempZipPath));
    env.insert("MALODY_MCZ_OUTPUT_DIR", QDir::toNativeSeparators(outputDir));
    process.setProcessEnvironment(env);

    QStringList args;
    args << "-NoProfile"
         << "-NonInteractive"
         << "-Command"
         << "$ErrorActionPreference='Stop'; "
            "$zip = $env:MALODY_MCZ_TEMP_ZIP; "
            "$dst = $env:MALODY_MCZ_OUTPUT_DIR; "
            "if ([string]::IsNullOrWhiteSpace($zip) -or [string]::IsNullOrWhiteSpace($dst)) "
            "{ throw 'Missing extraction paths in environment.' }; "
            "Expand-Archive -LiteralPath $zip -DestinationPath $dst -Force; "
            "Remove-Item -LiteralPath $zip -Force";
    process.start("powershell.exe", args);
#else
    process.start("unzip", QStringList() << "-o" << mczPath << "-d" << outputDir);
#endif

    if (!process.waitForFinished(30000))
    {
        Logger::error("ProjectIO::extractMcz - Extraction timeout");
        process.kill();
#ifdef Q_OS_WIN
        QFile::remove(tempZipPath);
#endif
        return false;
    }

    if (process.exitCode() != 0)
    {
        QString errMsg = process.readAllStandardError();
        Logger::error(QString("ProjectIO::extractMcz - Extraction failed: %1").arg(errMsg));
#ifdef Q_OS_WIN
        QFile::remove(tempZipPath);
#endif
        return false;
    }

#ifdef Q_OS_WIN
    if (QFile::exists(tempZipPath))
        QFile::remove(tempZipPath);
#endif

    outExtractedDir = outputDir;
    Logger::info(QString("ProjectIO::extractMcz - Successfully extracted to %1").arg(outputDir));
    return true;
}

bool ProjectIO::exportToMcz(const QString &outputMczPath, const QString &sourceChartPath)
{
    Logger::info(QString("ProjectIO::exportToMcz - Exporting to %1 from %2").arg(outputMczPath, sourceChartPath));

    if (!QFile::exists(sourceChartPath))
    {
        Logger::error(QString("ProjectIO::exportToMcz - Chart file not found: %1").arg(sourceChartPath));
        return false;
    }

    QString chartDir = QFileInfo(sourceChartPath).absolutePath();

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
    {
        Logger::error("ProjectIO::exportToMcz - Failed to create temporary directory");
        return false;
    }

    QString packDir = tempDir.path() + "/mczpack";
    QDir().mkpath(packDir);

    // 复制整个目录内容到打包目录
    if (!copyDir(chartDir, packDir))
    {
        Logger::error("ProjectIO::exportToMcz - Failed to copy chart directory");
        return false;
    }

    QString tempZipPath = tempDir.path() + "/output.zip";
    QProcess process;

#ifdef Q_OS_WIN
    // 固定命令 + 位置参数，避免命令拼接带来的 PowerShell 注入问题。
    QStringList args;
    args << "-NoProfile"
         << "-NonInteractive"
         << "-Command"
         << "$ErrorActionPreference='Stop'; "
            "$items = Get-ChildItem -LiteralPath $args[0] -Force; "
            "$items | Compress-Archive -DestinationPath $args[1] -CompressionLevel Optimal -Force"
         << QDir::toNativeSeparators(packDir)
         << QDir::toNativeSeparators(tempZipPath);
    process.start("powershell.exe", args);
#else
    process.setWorkingDirectory(packDir);
    process.start("zip", QStringList() << "-r" << "-q" << tempZipPath << ".");
#endif

    if (!process.waitForFinished(60000))
    {
        Logger::error("ProjectIO::exportToMcz - Zip process timeout");
        process.kill();
        return false;
    }

    if (process.exitCode() != 0)
    {
        QString errMsg = process.readAllStandardError();
        Logger::error(QString("ProjectIO::exportToMcz - Zip failed: %1").arg(errMsg));
        return false;
    }

    // 移除已存在的输出文件
    if (QFile::exists(outputMczPath))
    {
        QFile::remove(outputMczPath);
    }

    if (!QFile::rename(tempZipPath, outputMczPath))
    {
        Logger::error("ProjectIO::exportToMcz - Failed to rename zip to mcz");
        return false;
    }

    Logger::info("ProjectIO::exportToMcz - Export successful");
    return true;
}

QList<QPair<QString, QString>> ProjectIO::findChartsInDirectory(const QString &dirPath)
{
    QList<QPair<QString, QString>> charts;
    QDir dir(dirPath);
    if (!dir.exists())
        return charts;

    // 扫描当前目录下的 .mc 文件
    QStringList mcFiles = dir.entryList(QStringList() << "*.mc", QDir::Files);
    for (const QString &file : mcFiles)
    {
        QString fullPath = dir.absoluteFilePath(file);
        QString difficulty = getDifficultyFromMc(fullPath);
        if (difficulty.isEmpty())
            difficulty = QFileInfo(fullPath).baseName();
        charts.append(qMakePair(fullPath, difficulty));
    }

    // 递归子目录
    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &subDir : subDirs)
    {
        charts.append(findChartsInDirectory(dir.absoluteFilePath(subDir)));
    }

    return charts;
}

QString ProjectIO::getDifficultyFromMc(const QString &mcPath)
{
    QFile file(mcPath);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (doc.isObject())
    {
        QJsonObject root = doc.object();
        QJsonObject meta = root.value("meta").toObject();
        // Malody 使用 "version" 字段存储难度名
        return meta.value("version").toString();
    }
    return QString();
}
