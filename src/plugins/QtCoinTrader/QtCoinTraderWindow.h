#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>


namespace Ui {
class MainWindow;
}


class QtCoinTraderWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit QtCoinTraderWindow(QWidget *parent = 0);
    virtual ~QtCoinTraderWindow();
    void closeEvent(QCloseEvent *event);

private slots:
    void on_actionNew_triggered();

    void on_actionFullScreen_triggered(bool checked);

    void on_actionHideToolbar_triggered(bool checked);

    void on_actionHideMainMenu_triggered(bool checked);

    void on_actionSave_triggered();


    void onUndoAvailable();

    void onRedoAvailable();

    void onCopyAvailable();

    void onPasteAvailable();
    void on_actionOpen_triggered();

    void RunApplication(const QString &AppName );
signals:
    void copyAvailable(bool);
    void undoAvailable(bool);
    void redoAvailable(bool);
    void pasteAvailable(bool);
protected:
    virtual void mouseMoveEvent ( QMouseEvent * event );
protected slots:
    void CursorShow();

private:
    Ui::MainWindow *ui;

};

#endif // MAINWINDOW_H
