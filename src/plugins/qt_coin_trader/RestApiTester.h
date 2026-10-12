#ifndef RESTAPITESTER_H
#define RESTAPITESTER_H
#include<QNetworkReply>
#include <QDialog>

namespace Ui {
class RestApiTester;
}
class QWebEngineView;
class RestApiTester : public QDialog
{
    Q_OBJECT

public:
    explicit RestApiTester(QWidget *parent = 0);
    ~RestApiTester();

protected slots:
    void finished();
    void error(const QNetworkReply::NetworkError &Error);
    void redirected(const QUrl &url);
private slots:
    void on_sendButton_clicked();

private:
    Ui::RestApiTester *ui;
    QWebEngineView* m_webEngine;
};

#endif // RESTAPITESTER_H
