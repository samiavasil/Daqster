/************************************************************************
 *                        Daqster/IRuntimeHost.h - Copyright
 * Daqster software
 * Copyright (C) 2016, Vasil Vasilev,  Bulgaria
 *
 * This file is part of Daqster and its software development toolkit.
 *
 * Daqster is a free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public Licence as published by
 * the Free Software Foundation; either version 2 of the Licence, or (at
 * your option) any later version.
 *
 * Daqster is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library
 * General Public Licence for more details.
 *
 * You should have received a copy of the GNU Library General Public Licence
 * along with Daqster; if not, see <http://www.gnu.org/licenses/>.
 *
 * Initial version of this file was created on 28.09.2026
 ************************************************************************/
#ifndef IRUNTIMEHOST_H
#define IRUNTIMEHOST_H

#include <QString>

namespace Daqster {

/**
 * @brief The runtime flavour a host can execute a .flow scene in.
 *
 * Selected by the runner application from its command line (e.g. --headless)
 * and used to probe QPluginManager::runtimeHosts() for a matching host.
 */
enum class RuntimeMode {
    Headless, ///< No visible UI: HeadlessEngine, offscreen platform, fail-fast validation.
    Gui       ///< Visible deembedded-widget runtime: RuntimeShell + MDI workspaces.
};

/**
 * @brief The IRuntimeHost interface
 *
 * Capability interface for plugins that can *execute a .flow scene*.
 *
 * This is what makes the runner application mode-agnostic: the binary links
 * only FrameworkCore and never references a concrete plugin type. At startup it
 * asks QPluginManager::runtimeHosts(mode) for a host, and the plugin that
 * implements the requested mode does the work:
 *
 *   - FrameworkCorePlugin -> HeadlessEngine  (RuntimeMode::Headless)
 *   - FrameworkGuiPlugin  -> RuntimeShell    (RuntimeMode::Gui)
 *
 * Both implementations are owned by their plugin object and are QObject children
 * of it, so their lifetime follows plugin shutdown.
 *
 * NOTE: like INodeProvider, this is deliberately a plain (non-QObject) interface.
 * Implementers already inherit QObject through QBasePluginObject, so deriving
 * this from QObject too would make QObject an ambiguous base and break moc. It
 * is therefore never matched by QObject::qt_metacast() and discovery uses
 * dynamic_cast via QPluginManager::runtimeHosts().
 */
class IRuntimeHost {
public:
    virtual ~IRuntimeHost() = default;

    /**
     * @brief The runtime mode this host implements.
     *
     * A runner asks for one specific mode, so hosts of the other mode are
     * skipped rather than mis-selected.
     */
    virtual RuntimeMode runtimeMode() const = 0;

    /**
     * @brief Load and start a .flow scene.
     *
     * On success the host owns a live graph for the duration of the event loop
     * and the caller should then run the application event loop.
     *
     * @param flowPath Path to the .flow file
     * @return true on success; false after reporting the reason
     */
    virtual bool loadFlow(const QString& flowPath) = 0;

    /**
     * @brief Stop every running node in the loaded scene.
     *
     * Must be called explicitly before the host is destroyed so nodes get a
     * graceful shutdown while their graph is still valid.
     */
    virtual void stopAllNodes() = 0;
};

} // namespace Daqster

#define IRuntimeHost_IID "org.daqster.IRuntimeHost/1.0"
Q_DECLARE_INTERFACE(Daqster::IRuntimeHost, IRuntimeHost_IID)

#endif // IRUNTIMEHOST_H
