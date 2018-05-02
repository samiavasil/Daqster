import QtQuick 2.6

import QtQuick 2.5
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.3
//import QtQuick.Controls.Material 2.1
import QtQuick.Window 2.1

Rectangle {
    id: rectangle

    property alias currentIndex: listV.currentIndex
    property alias model: listV.model
    property alias bgopacity: slider.value
    property alias colorValue: rectangle.color
 //   property alias mouseArr: mouseArrea

    width: 100
    property alias slider: slider
    //    color: "#000000"
    //    gradient: Gradient {
    //        GradientStop {
    //            position: 0.00;
    //            color: "#000000";
    //        }
    //        GradientStop {
    //            position: 1.00;
    //            color: "#ffffff";
    //        }
    //    }
    anchors.bottom: parent.bottom

    //  radius: width/2
    signal sendIdx(int a)

    ColumnLayout {
        anchors.fill: parent
        clip: false

        ListView{
            id: listV
            Layout.fillWidth: true
            Layout.fillHeight: true
            boundsBehavior: Flickable.StopAtBounds

            flickableDirection: Flickable.AutoFlickDirection
            interactive: true
            highlightRangeMode: ListView.ApplyRange
            orientation: ListView.Vertical
       //     keyNavigationWraps: yes
            snapMode: ListView.SnapToItem
            //      flickableDirection: Flickable.AutoFlickDirection
            spacing: 2
            clip: true
            visible: true
            highlight: Rectangle { color: "lightsteelblue"; radius: 5 }

            //  orientation: ListView.Horizontal
            layoutDirection: Qt.LeftToRight
            delegate: SideBarDelegate{
                id: sideBarDelegate
                width: parent.width
                height: (3*width)/4
                value: model.title
                imgSource: model.imgSource
                //    font.pointSize: 20
                //     ToolTip.visible: hovered
                //     ToolTip.text: model.title
                MouseArea {
                    id: mouseArrea
                    anchors.fill: parent
                    onClicked: {
                        if (listV.currentIndex != index) {
                            listV.currentIndex = index
                            //      ListView.replace(model.source)
                            //   Qt.quit()
                            console.log("index = ", listV.currentIndex)
                            sendIdx(listV.currentIndex)

                        }
                        //     drawer.close()
                        //console.log(model, "  ", model.inTested )
                        model.inTested= !model.inTested
                        //console.log("model.inTested: ", model.inTested )
                    }
                }
            }

        }

        Slider{
            id:slider
            Layout.fillHeight: true
            Layout.maximumWidth: 5535
            Layout.maximumHeight: 40
            Layout.fillWidth: true
            Layout.preferredHeight: 37
            from: 0.8
            value: 0.9

        }
    }
}
