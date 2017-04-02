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
#include "global.h"
#include "PluginFilter.h"
#include "PluginDescription.h"
#include <QObject>
#include <QList>
#include <QMap>
#include <QString>
#include<QMutex>

namespace Daqster {


class QPluginListView;
class QPluginObjectsInterface;
class QBasePluginObject;
/**
 * @brief The QPluginManager class  is used to manage all availlable plugins.
 * It search for availlable/new plugins and add plugin factories for every one plugin.
 * For optimization this plugin create Qsetting based persistency store for plugins information.
 * On start it check configurable directories for plugins and compare with  persystency
 * information for new available plugins. If have a new plugin (some wich isn't stored in persistency)
 * it automatically load plugin interface object and read plugin information and update persistency.
 * QPluginManager create plugins factories only for these plugins wich are enabled.
 * By default new plugins are enabled(?) and to change to disable state you can be use PluginManagerGUI
 * wich can be called by member function of PluginManager - ShowPluginManagerGUI.
 * In order to have a good plugin files traceability plugin manager create Hash of every one plugin
 * file and when check for availability it compare and and file Hash's. Plugin files are different if
 * their Hash's are different.
 * Note: Please don't use instance of this class directly on your code. Instead get global instance to
 * to object from this class with function GetApplicationPluginManager.
 */
#include<assert.h>
class FRAME_WORKSHARED_EXPORT QPluginManager : public QObject
{
public:

  // Constructors/Destructors
  //  

   static QPluginManager* instance();

  /**
   * Return list with founded plugins. Return list can be filtered by criteria
   * described in input filter parameter.
   * @param  Filter Plugin filtration object
   */
  QList<Daqster::PluginDescription> GetPluginList ( const PluginFilter &Filter=PluginFilter() );


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

  QBasePluginObject *CreatePluginObject(const QString &KeyHash);



protected:
  /**
   * Empty Constructor
   */
  QPluginManager ( const QString& ConfigFile = QString("daqster.ini") );

  QPluginManager( QPluginManager const& );

  QPluginManager& operator= (QPluginManager const&);

  /**
   * Empty Destructor
   */
  virtual ~QPluginManager ();
  /**
   * @brief FileHash calculate Hash of some file
   * @param Filename
   * @param Hash result
   * @return true on success
   *         false otherwise
   */
  bool FileHash( const QString &Filename, QString& Hash );

  /**
   * @brief QPluginManager::LoadPluginsInfoFromPersistency Load plugins information from persistency
   */
  void LoadPluginsInfoFromPersistency();

   bool LoadPluginInterfaceObject(const QString &PluginFileName,const QString& Hash  );

   void StorePluginStateToPersistncy(QPluginObjectsInterface *ObjInterface);
protected:
  /*Pointer to sinleton obejct*/
  static QPluginManager* g_Instance;
  // Plugins Directory list
  QList<QString> m_DirList;
  // Map with founded plugins: Map file Hash with PluginDescription   bb
  QMap<QString, Daqster::PluginDescription> m_PluginsHashDescMap;
  // Map Hash to plugin base interface object QPluginObjectsInterface.
  QMap<QString,Daqster::QPluginObjectsInterface*> m_PluginMap;
  QString m_ConfigFile;


};


} // end of package namespace

#endif // QPLUGINMANAGER_H
