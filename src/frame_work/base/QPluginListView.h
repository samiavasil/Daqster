/************************************************************************
                        Daqster/QPluginListView.h.h - Copyright 
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


#ifndef QPLUGINLISTVIEW_H
#define QPLUGINLISTVIEW_H
#include "global.h"
#include <QWidget>
#include "PluginFilter.h"

namespace Ui {
    class PluginListView;
}

class QTreeWidgetItem;

namespace Daqster {


/**
  * class QPluginListView
  * This class implement plugins view Widget. It show plugins in list with
  * categories. It support plugin filtration features ( by type, subtypes, etc....
  * tbd ).
  */

class FRAME_WORKSHARED_EXPORT QPluginListView : public QWidget
{
    Q_OBJECT
public:

  // Constructors/Destructors

  /**
  * Constructor
  * @param  Filter Plugin filtrato parameter
  */
  QPluginListView ( QWidget* Parent = NULL ,const Daqster::PluginFilter& Filter = Daqster::PluginFilter() );

  /**
   * Empty Destructor
   */
  virtual ~QPluginListView ();

  /**
   * Set view plugin flter.
   * @param  Filter
   */
  void SetPluginFilter (const Daqster::PluginFilter& Filter);

protected slots:

  /**
   * @brief Refresh plugin list view slot
   */
  void RefreshView();

  void TreeItem(QTreeWidgetItem *item, int col);

  void ShowDetails();

signals:
    void EnableDisablePlugin( const QString &Hash, bool Enable );

protected:
  // Plugin filter
    Daqster::PluginFilter m_PluginFilter;

private:
  Ui::PluginListView* ui;
};
} // end of package namespace

#endif // QPLUGINLISTVIEW_H
