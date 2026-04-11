#pragma once

#include <QWidget>

class Chart;

class DensityCurve : public QWidget
{
    Q_OBJECT
public:
    explicit DensityCurve(QWidget *parent = nullptr);
    void setChart(const Chart *chart);

signals:
    void timeClicked(double timeMs);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void computeDensity();

    const Chart *m_chart;
    QVector<double> m_densityData; // per-pixel density
};