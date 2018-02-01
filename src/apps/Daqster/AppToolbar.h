#ifndef APPTOOLBAR_H
#define APPTOOLBAR_H

#include<QToolBar>


class AppToolbar : public QToolBar
{
    Q_OBJECT

public:
    explicit AppToolbar(QWidget *parent = 0);
    ~AppToolbar();

private slots:
    void OnActionTrigered();
signals:
    void PleaseRunApplication( QString );

private:
};

#endif // APPTOOLBAR_H
