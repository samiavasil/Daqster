#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace Daqster {

/**
 * @brief Read/write view of a running flow, exposed to the control plane.
 *
 * The REST/WebSocket control plane (REQ-SW-PL-054, REQ-SW-PL-063) must not
 * depend on a concrete runtime host: the same HTTP surface has to serve a
 * headless engine and a GUI runtime host. RuntimeController is that seam.
 *
 * It is deliberately a plain (non-QObject) interface: implementers already
 * inherit QObject through their host, and the control plane only needs the
 * synchronous getters/setters below. Node identifiers are the QtNodes
 * QtNodes::NodeId values, carried as quint64 so this header stays free of
 * QtNodes includes.
 *
 * REQ-SW-PL-048 (AppBootstrap hands ControlPlaneOptions to the host) /
 * REQ-SW-PL-054 (REST control plane) / REQ-SW-PL-063 (full graph control).
 */
class RuntimeController
{
public:
    virtual ~RuntimeController() = default;

    /// Lightweight per-node summary for `GET /api/nodes`.
    struct NodeInfo {
        quint64 id = 0;
        QString type;   ///< NodeDelegateModel::name() — the registered model type
        QString name;   ///< NodeDelegateModel::caption() — human-readable label
        QString state;  ///< "running" | "stopped"
    };

    /// One entry of the node palette: a registered model type and its category
    /// (e.g. "Sources/Video"), for `GET /api/node-types`.
    struct NodeTypeInfo {
        QString type;
        QString category;
    };

    /// One graph edge, as carried by the flow JSON (`ConnectionId`).
    struct ConnectionInfo {
        quint64 outNodeId = 0;
        int outPortIndex = 0;
        quint64 inNodeId = 0;
        int inPortIndex = 0;
    };

    /// All nodes in the current scene, in graph order.
    virtual QVector<NodeInfo> nodes() const = 0;

    /// All registered node types (the palette, `GET /api/node-types`).
    virtual QVector<NodeTypeInfo> nodeTypes() const = 0;

    /// All graph edges (`GET /api/connections`).
    virtual QVector<ConnectionInfo> connections() const = 0;

    /// Current configuration of one node (`NodeDelegateModel::save()`).
    virtual bool nodeConfig(quint64 nodeId, QJsonObject& out) const = 0;

    /// Apply a configuration patch to one node (`NodeDelegateModel::load()`).
    /// Implementations validate before applying — `load()` returns void.
    virtual bool setNodeConfig(quint64 nodeId, const QJsonObject& config) = 0;

    /// Combined view for `GET /api/nodes/{id}`: config + schema + position +
    /// label + ports + state. Empty object when the node does not exist.
    virtual QJsonObject nodeDetail(quint64 nodeId) const = 0;

    /// Start / stop a single node through IStartable / IStoppable.
    virtual bool startNode(quint64 nodeId) = 0;
    virtual bool stopNode(quint64 nodeId) = 0;

    /// Create a node of @p type at scene position (@p x, @p y), optionally
    /// applying @p config through `load()`. Returns the new node id, or 0 on
    /// failure (unknown type / graph not ready) with @p errorMessage filled.
    virtual quint64 createNode(const QString& type, double x, double y,
                               const QJsonObject& config,
                               QString* errorMessage = nullptr) = 0;

    /// Delete a node and every connection attached to it
    /// (`DELETE /api/nodes/{id}`). Returns false when the node does not exist.
    virtual bool removeNode(quint64 nodeId, QString* errorMessage = nullptr) = 0;

    /// Move a node (`PATCH /api/nodes/{id}` position).
    virtual bool setNodePosition(quint64 nodeId, double x, double y) = 0;

    /// Set a node's label and its visibility (`PATCH /api/nodes/{id}` label).
    virtual bool setNodeLabel(quint64 nodeId, const QString& label, bool visible) = 0;

    /// Connect two ports (`POST /api/connections`). Implementations run the
    /// graph model's connectionPossible() pre-check and return false with
    /// @p errorMessage when the ports cannot be connected.
    virtual bool addConnection(const ConnectionInfo& connection,
                               QString* errorMessage = nullptr) = 0;

    /// Disconnect two ports (`DELETE /api/connections`). Returns false when the
    /// connection does not exist.
    virtual bool removeConnection(const ConnectionInfo& connection) = 0;

    /// The whole scene as flow JSON: { "nodes": [...], "connections": [...],
    /// "ui": {...} } — the shape written to a .flow file.
    virtual QJsonObject flowJson() const = 0;

    /// Write `flowJson()` to @p path (`POST /api/flow/save`). Returns false
    /// with @p errorMessage when the file cannot be written.
    virtual bool saveFlow(const QString& path, QString* errorMessage = nullptr) = 0;

    /// Replace the live scene with @p flow (hot reload, `POST /api/flow`).
    /// Implementations stop every running node first and only report success
    /// after the new scene validated and its autoStart nodes have started.
    /// On failure the implementation leaves the previous scene stopped and
    /// fills @p errorMessage with a human-readable reason.
    virtual bool reloadFlow(const QJsonObject& flow, QString* errorMessage = nullptr) = 0;

    /// JSON Schema (Draft 7) for a node instance's config, enriched when the
    /// model implements Daqster::INodeConfigSchema (REQ-SW-PL-063 AC10);
    /// best-effort derived from `save()` otherwise.
    virtual QJsonObject nodeSchema(quint64 nodeId) const = 0;

    /// JSON Schema for a node *type* (palette preview / pre-create validation).
    /// The same schema `nodeSchema()` returns, but resolved without an instance.
    virtual QJsonObject typeSchema(const QString& type) const = 0;
};

} // namespace Daqster
