#include "BPMTimePanel.h"
#include "controller/ChartController.h"
#include "model/BpmEntry.h"
#include <QListWidget>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QStringList>

BPMTimePanel::BPMTimePanel(QWidget* parent)
    : RightPanel(parent), m_chartController(nullptr), m_selectedIndex(-1)
{
    setupUi();
}

void BPMTimePanel::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // BPM 列表
    m_bpmListWidget = new QListWidget(this);
    mainLayout->addWidget(m_bpmListWidget);
    connect(m_bpmListWidget, &QListWidget::currentRowChanged, this, &BPMTimePanel::onItemSelected);

    // 编辑区域
    QHBoxLayout* timeLayout = new QHBoxLayout;
    timeLayout->addWidget(new QLabel(tr("Time:")));
    m_timeEdit = new QLineEdit(this);
    m_timeEdit->setPlaceholderText("0:1/1");
    timeLayout->addWidget(m_timeEdit);
    mainLayout->addLayout(timeLayout);

    QHBoxLayout* bpmLayout = new QHBoxLayout;
    bpmLayout->addWidget(new QLabel(tr("BPM:")));
    m_bpmSpin = new QDoubleSpinBox(this);
    m_bpmSpin->setRange(1, 999);
    m_bpmSpin->setDecimals(3);
    m_bpmSpin->setValue(120);
    bpmLayout->addWidget(m_bpmSpin);
    mainLayout->addLayout(bpmLayout);

    QHBoxLayout* btnLayout = new QHBoxLayout;
    m_addBtn = new QPushButton(tr("Add/Update"), this);
    m_removeBtn = new QPushButton(tr("Remove"), this);
    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_removeBtn);
    mainLayout->addLayout(btnLayout);

    mainLayout->addStretch();

    connect(m_addBtn, &QPushButton::clicked, this, &BPMTimePanel::onAddClicked);
    connect(m_removeBtn, &QPushButton::clicked, this, &BPMTimePanel::onRemoveClicked);
    connect(m_bpmSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BPMTimePanel::onBpmChanged);
}

void BPMTimePanel::refreshBpmList()
{
    if (!m_chartController) return;
    m_bpmListWidget->clear();
    const auto& bpmList = m_chartController->chart()->bpmList();
    for (int i = 0; i < bpmList.size(); ++i) {
        const BpmEntry& bpm = bpmList[i];
        QString text = QString("%1:%2/%3\t%4")
            .arg(bpm.beatNum)
            .arg(bpm.numerator)
            .arg(bpm.denominator)
            .arg(bpm.bpm, 0, 'f', 3);
        m_bpmListWidget->addItem(text);
    }
}

void BPMTimePanel::onItemSelected(int row)
{
    if (row < 0) {
        m_selectedIndex = -1;
        m_timeEdit->clear();
        m_bpmSpin->setValue(120);
        return;
    }
    m_selectedIndex = row;
    const auto& bpmList = m_chartController->chart()->bpmList();
    if (row < bpmList.size()) {
        const BpmEntry& bpm = bpmList[row];
        m_timeEdit->setText(QString("%1:%2/%3").arg(bpm.beatNum).arg(bpm.numerator).arg(bpm.denominator));
        m_bpmSpin->setValue(bpm.bpm);
    }
}

void BPMTimePanel::onAddClicked()
{
    if (!m_chartController) return;
    // 解析时间
    QString timeStr = m_timeEdit->text();
    int beat = 0, num = 1, den = 1;
    if (timeStr.contains(':')) {
        QStringList parts = timeStr.split(':');
        if (parts.size() >= 2) {
            beat = parts[0].toInt();
            QString fraction = parts[1];
            if (fraction.contains('/')) {
                QStringList fracParts = fraction.split('/');
                if (fracParts.size() == 2) {
                    num = fracParts[0].toInt();
                    den = fracParts[1].toInt();
                }
            } else {
                num = fraction.toInt();
                den = 1;
            }
        }
    }
    BpmEntry newBpm(beat, num, den, m_bpmSpin->value());
    if (m_selectedIndex >= 0) {
        m_chartController->updateBpm(m_selectedIndex, newBpm);
        m_selectedIndex = -1;
    } else {
        m_chartController->addBpm(newBpm);
    }
    refreshBpmList();
    m_timeEdit->clear();
    m_bpmSpin->setValue(120);
}

void BPMTimePanel::onRemoveClicked()
{
    if (!m_chartController) return;
    if (m_selectedIndex >= 0) {
        m_chartController->removeBpm(m_selectedIndex);
        m_selectedIndex = -1;
        refreshBpmList();
    }
}

void BPMTimePanel::onBpmChanged(double)
{
    // 可实时预览，但暂时不做
}

void BPMTimePanel::setChartController(ChartController* controller)
{
    m_chartController = controller;
    connect(m_chartController, &ChartController::chartChanged, this, &BPMTimePanel::refreshBpmList);
    refreshBpmList();
}

void BPMTimePanel::setSelectionController(SelectionController* controller)
{
    Q_UNUSED(controller);
}