import QtQuick 2.0
import QtQuick.Controls 2.3
import QtCharts 2.0

import com.github.samiavasil.cointrader.exchangeapi 1.0
import com.github.samiavasil.cointrader.exchange 1.0
import com.github.qtrest.jsonrestlistmodel 1.0
import com.github.qtrest.pagination 1.0

Item {
    property alias text: txt.text
    property alias color: rec.color
    property alias source: img.source


    Rectangle {
        id : rec
        anchors.fill: parent
        anchors.margins: 10
        Text {
            id: txt
            anchors.top: parent.top
            anchors.left: parent.left
            //    width:  parent.width
            //   height: parent.height
        }
        Image {
            id: img
            anchors.top: parent.top
            anchors.topMargin: 17
            //            anchors.horizontalCenterOffset: 0
            //            anchors.bottom: parent.bottom
            //            anchors.bottomMargin: 39
            //            anchors.right: parent.right

            anchors.horizontalCenter: parent.horizontalCenter
            fillMode: Image.TileVertically
        }

        //        ChartView {

        //            anchors.top: img.bottom
        //            anchors.bottom: parent.bottom
        //            anchors.left: parent.left
        //            anchors.right: parent.right
        //            anchors.topMargin: 10
        //            theme: ChartView.ChartThemeBrownSand
        //            antialiasing: true

        ////            LineSeries {
        ////                id: pieSeries
        ////                PieSlice { label: "eaten"; value: 94.9 }
        ////                PieSlice { label: "not yet eaten"; value: 5.1 }
        ////            }

        //            LineSeries {
        //                name: "LineSeries"
        //                XYPoint { x: 0; y: 0 }
        //                XYPoint { x: 1.1; y: 2.1 }
        //                XYPoint { x: 1.9; y: 3.3 }
        //                XYPoint { x: 2.1; y: 2.1 }
        //                XYPoint { x: 2.9; y: 4.9 }
        //                XYPoint { x: 3.4; y: 3.0 }
        //                XYPoint { x: 4.1; y: 3.3 }
        //            }
        //        }

        ExchangeApi {
            id: exchangeApi

            baseUrl: "https://bittrex.com/api"

            authTokenHeader: "Authorization"
            authToken: "Bearer 8aef452ee3b32466209535b96d456b06"

            Component.onCompleted: console.log("completed!");
        }

        //    ExchangeModel {
        //        id: categoriesRestModel
        //        api: exchangeApi

        //        idField: 'id'

        //        requests {
        //            get: "/v1.1/public/getmarkets"
        // //           getDetails: "/v1/coupon/{MarketCurrency}"
        //        }

        //        //sort: ['categoryName']
        //        sort: ['BliaBlia']
        //        pagination {
        //            policy: Pagination.PageNumber
        //            perPage: 20
        //            currentPageHeader: "X-Pagination-Current-Page"
        //            totalCountHeader: "X-Pagination-Total-Count"
        //            pageCountHeader: "X-Pagination-Page-Count"
        //        }

        //        Component.onCompleted: { console.log(pagination.perPage); reload(); }
        //    }

        ListView {
            //anchors.fill: parent
            anchors.top: img.bottom
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 10
            property string titleText: qsTr("Actual")

            function filter(filters)
            {
                filters.isArchive = 0;
                coupons.filters = filters;
                coupons.reload();
            }
            //JsonRestListModel
            ExchangeModel{
                id:  exchangeModel
                api: exchangeApi

                idField: 'id'

//                requests {
//                    get: "/v1.1/public/getmarkets"
//                    //           getDetails: "/v1/coupon/{MarketCurrency}"
//                }

                //sort: ['categoryName']
                sort: ['BliaBlia']
                pagination {
                    policy: Pagination.PageNumber
                    perPage: 20
                    currentPageHeader: "X-Pagination-Current-Page"
                    totalCountHeader: "X-Pagination-Total-Count"
                    pageCountHeader: "X-Pagination-Page-Count"
                }

                Component.onCompleted: { console.log(pagination.perPage); reload(); }
            }
            model:exchangeModel
            delegate: Text {
                id: name
                text: MarketCurrency
                //qsTr("text")
            }
        }
    }
    ToolButton{
        width: 37
        height: 40
        text: qsTr("Quit Button")
        anchors.top: parent.top
        anchors.right: parent.right
    }
}
