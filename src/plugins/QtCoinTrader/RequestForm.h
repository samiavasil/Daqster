#ifndef REQUESTFORM_H
#define REQUESTFORM_H

#include <QWidget>
#include<QList>


class QNetworkReply;
class QNetworkAccessManager;
class QNetworkRequest;
class QHttpMultiPart;

namespace Ui {
class RequestForm;
}

class RequestForm : public QWidget
{
    Q_OBJECT

public:
    explicit RequestForm( QNetworkAccessManager* netMng,QWidget *parent = 0);
    ~RequestForm();

protected:
    typedef enum{
        E_GET,
        E_POST,
        E_PUT,
        E_DELETE,
        E_HEAD,
        E_OPTIONS,
        E_PATCH
    }reqEnum_t;

    QNetworkReply *sendRequest(const QNetworkRequest &request, const reqEnum_t &reqType, QHttpMultiPart *multiPart);
    void timerEvent(QTimerEvent *event);
protected slots:
    void activateRequest( bool activate );
    void httpFinished();
    void httpReadyRead();
    void updateDataReadProgress(qint64 a, qint64 b);
    void redirected(const QUrl &url);
private:
    Ui::RequestForm *ui;
    QNetworkAccessManager* m_netMng;
    QList<QNetworkReply*> m_netRereplyList;
    int m_timerID;
};

#endif // REQUESTFORM_H
