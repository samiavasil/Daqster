/************************************************************************
                        Daqster/QPluginManager.h.h - Copyright 
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


#ifndef QPLUGINMANAGER_H
#define QPLUGINMANAGER_H
#include "base/global.h"
#include <QObject>
#include "PluginFilter.h"
#include <QList>
#include <QMap>
#include <QString>



namespace Daqster {

class PluginFilter;
class QPluginListView;
class QPluginObjectsInterface;

class FRAME_WORKSHARED_EXPORT QPluginManager : public QObject
{
public:

  // Constructors/Destructors
  //  


  /**
   * Empty Constructor
   */
  QPluginManager ();

  /**
   * Empty Destructor
   */
  virtual ~QPluginManager ();



  /**
   * Return list with founded plugins. Return list can be filtered by criteria
   * described in input filter parameter.
   * @param  Filter Plugin filtration object
   */
  void GetPluginList (Daqster::PluginFilter Filter);


  /**
   * This function create PlunListView widget. This function internaly (on PluginView
   * creation) create signal/slot connection betwen PluginManager and PluginViews in
   * order to have dynamic refresh of vies if new plugins are loaded/finded..
   * @return Daqster::QPluginListView*
   * @param  Parrent Pointer to parent QWidget.
   * @param  Filter Optional list filter.
   */
  Daqster::QPluginListView*  CreatePluginListView (QWidget* Parrent = NULL, Daqster::PluginFilter* Filter = NULL);


  /**
   * Search for plugins in configured directories.
   */
  void SearchForPlugins ();


  /**
   * Add directory to plugin search path
   * @param  Directory Directory path.
   */
  void AddPluginsDirectory (const QString& Directory);

  /**
   * Show plugin manager GUI widget. In this GUI you can see available plugins,
   * rescan for new plugins, dynamic unload , enable/disable plugin loading.
   */
  void ShowPluginManagerGui ();

protected:

  // Protected attributes
  //  

  // List with founded plugins
  QList<Daqster::PluginDescription> PluginsList;

protected:

  // Map contains path to plugin and pointer to plugin base interface object QPluginObjectsInterface.
  QMap<QString,Daqster::QPluginObjectsInterface*> m_PluginMap;
  QList<QString> m_DirList;

};
} // end of package namespace

#endif // QPLUGINMANAGER_H
