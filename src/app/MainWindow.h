#pragma once

#include <QMainWindow>

class ChartController;
class SelectionController;
class PlaybackController;
class Skin;
class RightPanel;
class QSplitter;
class QMenu;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // 构造函数，增加 Skin 参数用于皮肤切换
    explicit MainWindow(ChartController* chartCtrl,
                        SelectionController* selCtrl,
                        PlaybackController* playCtrl,
                        Skin* skin,
                        QWidget* parent = nullptr);
    ~MainWindow();

    // 切换皮肤（由外部调用）
    void setSkin(Skin* skin);

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void openChart();
    void saveChart();
    void saveChartAs();
    void exportMcz();
    void undo();
    void redo();
    void toggleColorMode(bool on);
    void toggleHyperfruitMode(bool on);
    void togglePlayback();
    void changeSkin(const QString& skinName);  // 皮肤菜单槽函数

private:
    void setupUi();
    void createMenus();
    void createCentralArea();
    void retranslateUi();
    void populateSkinMenu();  // 填充皮肤菜单

    class Private;
    Private* d;
};