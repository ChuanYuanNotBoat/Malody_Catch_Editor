#include "ProjectIO.h"
#include "utils/Logger.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QDateTime> // 用于生成安全临时文件名

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
    // 1. 复制 MCZ 到安全名称的临时 ZIP（Qt 能处理特殊字符）
    QString tempZipName = "temp_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".zip";
    QString tempZipPath = QDir::toNativeSeparators(outputDir + "/" + tempZipName);

    if (!QFile::copy(mczPath, tempZipPath))
    {
        Logger::error("ProjectIO::extractMcz - Failed to copy MCZ to temporary ZIP");
        return false;
    }

    // 2. 创建临时 PowerShell 脚本，避免命令行解析问题
    QString scriptPath = outputDir + "/extract.ps1";
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        Logger::error("ProjectIO::extractMcz - Failed to create extraction script");
        QFile::remove(tempZipPath);
        return false;
    }

    QTextStream out(&scriptFile);
    // 对路径中的单引号进行转义（PowerShell 中单引号字符串内的单引号需写为两个）
    QString safeZipPath = tempZipPath;
    safeZipPath.replace('\'', "''");
    QString safeOutputDir = QDir::toNativeSeparators(outputDir);
    safeOutputDir.replace('\'', "''");

    out << QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force\n")
               .arg(safeZipPath, safeOutputDir);
    out << QString("Remove-Item '%1'\n").arg(safeZipPath);
    scriptFile.close();

    // 3. 执行脚本
    QStringList args;
    args << "-NoProfile" << "-NonInteractive" << "-File" << QDir::toNativeSeparators(scriptPath);
    process.start("powershell.exe", args);
#else
    process.start("unzip", QStringList() << "-o" << mczPath << "-d" << outputDir);
#endif

    if (!process.waitForFinished(30000))
    {
        Logger::error("ProjectIO::extractMcz - Extraction timeout");
        process.kill();
#ifdef Q_OS_WIN
        QFile::remove(scriptPath);
        QFile::remove(tempZipPath);
#endif
        return false;
    }

    if (process.exitCode() != 0)
    {
        QString errMsg = process.readAllStandardError();
        Logger::error(QString("ProjectIO::extractMcz - Extraction failed: %1").arg(errMsg));
#ifdef Q_OS_WIN
        QFile::remove(scriptPath);
        QFile::remove(tempZipPath);
#endif
        return false;
    }

#ifdef Q_OS_WIN
    // 清理临时文件
    QFile::remove(scriptPath);
    // tempZipPath 已由脚本中的 Remove-Item 删除，但为确保，再次检查
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
    // 同样使用脚本方式避免转义问题
    QString scriptPath = tempDir.path() + "/compress.ps1";
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        Logger::error("ProjectIO::exportToMcz - Failed to create compression script");
        return false;
    }

    QTextStream out(&scriptFile);
    QString safePackDir = QDir::toNativeSeparators(packDir);
    safePackDir.replace('\'', "''");
    QString safeTempZip = QDir::toNativeSeparators(tempZipPath);
    safeTempZip.replace('\'', "''");
    out << QString("Compress-Archive -Path '%1\\*' -DestinationPath '%2' -CompressionLevel Optimal -Force\n")
               .arg(safePackDir, safeTempZip);
    scriptFile.close();

    QStringList args;
    args << "-NoProfile" << "-NonInteractive" << "-File" << QString("\"%1\"").arg(scriptPath);
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