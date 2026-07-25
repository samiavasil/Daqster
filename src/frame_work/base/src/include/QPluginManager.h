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
#include "build_cfg.h"
#include "PluginFilter.h"
#include "PluginDescription.h"
#include <QObject>
#include <QList>
#include <QMap>
#include <QString>
#include <memory>
#include<QMutex>

class QDialog;

namespace Daqster {

class PluginFilter;
class QPluginInterface;
class QBasePluginObject;
class PluginDiscovery;
class PluginRegistry;
class PluginPersistence;

/**
 * @brief The QPluginManager class — thin facade delegating to split classes.
 *
 * QPluginManager is the public API for plugin management. Internally it
 * delegates to three focused classes:
 *   - PluginDiscovery  — file scanning and hash computation
 *   - PluginRegistry   — runtime registration, lifecycle, capability discovery
 *   - PluginPersistence — QSettings-based state storage
 *
 * Note: Please don't use instance of this class directly on your code.
 * Instead get global instance with QPluginManager::instance().
 */
class FRAME_WORKSHARED_EXPORT QPluginManager : public QObject // skipcq: CXX-W2009
{
    Q_OBJECT
public:

  // Constructors/Destructors
  //  

   static QPluginManager* instance();

   static bool Initialize();

  /**
   * Return list with founded plugins. Return list can be filtered by criteria
   * described in input filter parameter.
   * @param  Filter Plugin filtration object
   */
  QList<Daqster::PluginDescription> GetPluginList ( const PluginFilter &Filter = PluginFilter());

  Daqster::PluginDescription GetPluginDescriptionByHash ( const QString &Hash );

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
   * Implementation is in frame_work_gui library.
   */
  void ShowPluginManagerGui ( QWidget *Parent = nullptr );

   QBasePluginObject *CreatePluginObject(const QString &KeyHash, QObject *Parent = nullptr);

   /**
    * @brief Return all plugin instances that implement a given interface (by IID).
    */
   QObjectList instances(const char* iid);

public slots:
   void EnableDisablePlugin( const QString& Hash, bool Enable );

   void EnableDisablePluginList( const QList<QString>& HashList, bool Enable );

   void AllPluginObjectsDestroyed( const QString& Hash );

   void ShutdownPluginManager();

signals:
  void PluginsListChangeDetected();
  void AllPluginsShutdownFinished( bool Status );

protected:
  QPluginManager ( const QString& ConfigFile = QString("daqster.ini") );
  QPluginManager( QPluginManager const& );
  QPluginManager& operator= (QPluginManager const&);
  virtual ~QPluginManager ();

  // ── Delegate methods (implemented via split classes) ──────────
  void LoadPluginsInfoFromPersistency();
  bool LoadPluginInterfaceObject(const QString &PluginFileName,const QString& Hash);
  void ShutdownPlugin(const QString &Hash);

protected:
  static QPluginManager* g_Instance;

  // ── Split classes (owned) ─────────────────────────────────────
  std::unique_ptr<PluginDiscovery> m_discovery;
  std::unique_ptr<PluginRegistry> m_registry;
  std::unique_ptr<PluginPersistence> m_persistence;
};


} // end of package namespace

#endif // QPLUGINMANAGER_H
