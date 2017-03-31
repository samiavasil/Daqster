/************************************************************************
                        Daqster/QBasePluginObject.h.h - Copyright vvasilev
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

Initial version of this file was created on 12.03.2017 at 20:54:50
**************************************************************************/

#ifndef QBASEPLUGINOBJECT_H
#define QBASEPLUGINOBJECT_H
#include <QObject>
#include "global.h"

namespace Daqster {

class QPluginObjectsInterface;
/**
  * class QBasePluginObject
  * 
  */

class FRAME_WORKSHARED_EXPORT QBasePluginObject : public QObject
{
    Q_OBJECT
public:

  // Constructors/Destructors
  //  
  /**
   * Empty Constructor
   */
  QBasePluginObject (  QObject* Parent = NULL );

  /**
   * Empty Destructor
   */
  virtual ~QBasePluginObject ();

protected:
  // Pointer to plugin interface object
  const Daqster::QPluginObjectsInterface* m_InterfaceObject;
};
} // end of package namespace

#endif // QBASEPLUGINOBJECT_H
