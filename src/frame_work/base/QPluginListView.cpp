/************************************************************************
                        Daqster/QPluginListView.cpp.cpp - Copyright 
Daqster software
Copyright (C) 2016, Vasil Vasilev,  Bulgaria

This file is part of Daqster and its software development toolkit.

Daqster is a free software; you can redistribute it and/or modify it
under the terms of the GNU Library General Public Licence as published by
the Free Software Foundation; either version 2 of the Licence, or (at
your option) any later version.

Daqster is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library
General Public Licence for more details.

Initial version of this file was created on 16.03.2017 at 11:40:20
**************************************************************************/

#include "QPluginListView.h"
#include "ui_pluginlistview.h"
#include "QPluginManager.h"
#include <QMap>


namespace Daqster {
// Constructors/Destructors
//  

/**
 * Constructor
 * @param  Filter Plugin filtrato parameter
 */
QPluginListView::QPluginListView ( QWidget* Parent ,const Daqster::PluginFilter& Filter ):QWidget(Parent)
{
    m_PluginFilter = Filter;
    ui = new Ui::PluginListView();
    ui->setupUi( this );
    QTreeWidget *treeWidget = ui->treeWidget;
    treeWidget->setColumnCount(5);


    QMap<PluginDescription::PluginType_t, QTreeWidgetItem *> Map;
    QList<Daqster::PluginDescription> PlugList = QPluginManager::instance()->GetPluginList( Filter );
    PluginDescription::PluginType_t Type;
    QTreeWidgetItem *root_it, *it;
    foreach ( Daqster::PluginDescription Desc , PlugList )
    {
        Type = (PluginDescription::PluginType_t)Desc.GetProperty(PLUGIN_TYPE).toUInt();
        root_it   = Map.value( Type, NULL );
        if( NULL == root_it ){
           root_it = new QTreeWidgetItem((QTreeWidget*)0);
           root_it->setData( 0,Qt::DisplayRole, tr("Plugin Type %1").arg(Type) );
           root_it->setFlags(Qt::ItemIsUserTristate|Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
           root_it->setCheckState( 1, Qt::PartiallyChecked);

           Map[Type] = root_it;
        }
        if( root_it ){
            it = new QTreeWidgetItem((QTreeWidget*)0);
            if( NULL != it ){
                it->setIcon( 0, Desc.GetProperty( PLUGIN_ICON ).value<QIcon>() );
                it->setData( 0,Qt::DisplayRole, Desc.GetProperty(PLUGIN_NAME).toString() );
                it->setCheckState( 1, Desc.IsEnabled() ? Qt::Checked : Qt::Unchecked );
                it->setData( 2, Qt::DisplayRole, Desc.GetProperty(PLUGIN_VERSION).toString() );
                it->setData( 3, Qt::DisplayRole, Desc.GetProperty(PLUGIN_AUTHOR).toString() );
                it->setData( 4, Qt::DisplayRole, Desc.GetProperty(PLUGIN_DESCRIPTION).toString() );
                root_it->addChild( it );
            }
        }

    }



/*************************************
 *
 *   QString m_Name;
 *   QString m_Author;
 *   bool m_Enabled;
 *   QString m_Version;
 *   QString m_Description;


 *  QString m_DetailDescription;
 *  QIcon m_Icon;
 *  QString m_License;
 *  QString m_Location;

 *  Daqster::PluginType_t m_PluginType;
 *  QString m_PluginTypeName;
 *  QString m_Hash;
 *
 ***************************************/

    QStringList HeaderList;
    HeaderList << "Name" << "Enable" << "Version" << "Author" << "Description"; //"Status" <<
    treeWidget->setHeaderLabels( HeaderList );
    treeWidget->insertTopLevelItems(0, Map.values());
}

QPluginListView::~QPluginListView () {
    delete ui;
}

/**
 * Set view plugin flter.
 * @param  Filter
 */
void QPluginListView::SetPluginFilter (const PluginFilter &Filter)
{
    m_PluginFilter = Filter;
}

}
