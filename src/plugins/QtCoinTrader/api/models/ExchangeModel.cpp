#include "ExchangeModel.h"

CouponModel::CouponModel(QObject *parent) : AbstractJsonRestListModel(parent)
{

}

QNetworkReply *CouponModel::fetchMoreImpl(const QModelIndex &parent)
{
    Q_UNUSED(parent)

    return static_cast<SkidKZApi *>(apiInstance())->getCoupons(sort(), pagination(), filters(), fields());
}

QNetworkReply *CouponModel::fetchDetailImpl(QString id)
{
    return static_cast<SkidKZApi *>(apiInstance())->getCouponDetail(id);
}

bool CouponModel::ResultToListPreparation(const QJsonDocument &document, QJsonArray& jsonArray ){
    bool Ret = false;
    if( document.isObject() ){
        QJsonObject obj = document.object();
        jsonArray=obj.value( "result" ).toArray();
        Ret = true;
    }
    return Ret;
}


QVariantMap CouponModel::preProcessItem(QVariantMap item)
{
#if 0
//    QDate date = QDateTime::fromString(item.value("createTimestamp").toString(), "yyyy-MM-dd hh:mm:ss").date();
    QDate date = QDateTime::fromString(item.value("Created").toString(), "yyyy-MM-ddThh:mm:ss").date();
 //   "Created":"2014-02-13T00:00:00"
  //  QDate date = QDateTime::currentDateTime().date();
    item.insert("createDate", date.toString("dd.MM.yyyy"));

    QString originalCouponPrice = item.value("originalCouponPrice").toString().trimmed();
    if (originalCouponPrice.isEmpty()) { originalCouponPrice = "?"; }
    QString discountPercent = item.value("discountPercent").toString().trimmed().remove("—").remove("-").remove("%");
    if (discountPercent.isEmpty()) { discountPercent = "?"; }
    QString originalPrice = item.value("originalPrice").toString().trimmed();
    if (originalPrice.isEmpty()) { originalPrice = "?"; }
    QString discountPrice = item.value("discountPrice").toString().remove("тг.").trimmed();
    if (discountPrice.isEmpty()) { discountPrice = "?"; }

    QString discountType = item.value("discountType").toString();
    QString discountString = tr("Undefined Type");
    if (discountType == "freeCoupon" || discountType == "coupon") {
        discountString = tr("Coupon: %1. Discount: %2%").arg(originalCouponPrice).arg(discountPercent);
    } else if (discountType == "full") {
        discountString = tr("Cost: %1. Certificate: %2. Discount: %3%").arg(originalPrice).arg(discountPrice).arg(discountPercent);
    }

    item.insert("discountString", discountString);
#endif
//    item.insert("id", "1");
//    item.insert("MarketCurrency", "2.0");
//    item.insert("MarketCurrencyLong","wqeq");
//    item.insert("BaseCurrency","VVV");
//    item.insert("BaseCurrencyLong","VVVVsamia");
//    item.insert("MarketName","KudKudqk");
//    item.insert("MinTradeSize","23");
//    item.insert("LogoUrl","https://cdn2.iconfinder.com/data/icons/food-180/252/tableware-256.png");
    return item;
}

