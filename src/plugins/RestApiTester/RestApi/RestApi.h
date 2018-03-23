#ifndef RESTAPI_H
#define RESTAPI_H

#include "restapi_global.h"
#include <QObject>
#include <QNetworkReply>
#include <QNetworkRequest>

class QNetworkAccessManager;
class QHttpMultiPart;

class RESTAPISHARED_EXPORT RestApi : public QObject
{
    Q_OBJECT
public:
    typedef enum{
        GET,
        POST,
        PUT,
        DELETE,
        HEAD,
        OPTIONS,
        PATCH
    }RestReq_t;

    typedef enum{
        HTTP,
        HTTPS
    }RestProtocol_t;

    explicit RestApi(QObject *parent = 0);

    ~RestApi();

    bool CheckReplyIsError(QNetworkReply *reply);

    //Accept header for JSON/XML data
    Q_PROPERTY(QByteArray Accept READ Accept WRITE SetAccept NOTIFY AcceptChanged)
    Q_PROPERTY(QByteArray AcceptHeader READ AcceptHeader WRITE SetAcceptHeader NOTIFY AcceptHeaderChanged)
    //Specify Auth token for each request. Set this before run your requests (You may use Basic auth and Bearer token auth)
    Q_PROPERTY(QByteArray BaseUrl READ BaseUrl WRITE SetBaseUrl NOTIFY BaseUrlChanged)
    //Specify Auth token for each request. Set this before run your requests (You may use Basic auth and Bearer token auth)
    Q_PROPERTY(QByteArray AuthToken READ AuthToken WRITE SetAuthToken NOTIFY AuthTokenChanged)
    //Specify Auth token header name. Default is 'Authorization' (You may use Basic auth and Bearer token auth)
    Q_PROPERTY(QByteArray AuthTokenHeader READ AuthTokenHeader WRITE SetAuthTokenHeader NOTIFY AuthTokenHeaderChanged)
    //--------------------

    QByteArray BaseUrl() const;
    QByteArray Accept() const;
    QByteArray AcceptHeader() const;
    QByteArray AuthToken() const;
    QByteArray AuthTokenHeader() const;

    virtual QNetworkReply *SendRequest(const RestReq_t& ReqType, const QString &Path, const QVariantMap &Query=QVariantMap(),
                                       const QVariantMap &Headers = QVariantMap(), const QByteArray &Data = QByteArray() );

    static const QMap<RestReq_t, QString> &RequestNamesMap();

    static const QMap<RestProtocol_t, QString> &GprotoMap();

public slots:
    void SetBaseUrl(QByteArray BaseUrl);
    void SetAccept(QString Accept);
    void SetAcceptHeader(QByteArray AcceptHeader);
    void SetAuthToken(QByteArray AuthToken);
    void SetAuthTokenHeader(QByteArray AuthTokenHeader);

signals:
    void ReplyError(QNetworkReply *reply, QNetworkReply::NetworkError error, QString errorString);
    void AcceptChanged(QByteArray Accept);
    void BaseUrlChanged(QByteArray BaseUrl);
    void AcceptHeaderChanged(QByteArray AcceptHeader);
    void AuthTokenChanged(QByteArray AuthToken);
    void AuthTokenHeaderChanged(QByteArray AuthTokenHeader);

protected:

    QNetworkReply *Get(QUrl url);
    QNetworkReply *Post(QUrl url, QIODevice *data);
    QNetworkReply *Post(QUrl url, const QByteArray &data);
    QNetworkReply *Post(QUrl url, QHttpMultiPart *multiPart);
    QNetworkReply *Put(QUrl url, QIODevice *data);
    QNetworkReply *Put(QUrl url, const QByteArray &data);
    QNetworkReply *Put(QUrl url, QHttpMultiPart *multiPart);
    QNetworkReply *DeleteResource(QUrl url);
    QNetworkReply *Head(QUrl url);
    QNetworkReply *Options(QUrl url);
    QNetworkReply *Patch(QUrl url);

    static QNetworkAccessManager *m_Manager;

    virtual QNetworkRequest CreateRequest(const QUrl &url) const;

    void SetRawHeaders(QNetworkRequest *request);
    void ConnectReplyToErrors(QNetworkReply *reply);

protected slots:
    void ReplyFinished(QNetworkReply *reply);
    void ReplyError(QNetworkReply::NetworkError error);
    void SlotSslErrors(QList<QSslError> errors);

private:

    QByteArray m_Accept;
    QByteArray m_BaseUrl;
    QByteArray m_AcceptHeader;
    QByteArray m_AuthToken;
    QByteArray m_AuthTokenHeader;
    static const QMap<RestApi::RestReq_t, QString> _GregMap;
    static const QMap<RestApi::RestProtocol_t, QString> _GprotoMap;
};
#endif // RESTAPI_H
