import QtQuick 2.0
import QtQuick.Controls 2.3
import QtCharts 2.0

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

        ChartView {

            anchors.top: img.bottom
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 10
            theme: ChartView.ChartThemeBrownSand
            antialiasing: true

//            LineSeries {
//                id: pieSeries
//                PieSlice { label: "eaten"; value: 94.9 }
//                PieSlice { label: "not yet eaten"; value: 5.1 }
//            }

            LineSeries {
                name: "LineSeries"
                XYPoint { x: 0; y: 0 }
                XYPoint { x: 1.1; y: 2.1 }
                XYPoint { x: 1.9; y: 3.3 }
                XYPoint { x: 2.1; y: 2.1 }
                XYPoint { x: 2.9; y: 4.9 }
                XYPoint { x: 3.4; y: 3.0 }
                XYPoint { x: 4.1; y: 3.3 }
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
