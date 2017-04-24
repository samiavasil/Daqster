/************************************************************************
                        Daqster/QBasePluginObject.cpp.cpp - Copyright vvasilev
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

#include "QBasePluginObject.h"

namespace Daqster {
// Constructors/Destructors
//  

QBasePluginObject::QBasePluginObject (QObject *Parent):QObject( Parent ),m_PoState(QBasePluginObject::WORKING_STATE)
{
    m_InterfaceObject = NULL;
}

QBasePluginObject::~QBasePluginObject ()
{

}

QBasePluginObject::eShutdownStatus QBasePluginObject::ShutdownPluginObject()
{
    m_PoState = SHUTTING_DOWN_STATE;
    deleteLater();
    m_PoState = TURNED_OFF_STATE;
    return m_PoState;
}

QBasePluginObject::eShutdownStatus QBasePluginObject::GetPluginObjectStatus()
{
    return m_PoState;
}

}
