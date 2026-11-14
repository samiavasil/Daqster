#include "ControlPlaneRouter.h"

#include "RuntimeController.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSet>

namespace Daqster {

namespace {

ControlPlaneRouter::Result makeResult(int status, const QJsonObject& body)
{
    ControlPlaneRouter::Result r;
    r.handled = true;
    r.status = status;
    r.body = body;
    return r;
}

ControlPlaneRouter::Result errorResult(int status, const QString& message)
{
    return makeResult(status, QJsonObject{{QStringLiteral("error"), message}});
}

ControlPlaneRouter::Result methodNotAllowed()
{
    return errorResult(405, QStringLiteral("method not allowed"));
}

bool toNodeId(const QString& text, quint64* id)
{
    bool ok = false;
    const quint64 value = text.toULongLong(&ok);
    if (ok)
        *id = value;
    return ok;
}

/// Read a required integer field; returns false when missing or wrong type.
bool readInt(const QJsonObject& obj, const QString& key, int* out)
{
    if (!obj.contains(key))
        return false;
    const QJsonValue v = obj.value(key);
    if (!v.isDouble())
        return false;
    *out = v.toInt();
    return true;
}

RuntimeController::ConnectionInfo connectionFromJson(const QJsonObject& body, bool* ok)
{
    RuntimeController::ConnectionInfo c;
    *ok = readInt(body, QStringLiteral("outPortIndex"), &c.outPortIndex)
       && readInt(body, QStringLiteral("inPortIndex"), &c.inPortIndex);
    if (!*ok)
        return c;

    int outNode = 0;
    int inNode = 0;
    if (!readInt(body, QStringLiteral("outNodeId"), &outNode)
        || !readInt(body, QStringLiteral("inNodeId"), &inNode)) {
        *ok = false;
        return c;
    }
    c.outNodeId = static_cast<quint64>(outNode < 0 ? 0 : outNode);
    c.inNodeId = static_cast<quint64>(inNode < 0 ? 0 : inNode);
    return c;
}

QJsonObject connectionToJson(const RuntimeController::ConnectionInfo& c)
{
    QJsonObject o;
    o.insert(QStringLiteral("outNodeId"), static_cast<qint64>(c.outNodeId));
    o.insert(QStringLiteral("outPortIndex"), c.outPortIndex);
    o.insert(QStringLiteral("inNodeId"), static_cast<qint64>(c.inNodeId));
    o.insert(QStringLiteral("inPortIndex"), c.inPortIndex);
    return o;
}

} // namespace

ControlPlaneRouter::ControlPlaneRouter(RuntimeController* controller)
    : m_controller(controller)
{
}

bool ControlPlaneRouter::isNodeRunning(quint64 nodeId) const
{
    // `state` is the field `GET /api/nodes` and `GET /api/nodes/{id}` report
    // ("running" / "stopped"); both hosts fill it from their own running set.
    return m_controller->nodeDetail(nodeId).value(QStringLiteral("state")).toString()
           == QLatin1String("running");
}

bool ControlPlaneRouter::isNodeStartable(quint64 nodeId) const
{
    // Both hosts fill `startable` from the same cast startNode() makes; a host
    // (or a test double) that does not report it is read as "not startable".
    return m_controller->nodeDetail(nodeId).value(QStringLiteral("startable")).toBool();
}

QJsonObject ControlPlaneRouter::configMutationResult(quint64 nodeId,
                                                     const QJsonObject& before,
                                                     bool beforeKnown, bool applied) const
{
    // A running node does not re-read its configuration: `load()` only changes
    // what the next start() will use. Say that instead of restarting the node
    // as a side effect of a config write — but only when the write actually
    // touched a field the node cannot apply live (REQ-SW-PL-091).
    const bool started = applied && isNodeRunning(nodeId);

    bool restartRequired = false;
    if (started) {
        if (!beforeKnown) {
            // No snapshot, no diff: a write to a running node we could not read
            // first is reported conservatively, never as "nothing to do".
            restartRequired = isNodeStartable(nodeId);
        } else {
            QJsonObject after;
            const bool afterKnown = m_controller->nodeConfig(nodeId, after);
            restartRequired = afterKnown ? configChangeNeedsRestart(nodeId, before, after)
                                         : isNodeStartable(nodeId);
        }
    }

    QJsonObject out;
    out.insert(QStringLiteral("applied"), applied);
    out.insert(QStringLiteral("started"), started);
    out.insert(QStringLiteral("restartRequired"), restartRequired);
    return out;
}

QSet<QString> ControlPlaneRouter::changedConfigKeys(const QJsonObject& before,
                                                    const QJsonObject& after)
{
    // Both directions: a key the write removed is a change too (a node that
    // falls back to its default for it still has to be restarted).
    QSet<QString> changed;
    for (auto it = after.constBegin(); it != after.constEnd(); ++it) {
        if (before.value(it.key()) != it.value())
            changed.insert(it.key());
    }
    for (auto it = before.constBegin(); it != before.constEnd(); ++it) {
        if (!after.contains(it.key()))
            changed.insert(it.key());
    }
    return changed;
}

bool ControlPlaneRouter::configChangeNeedsRestart(quint64 nodeId, const QJsonObject& before,
                                                  const QJsonObject& after) const
{
    const QSet<QString> changed = changedConfigKeys(before, after);
    if (changed.isEmpty())
        return false;

    // The schema is the per-field authority: `"x-restart": true|false` says
    // whether the running node picks the new value up. A derived schema (no
    // marker) only tells the field exists, so the node's own nature decides.
    const QJsonObject properties =
        m_controller->nodeSchema(nodeId).value(QStringLiteral("properties")).toObject();
    const bool startable = isNodeStartable(nodeId);

    for (const QString& key : changed) {
        const QJsonValue marker =
            properties.value(key).toObject().value(QStringLiteral("x-restart"));
        if (marker.isBool() ? marker.toBool() : startable)
            return true;
    }
    return false;
}

ControlPlaneRouter::Result ControlPlaneRouter::route(const QByteArray& method,
                                                     const QString& path,
                                                     const QJsonObject& body,
                                                     bool bodyValid)
{
    if (!m_controller)
        return errorResult(500, QStringLiteral("no runtime controller"));

    const bool mutating = method == "POST" || method == "PATCH" || method == "PUT"
                       || method == "DELETE";
    if (mutating && !bodyValid)
        return errorResult(400, QStringLiteral("invalid JSON body"));

    // ── Collection endpoints ────────────────────────────────────────────────
    if (path == QLatin1String("/api/health")) {
        if (method != "GET") return methodNotAllowed();
        QJsonObject out;
        out.insert(QStringLiteral("status"), QStringLiteral("ok"));
        out.insert(QStringLiteral("nodes"), m_controller->nodes().size());
        return makeResult(200, out);
    }

    if (path == QLatin1String("/api/node-types")) {
        if (method != "GET") return methodNotAllowed();
        QJsonArray types;
        for (const RuntimeController::NodeTypeInfo& t : m_controller->nodeTypes()) {
            types.append(QJsonObject{{QStringLiteral("type"), t.type},
                                     {QStringLiteral("category"), t.category}});
        }
        return makeResult(200, QJsonObject{{QStringLiteral("types"), types}});
    }

    if (path == QLatin1String("/api/nodes")) {
        if (method == "GET") {
            QJsonArray arr;
            for (const RuntimeController::NodeInfo& n : m_controller->nodes()) {
                arr.append(QJsonObject{{QStringLiteral("id"), static_cast<qint64>(n.id)},
                                       {QStringLiteral("type"), n.type},
                                       {QStringLiteral("name"), n.name},
                                       {QStringLiteral("state"), n.state}});
            }
            return makeResult(200, QJsonObject{{QStringLiteral("nodes"), arr}});
        }
        if (method == "POST") {
            const QString type = body.value(QStringLiteral("type")).toString();
            if (type.isEmpty())
                return errorResult(400, QStringLiteral("missing 'type'"));

            double x = 0.0;
            double y = 0.0;
            const QJsonObject pos = body.value(QStringLiteral("position")).toObject();
            x = pos.value(QStringLiteral("x")).toDouble(0.0);
            y = pos.value(QStringLiteral("y")).toDouble(0.0);
            const QJsonObject config = body.value(QStringLiteral("config")).toObject();

            QString error;
            const quint64 id = m_controller->createNode(type, x, y, config, &error);
            if (id == 0)
                return errorResult(400, error.isEmpty()
                                            ? QStringLiteral("cannot create node")
                                            : error);

            QJsonObject out;
            out.insert(QStringLiteral("id"), static_cast<qint64>(id));
            const QJsonObject detail = m_controller->nodeDetail(id);
            if (!detail.isEmpty())
                out.insert(QStringLiteral("node"), detail);
            return makeResult(201, out);
        }
        return methodNotAllowed();
    }

    if (path == QLatin1String("/api/connections")) {
        if (method == "GET") {
            QJsonArray arr;
            for (const RuntimeController::ConnectionInfo& c : m_controller->connections())
                arr.append(connectionToJson(c));
            return makeResult(200, QJsonObject{{QStringLiteral("connections"), arr}});
        }
        if (method == "POST" || method == "DELETE") {
            bool ok = false;
            const RuntimeController::ConnectionInfo c = connectionFromJson(body, &ok);
            if (!ok)
                return errorResult(400, QStringLiteral(
                    "connection requires outNodeId, outPortIndex, inNodeId, inPortIndex"));

            if (method == "POST") {
                QString error;
                if (!m_controller->addConnection(c, &error))
                    return errorResult(400, error.isEmpty()
                                                ? QStringLiteral("connection not possible")
                                                : error);
                return makeResult(200, QJsonObject{{QStringLiteral("connected"), true}});
            }
            if (!m_controller->removeConnection(c))
                return errorResult(404, QStringLiteral("connection not found"));
            return makeResult(200, QJsonObject{{QStringLiteral("disconnected"), true}});
        }
        return methodNotAllowed();
    }

    if (path == QLatin1String("/api/flow")) {
        if (method == "GET")
            return makeResult(200, m_controller->flowJson());
        if (method == "POST") {
            QJsonObject flow = body;
            // Convenience: `{ "path": "/abs/file.flow" }` hot-reloads from disk.
            if (flow.contains(QStringLiteral("path")) && !flow.contains(QStringLiteral("nodes"))) {
                const QString flowPath = flow.value(QStringLiteral("path")).toString();
                QFile file(flowPath);
                if (flowPath.isEmpty() || !file.open(QIODevice::ReadOnly))
                    return errorResult(400, QStringLiteral("cannot open flow file: %1").arg(flowPath));
                QJsonParseError fileErr{};
                const QJsonDocument fileDoc = QJsonDocument::fromJson(file.readAll(), &fileErr);
                if (fileErr.error != QJsonParseError::NoError || !fileDoc.isObject())
                    return errorResult(400, QStringLiteral("invalid JSON in flow file: %1")
                                                .arg(fileErr.errorString()));
                flow = fileDoc.object();
            }

            // Reject structurally wrong documents instead of silently loading an
            // empty scene (a `"nodes": "nope"` would otherwise pass as zero nodes).
            if ((flow.contains(QStringLiteral("nodes"))
                 && !flow.value(QStringLiteral("nodes")).isArray())
                || (flow.contains(QStringLiteral("connections"))
                    && !flow.value(QStringLiteral("connections")).isArray())
                || (flow.contains(QStringLiteral("ui"))
                    && !flow.value(QStringLiteral("ui")).isObject())) {
                return errorResult(400, QStringLiteral(
                    "flow fields must be: nodes[], connections[], ui{}"));
            }

            QString error;
            const bool applied = m_controller->reloadFlow(flow, &error);
            if (!applied)
                return errorResult(400, error.isEmpty()
                                            ? QStringLiteral("cannot load flow")
                                            : error);

            // REQ-SW-PL-082: the scene loaded. A plugin verdict no longer
            // blocks it — it only means its autoStart nodes were held back, so
            // the reply says both things instead of pretending either.
            const QJsonArray problems = m_controller->lastSceneProblems();
            QJsonObject out;
            out.insert(QStringLiteral("applied"), true);
            out.insert(QStringLiteral("started"), problems.isEmpty());
            out.insert(QStringLiteral("problems"), problems);
            return makeResult(200, out);
        }
        return methodNotAllowed();
    }

    if (path == QLatin1String("/api/flow/close")) {
        if (method != "POST") return methodNotAllowed();

        // REQ-SW-PL-078: the close half of every other flow endpoint. Stops the
        // nodes (sources first), notifies the plugins and frees the graph; the
        // reply reports what was released instead of a bare `true`.
        QString error;
        const bool closed = m_controller->closeFlow(&error);
        QJsonObject out;
        out.insert(QStringLiteral("closed"), closed);
        if (!closed && !error.isEmpty())
            out.insert(QStringLiteral("error"), error);
        return makeResult(closed ? 200 : 409, out);
    }

    if (path == QLatin1String("/api/flow/save")) {
        if (method != "POST") return methodNotAllowed();
        const QString filePath = body.value(QStringLiteral("path")).toString();
        if (filePath.isEmpty())
            return errorResult(400, QStringLiteral("missing 'path'"));
        QString error;
        if (!m_controller->saveFlow(filePath, &error))
            return errorResult(400, error.isEmpty()
                                        ? QStringLiteral("cannot write flow file")
                                        : error);
        return makeResult(200, QJsonObject{{QStringLiteral("saved"), true},
                                           {QStringLiteral("path"), filePath}});
    }

    // ── Node-scoped endpoints: /api/nodes/{id}[/<action>] ──────────────────
    if (path.startsWith(QLatin1String("/api/nodes/"))) {
        const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (parts.size() >= 3 && parts.at(0) == QLatin1String("api")
            && parts.at(1) == QLatin1String("nodes")) {
            quint64 id = 0;
            if (!toNodeId(parts.at(2), &id))
                return errorResult(400, QStringLiteral("bad node id"));

            const QString action = parts.size() >= 4 ? parts.at(3) : QString();

            if (action.isEmpty()) {
                if (method == "GET") {
                    const QJsonObject detail = m_controller->nodeDetail(id);
                    if (detail.isEmpty())
                        return errorResult(404, QStringLiteral("node not found"));
                    return makeResult(200, detail);
                }
                if (method == "DELETE") {
                    QString error;
                    if (!m_controller->removeNode(id, &error))
                        return errorResult(404, error.isEmpty()
                                                    ? QStringLiteral("node not found")
                                                    : error);
                    return makeResult(200, QJsonObject{{QStringLiteral("removed"), true}});
                }
                if (method == "PATCH") {
                    if (m_controller->nodeDetail(id).isEmpty())
                        return errorResult(404, QStringLiteral("node not found"));
                    // Optional "config" first: it is the only part that can be
                    // rejected, so a refused patch must not move the node.
                    bool configApplied = false;
                    QJsonObject configBefore;
                    bool configBeforeKnown = false;
                    if (body.contains(QStringLiteral("config"))) {
                        const QJsonValue cfg = body.value(QStringLiteral("config"));
                        if (!cfg.isObject())
                            return errorResult(400, QStringLiteral("'config' must be an object"));
                        // Snapshot first: restartRequired is the diff of what the
                        // write actually changed, not "the node was running".
                        configBeforeKnown = m_controller->nodeConfig(id, configBefore);
                        // Same seam as POST /api/nodes/{id}/config — a second
                        // implementation of a config write would drift from it.
                        if (!m_controller->setNodeConfig(id, cfg.toObject()))
                            return errorResult(400, QStringLiteral("node rejected 'config'"));
                        configApplied = true;
                    }
                    if (body.contains(QStringLiteral("position"))) {
                        const QJsonObject pos = body.value(QStringLiteral("position")).toObject();
                        m_controller->setNodePosition(id,
                                                      pos.value(QStringLiteral("x")).toDouble(0.0),
                                                      pos.value(QStringLiteral("y")).toDouble(0.0));
                    }
                    if (body.contains(QStringLiteral("label"))
                        || body.contains(QStringLiteral("labelVisible"))) {
                        const QString label = body.value(QStringLiteral("label")).toString();
                        const bool visible = body.contains(QStringLiteral("labelVisible"))
                            ? body.value(QStringLiteral("labelVisible")).toBool(true)
                            : true;
                        m_controller->setNodeLabel(id, label, visible);
                    }
                    if (configApplied)
                        return makeResult(200, configMutationResult(id, configBefore,
                                                                    configBeforeKnown, true));
                    // Position/label do not need a restart; the state is still
                    // reported so the caller does not have to ask twice.
                    return makeResult(200,
                                      QJsonObject{{QStringLiteral("applied"), true},
                                                  {QStringLiteral("started"), isNodeRunning(id)},
                                                  {QStringLiteral("restartRequired"), false}});
                }
                return methodNotAllowed();
            }

            if (action == QLatin1String("config")) {
                if (method == "GET") {
                    QJsonObject cfg;
                    if (m_controller->nodeConfig(id, cfg))
                        return makeResult(200, cfg);
                    return errorResult(404, QStringLiteral("node not found"));
                }
                if (method == "POST") {
                    QJsonObject before;
                    const bool beforeKnown = m_controller->nodeConfig(id, before);
                    const bool applied = m_controller->setNodeConfig(id, body);
                    return makeResult(applied ? 200 : 400,
                                      configMutationResult(id, before, beforeKnown, applied));
                }
                return methodNotAllowed();
            }

            if (action == QLatin1String("schema")) {
                if (method != "GET") return methodNotAllowed();
                const QJsonObject schema = m_controller->nodeSchema(id);
                if (schema.isEmpty())
                    return errorResult(404, QStringLiteral("node not found"));
                return makeResult(200, schema);
            }

            if (action == QLatin1String("start") || action == QLatin1String("stop")) {
                if (method != "POST") return methodNotAllowed();
                const bool ok = (action == QLatin1String("start"))
                    ? m_controller->startNode(id)
                    : m_controller->stopNode(id);
                return makeResult(ok ? 200 : 400, QJsonObject{{QStringLiteral("ok"), ok}});
            }
        }
    }

    Result r;
    r.handled = false;
    return r;
}

} // namespace Daqster
