/************************************************************************
                        Daqster/QPluginManager.cpp.cpp - Copyright 
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

#include "QPluginManager.h"
#include "QPluginListView.h"
#include "PluginFilter.h"
#include "QPluginObjectsInterface.h"

namespace Daqster {
// Constructors/Destructors
//  

QPluginManager::QPluginManager () {

}

QPluginManager::~QPluginManager () { }


/**
 * Return list with founded plugins. Return list can be filtered by criteria
 * described in input filter parameter.
 * @param  Filter Plugin filtration object
 */
void QPluginManager::GetPluginList (Daqster::PluginFilter Filter)
{
}


/**
 * This function create PlunListView widget. This function internaly (on PluginView
 * creation) create signal/slot connection betwen PluginManager and PluginViews in
 * order to have dynamic refresh of vies if new plugins are loaded/finded..
 * @return Daqster::QPluginListView*
 * @param  Parrent Pointer to parent QWidget.
 * @param  Filter Optional list filter.
 */
Daqster::QPluginListView*  QPluginManager::CreatePluginListView (QWidget* Parrent, PluginFilter *Filter )
{
}


/**
 * Search for plugins in configured directories.
 */
void QPluginManager::SearchForPlugins ()
{
}


/**
 * Add directory to plugin search path
 * @param  Directory Directory path.
 */
void QPluginManager::AddPluginsDirectory (const QString& Directory)
{
}


/**
 * Show plugin manager GUI widget. In this GUI you can see available plugins,
 * rescan for new plugins, dynamic unload , enable/disable plugin loading.
 */
void QPluginManager::ShowPluginManagerGui ()
{
}
}

