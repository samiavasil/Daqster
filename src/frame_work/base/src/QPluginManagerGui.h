/************************************************************************
                        Daqster/QPluginManagerGui.h.h - Copyright 
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


#ifndef QPLUGINMANAGERGUI_H
#define QPLUGINMANAGERGUI_H
#include "global.h"
#include <QDialog>

namespace Ui {
   class PluginManagerGui;
}


namespace Daqster {
class QPluginListView;

class FRAME_WORKSHARED_EXPORT QPluginManagerGui : public QDialog
{
public:

  // Constructors/Destructors
  //  


  /**
   * Empty Constructor
   */
  QPluginManagerGui (QWidget *Parent = NULL );

  /**
   * Empty Destructor
   */
  virtual ~QPluginManagerGui ();


protected:

  // Protected attributes

  // Plugn list view
  Daqster::QPluginListView*  m_PluginList;
private:
  Ui::PluginManagerGui* ui;
};
} // end of package namespace

#endif // QPLUGINMANAGERGUI_H
