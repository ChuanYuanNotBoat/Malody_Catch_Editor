#pragma once

#include <QWidget>

class QPushButton;
class QButtonGroup;

class SpeedPopup : public QWidget {
    Q_OBJECT
public:
    explicit SpeedPopup(QWidget* parent = nullptr);
    void setSpeed(double speed);
    double speed() const { return m_currentSpeed; }

signals:
    void speedChanged(double speed);

private slots:
    void onSpeedSelected(int id);

private:
    QButtonGroup* m_buttonGroup;
    double m_currentSpeed;
};