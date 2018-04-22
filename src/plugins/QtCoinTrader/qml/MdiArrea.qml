import QtQuick 2.0
//import QtQuick.Layouts 1.3
import QtQml.Models 2.2
import QtQuick.Controls 2.2

GridView{
    id: grid
    property int maxCol: 1
    property int maxRow: 2
    property int visibleNum: maxCol*maxRow
    property int visRow:     ((visibleNum/maxCol) + ( visibleNum%maxCol ? 1 : 0 ))
    property int visCol:  ( (visibleNum/visRow) + ( visibleNum%visRow ? 1 : 0 ) )


    function  addToModel(   a ) {
        lModel.append(  a )
    }

    keyNavigationWraps: true
    interactive: true
    clip: true
    cellHeight:  height/visRow
    cellWidth:   width/visCol

    CheckBox{
        checked: false
        onClicked: {
            lModel.append(   { colorM: "orange" } )
        }
    }

    model: DelegateModel{

        model:ViewModel {
            id: lModel

            //                ListElement { colorM: "orange"
            //                              textL:   "test"
            //                }
        }



        delegate:  ViewWin{
            width: grid.cellWidth
            height: grid.cellHeight
            text:  textL
            color: colorM
            source: sourceM
        }

    }

}

