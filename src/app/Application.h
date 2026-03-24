#pragma once

#include <QApplication>

class ChartController;
class SelectionController;
class PlaybackController;
class PluginManager;
class Skin;
class MainWindow;

class Application : public QApplication {
    Q_OBJECT
public:
    Application(int& argc, char** argv);
    ~Application();

    ChartController* chartController() const { return m_chartController; }
    SelectionController* selectionController() const { return m_selectionController; }
    PlaybackController* playbackController() const { return m_playbackController; }
    PluginManager* pluginManager() const { return m_pluginManager; }
    Skin* skin() const { return m_skin; }
    MainWindow* mainWindow() const { return m_mainWindow; }

    bool initialize();
    void loadLastProject();
    void loadLanguage();

private:
    ChartController* m_chartController;
    SelectionController* m_selectionController;
    PlaybackController* m_playbackController;
    PluginManager* m_pluginManager;
    Skin* m_skin;
    MainWindow* m_mainWindow;
};