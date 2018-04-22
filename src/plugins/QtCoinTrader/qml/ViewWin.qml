import QtQuick 2.0
import QtQuick.Controls 2.3

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
    }
    ToolButton{
        width: 37
        height: 40
        text: qsTr("Quit Button")
        anchors.top: parent.top
        anchors.right: parent.right
    }
}
