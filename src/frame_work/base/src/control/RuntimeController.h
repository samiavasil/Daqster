#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace Daqster {

/**
 * @brief Read/write view of a running flow, exposed to the control plane.
 *
 * The REST/WebSocket control plane (REQ-SW-PL-054) must not depend on a
 * concrete runtime host: the same HTTP surface has to serve a headless engine
 * and — later — a GUI runtime host. RuntimeController is that seam.
 *
 * It is deliberately a plain (non-QObject) interface: implementers already
 * inherit QObject through their host, and the control plane only needs the
 * synchronous getters/setters below. Node identifiers are the QtNodes
 * QtNodes::NodeId values, carried as quint64 so this header stays free of
 * QtNodes includes.
 *
 * REQ-SW-PL-048 (AppBootstrap hands ControlPlaneOptions to the host) /
 * REQ-SW-PL-054 (REST control plane).
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

    /// All nodes in the current scene, in graph order.
    virtual QVector<NodeInfo> nodes() const = 0;

    /// Current configuration of one node (`NodeDelegateModel::save()`).
    virtual bool nodeConfig(quint64 nodeId, QJsonObject& out) const = 0;

    /// Apply a configuration patch to one node (`NodeDelegateModel::load()`).
    /// Implementations validate before applying — `load()` returns void.
    virtual bool setNodeConfig(quint64 nodeId, const QJsonObject& config) = 0;

    /// Start / stop a single node through IStartable / IStoppable.
    virtual bool startNode(quint64 nodeId) = 0;
    virtual bool stopNode(quint64 nodeId) = 0;

    /// The whole scene as flow JSON: { "nodes": [...], "connections": [...],
    /// "ui": {...} } — the shape written to a .flow file.
    virtual QJsonObject flowJson() const = 0;

    /// JSON Schema (Draft 7, best-effort) for a node type, generated from a
    /// throw-away model instance's save() output.
    virtual QJsonObject nodeSchema(quint64 nodeId) const = 0;
};

} // namespace Daqster
