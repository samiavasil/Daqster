import QtQuick 2.0
import QtQuick.Controls 2.3
import QtCharts 2.9

import com.github.samiavasil.cointrader.exchangeapi 1.0
import com.github.samiavasil.cointrader.exchange 1.0
//import com.github.qtrest.jsonrestlistmodel 1.0
//import com.github.qtrest.pagination 1.0
import com.github.samiavasil.cointrader.randdata 1.0
import QtQuick.Layouts 1.3
import QtQuick.Controls 1.4

Item {
    property alias text: txt.text
    property alias color: rec.color
    property alias source: img.source
    z: -2147483647


    Rectangle {
        id : rec
        anchors.fill: parent
        anchors.margins: 10
        Text {
            id: txt
            anchors.top: parent.top
            anchors.left: parent.left
        }
        Image {
            id: img
            anchors.top: parent.top
            // anchors.topMargin: 17
            anchors.horizontalCenter: parent.horizontalCenter
            fillMode: Image.TileVertically
        }


        //        Connections {
        //            target: dataFromCpp
        //            onWValueChanged: {
        //                //qmlString = signalString
        //                //mView.chart = false; // HACK. Force it to update.
        //                //console.log( dataFromCpp.name )
        //                //mView.createSeries(ChartView.SeriesTypeLine, "Test", mView.axisX(dataFromCpp), mView.axisY(dataFromCpp));

        //            }
        //        }

        ExchangeApi {
            id: exchangeApi

            baseUrl: "https://bittrex.com/api"

            authTokenHeader: "Authorization"
            authToken: "Bearer 8aef452ee3b32466209535b96d456b06"

            Component.onCompleted: console.log("completed!");
        }
        //JsonRestListModel  //ExchangeModel
        ExchangeModel{
            id:  exchangeModel
            api: exchangeApi

            idField: 'MarketName'

            requests {
                get:    "/v1.1/public/getmarkets"
                getDetails: "/v1.1/public/getmarketsummary?market={id}"
            }

            //sort: ['categoryName']
            sort: ['MarketCurrency']
            pagination {
                policy: Pagination.PageNumber
                perPage: 20
                currentPageHeader: "X-Pagination-Current-Page"
                totalCountHeader: "X-Pagination-Total-Count"
                pageCountHeader: "X-Pagination-Page-Count"
            }

            Component.onCompleted: { console.log(pagination.perPage); reload(); }
        }

    }
    SplitView{
        anchors.fill: parent
        anchors.margins:  10
        //            Rectangle{
        //                color: "#61f392"
        //                Layout.fillWidth: true
        //                Layout.fillHeight: true
        //                Layout.minimumWidth: 1
        ListView {
            id: marketList
            //                    anchors.fill: Layout
            Layout.fillWidth:  false
            Layout.fillHeight: true
            width: parent.width/8
            //                    Layout.alignment: Qt.AlignLeft
            //                    Layout.minimumWidth: 1
            //                    Layout.preferredWidth: 4
            //        QtCoinTrader        //    Layout.preferredWidth: 200
            //                    anchors.topMargin: 10
            property string titleText: qsTr("Actual")

            function filter(filters)
            {
                filters.isArchive = 0;
                coupons.filters = filters;
                coupons.reload();
            }

            model:exchangeModel
            delegate:SideBarDelegate{
                width: marketList.width
                height: marketList.height/4
                value: indexAt(  originX, originY ) + MarketName
                imgSource: LogoUrl

            }

            /* ItemDelegate {

                //    property string value: ""
                //    property alias imgSource: image.source

                    height: parent.parent.height/4
                    highlighted: ListView.isCurrentItem
                    anchors.horizontalCenter: parent.horizontalCenter
                    Text {
                        id: name
                        //  width:  parent.width
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: MinTradeSize
                        // text: MarketCurrency
                        //qsTr("text")
                    }
                }*/
            //                }
        }
        ChartView {
            id: mView
            Layout.fillWidth: true
            Layout.fillHeight: true
            //                Layout.preferredWidth: 400

            Layout.minimumWidth: 4
            //                anchors.topMargin: 10
            theme: ChartView.ChartThemeBrownSand
            antialiasing: true

            ValueAxis{
                id: valueAxisX
                min: 0
                max: 100
                tickCount: 12
                //labelFormat: "%2.0f:00"
            }
            ValueAxis{
                id: valueAxisY
                min:0
                max: 100
                tickCount: 50
            }
            RandData{
                id: dataRand
                //                        type:
            }

            //            LineSeries {
            //                id: pieSeries
            //                PieSlice { label: "eaten"; value: 94.9 }
            //                PieSlice { label: "not yet eaten"; value: 5.1 }
            //            }

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
            //            PieSeries {
            //                id: pieSeries
            //                PieSlice { label: "eaten"; value: 94.9 }
            //                PieSlice { label: "not yet eaten"; value: 5.1 }
            //            }
            //                    Component.onCompleted: {
            //                        console.log( "Xaxis: ", mView.series(0) )
            //                    //    mView.createSeries(ChartView.SeriesTypeLine, dataFromCpp.name, mView.axisX(dataFromCpp), mView.axisY(dataFromCpp));

            //                        mView.setAxisX( valueAxisX, dataFromCpp)
            //                        mView.setAxisY( valueAxisY, dataFromCpp)
            //                    }
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
