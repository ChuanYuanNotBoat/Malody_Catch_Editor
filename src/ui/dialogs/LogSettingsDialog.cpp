#include "LogSettingsDialog.h"
#include "utils/Logger.h"
#include "utils/PerformanceTimer.h"
#include "utils/DiagnosticCollector.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QCloseEvent>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>

LogSettingsDialog::LogSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("日志设置"));
    setModal(true);
    setMinimumWidth(500);
    setupUI();
    loadSettings();
}

void LogSettingsDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 日志级别组
    QGroupBox *levelGroup = new QGroupBox(tr("日志级别"), this);
    QVBoxLayout *levelLayout = new QVBoxLayout(levelGroup);

    QHBoxLayout *levelHLayout = new QHBoxLayout();
    levelHLayout->addWidget(new QLabel(tr("当前级别：")));
    m_logLevelCombo = new QComboBox();
    m_logLevelCombo->addItem(tr("DEBUG - 详细调试信息"), 0);
    m_logLevelCombo->addItem(tr("INFO - 一般信息"), 1);
    m_logLevelCombo->addItem(tr("WARN - 警告信息"), 2);
    m_logLevelCombo->addItem(tr("ERROR - 错误信息"), 3);
    levelHLayout->addWidget(m_logLevelCombo);
    levelLayout->addLayout(levelHLayout);
    mainLayout->addWidget(levelGroup);

    // 功能选项组
    QGroupBox *featuresGroup = new QGroupBox(tr("功能选项"), this);
    QVBoxLayout *featuresLayout = new QVBoxLayout(featuresGroup);

    m_verboseModeCheckbox = new QCheckBox(tr("详细日志模式 - 记录每个操作的详细信息"));
    m_jsonLoggingCheckbox = new QCheckBox(tr("JSON结构化日志 - 便于自动化分析"));
    m_performanceTimingCheckbox = new QCheckBox(tr("性能计时 - 记录各操作的耗时"));

    featuresLayout->addWidget(m_verboseModeCheckbox);
    featuresLayout->addWidget(m_jsonLoggingCheckbox);
    featuresLayout->addWidget(m_performanceTimingCheckbox);
    mainLayout->addWidget(featuresGroup);

    // 操作按钮组
    QGroupBox *actionsGroup = new QGroupBox(tr("操作"), this);
    QVBoxLayout *actionsLayout = new QVBoxLayout(actionsGroup);

    m_showLogButton = new QPushButton(tr("查看当前日志文件"));
    m_clearLogsButton = new QPushButton(tr("清除诊断数据"));
    m_exportButton = new QPushButton(tr("导出诊断报告"));

    actionsLayout->addWidget(m_showLogButton);
    actionsLayout->addWidget(m_clearLogsButton);
    actionsLayout->addWidget(m_exportButton);
    mainLayout->addWidget(actionsGroup);

    // 对话框按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_okButton = new QPushButton(tr("确定"));
    m_cancelButton = new QPushButton(tr("取消"));
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(buttonLayout);

    // 连接信号
    connect(m_verboseModeCheckbox, QOverload<int>::of(&QCheckBox::stateChanged),
            this, &LogSettingsDialog::onVerboseModeChanged);
    connect(m_jsonLoggingCheckbox, QOverload<int>::of(&QCheckBox::stateChanged),
            this, &LogSettingsDialog::onJsonLoggingChanged);
    connect(m_performanceTimingCheckbox, QOverload<int>::of(&QCheckBox::stateChanged),
            this, &LogSettingsDialog::onPerformanceTimingChanged);
    connect(m_logLevelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LogSettingsDialog::onLogLevelChanged);
    connect(m_clearLogsButton, &QPushButton::clicked,
            this, &LogSettingsDialog::onClearLogsClicked);
    connect(m_showLogButton, &QPushButton::clicked,
            this, &LogSettingsDialog::onShowLogDialogClicked);
    connect(m_exportButton, &QPushButton::clicked,
            this, &LogSettingsDialog::onExportDiagnosticsClicked);
    connect(m_okButton, &QPushButton::clicked,
            this, &QDialog::accept);
    connect(m_cancelButton, &QPushButton::clicked,
            this, &QDialog::reject);
}

void LogSettingsDialog::loadSettings()
{
    m_verboseModeCheckbox->setChecked(Logger::isVerbose());
    m_jsonLoggingCheckbox->setChecked(Logger::isJsonLoggingEnabled());
    m_performanceTimingCheckbox->setChecked(PerformanceTimer::isEnabled());
}

void LogSettingsDialog::saveSettings()
{
    Logger::setVerbose(m_verboseModeCheckbox->isChecked());
    Logger::setJsonLoggingEnabled(m_jsonLoggingCheckbox->isChecked());
    PerformanceTimer::setEnabled(m_performanceTimingCheckbox->isChecked());
}

void LogSettingsDialog::onVerboseModeChanged(int state)
{
    Logger::setVerbose(state == Qt::Checked);
    Logger::info(QString("日志：详细模式 %1").arg(state == Qt::Checked ? "启用" : "禁用"));
}

void LogSettingsDialog::onJsonLoggingChanged(int state)
{
    Logger::setJsonLoggingEnabled(state == Qt::Checked);
    Logger::info(QString("日志：JSON输出 %1").arg(state == Qt::Checked ? "启用" : "禁用"));
}

void LogSettingsDialog::onPerformanceTimingChanged(int state)
{
    PerformanceTimer::setEnabled(state == Qt::Checked);
    Logger::info(QString("日志：性能计时 %1").arg(state == Qt::Checked ? "启用" : "禁用"));
}

void LogSettingsDialog::onLogLevelChanged(int index)
{
    // 此功能为预留，当前Logger不支持级别过滤
    // 可在未来扩展Logger来支持此功能
    Logger::info(QString("日志级别已选择（功能预留）"));
}

void LogSettingsDialog::onClearLogsClicked()
{
    int ret = QMessageBox::question(this, tr("确认清除"),
                                    tr("确定要清除所有诊断数据吗？"),
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes)
    {
        DiagnosticCollector::instance().clear();
        PerformanceTimer::clearStatistics();
        Logger::info("诊断数据已清除");
        QMessageBox::information(this, tr("成功"), tr("诊断数据已清除"));
    }
}

void LogSettingsDialog::onShowLogDialogClicked()
{
    QString logPath = Logger::logFilePath();
    if (logPath.isEmpty())
    {
        QMessageBox::warning(this, tr("错误"), tr("没有活跃的日志文件"));
        return;
    }

    // 在资源管理器中打开日志文件所在目录
    QDesktopServices::openUrl(QUrl::fromLocalFile(logPath));
}

void LogSettingsDialog::onExportDiagnosticsClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("导出诊断报告"), "",
                                                    tr("JSON文件 (*.json);;文本文件 (*.txt)"));

    if (fileName.isEmpty())
    {
        return;
    }

    auto &diagnostic = DiagnosticCollector::instance();
    auto report = diagnostic.generateReport();

    if (fileName.endsWith(".json"))
    {
        auto jsonDoc = diagnostic.toJsonDocument();
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            file.write(jsonDoc.toJson());
            file.close();
            QMessageBox::information(this, tr("成功"),
                                     tr("诊断报告已导出到：%1").arg(fileName));
        }
        else
        {
            QMessageBox::warning(this, tr("错误"), tr("无法保存文件"));
        }
    }
    else
    {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            file.write(report.toFormattedString().toUtf8());
            file.write("\n\n");
            PerformanceTimer::logAllStatistics();
            file.close();
            QMessageBox::information(this, tr("成功"),
                                     tr("诊断报告已导出到：%1").arg(fileName));
        }
        else
        {
            QMessageBox::warning(this, tr("错误"), tr("无法保存文件"));
        }
    }
}

void LogSettingsDialog::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QDialog::closeEvent(event);
}
