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
************************************************************************/

#include "QPluginListView.h"
#include "debug.h"
#include "ui_pluginlistview.h"
#include "QPluginManager.h"
#include <QMap>
#include <QTreeWidgetItem>
#include "PluginDetails.h"

namespace Daqster {

#define TREE_DATA_ROLE (Qt::UserRole+1)
#define CHECK_ROOT_HELPER (TREE_DATA_ROLE+1)

QPluginListView::QPluginListView ( QWidget* Parent ,const Daqster::PluginFilter& Filter ):QWidget(Parent)
{
    m_PluginFilter = Filter;
    ui = new Ui::PluginListView();
    ui->setupUi( this );
    ui->treeWidget->setColumnCount( 5 );
    RefreshView();

    connect( QPluginManager::instance(), SIGNAL(PluginsListChangeDetected()), this, SLOT(RefreshView()), Qt::QueuedConnection );
    connect( this, SIGNAL(EnableDisablePlugin(QString,bool)), QPluginManager::instance(), SLOT(EnableDisablePlugin(QString,bool)), Qt::QueuedConnection );
    connect( ui->treeWidget,SIGNAL(itemChanged(QTreeWidgetItem*,int)),this, SLOT(TreeItem(QTreeWidgetItem*,int)),Qt::QueuedConnection);
    connect(ui->detailsButton, SIGNAL(pressed()), this, SLOT(ShowDetails()) );
}

void QPluginListView::TreeItem( QTreeWidgetItem* item, int col ){
    if( nullptr != item && col == 1){
        DEBUG << "Plugin " << item->data( col, TREE_DATA_ROLE).toString() << ": " << item->checkState(col);
        if( nullptr != item->parent() ){
            bool Enable = item->checkState(col) == Qt::Unchecked ? false : true;
            emit EnableDisablePlugin(  item->data( col, TREE_DATA_ROLE).toString(), Enable );
        }
        else{
            switch ( item->checkState(col) ) {
            case Qt::Checked:{
                for( int i = 0; i < item->childCount(); i++ ) {
                    emit EnableDisablePlugin(  item->child( i )->data( col, TREE_DATA_ROLE).toString(), true );
                }
                break;
            }
            case Qt::Unchecked:{
                for( int i = 0; i < item->childCount(); i++ ) {
                    emit EnableDisablePlugin(  item->child( i )->data( col, TREE_DATA_ROLE).toString(), false );
                }
                break;
            }
            case Qt::PartiallyChecked:{
                item->setCheckState( col, Qt::Checked );
                break;
            }
            default:
                break;
            }
        }
    }
}

void QPluginListView::ShowDetails()
{
    PluginDetails Details;
    QTreeWidgetItem* item = ui->treeWidget->currentItem();
    if( nullptr != item ){
        Details.setPluginDescription( QPluginManager::instance()->GetPluginDescriptionByHash(item->data( 1, TREE_DATA_ROLE).toString() ) );
        Details.exec();
    }
}

QPluginListView::~QPluginListView () {
    delete ui;
}

void QPluginListView::SetPluginFilter (const PluginFilter &Filter)
{
    m_PluginFilter = Filter;
}

/**
 * Helper: add a plugin description as a child under a root tree item.
 */
static void addPluginToRoot(QTreeWidgetItem* root_it, const Daqster::PluginDescription& Desc)
{
    QTreeWidgetItem *it = new QTreeWidgetItem(static_cast<QTreeWidget*>(nullptr));
    Qt::CheckState CheckState = Desc.IsEnabled() ? Qt::Checked : Qt::Unchecked;
    Qt::CheckState RootCheckState = root_it->checkState(1);
    int childCounter = root_it->data(0, CHECK_ROOT_HELPER).toInt();

    it->setIcon(0, Desc.GetIcon());
    it->setData(0, Qt::DisplayRole, Desc.GetProperty(PLUGIN_NAME).toString());
    it->setCheckState(1, CheckState);
    it->setData(1, TREE_DATA_ROLE, Desc.GetProperty(PLUGIN_HASH));
    it->setData(2, Qt::DisplayRole, Desc.GetProperty(PLUGIN_VERSION).toString());
    it->setData(3, Qt::DisplayRole, Desc.GetProperty(PLUGIN_AUTHOR).toString());
    it->setData(4, Qt::DisplayRole, Desc.GetProperty(PLUGIN_DESCRIPTION).toString());
    root_it->addChild(it);

    if (childCounter == 0) {
        RootCheckState = CheckState;
    } else if (CheckState != RootCheckState) {
        RootCheckState = Qt::PartiallyChecked;
    }
    childCounter++;
    root_it->setData(0, CHECK_ROOT_HELPER, childCounter);
    root_it->setCheckState(1, RootCheckState);
}

/**
 * @brief Refresh plugin list view slot
 *
 * Groups plugins by PLUGIN_TYPE_NAME from PluginDescription.
 * Plugins without a type name are grouped under "Plugins".
 */
void QPluginListView::RefreshView(){
    QMap<QString, QTreeWidgetItem *> Map;
    QList<Daqster::PluginDescription> PlugList = QPluginManager::instance()->GetPluginList( m_PluginFilter );
    QTreeWidget *treeWidget = nullptr;
    ui->treeWidget->blockSignals(true);

    treeWidget = ui->treeWidget;
    treeWidget->setColumnCount(5);
    ui->treeWidget->clear();

    foreach ( const Daqster::PluginDescription& Desc , PlugList )
    {
        QString typeName = Desc.GetProperty(PLUGIN_TYPE_NAME).toString();
        if (typeName.isEmpty()) {
            typeName = tr("Plugins");
        }

        QTreeWidgetItem *root_it = Map.value(typeName, nullptr);
        if( nullptr == root_it ){
            root_it = new QTreeWidgetItem(static_cast<QTreeWidget*>(nullptr));
            root_it->setData( 0, Qt::DisplayRole, typeName );
            root_it->setData( 0, CHECK_ROOT_HELPER, 0 );
            root_it->setFlags(Qt::ItemIsUserTristate|Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
            root_it->setCheckState( 1, Qt::Unchecked);
            Map[typeName] = root_it;
        }

        addPluginToRoot(root_it, Desc);
    }

    QStringList HeaderList;
    HeaderList << "Name" << "Enable" << "Version" << "Author" << "Description";
    treeWidget->setHeaderLabels( HeaderList );
    treeWidget->header()->setSectionResizeMode( QHeaderView::ResizeToContents );
    treeWidget->insertTopLevelItems(0, Map.values());
    treeWidget->expandAll();
    ui->treeWidget->blockSignals(false);
}

}
