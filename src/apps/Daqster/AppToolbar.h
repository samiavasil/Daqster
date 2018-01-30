#ifndef APPTOOLBAR_H
#define APPTOOLBAR_H

#include<QToolBar>


class AppToolbar : public QToolBar
{
    Q_OBJECT

public:
    explicit AppToolbar(QWidget *parent = 0);
    ~AppToolbar();
private:
};

#endif // APPTOOLBAR_H
