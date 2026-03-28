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
    explicit MainWindow(ChartController* chartCtrl,
                        SelectionController* selCtrl,
                        PlaybackController* playCtrl,
                        Skin* skin,
                        QWidget* parent = nullptr);
    ~MainWindow();

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
    void changeSkin(const QString& skinName);
    void adjustNoteSize();                       // 改为打开对话框
    void calibrateSkin();                        // 新增：皮肤校准
    void configureOutline();                     // 新增：描边设置

private:
    void setupUi();
    void createMenus();
    void createCentralArea();
    void retranslateUi();
    void populateSkinMenu();

    class Private;
    Private* d;
};