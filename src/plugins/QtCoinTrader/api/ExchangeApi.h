#ifndef SKIDKZAPI_H
#define SKIDKZAPI_H

#include "apibase.h"
#include <QtQml>

class ExchangeApi : public APIBase
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit ExchangeApi();

    static void declareQML() {
        qmlRegisterType<ExchangeApi>("com.github.samiavasil.cointrader.exchangeapi", 1, 0, "ExchangeApi");
    }

    //requests
    QNetworkReply *handleRequest(QString path, QStringList sort, Pagination *pagination,
                           QVariantMap filters = QVariantMap(), QStringList fields = QStringList(), QString id = 0);

    //get list of objects
    QNetworkReply *getMarkets(QStringList sort, Pagination *pagination,
                              QVariantMap filters = QVariantMap(), QStringList fields = QStringList());
    //get full data for specified item
    QNetworkReply *getCouponDetail(QString id);

    QNetworkReply *getCategories(QStringList sort, Pagination *pagination);
};

#endif // SKIDKZAPI_H
