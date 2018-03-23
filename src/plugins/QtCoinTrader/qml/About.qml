import QtQuick 2.6

import QtQuick 2.5
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.1
import QtQuick.Window 2.1

ApplicationWindow {
    id: applicationWindow
    opacity: listV.bgopacity
//    background: none.none
    visible: true
    //    width: 625
    //    height: 420
    visibility: "Maximized"
    //  color: "#442099"
    flags:  Qt.FramelessWindowHint
    Material.theme: Material.Light
    Material.accent: Material.ShadeA700
    //  Material.primary: Material.Amber
    Material.background:  Material.Shade50

    SideBar {
        id: listV
        x: 0
        currentIndex: -1
        width: 200
    //    opacity: 1
        color:"#ff0000"
        anchors.top: parent.top
        anchors.topMargin: 0

        visible: toolButton.checked
        delegate: SideBarDelegate{
            id: sideBarDelegate
            width: parent.width
            value: model.title
        }
        model: ListModel {

            ListElement { title: qsTr("Actual")}//; source: "qrc:/ActualCouponsList.qml";}
            ListElement { title: qsTr("Archive")}//; source: "qrc:/ArchiveCouponsList.qml" }
            ListElement { title: qsTr("Statistics")}//; source: "qrc:/Statistics.qml" }
            ListElement { title: qsTr("About")}//; source: "qrc:/About.qml" }
            ListElement { title: qsTr("Actual")}//; source: "qrc:/ActualCouponsList.qml";}
            ListElement { title: qsTr("Archive")}//; source: "qrc:/ArchiveCouponsList.qml" }
            ListElement { title: qsTr("Statistics")}//; source: "qrc:/Statistics.qml" }
            ListElement { title: qsTr("About")}//; source: "qrc:/About.qml" }
            ListElement { title: qsTr("Actual")}//; source: "qrc:/ActualCouponsList.qml";}
            ListElement { title: qsTr("Archive")}//; source: "qrc:/ArchiveCouponsList.qml" }
            ListElement { title: qsTr("Statistics")}//; source: "qrc:/Statistics.qml" }
            ListElement { title: qsTr("About")}//; source: "qrc:/About.qml" }
            ListElement { title: qsTr("Actual")}//; source: "qrc:/ActualCouponsList.qml";}
            ListElement { title: qsTr("Archive")}//; source: "qrc:/ArchiveCouponsList.qml" }
            ListElement { title: qsTr("Statistics")}//; source: "qrc:/Statistics.qml" }
            ListElement { title: qsTr("About")}//; source: "qrc:/About.qml" }
            ListElement { title: qsTr("Actual")}//; source: "qrc:/ActualCouponsList.qml";}
            ListElement { title: qsTr("Archive")}//; source: "qrc:/ArchiveCouponsList.qml" }
            ListElement { title: qsTr("Statistics")}//; source: "qrc:/Statistics.qml" }
            ListElement { title: qsTr("About")}//; source: "qrc:/About.qml" }
            ListElement { title: qsTr("Actual")}//; source: "qrc:/ActualCouponsList.qml";}
            ListElement { title: qsTr("Archive")}//; source: "qrc:/ArchiveCouponsList.qml" }
            ListElement { title: qsTr("Statistics")}//; source: "qrc:/Statistics.qml" }
            ListElement { title: qsTr("About")}//; source: "qrc:/About.qml" }
            ListElement { title: qsTr("Actual")}//; source: "qrc:/ActualCouponsList.qml";}
            ListElement { title: qsTr("Archive")}//; source: "qrc:/ArchiveCouponsList.qml" }
            ListElement { title: qsTr("Statistics")}//; source: "qrc:/Statistics.qml" }
            ListElement { title: qsTr("About")}//; source: "qrc:/About.qml" }
            ListElement { title: qsTr("Actual")}//; source: "qrc:/ActualCouponsList.qml";}
            ListElement { title: qsTr("Archive")}//; source: "qrc:/ArchiveCouponsList.qml" }
            ListElement { title: qsTr("Statistics")}//; source: "qrc:/Statistics.qml" }
            ListElement { title: qsTr("About")}//; source: "qrc:/About.qml" }
            ListElement { title: qsTr("Actual")}//; source: "qrc:/ActualCouponsList.qml";}
            ListElement { title: qsTr("Archive")}//; source: "qrc:/ArchiveCouponsList.qml" }
            ListElement { title: qsTr("Statistics")}//; source: "qrc:/Statistics.qml" }
            ListElement { title: qsTr("About")}//; source: "qrc:/About.qml" }
            ListElement { title: qsTr("Actual")}//; source: "qrc:/ActualCouponsList.qml";}
            ListElement { title: qsTr("Archive")}//; source: "qrc:/ArchiveCouponsList.qml" }
            ListElement { title: qsTr("Statistics")}//; source: "qrc:/Statistics.qml" }
            ListElement { title: qsTr("About")}//; source: "qrc:/About.qml" }
            ListElement { title: qsTr("Actual")}//; source: "qrc:/ActualCouponsList.qml";}
            ListElement { title: qsTr("Archive")}//; source: "qrc:/ArchiveCouponsList.qml" }
            ListElement { title: qsTr("Statistics")}//; source: "qrc:/Statistics.qml" }
            ListElement { title: qsTr("About")}//; source: "qrc:/About.qml" }
            ListElement { title: qsTr("Actual")}//; source: "qrc:/ActualCouponsList.qml";}
            ListElement { title: qsTr("Archive")}//; source: "qrc:/ArchiveCouponsList.qml" }
            ListElement { title: qsTr("Statistics")}//; source: "qrc:/Statistics.qml" }
            ListElement { title: qsTr("About")}//; source: "qrc:/About.qml" }
            ListElement { title: qsTr("Actual")}//; source: "qrc:/ActualCouponsList.qml";}
            ListElement { title: qsTr("Archive")}//; source: "qrc:/ArchiveCouponsList.qml" }
            ListElement { title: qsTr("Statistics")}//; source: "qrc:/Statistics.qml" }
            ListElement { title: qsTr("About")}//; source: "qrc:/About.qml" }

        }
    }
    /*
    Image {
        id: image
        anchors.fill: parent

        source: "qrc:/qtquickplugin/images/template_image.png"


//        SideBar {
//            id: sideBar
//            x: 8
//            width: 40
//            anchors.top: parent.top
//            anchors.topMargin: 37
//            anchors.bottom: parent.bottom
//            anchors.bottomMargin: 0

//        }
    }
*/

    ToolButton {
        id: toolButton
        width: 37
        height: 40
        text: qsTr("Tool Button")
        spacing: 9
        anchors.top: parent.top
        anchors.topMargin: 0
        anchors.left: parent.left
        anchors.leftMargin: 0
        focusPolicy: Qt.WheelFocus
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
    /*
    ListView {
        id: listView
        width: parent.width/5 > 110?parent.width/5:110
        anchors.left: parent.left
        anchors.leftMargin: 404
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
        anchors.top: toolBar.bottom
        anchors.topMargin: 44
        delegate: Item {
            x: 5
            width: parent.width
            height: 40
            Row {
                id: row1
                width: parent.width
                Rectangle {
                    id: reCto
                    // anchors.right: parent.left
                    width: parent.width
                    height: 40
                    color: colorCode
                }
                //                Text {
                //                    text: name
                //                    color: colorCode
                //                    font.bold: true
                //                    anchors.left:reCto.right

                //                }
                spacing: 10
            }
        }
        model: ListModel {
            ListElement {
                name: "Yellow"
                colorCode: "yellow"
            }

            ListElement {
                name: "Red"
                colorCode: "red"
            }

            ListElement {
                name: "Blue"
                colorCode: "blue"
            }

            ListElement {
                name: "Green"
                colorCode: "green"
            }
        }
    }

    */
}
