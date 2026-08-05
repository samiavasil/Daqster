#ifndef APPSELECTIONDIALOG_H
#define APPSELECTIONDIALOG_H

#include <QDialog>
#include <QSettings>

class QTreeWidget;
class QTreeWidgetItem;
class QComboBox;

class AppSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AppSelectionDialog(QWidget *parent = nullptr);
    ~AppSelectionDialog();

    void LoadPlugins();
    bool IsPluginVisible(const QString& pluginName) const;

public slots:
    void SaveState();
    void SelectAll();
    void DeselectAll();

protected:
    void showEvent(QShowEvent* e) override;

private:
    void RestoreState();

    QTreeWidget* m_tree;
    QComboBox*   m_themeCombo;
    QSettings    m_settings;
};

#endif // APPSELECTIONDIALOG_H
