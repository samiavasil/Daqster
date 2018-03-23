import QtQuick 2.0
import QtQuick.Controls 2.3
ItemDelegate {
    property string value: ""
  //  id: sideBarDelegate
    width: parent.width

    highlighted: ListView.isCurrentItem
    anchors.horizontalCenter: parent.horizontalCenter
    Text{
        color: "#e5ea19"
        text: value
        verticalAlignment: Text.AlignVCenter
        font.pointSize: 12
        font.family: "Times New Roman"
        fontSizeMode: Text.HorizontalFit
        anchors.fill: parent
        horizontalAlignment: Text.AlignHCenter


    }

    //             onClicked: {
    //                 if (listView.currentIndex != index) {
    //                     listView.currentIndex = index
    //                     stackView.replace(model.source)
    //                 }
    //                 drawer.close()
    //             }
}

//Item {
//    id: sideBarDelegate
// anchors.fill: parent
//    width: parent.width
//    Text {
//        property string name: title
//        color: "#ec34f0"

//        font.bold: true
//        horizontalAlignment: Text.AlignHCenter
//        font.pointSize: 10
//        text: "name"
//        styleColor: "#f31515"
//        anchors.fill: parent
//    }
//}

