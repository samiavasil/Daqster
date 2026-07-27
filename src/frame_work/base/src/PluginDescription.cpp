/************************************************************************
                        Daqster/PluginDescription.cpp - Copyright
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

Initial version of this file was created on 16.03.2017 at 12:33:53
**************************************************************************/

#include "PluginDescription.h"
#include "debug.h"
#include<QObject>
#include<QSettings>
#include<QMetaProperty>

namespace Daqster {


/*This is internal private class to store Plugin description*/
class PrivateDescription : public QObject{
    public:
    explicit PrivateDescription( QObject* Parent = nullptr ):QObject(Parent){

    }
    virtual ~PrivateDescription(){

    }
};

QDebug operator<<(QDebug ds, const PluginDescription &obj)
{
    ds << "Enabled: " << obj.IsEnabled() << "\n";
    for (const QByteArray& Name : obj.m_PrivateDescription->dynamicPropertyNames()) {
        ds << Name << ": {" << obj.m_PrivateDescription->property( Name ).toString() << "}\n";
    }
    return ds;
}


// Constructors/Destructors
//  

PluginDescription::PluginDescription()
{
   m_Enabled = true;
   m_PrivateDescription = new PrivateDescription();
}

/**
* @brief Copy constructor
* @param b
*/
PluginDescription::PluginDescription(const PluginDescription& b)
{
    m_PrivateDescription = new PrivateDescription();
    m_Enabled = b.m_Enabled;
    m_Icon = b.m_Icon;
    CopyDynamicProperties( b );
}

PluginDescription::~PluginDescription () {
    if( nullptr != m_PrivateDescription ){
        m_PrivateDescription->deleteLater();
    }
}

/**
* @brief Set Dynamic Property. There is a Predefined properties NAMES but if you
* want you can set additional Properties
* @param name    - If property name isn't defined it dynamicaly created new property
* @param value
*/
void PluginDescription::SetProperty( const char *name, const QVariant &value )
{
    m_PrivateDescription->setProperty( name, value );
}

/**
 * @brief Return Property
 * @param name
 * @return Return property with this name if exist, else return invalid QVariant
 */
QVariant  PluginDescription::GetProperty( const char *name ) const
{
    return m_PrivateDescription->property( name );
}

/**
 * @brief Get List with Properties Names
 * @return List with Properties Names
 */
QList<QByteArray>  PluginDescription::GetPropertiesNames( ) const
{
    return  m_PrivateDescription->dynamicPropertyNames();
}

QMap<QString, QVariant> PluginDescription::GetAllProperties() const
{
    QMap<QString, QVariant> map;
    QList<QByteArray>names = m_PrivateDescription->dynamicPropertyNames();
    for (const auto& name : names) {
        map[name.data()] = m_PrivateDescription->property( name.data() );
    }
    return map;
}

bool  PluginDescription::IsEmpty() const
{
    return ( 0 == m_PrivateDescription->dynamicPropertyNames().count() );
}


void PluginDescription::CopyDynamicProperties( const PluginDescription &b ){
    QList<QByteArray> names = m_PrivateDescription->dynamicPropertyNames();
    /*Delete old properies and copy new ones*/
    QVariant Invalid;
    for (const QByteArray& name : names) {
        m_PrivateDescription->setProperty( name,Invalid );
    }

    names = b.GetPropertiesNames();
    for (const QByteArray& name : names) {
        if( m_PrivateDescription->setProperty( name, b.GetProperty(name) ) ){
            DEBUG << "Strange - set of this dynamic property should return false here. Chek it - maybe it is defined with Q_PROPERTY  macro";
        }
    }
}

/**
 * @brief Overloading Equal operator
 * @param PluginDescription object
 * @return PluginDescription
 */
PluginDescription & PluginDescription::operator=(const PluginDescription &b){
    CopyDynamicProperties( b );
    /*Copy static properties*/
    m_Enabled = b.m_Enabled;
    m_Icon = b.m_Icon;
    return *this;
}

/**
 * @brief Return is plugin enabled
 * @return true/false
 */
bool PluginDescription::IsEnabled() const
{
    return m_Enabled;
}

/**
 * @brief Overloading operator ==
 * @param PluginDescription object
 * @return true if objects are equal
 */
bool  PluginDescription::operator==(const PluginDescription &b) const {
    bool ret = false;
    if( m_Enabled == b.m_Enabled ){
        QList<QByteArray> ThisNames = this->m_PrivateDescription->dynamicPropertyNames();
        QList<QByteArray> BNames    = b.m_PrivateDescription->dynamicPropertyNames();
        if( ThisNames.count() == BNames.count() ){
            ret = true;
            for (const QByteArray& Name : ThisNames) {
                if( this->GetProperty(Name) !=  b.GetProperty(Name) ){
                    ret = false;
                    break;
                }
            }
        }
    }
   return ret;
}

/**
 * @brief Enable plugin
 * @param En - true/false
 */
void PluginDescription::Enable( bool En )
{
    m_Enabled = En;
}

/**
 * @brief Store Plugin Parammeters to Qsetting store
 * @param Store
 * @return
 */
bool PluginDescription::StorePluginParamsToPersistency( QSettings &Store ) const
{
    QList<QByteArray> names = m_PrivateDescription->dynamicPropertyNames();
    for (const QByteArray& name : names) {
        Store.setValue( name, m_PrivateDescription->property(name) );
    }
    Store.setValue( "Enabled", m_Enabled );
    return true;
}

/**
 * @brief Get Plugin Parammeters from Qsetting store
 * @param Store
 * @return
 */
bool PluginDescription::GetPluginParamsFromPersistency( QSettings &Store )
{
    bool ret = false;
    QList<QByteArray> names = m_PrivateDescription->dynamicPropertyNames();
    /*Delete old properies and copy new ones*/
    QVariant Invalid;
    for (const QByteArray& name : names) {
        m_PrivateDescription->setProperty( name,Invalid );
    }

    QStringList list = Store.childKeys();
    for (const QString& name : list) {
        if( m_PrivateDescription->setProperty( name.toUtf8().data(), Store.value(name, "" ) ) ){
            DEBUG << "Strange - set of this dynamic property should return false here. Chek it - maybe it is defined with Q_PROPERTY  macro";
        }
    }
    m_Enabled = Store.value( "Enabled", false ).toBool();
    ret = true;

    return ret;
}

void PluginDescription::SetIcon(const QIcon &Icon)
{
    m_Icon = Icon;
}

QIcon PluginDescription::GetIcon() const
{
    return m_Icon;
}



}
