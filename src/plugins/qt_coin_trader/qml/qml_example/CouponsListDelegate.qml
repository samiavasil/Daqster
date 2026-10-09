import QtQuick 2.6
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3

Item {
    id: couponDelegate
    height: information.height

    property string detailSource: "qrc:/CouponDetail.qml"

    Rectangle {
        id: rectangle1
        anchors.fill: parent
        color: "black"
        opacity: 0.5

        border.color: "black"
        border.width: 0
    }

    Column {
        id: information
        width: parent.width
        //height: parent.height - couponsList.spacing
        anchors.centerIn: parent

        Item {
            id: imageContainer
            width: parent.width
            height: width * 0.5

            anchors {
                horizontalCenter: parent.horizontalCenter
            }

            Image {
                id: bgImage
                width: parent.width
                height: parent.height
                source: "qrc:/images/skid_bg_2.jpg"
                fillMode: Image.PreserveAspectCrop
                clip: true
                visible: image.status == Image.Error || image.status == Image.Null
            }

            Item {
                anchors.fill: parent
                Image {
                    id: image
                    width: parent.width
                    height: parent.height
                    source: LogoUrl
                    fillMode: Image.PreserveAspectFit
                    clip: true
                }

                Item {
                    width: parent.width
                    height: discountText.height*1.1

                    Rectangle {
                        anchors.fill: parent
                        color:"black"
                        opacity: 0.3
                    }

                    Text {
                        id: discountText
                        text: MarketCurrencyLong
                        font.bold: true
                        font.pointSize: 14
                        color: "white"
                        wrapMode: Text.WordWrap
                        width: parent.width-settings.spacing
                        clip: true
                        maximumLineCount: 2
                        horizontalAlignment: Text.AlignHCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }

            }

            ProgressBar {
                id: progressBar
                indeterminate: true
                visible: image.status != Image.Ready
                width: parent.width / 2
                anchors.centerIn: parent
            }
        }

        Text {
            id: titleText
            text: title
            font.bold: true
            font.pointSize: 16
            color: "yellow"
            wrapMode: Text.WordWrap
            width: parent.width-settings.spacing
            clip: true
            maximumLineCount: 2
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text {
            id: description
            text: BaseCurrency
            font.bold: true
            font.pointSize: 10
            color: "red"
            wrapMode: Text.WordWrap
            width: parent.width-settings.spacing
            clip: true
            maximumLineCount: 4
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Rectangle {
            width: parent.width
            height: 2
            border.color: "green"
            color: "green"
        }
/*
        RowLayout {
            width: parent.width
            height: boughtCol.height*1.5
            spacing: 0

            Item {
                id: r1
                height: boughtCol.height

                Layout.fillWidth: true
                Column {
                    id: boughtCol
                    anchors.centerIn: parent
                    Text {
                        id: boughtText
                        text: qsTr("MarketCurrency")
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "yellow"
                        font.pointSize: 10
                    }
                    Text {
                        id: boughtLbl
                        text: MarketCurrencyLong
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "white"
                        font.pointSize: 8
                    }
                }
            }

            Rectangle {
                width: 2
                height: parent.height
                border.color: "green"
                color: "green"
                border.width: 3
            }

            Item {
                id: r2
                height: cityCol.height

                Layout.fillWidth: true
                Column {
                    id: cityCol
                    anchors.centerIn: parent
                    Text {
                        id: cityText
                        text: qsTr("BaseCurrency")
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "yellow"
                        font.pointSize: 10
                    }
                    Text {
                        id: cityLbl
                        text: BaseCurrencyLong
                        elide: Text.ElideNone
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "white"
                        font.pointSize: 8
                    }
                }
            }

            Rectangle {
                width: 2
                height: parent.height
                border.color: "green"
                color: "green"
            }

            Item {
                id: r3
                height: dateCol.height

                Layout.fillWidth: true
                Column {
                    id: dateCol
                    anchors.centerIn: parent
                    Text {
                        id: dateText
                        text: qsTr("MarketName")
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "yellow"
                        font.pointSize: 10
                    }
                    Text {
                        id: dateLbl
                        text: MarketName
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "white"
                        font.pointSize: 8
                    }
                }
            }

            Rectangle {
                width: 2
                height: parent.height
                border.color: "green"
                color: "green"
            }

            Item {
                id: r4
                height: serviceCol.height

                Layout.fillWidth: true
                Column {
                    id: serviceCol
                    anchors.centerIn: parent
                    Text {
                        id: serviceText
                        text: qsTr("MinTradeSize")
                        font.family: "Times New Roman"
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "yellow"
                        font.pointSize: 10
                    }
                    Text {
                        id: serviceLbl
                        text: MinTradeSize
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "white"
                        font.pointSize: 8
                    }
                }
            }
        }
 */
    }

    MouseArea {
        id: detail
        anchors.fill: parent

        onClicked: {
            couponsModel.fetchDetail(BaseCurrency+"-"+MarketCurrency)
            stackView.push(detailSource, {detailsModel: couponsModel.detailsModel, couponsModel: couponsModel})
            drawer.close()
        }
    }
}
