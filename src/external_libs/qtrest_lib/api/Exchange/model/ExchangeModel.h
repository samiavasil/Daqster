#ifndef COUPONMODEL_H
#define COUPONMODEL_H

#include "qtrest_lib_global.h"
//#include "abstractjsonrestlistmodel.h"
#include "jsonrestlistmodel.h"
#include "api/Exchange/ExchangeApi.h"


class QTREST_LIBSHARED_EXPORT ExchangeModel : public JsonRestListModel//AbstractJsonRestListModel
{
    Q_OBJECT

public:
    explicit ExchangeModel(QObject *parent = 0);

    static void declareQML() {
        AbstractJsonRestListModel::declareQML();
        qmlRegisterType<ExchangeModel>("com.github.samiavasil.cointrader.exchange", 1, 0, "ExchangeModel");
    }

protected:
 //   QNetworkReply *fetchMoreImpl(const QModelIndex &parent);
 //   QNetworkReply *fetchDetailImpl(QString id);
    QVariantMap preProcessItem(QVariantMap item);
    bool ResultToListPreparation(const QJsonDocument &document, QJsonArray &jsonArray);
};

#endif // COUPONMODEL_H
