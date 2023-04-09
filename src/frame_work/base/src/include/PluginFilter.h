/************************************************************************
                        Daqster/PluginFilter.h.h - Copyright 
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


#ifndef PLUGINFILTER_H
#define PLUGINFILTER_H
#include "build_cfg.h"
#include <QMap>

namespace Daqster {

class PluginDescription;

/**
  * class PluginFilter
  * Class is used for plugin filtration by some properties.
  */

class FRAME_WORKSHARED_EXPORT PluginFilter
{
public:

  // Constructors/Destructors
  //  

  /**
   * Empty Constructor
   */
  PluginFilter ();

  /**
   * Empty Destructor
   */
  virtual ~PluginFilter ();

  void AddFilter( const QString& Property,const QString& Value );
  /**
   * This function test is the plugin described with input parameter is filtered or
   * not.
   * @return bool
   * @param  _Description Plugin description
   */
  bool IsFiltered (const  Daqster::PluginDescription& Description) const;
private:
    QMap<QString,QString> m_MapProperties;
};
} // end of package namespace

#endif // PLUGINFILTER_H
