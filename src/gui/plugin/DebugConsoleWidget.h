#ifndef DEBUGCONSOLEWIDGET_H
#define DEBUGCONSOLEWIDGET_H

#include "build_cfg.h"
#include <QWidget>

namespace Daqster {

class DAQSTER_GUI_EXPORT DebugConsoleWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DebugConsoleWidget(QWidget *parent = nullptr);
    ~DebugConsoleWidget();

private slots:
    void onEnableAllToggled(bool checked);
    void onCategoryToggled(const QString &category, bool checked);
    void onFileToggled(bool checked);
    void onBrowseClicked();
    void onResetClicked();

private:
    void setupUi();
    void loadCurrentState();

    struct Private;
    Private *d;
};

} // namespace Daqster

#endif // DEBUGCONSOLEWIDGET_H