import QtQuick 2.0
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.3

ItemDelegate {

    property string value: ""
    property alias imgSource: image.source
    property alias color: delText.color
    //   height: 80
    highlighted: ListView.isCurrentItem
    anchors.horizontalCenter: parent.horizontalCenter
    ColumnLayout{
        anchors.fill: parent
        Image {
            id: image

            horizontalAlignment: Image.AlignHCenter
         //   width:
        //    height: parent.height
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 5
            fillMode: Image.PreserveAspectFit
        }

        Text{
            id: delText
            text: value
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.bottomMargin: 5
       //     verticalAlignment: Text.AlignVCenter

            font.family: "Times New Roman";
          //  font.pointSize: 15;
           // font.bold: true

            minimumPointSize: 4
            fontSizeMode: Text.HorizontalFit
            horizontalAlignment: Text.AlignHCenter

        }
    }


    //    MouseArea {
    //        anchors.fill: parent
    //        onClicked: list.currentIndex = index
    //    }

}

