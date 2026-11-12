#pragma once

#include "framework_core_export.h"

#include <QByteArray>
#include <QJsonObject>
#include <QSet>
#include <QString>

namespace Daqster {

class RuntimeController;

/**
 * @brief Transport-independent router for the control-plane data API.
 *
 * REQ-SW-PL-063 AC11. Before this class, `ControlPlaneServer::handleApi()`
 * both routed and wrote to a QTcpSocket, so the only way to drive the API was
 * over HTTP. Extracting the routing into a pure `method + path + body ->
 * result` function lets the HTTP transport and the WebSocket RPC path
 * (`/api/events`) share one implementation, and makes the whole API testable
 * without a socket.
 *
 * Only the *data* API lives here (health, node-types, nodes, connections,
 * flow). The HTML docs page (`/api/docs`), the OpenAPI document
 * (`/api/openapi.json`) and static assets stay in ControlPlaneServer, which
 * also owns framing, auth and CORS.
 */
class FRAMEWORK_CORE_EXPORT ControlPlaneRouter
{
public:
    struct Result {
        bool handled = false;      ///< false => the path is not part of this API
        int status = 200;          ///< HTTP-style status code
        QJsonObject body;          ///< JSON response body (always an object)
    };

    explicit ControlPlaneRouter(RuntimeController* controller);

    /// Route one data-API request. @p method is uppercase ("GET", ...).
    /// @p body is the parsed JSON body; @p bodyValid is false when the request
    /// carried a body that failed to parse, in which case mutating methods are
    /// rejected with 400.
    Result route(const QByteArray& method, const QString& path,
                 const QJsonObject& body, bool bodyValid = true);

private:
    /// Whether the node currently reports `state == "running"` — the same field
    /// `GET /api/nodes` exposes, so both hosts answer from one place.
    bool isNodeRunning(quint64 nodeId) const;

    /// Whether the node accepts `start()` — the `startable` field of
    /// `nodeDetail()`, filled from the same `dynamic_cast<Daqster::IStartable*>`
    /// the hosts use in `startNode()`. Read only as the fallback for a changed
    /// key whose schema carries no `x-restart` marker.
    bool isNodeStartable(quint64 nodeId) const;

    /// Reply body for a configuration mutation: `applied` plus `started` and
    /// `restartRequired`.
    ///
    /// A started node keeps the configuration it was started with: the reply
    /// says so instead of stopping/starting it behind the caller's back.
    ///
    /// `restartRequired` is exact (REQ-SW-PL-091): the node must be running AND
    /// at least one key that actually changed between @p before (the snapshot
    /// taken **before** the write, read back here afterwards) and the current
    /// config must need a restart — `"x-restart": true` in the node's schema,
    /// or, when the schema carries no marker (derived from `save()`), the node
    /// implements IStartable. @p beforeKnown is false when the host could not
    /// read the config first; the diff is then unknown and the answer falls
    /// back to the node's startability.
    QJsonObject configMutationResult(quint64 nodeId, const QJsonObject& before,
                                     bool beforeKnown, bool applied) const;

    /// Keys whose value differs between @p before and @p after, in both
    /// directions (removed keys included).
    static QSet<QString> changedConfigKeys(const QJsonObject& before,
                                          const QJsonObject& after);

    /// Whether at least one key in `changedConfigKeys()` needs a restart:
    /// `"x-restart": true` in the node's schema, or — when the schema has no
    /// marker for that key (derived from `save()`) — the node implements
    /// IStartable. `"x-restart": false` never needs one.
    bool configChangeNeedsRestart(quint64 nodeId, const QJsonObject& before,
                                  const QJsonObject& after) const;

    RuntimeController* m_controller = nullptr;
};

} // namespace Daqster
