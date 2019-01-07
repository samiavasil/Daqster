#include "RequestForm.h"
#include "ui_RequestForm.h"
#include<QNetworkAccessManager>
#include<QNetworkReply>
#include<QHttpMultiPart>


#include<QDebug>
#define TIMER_MS 10000

#define DEBUG  DBB("//")
//qDebug() << __FILE__ << " Line: " << __LINE__ << " "
#define DEFAULT_ADDR "localhost:8001/radio/stations"

static const char* servType[] = {
    "http",
    "https"
};



static const char* reqType[] = {
    "GET",
    "POST",
    "PUT",
    "DELETE",
    "HEAD",
    "OPTIONS",
    "PATCH"
};


RequestForm::RequestForm(QNetworkAccessManager *netMng, QWidget *parent) :m_netMng(netMng),QWidget(parent), ui(new Ui::RequestForm)
{
    m_timerID = -1;
    ui->setupUi(this);
    for( int i = 0; i < sizeof( servType )/sizeof( servType[0] ); i++ )
    {
        ui->comboServType->addItem( tr("%1").arg(servType[i]).toUpper() );
    }

    ui->lineEditAddr->setText( DEFAULT_ADDR );
    for( int i = 0; i < sizeof( reqType )/sizeof( reqType[0] ); i++ )
    {
        ui->comboReqType->addItem( tr("%1").arg(reqType[i]).toUpper() );
    }
    connect(ui->toolButtonActivate, SIGNAL(clicked(bool)), this, SLOT(activateRequest(bool)));
}


RequestForm::~RequestForm()
{
    delete ui;
}


void RequestForm::redirected(const QUrl& url ){
    qDebug() << "Redirected to:" << url.toString();
}

void RequestForm::activateRequest(bool activate )
{
    if( activate )
    {
        QHttpMultiPart multiPart;
        QString url(ui->comboServType->currentText() + "://" + ui->lineEditAddr->text());
        QNetworkRequest req(url);
        reqEnum_t type = (reqEnum_t)ui->comboReqType->currentIndex();
        req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        QNetworkReply* reply = sendRequest( req,type , &multiPart );

        connect(reply,SIGNAL(redirected(QUrl)),this,SLOT(redirected(QUrl)));
        m_timerID = startTimer( TIMER_MS );
        m_netRereplyList.append( reply );
    }
    else
    {
        killTimer( m_timerID );
        QMutableListIterator<QNetworkReply*> i(m_netRereplyList);
        while (i.hasNext()) {
            QNetworkReply* reply = i.next();
            if( reply )
            {
                reply->abort();
                reply->deleteLater();
            }
            i.remove();
        }
    }
}

void RequestForm::timerEvent( QTimerEvent *event )
{
//    foreach ( req, m_netRereplyList ) {
//      sendRequest( req,(reqEnum_t)ui->comboReqType->currentIndex() , NULL );
//    }
    QMutableListIterator<QNetworkReply*> i(m_netRereplyList);
    while (i.hasNext()) {
        QNetworkReply* reply = i.next();
        if( reply )
        {
            QNetworkReply* reply_new = sendRequest( reply->request(),(reqEnum_t)ui->comboReqType->currentIndex() , NULL );
            if( !reply->isRunning() )
            {
               reply->deleteLater();

               if( reply_new )
               {
                   i.setValue(reply_new);
                 // m_netRereplyList.push_front(  );
               }
               else
               {
                   i.remove();
               }
            }

        }
    }

}

QNetworkReply* RequestForm::sendRequest(const QNetworkRequest& request, const reqEnum_t &reqType,  QHttpMultiPart* multiPart )
{
    QNetworkReply *reply = NULL;
    //DEBUG << "Request URL: " << request.url();
    if( m_netMng )
    {
        switch ( reqType ) {

        case E_GET:
        {
            reply = m_netMng->get( request );
            break;
        }
        case E_POST:
        {

            reply = m_netMng->post( request, multiPart );
            break;
        }
        case E_PUT:
        {
            //DEBUG << "Put request isn't supported";
            //reply = replm_netMng->put( QNetworkRequest(url), NULL );
            break;
        }
        case E_DELETE:
        {
            //DEBUG << "Delete request isn't supported";
            //m_netMng->de( QNetworkRequest(url), NULL );
            break;
        }
        case E_HEAD:
        {
            reply = m_netMng->head( request );
            break;
        }
        case E_OPTIONS:
        {
            //DEBUG << "Option request isn't supported";
            //m_netMng->op( QNetworkRequest(url), NULL );
            break;
        }
        case E_PATCH:
        {
            //DEBUG << "Patch request isn't supported";
            //m_netMng->pa( QNetworkRequest(url), NULL );
            break;
        }

        default:
            break;
        }

        if( reply )
        {
            connect(reply, SIGNAL(finished()),  this, SLOT(httpFinished()));
            connect(reply, SIGNAL(readyRead()),  this, SLOT(httpReadyRead()));
            connect(reply, SIGNAL(downloadProgress(qint64,qint64)),  this, SLOT(updateDataReadProgress(qint64,qint64)) );
        }
    }
    return reply;
}

void RequestForm::httpFinished()
{

 QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
 if( reply )
 {
 //   send();
   //DEBUG << "REPLY httpFinished()" << reply->attribute(QNetworkRequest::SourceIsFromCacheAttribute);
 }
 else
 {
    //DEBUG << "Some REPLY httpFinished(): ";
 }

 if (reply->error()) {

     //DEBUG << tr("HTTP Download failed: %1.").arg(reply->errorString());
 }

}

 void RequestForm::httpReadyRead()
 {
     QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
     if( reply )
     {
       //DEBUG << "httpReadyRead" << reply;
     }
     else
     {
       //DEBUG << "Some httpReadyRead";
     }
 }

 void RequestForm::updateDataReadProgress(qint64 a,qint64 b)
 {
     //DEBUG << "REPLY updateDataReadProgress:  " << a << " " << b;
 }
