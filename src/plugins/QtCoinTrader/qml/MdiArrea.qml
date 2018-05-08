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
    property alias modelGrid: delModel.model

    function  addToModel( a ) {
       // delModel.model.append(  a )

    }

    function setGroups(index) {
        console.log('set Groups for', index)
        var groups = delModel.items.get(index).groups
      //  groups.contains('tested')
        delModel.items.setGroups(index, 1, (groups.indexOf('tested')>=0)?['items']:['items', 'tested'])
    }

    keyNavigationWraps: true
    interactive: true
    clip: true
    cellHeight:  height/visRow
    cellWidth:   width/visCol

    CheckBox{
        checked: false
        onClicked: {
            modelGrid.append(   { colorM: "orange" } )
        }
    }

    model: DelegateModel{
         id : delModel

//        model:ViewModel  {
//            id: lModel

//            //                ListElement { colorM: "orange"
//            //                              textL:   "test"
//            //                }
//        }

        groups : [DelegateModelGroup {name: "tested"}]

        filterOnGroup:"tested"

        delegate:  ViewWin{
            id: item
            width: grid.cellWidth
            height: grid.cellHeight
            text:   title
//            text: {
//                var text = "Name: " + title
//                if ( item.DelegateModel.inTested )
//                    text += " (" + item.DelegateModel.testedIndex + ")"
//                item.DelegateModel.inTested =!item.DelegateModel.inTested
//                return text;
//            }
            color:  colorM
            source: imgSource

        }

        //     drawer.close()
//        console.log(model, "  ", model.inTested )
//        model.inTested= !model.inTested
//        console.log("model.inTested: ", model.inTested )
    }

}

