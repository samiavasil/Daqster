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
    QList<QTreeWidgetItem *> items;
    QList<Daqster::PluginDescription> PlugList = QPluginManager::instance()->GetPluginList( Filter );
    foreach ( Daqster::PluginDescription Desc , PlugList )
    {
        QTreeWidgetItem* it = new QTreeWidgetItem((QTreeWidget*)0);
        for(int j =0;j<5;j++){
            it->setData( j,Qt::DisplayRole, QString("Col %1").arg(j)  );
        }
        items.append(it);
    }

/*    QString m_Author;
    QString m_Description;
    QString m_DetailDescription;
    QIcon m_Icon;
    QString m_License;
    QString m_Location;
    QString m_Name;
    Daqster::PluginType_t m_PluginType;
    QString m_PluginTypeName;
    QString m_Version;
    bool m_Enabled;
    QString m_Hash;
  */



    treeWidget->insertTopLevelItems(0, items);
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
