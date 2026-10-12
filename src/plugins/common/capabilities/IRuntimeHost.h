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

#include <QObject>   // Q_DECLARE_INTERFACE / qobject_interface_iid
#include <QString>
#include <QtGlobal>

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
 * @brief Configuration for the optional remote-control plane (REQ-SW-PL-054).
 *
 * The shared AppBootstrap parses the `--rest-*` command-line options into this
 * struct and hands it to the selected IRuntimeHost through
 * configureControlPlane(). A host that does not implement remote control
 * returns false, and the bootstrap treats a requested-but-unsupported control
 * plane as a startup error (fail loudly — never silently pretend the server is
 * up).
 *
 * Defaults are the safe ones: a loopback-only HTTP server, disabled unless the
 * caller explicitly asks for it with `--rest-port`.
 */
struct ControlPlaneOptions {
    /// True only when the caller explicitly enabled the control plane.
    bool enabled = false;
    /// TCP port for the HTTP/WebSocket server (default 8080).
    quint16 port = 8080;
    /// Bind address. Loopback by default; a non-loopback bind without a token
    /// must be refused (see REQ-SW-PL-054).
    QString bindAddress = QStringLiteral("127.0.0.1");
    /// Optional Bearer token. Empty means "no authentication" and is only
    /// acceptable together with a loopback bind address.
    QString authToken;
    /// Optional directory with the control web UI to serve at `/`.
    QString webRoot;
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
 *   - RuntimeHostHeadlessPlugin -> HeadlessEngine  (RuntimeMode::Headless)
 *   - RuntimeHostGuiPlugin  -> RuntimeShell    (RuntimeMode::Gui)
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

    /**
     * @brief Enable the optional remote-control plane (REQ-SW-PL-054).
     *
     * Called by AppBootstrap with the `--rest-*` command-line options before
     * loadFlow(). The returned bool is the host's answer to "can you actually
     * serve this?":
     *
     *   - true  — the host will expose the control plane,
     *   - false — remote control is not implemented (or the options were
     *             rejected, e.g. non-loopback bind without a token).
     *
     * The default implementation returns false, so existing hosts stay valid
     * without changes and a requested-but-unsupported control plane surfaces as
     * a startup error instead of a silent no-op.
     *
     * NOTE: the interface stays plain (non-QObject) on purpose — see the class
     * comment. Keeping this virtual inline avoids forcing the plugin ABI to
     * grow a slot.
     */
    virtual bool configureControlPlane(const ControlPlaneOptions& options)
    {
        Q_UNUSED(options);
        return false;
    }
};

} // namespace Daqster

#define IRuntimeHost_IID "org.daqster.IRuntimeHost/1.0"
Q_DECLARE_INTERFACE(Daqster::IRuntimeHost, IRuntimeHost_IID)

#endif // IRUNTIMEHOST_H
