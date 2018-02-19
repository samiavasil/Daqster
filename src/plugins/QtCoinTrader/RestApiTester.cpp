#include "RestApiTester.h"
#include "ui_restapitester.h"
#include "RestApi.h"
#include<QNetworkReply>
#include<QWebEngineView>

RestApiTester::RestApiTester(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RestApiTester)
{
    ui->setupUi(this);
    for(auto Idx = RestApi::RequestNamesMap().begin();Idx != RestApi::RequestNamesMap().end();Idx++ ){
        ui->reqTtypeComboBox->addItem( Idx.value(), Idx.key() );
    }

    for(auto Idx = RestApi::GprotoMap().begin();Idx != RestApi::GprotoMap().end();Idx++ ){
        ui->protoComboBox->addItem( Idx.value(), Idx.key() );
    }

    m_webEngine = new QWebEngineView();
    QLayout* layout = new QHBoxLayout();
    layout->addWidget( m_webEngine );
    ui->uiFrame->setLayout(layout);
    ui->uiFrame->layout()->addWidget(m_webEngine);
}

RestApiTester::~RestApiTester()
{
    delete ui;
}

#if 0

downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
encrypted()
error(QNetworkReply::NetworkError code)
finished()
metaDataChanged()
preSharedKeyAuthenticationRequired(QSslPreSharedKeyAuthenticator *authenticator)
redirectAllowed()
redirected(const QUrl &url)
sslErrors(const QList<QSslError> &errors)
uploadProgress(qint64 bytesSent, qint64 bytesTotal)

#endif

void RestApiTester::redirected(const QUrl &url){
    qDebug() << "Redirection To:" << url.toString();
}


void RestApiTester::on_sendButton_clicked()
{

    RestApi Api;
    QString url = tr("%1://%2").arg(ui->protoComboBox->currentText()).arg(ui->urlEdit->text());
    Api.SetBaseUrl( url.toUtf8() );
    QNetworkReply * Replay = Api.SendRequest( (RestApi::RestReq_t)ui->reqTtypeComboBox->currentData().toInt(),"/" );
    connect(Replay,SIGNAL(error(QNetworkReply::NetworkError)),this, SLOT(error(QNetworkReply::NetworkError)));
    connect(Replay,SIGNAL( finished()),this, SLOT(finished()));
    connect(Replay,SIGNAL( redirected(QUrl)),this, SLOT(redirected(QUrl)));

}

void RestApiTester::finished(){
        QNetworkReply * Replay = dynamic_cast<QNetworkReply *>(QObject::sender());
        if( NULL != Replay ){
            m_webEngine->setHtml(Replay->readAll());
       //     ui->logOutputEdit->setText(QString(Replay->readAll()));
        }
}

void RestApiTester::error( const QNetworkReply::NetworkError& Error ){
        QString errStr = tr("Bad Boy you have a bad error:  %1").arg(Error);
        ui->logOutputEdit->setText(errStr);
}
