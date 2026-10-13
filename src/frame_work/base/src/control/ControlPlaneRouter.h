#pragma once

#include "framework_core_export.h"

#include <QByteArray>
#include <QJsonObject>
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
    RuntimeController* m_controller = nullptr;
};

} // namespace Daqster
