#include "RestApi.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>
#include<QDebug>

const QMap<RestApi::RestReq_t, QString> RestApi::_GregMap( {{RestApi::GET,"GET"},
                                                    {RestApi::POST,"POST"},
                                                    {RestApi::PUT,"PUT"},
                                                    {RestApi::DELETE,"DELETE"},
                                                    {RestApi::HEAD,"HEAD"},
                                                    {RestApi::OPTIONS,"OPTIONS"},
                                                    {RestApi::PATCH,"PATCH"}});

const QMap<RestApi::RestProtocol_t, QString> RestApi::_GprotoMap( {{RestApi::HTTP,"HTTP"},
                                                              {RestApi::HTTPS,"HTTPS"}
                                                             });

QNetworkAccessManager* RestApi::m_Manager=NULL;

RestApi::RestApi(QObject *parent) : QObject(parent), m_AcceptHeader("Accept"), m_AuthTokenHeader("Authorization")
{
    if( NULL == m_Manager )
    m_Manager = new QNetworkAccessManager();

    connect(m_Manager, SIGNAL(finished(QNetworkReply *)),
            this, SLOT(ReplyFinished(QNetworkReply *)));
}

RestApi::~RestApi()
{

}

void RestApi::ReplyFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << reply->error() << reply->errorString();
        emit ReplyError(reply, reply->error(), reply->errorString());
    }
}

void RestApi::ReplyError(QNetworkReply::NetworkError error)
{
    qDebug() << "Error" << error;
}

void RestApi::SlotSslErrors(QList<QSslError> errors)
{
    qDebug() << errors;
}

const QMap<RestApi::RestProtocol_t, QString> &RestApi::GprotoMap()
{
    return _GprotoMap;
}

const QMap<RestApi::RestReq_t, QString> &RestApi::RequestNamesMap()
{
    return _GregMap;
}

void RestApi::SetRawHeaders(QNetworkRequest *request)
{
    request->setRawHeader(AcceptHeader(), Accept());
    request->setRawHeader(AuthTokenHeader(), AuthToken());
}

void RestApi::ConnectReplyToErrors(QNetworkReply *reply)
{
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(ReplyError(QNetworkReply::NetworkError)));

    connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
            this, SLOT(SlotSslErrors(QList<QSslError>)));
}


bool RestApi::CheckReplyIsError(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << reply->rawHeaderList();
        qDebug() << reply->bytesAvailable() << reply->errorString();
        return true;
    } else {
        return false;
    }
}

QByteArray RestApi::Accept() const
{
    return m_Accept;
}

QByteArray RestApi::BaseUrl() const
{
    return m_BaseUrl;
}

QByteArray RestApi::AcceptHeader() const
{
    return m_AcceptHeader;
}

QByteArray RestApi::AuthToken() const
{
    return m_AuthToken;
}

QByteArray RestApi::AuthTokenHeader() const
{
    return m_AuthTokenHeader;
}

QNetworkReply *RestApi::SendRequest(const RestReq_t &ReqType, const QString& Path, const QVariantMap& Query, const QVariantMap& Headers,
                                    const QByteArray &Data )
{
    QUrl url = QUrl( BaseUrl() + "/" + Path );
    QUrlQuery query;

    //Specify sort GET param
    auto qIter = Query.begin();
    while(  qIter != Query.end() ) {
        query.addQueryItem( qIter.key(), qIter.value().toByteArray() );
        qIter++;
    }

    url.setQuery(query.query());
    QNetworkRequest request = CreateRequest(url);

    /*Set headers*/
    auto hIter = Headers.begin();
    while(  hIter != Headers.end() ) {
        request.setRawHeader( hIter.key().toUtf8() , hIter.value().toByteArray() );
        hIter++;
    }
    SetRawHeaders(&request);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    QNetworkReply *reply = NULL;

    switch( ReqType ){
    case GET:{
        reply = m_Manager->get(request);
        qDebug() << "Repl... Is Running '" << reply->isRunning() << "' ";
        break;
    }
    case POST:{
        reply = m_Manager->post(request, Data);
        break;
    }
    case PUT:{
        reply = m_Manager->put(request,Data);
        break;
    }
    case DELETE:{
        reply = m_Manager->deleteResource(request);
        break;
    }
    case HEAD:{
        reply = m_Manager->head(request);
        break;
    }
    case OPTIONS:{
        reply = m_Manager->sendCustomRequest(request,"OPTIONS");
        break;
    }
    case PATCH:{
        reply = m_Manager->sendCustomRequest(request,"PATCH");
        break;
    }
    }

    if( NULL != reply ){
        ConnectReplyToErrors(reply);
    }
    return reply;
}

void RestApi::SetAccept(QString accept)
{
    QByteArray newData;
    newData.append(accept);

    if (m_Accept == newData)
        return;

    m_Accept = newData;
    emit AcceptChanged(newData);
}

void RestApi::SetBaseUrl(QByteArray baseUrl)
{
    if (m_BaseUrl == baseUrl)
        return;

    m_BaseUrl = baseUrl;
    emit BaseUrlChanged(baseUrl);
}

void RestApi::SetAcceptHeader(QByteArray acceptHeader)
{
    if (m_AcceptHeader == acceptHeader)
        return;

    m_AcceptHeader = acceptHeader;
    emit AcceptHeaderChanged(acceptHeader);
}

void RestApi::SetAuthToken(QByteArray authToken)
{
    if (m_AuthToken == authToken)
        return;

    m_AuthToken = authToken;
    emit AuthTokenChanged(authToken);
}

void RestApi::SetAuthTokenHeader(QByteArray authTokenHeader)
{
    if (m_AuthTokenHeader == authTokenHeader)
        return;

    m_AuthTokenHeader = authTokenHeader;
    emit AuthTokenHeaderChanged(authTokenHeader);
}

QNetworkReply *RestApi::Get(QUrl url)
{
    QNetworkRequest request = CreateRequest(url);
    SetRawHeaders(&request);

    QNetworkReply *reply = m_Manager->get(request);
    ConnectReplyToErrors(reply);
    return reply;
}

QNetworkReply *RestApi::Post(QUrl url, QIODevice *data)
{
    QNetworkRequest request = CreateRequest(url);
    SetRawHeaders(&request);

    QNetworkReply *reply = m_Manager->post(request, data);
    ConnectReplyToErrors(reply);
    return reply;
}

QNetworkReply *RestApi::Post(QUrl url,const QByteArray &data)
{
    QNetworkRequest request = CreateRequest(url);
    SetRawHeaders(&request);

    QNetworkReply *reply = m_Manager->post(request, data);
    ConnectReplyToErrors(reply);
    return reply;
}

QNetworkReply *RestApi::Post(QUrl url, QHttpMultiPart *multiPart)
{
    QNetworkRequest request = CreateRequest(url);
    SetRawHeaders(&request);

    QNetworkReply *reply = m_Manager->post(request, multiPart);
    ConnectReplyToErrors(reply);
    return reply;
}

QNetworkReply *RestApi::Put(QUrl url, QIODevice *data)
{
    QNetworkRequest request = CreateRequest(url);
    SetRawHeaders(&request);

    QNetworkReply *reply = m_Manager->put(request, data);
    ConnectReplyToErrors(reply);
    return reply;
}

QNetworkReply *RestApi::Put(QUrl url, const QByteArray &data)
{
    QNetworkRequest request = CreateRequest(url);
    SetRawHeaders(&request);

    QNetworkReply *reply = m_Manager->put(request, data);
    ConnectReplyToErrors(reply);
    return reply;
}

QNetworkReply *RestApi::Put(QUrl url, QHttpMultiPart *multiPart)
{
    QNetworkRequest request = CreateRequest(url);
    SetRawHeaders(&request);

    QNetworkReply *reply = m_Manager->put(request, multiPart);
    ConnectReplyToErrors(reply);
    return reply;
}

QNetworkReply *RestApi::DeleteResource(QUrl url)
{
    QNetworkRequest request = CreateRequest(url);
    SetRawHeaders(&request);

    QNetworkReply *reply = m_Manager->deleteResource(request);
    ConnectReplyToErrors(reply);
    return reply;
}

QNetworkReply *RestApi::Head(QUrl url)
{
    QNetworkRequest request = CreateRequest(url);
    SetRawHeaders(&request);

    QNetworkReply *reply = m_Manager->head(request);
    ConnectReplyToErrors(reply);
    return reply;
}

QNetworkReply *RestApi::Options(QUrl url)
{
    QNetworkRequest request = CreateRequest(url);
    SetRawHeaders(&request);

    QNetworkReply *reply = m_Manager->sendCustomRequest(request,"OPTIONS");
    ConnectReplyToErrors(reply);
    return reply;
}

QNetworkReply *RestApi::Patch(QUrl url)
{
    QNetworkRequest request = CreateRequest(url);
    SetRawHeaders(&request);

    QNetworkReply *reply = m_Manager->sendCustomRequest(request,"PATCH");
    ConnectReplyToErrors(reply);
    return reply;
}

QNetworkRequest RestApi::CreateRequest(const QUrl &url) const
{
    return QNetworkRequest(url);
}
