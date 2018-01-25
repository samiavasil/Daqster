#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>


namespace Ui {
class MainWindow;
}


class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    virtual ~MainWindow();
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
signals:
    void copyAvailable(bool);
    void undoAvailable(bool);
    void redoAvailable(bool);
    void pasteAvailable(bool);
protected:
    virtual void mouseMoveEvent ( QMouseEvent * event );
protected:


private:
    Ui::MainWindow *ui;

};

#endif // MAINWINDOW_H
