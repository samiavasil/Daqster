/************************************************************************
                          Daqster/INodeProvider.h - Copyright
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

Initial version of this file was created on 15.07.2026
************************************************************************/
#ifndef INODEPROVIDER_H
#define INODEPROVIDER_H

#include <functional>
#include <memory>

namespace QtNodes {
class NodeDelegateModel;
class NodeDelegateModelRegistry;
}

namespace Daqster {

/**
 * @brief The INodeProvider interface
 *
 * Capability interface for plugins that supply node types to the node editor.
 * Any plugin can implement this interface alongside QBasePluginObject to
 * provide custom nodes without modifying the framework.
 *
 * PluginManager discovers these via QPluginManager::instances(INodeProvider_IID).
 * Classification (group name in PluginManager GUI) comes from PluginDescription
 * properties set in the QPluginInterface constructor (PLUGIN_TYPE_NAME).
 *
 * The node_editor_ide plugin calls registerNodes() on each discovered
 * INodeProvider to populate its NodeDelegateModelRegistry.
 */
class INodeProvider {
public:
    virtual ~INodeProvider() = default;

    /**
     * @brief Register all node types from this provider into the given registry.
     *
     * Called by the node editor IDE when it discovers this provider.
     * Implementations should call registry->registerModel<T>(category) for
     * each node type they provide.
     *
     * @param registry The node editor's model registry (owned by NodeEditorWidget)
     */
    virtual void registerNodes(QtNodes::NodeDelegateModelRegistry& registry) const = 0;
};

} // namespace Daqster

#define INodeProvider_IID "org.daqster.INodeProvider/1.0"
Q_DECLARE_INTERFACE(Daqster::INodeProvider, INodeProvider_IID)

#endif // INODEPROVIDER_H
