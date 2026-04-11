#include "SpeedPopup.h"
#include <QHBoxLayout>
#include <QRadioButton>
#include <QButtonGroup>

SpeedPopup::SpeedPopup(QWidget *parent)
    : QWidget(parent), m_currentSpeed(1.0)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    m_buttonGroup = new QButtonGroup(this);
    QList<double> speeds = {0.25, 0.5, 0.75, 1.0};
    for (double sp : speeds)
    {
        QRadioButton *btn = new QRadioButton(tr("%1x").arg(sp), this);
        btn->setCheckable(true);
        m_buttonGroup->addButton(btn, static_cast<int>(sp * 100));
        layout->addWidget(btn);
        if (qFuzzyCompare(sp, m_currentSpeed))
            btn->setChecked(true);
    }
    connect(m_buttonGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, &SpeedPopup::onSpeedSelected);
    setWindowFlags(Qt::Popup);
}

void SpeedPopup::setSpeed(double speed)
{
    m_currentSpeed = speed;
    int id = static_cast<int>(speed * 100);
    if (QAbstractButton *btn = m_buttonGroup->button(id))
        btn->setChecked(true);
    else
        m_buttonGroup->setExclusive(false);
}

void SpeedPopup::onSpeedSelected(int id)
{
    m_currentSpeed = id / 100.0;
    emit speedChanged(m_currentSpeed);
    close();
}