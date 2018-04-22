import QtQuick 2.6

import QtQuick 2.5
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.1
import QtQuick.Window 2.1

ApplicationWindow {
    id: applicationWindow
    opacity: sideBar.bgopacity
   //  Binding cols.value: mdiArea.maxCol

   // property Binding rows.value: mdiArea.maxRow
    //    background: none.none
    visible: true
//        width: 625
//        height: 420
    visibility: "Maximized"
    //  color: "#442099"
  //  flags:  Qt.FramelessWindowHint
    Material.theme: Material.Light
    Material.accent: Material.ShadeA700
    //  Material.primary: Material.Amber
    Material.background:  Material.Shade50
    Binding {
        target: mdiArea
        property: "maxCol"
        value: cols.value
    }
    Binding {
        target: mdiArea
        property: "maxRow"
        value: rows.value
    }

    Connections {
        target: sideBar
        onCurrentIndexChanged: {
            //rect.color = Qt.rgba(Math.random(), Math.random(), Math.random(), 1);
            console.log("URAAAAAAAAAAAA ", sideBar.model.get( sideBar.currentIndex ).colorM )
            console.log("URAAAAAAAAAAAA ", sideBar.model.get( sideBar.currentIndex ).title )
            console.log("URAAAAAAAAAAAA ", sideBar.model.get( sideBar.currentIndex ).imgSource )
            mdiArea.addToModel( { "colorM" : sideBar.model.get( sideBar.currentIndex ).colorM,
                                   "textL" : sideBar.model.get( sideBar.currentIndex ).title,
                                   "sourceM" : sideBar.model.get( sideBar.currentIndex ).imgSource
                                }
                               )
        }
    }

    header: ToolBar {
        ToolButton {
            id: toolButton
            width: 37
            height: 40
            Image {
                id: name
                fillMode: Image.PreserveAspectFit
                source: "qrc:/images/save.png"
            }
            text: qsTr("Tool Button")
            spacing: 9
            anchors.top: parent.top
            anchors.topMargin: 0
            anchors.left: parent.left
            anchors.leftMargin: 0
            focusPolicy: Qt.WheelFocus
            checked: true
            checkable: true

        }
        ToolButton {
            id: toolQuitButton
            x: 603
            width: 37
            height: 40
            text: qsTr("Quit Button")
            anchors.right: parent.right
            anchors.rightMargin: 0
            spacing: 9
            anchors.top: parent.top
            anchors.topMargin: 0
            focusPolicy: Qt.WheelFocus
            checkable: true
            onClicked: {
                Qt.quit()
            }
        }
        SpinBox{
            id: rows
            value: 2
            anchors.left: toolButton.right
            anchors.top:toolButton.top
            from: 1

        }
        SpinBox{
            id: cols
            value: 2
            anchors.left: rows.right
            anchors.top:rows.top
            from: 1
        }
    }




    SideBar {
        id: sideBar
        width: 145
        color: "#555753"
        border.width: 1
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.top: parent.top
        anchors.topMargin: 0
        currentIndex: 0
        visible: toolButton.checked


        model: ListModel {
            ListElement { title: qsTr("Bitfinex")
                imgSource: "https://s2.coinmarketcap.com/static/img/exchanges/32x32/37.png"
                colorM: "magenta"
            } //; source: "qrc:/ArchiveCouponsList.qml"

            ListElement { title: qsTr("Binance")
                imgSource: "https://s2.coinmarketcap.com/static/img/exchanges/32x32/270.png"
                colorM: "chartreuse"
            } //; source: "qrc:/About.qml" }
            ListElement { title: qsTr("Bitinka")
                imgSource: "https://s2.coinmarketcap.com/static/img/exchanges/32x32/327.png"
                colorM: "indigo"
            }
            ListElement { title: qsTr("Bitstamp")
                imgSource: "https://s2.coinmarketcap.com/static/img/exchanges/32x32/70.png"
                colorM: "orange"
            }
            ListElement { title: qsTr("Bittrex")
                imgSource: "https://s2.coinmarketcap.com/static/img/exchanges/32x32/22.png"
                colorM: "purple"
            }   //; source: "qrc:/ArchiveCouponsList.qml" }
            ListElement { title: qsTr("Kraken")
                imgSource: "https://s2.coinmarketcap.com/static/img/exchanges/32x32/24.png"
                colorM: "cyan"
            }
            ListElement { title: qsTr("OKEx_")
                imgSource: "https://s2.coinmarketcap.com/static/img/exchanges/32x32/294.png"
                colorM: "magenta"
            }
            ListElement { title: qsTr("HitBTC")
                imgSource: "https://s2.coinmarketcap.com/static/img/exchanges/32x32/42.png"
                colorM: "indigo"
            }
            ListElement { title: qsTr("Lbank")
                imgSource: "https://s2.coinmarketcap.com/static/img/exchanges/32x32/333.png"
                colorM: "purple"
            }
            ListElement { title: qsTr("EXX")
                imgSource: "https://s2.coinmarketcap.com/static/img/exchanges/32x32/331.png"
                colorM: "cyan"
            }

        }

    }

    MdiArrea{
        id: mdiArea
        x: 6
        y: 0
        anchors.left: parent.left
        anchors.bottom: sideBar.bottom
        anchors.leftMargin: sideBar.visible?sideBar.width:0
        anchors.top: parent.top
        anchors.right: parent.right
    }


}
