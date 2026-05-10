#pragma once

#include <QDialog>
#include <QVector>
#include "plugin/PluginManager.h"

class QLabel;
class QPushButton;
class QTableWidget;
class QTextEdit;
class QTableWidgetItem;

class PluginManagerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PluginManagerDialog(PluginManager *pluginManager, QWidget *parent = nullptr);

signals:
    void pluginsReloading();
    void pluginsReloaded();

private slots:
    void reloadPlugins();
    void reloadSelectedPlugin();
    void openPluginsFolder();
    void updateDetails();
    void onItemChanged(QTableWidgetItem *item);

private:
    void rebuildTable();
    QString selectedPluginId() const;
    QString safeText(const QString &value) const;

private:
    PluginManager *m_pluginManager = nullptr;
    QVector<PluginManager::PluginInfo> m_infos;
    bool m_updatingTable = false;
    QLabel *m_summaryLabel = nullptr;
    QTableWidget *m_table = nullptr;
    QTextEdit *m_details = nullptr;
    QPushButton *m_reloadSelectedButton = nullptr;
    QPushButton *m_reloadButton = nullptr;
    QPushButton *m_openFolderButton = nullptr;
};
