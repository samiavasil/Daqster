import QtQuick 2.0
import QtQuick.Controls 2.3
ItemDelegate {

    property string value: ""
    property alias imgSource: image.source

    height: 80
    highlighted: ListView.isCurrentItem
    anchors.horizontalCenter: parent.horizontalCenter

    Image {
        id: image
        anchors.top: parent.top
        anchors.topMargin: 17
        anchors.horizontalCenterOffset: 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 39
        fillMode: Image.PreserveAspectFit
    }
    Text{
        id: delText
        color: "#e5ea19"
        text: value
        anchors.right: parent.right
        anchors.rightMargin: 0
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.top: parent.top
        anchors.topMargin: 55
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
        verticalAlignment: Text.AlignVCenter

        font.family: "Times New Roman";
        font.pointSize: 10;
        font.bold: true


        fontSizeMode: Text.HorizontalFit
        horizontalAlignment: Text.AlignHCenter


    }

//    MouseArea {
//        anchors.fill: parent
//        onClicked: list.currentIndex = index
//    }

}

