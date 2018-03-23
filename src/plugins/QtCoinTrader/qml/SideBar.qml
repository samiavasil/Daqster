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
    property alias delegate: listV.delegate
    property alias bgopacity: slider.value
    //color: "#005566"
    width: 100
    color: "#000000"
    gradient: Gradient {
        GradientStop {
            position: 0.00;
            color: "#000000";
        }
        GradientStop {
            position: 1.00;
            color: "#ffffff";
        }
    }
    anchors.bottom: parent.bottom

    //  radius: width/2
    ListView{
        id: listV
        anchors.bottomMargin: 40
        anchors.fill: parent
        anchors.bottom: slider.top
        interactive: true
        highlightRangeMode: ListView.StrictlyEnforceRange
        orientation: ListView.Vertical
        keyNavigationWraps: false
        snapMode: ListView.NoSnap
        flickableDirection: Flickable.AutoFlickDirection
        spacing: 2
        clip: false
        visible: true
        highlight: Rectangle{color:"#00ff00"}
        //  orientation: ListView.Horizontal
        layoutDirection: Qt.LeftToRight
    }

    Slider{
        id:slider
        y: 440
        height: 40
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0
        anchors.right: parent.right
        anchors.rightMargin: 0
        anchors.left: parent.left
        anchors.leftMargin: 0
        from: 0.8
        value: 0.9

    }
}
