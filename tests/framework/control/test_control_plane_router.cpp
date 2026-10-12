// Unit tests for the transport-independent control-plane router and the
// NodeSchemaBuilder enrichment merge (REQ-SW-PL-063 AC11 + AC16).
//
// The router is a pure `method + path + body -> result` function, so the whole
// data API can be exercised without a socket. A fake RuntimeController records
// the calls the router makes and returns canned data, which lets every branch
// (success, 400, 404, 405) be asserted.

#include <QtTest>

#include "ControlPlaneRouter.h"
#include "NodeSchemaBuilder.h"
#include "RuntimeController.h"

#include <QJsonArray>
#include <QJsonObject>

namespace {

class FakeController : public Daqster::RuntimeController
{
public:
    QVector<NodeInfo> nodes() const override { return m_nodes; }

    QVector<NodeTypeInfo> nodeTypes() const override
    {
        return {NodeTypeInfo{QStringLiteral("VideoEffect"), QStringLiteral("Processing/Video")},
                NodeTypeInfo{QStringLiteral("SystemMonitor"), QStringLiteral("Sources/System")}};
    }

    QVector<ConnectionInfo> connections() const override { return m_connections; }

    bool nodeConfig(quint64 nodeId, QJsonObject& out) const override
    {
        if (!m_configs.contains(nodeId))
            return false;
        out = m_configs.value(nodeId);
        return true;
    }

    bool setNodeConfig(quint64 nodeId, const QJsonObject& config) override
    {
        if (!m_configs.contains(nodeId))
            return false;
        m_configs[nodeId] = config;
        return true;
    }

    QJsonObject nodeDetail(quint64 nodeId) const override
    {
        if (!m_configs.contains(nodeId))
            return {};
        QJsonObject out;
        out.insert(QStringLiteral("id"), static_cast<qint64>(nodeId));
        out.insert(QStringLiteral("type"), QStringLiteral("VideoEffect"));
        out.insert(QStringLiteral("config"), m_configs.value(nodeId));
        return out;
    }

    bool startNode(quint64 nodeId) override
    {
        if (!m_configs.contains(nodeId))
            return false;
        ++m_startCalls;
        return true;
    }

    bool stopNode(quint64 nodeId) override
    {
        if (!m_configs.contains(nodeId))
            return false;
        ++m_stopCalls;
        return true;
    }

    quint64 createNode(const QString& type, double x, double y,
                       const QJsonObject&, QString* errorMessage) override
    {
        if (type == QLatin1String("Unknown")) {
            if (errorMessage)
                *errorMessage = QStringLiteral("unknown node type: %1").arg(type);
            return 0;
        }
        const quint64 id = ++m_nextId;
        m_configs[id] = QJsonObject{};
        m_createdAt = QPointF(x, y);
        return id;
    }

    bool removeNode(quint64 nodeId, QString*) override
    {
        // Removing a node also drops its connections, like the graph model.
        for (int i = m_connections.size() - 1; i >= 0; --i) {
            const ConnectionInfo& c = m_connections.at(i);
            if (c.outNodeId == nodeId || c.inNodeId == nodeId)
                m_connections.removeAt(i);
        }
        return m_configs.remove(nodeId) > 0;
    }

    bool setNodePosition(quint64, double x, double y) override
    {
        m_movedTo = QPointF(x, y);
        return true;
    }

    bool setNodeLabel(quint64, const QString& label, bool visible) override
    {
        m_label = label;
        m_labelVisible = visible;
        return true;
    }

    bool addConnection(const ConnectionInfo& connection, QString* errorMessage) override
    {
        if (m_rejectConnections) {
            if (errorMessage)
                *errorMessage = QStringLiteral("connection not possible");
            return false;
        }
        m_connections.append(connection);
        return true;
    }

    bool removeConnection(const ConnectionInfo& connection) override
    {
        for (int i = 0; i < m_connections.size(); ++i) {
            const ConnectionInfo& c = m_connections.at(i);
            if (c.outNodeId == connection.outNodeId
                && c.outPortIndex == connection.outPortIndex
                && c.inNodeId == connection.inNodeId
                && c.inPortIndex == connection.inPortIndex) {
                m_connections.removeAt(i);
                return true;
            }
        }
        return false;
    }

    QJsonObject flowJson() const override
    {
        QJsonObject flow;
        flow.insert(QStringLiteral("nodes"), QJsonArray{});
        flow.insert(QStringLiteral("connections"), QJsonArray{});
        return flow;
    }

    bool saveFlow(const QString& path, QString* errorMessage) override
    {
        if (path.contains(QLatin1String("forbidden"))) {
            if (errorMessage)
                *errorMessage = QStringLiteral("cannot write");
            return false;
        }
        m_savedPath = path;
        return true;
    }

    bool reloadFlow(const QJsonObject& flow, QString* errorMessage) override
    {
        if (flow.contains(QStringLiteral("fail"))) {
            if (errorMessage)
                *errorMessage = QStringLiteral("reload failed");
            return false;
        }
        m_reloaded = true;
        return true;
    }

    QJsonObject nodeSchema(quint64 nodeId) const override
    {
        if (!m_configs.contains(nodeId))
            return {};
        QJsonObject schema;
        schema.insert(QStringLiteral("type"), QStringLiteral("object"));
        return schema;
    }

    QJsonObject typeSchema(const QString& type) const override
    {
        QJsonObject schema;
        schema.insert(QStringLiteral("title"), type);
        return schema;
    }

    // Test-facing state.
    QVector<NodeInfo> m_nodes;
    QHash<quint64, QJsonObject> m_configs;
    QVector<ConnectionInfo> m_connections;
    quint64 m_nextId = 0;
    int m_startCalls = 0;
    int m_stopCalls = 0;
    bool m_rejectConnections = false;
    bool m_reloaded = false;
    QString m_savedPath;
    QString m_label;
    bool m_labelVisible = true;
    QPointF m_createdAt;
    QPointF m_movedTo;
};

} // namespace

class ControlPlaneRouterTest : public QObject
{
    Q_OBJECT

private slots:
    void routesHealth()
    {
        FakeController controller;
        controller.m_nodes.append(Daqster::RuntimeController::NodeInfo{1, QStringLiteral("A"),
                                                                       QStringLiteral("A"),
                                                                       QStringLiteral("running")});
        Daqster::ControlPlaneRouter router(&controller);

        const auto r = router.route("GET", QStringLiteral("/api/health"), {});
        QVERIFY(r.handled);
        QCOMPARE(r.status, 200);
        QCOMPARE(r.body.value(QStringLiteral("status")).toString(), QStringLiteral("ok"));
        QCOMPARE(r.body.value(QStringLiteral("nodes")).toInt(), 1);
    }

    void listsNodeTypes()
    {
        FakeController controller;
        Daqster::ControlPlaneRouter router(&controller);

        const auto r = router.route("GET", QStringLiteral("/api/node-types"), {});
        QVERIFY(r.handled);
        const QJsonArray types = r.body.value(QStringLiteral("types")).toArray();
        QCOMPARE(types.size(), 2);
        QCOMPARE(types.at(0).toObject().value(QStringLiteral("type")).toString(),
                 QStringLiteral("VideoEffect"));
        QCOMPARE(types.at(0).toObject().value(QStringLiteral("category")).toString(),
                 QStringLiteral("Processing/Video"));
    }

    void createsNode()
    {
        FakeController controller;
        Daqster::ControlPlaneRouter router(&controller);

        QJsonObject body;
        body.insert(QStringLiteral("type"), QStringLiteral("SystemMonitor"));
        QJsonObject position;
        position.insert(QStringLiteral("x"), 120.0);
        position.insert(QStringLiteral("y"), 40.0);
        body.insert(QStringLiteral("position"), position);

        const auto r = router.route("POST", QStringLiteral("/api/nodes"), body);
        QVERIFY(r.handled);
        QCOMPARE(r.status, 201);
        QCOMPARE(r.body.value(QStringLiteral("id")).toInt(), 1);
        QCOMPARE(controller.m_createdAt, QPointF(120.0, 40.0));
        QVERIFY(r.body.contains(QStringLiteral("node")));
    }

    void createNodeRejectsUnknownType()
    {
        FakeController controller;
        Daqster::ControlPlaneRouter router(&controller);

        QJsonObject body;
        body.insert(QStringLiteral("type"), QStringLiteral("Unknown"));
        const auto r = router.route("POST", QStringLiteral("/api/nodes"), body);
        QVERIFY(r.handled);
        QCOMPARE(r.status, 400);
        QCOMPARE(r.body.value(QStringLiteral("error")).toString(),
                 QStringLiteral("unknown node type: Unknown"));
    }

    void createNodeRejectsMissingType()
    {
        FakeController controller;
        Daqster::ControlPlaneRouter router(&controller);
        const auto r = router.route("POST", QStringLiteral("/api/nodes"), {});
        QCOMPARE(r.status, 400);
    }

    void deletesNode()
    {
        FakeController controller;
        controller.m_configs[7] = QJsonObject{};
        Daqster::ControlPlaneRouter router(&controller);

        const auto r = router.route("DELETE", QStringLiteral("/api/nodes/7"), {});
        QVERIFY(r.handled);
        QCOMPARE(r.status, 200);
        QVERIFY(!controller.m_configs.contains(7));
    }

    void deleteMissingNodeIs404()
    {
        FakeController controller;
        Daqster::ControlPlaneRouter router(&controller);
        const auto r = router.route("DELETE", QStringLiteral("/api/nodes/9"), {});
        QCOMPARE(r.status, 404);
    }

    void patchesPositionAndLabel()
    {
        FakeController controller;
        controller.m_configs[3] = QJsonObject{};
        Daqster::ControlPlaneRouter router(&controller);

        QJsonObject body;
        QJsonObject position;
        position.insert(QStringLiteral("x"), 5.0);
        position.insert(QStringLiteral("y"), 6.0);
        body.insert(QStringLiteral("position"), position);
        body.insert(QStringLiteral("label"), QStringLiteral("Renamed"));
        body.insert(QStringLiteral("labelVisible"), false);

        const auto r = router.route("PATCH", QStringLiteral("/api/nodes/3"), body);
        QVERIFY(r.handled);
        QCOMPARE(r.status, 200);
        QCOMPARE(controller.m_movedTo, QPointF(5.0, 6.0));
        QCOMPARE(controller.m_label, QStringLiteral("Renamed"));
        QCOMPARE(controller.m_labelVisible, false);
    }

    void patchMissingNodeIs404()
    {
        FakeController controller;
        Daqster::ControlPlaneRouter router(&controller);
        const auto r = router.route("PATCH", QStringLiteral("/api/nodes/4"), {});
        QCOMPARE(r.status, 404);
    }

    void nodeDetailAndConfig()
    {
        FakeController controller;
        controller.m_configs[2] = QJsonObject{{QStringLiteral("brightness"), 10}};
        Daqster::ControlPlaneRouter router(&controller);

        const auto detail = router.route("GET", QStringLiteral("/api/nodes/2"), {});
        QVERIFY(detail.handled);
        QCOMPARE(detail.status, 200);
        QCOMPARE(detail.body.value(QStringLiteral("id")).toInt(), 2);

        const auto cfg = router.route("GET", QStringLiteral("/api/nodes/2/config"), {});
        QCOMPARE(cfg.status, 200);
        QCOMPARE(cfg.body.value(QStringLiteral("brightness")).toInt(), 10);

        const auto missing = router.route("GET", QStringLiteral("/api/nodes/8/config"), {});
        QCOMPARE(missing.status, 404);
    }

    void setsConfig()
    {
        FakeController controller;
        controller.m_configs[5] = QJsonObject{};
        Daqster::ControlPlaneRouter router(&controller);

        QJsonObject body;
        body.insert(QStringLiteral("brightness"), 42);
        const auto r = router.route("POST", QStringLiteral("/api/nodes/5/config"), body);
        QVERIFY(r.handled);
        QCOMPARE(r.status, 200);
        QCOMPARE(controller.m_configs.value(5).value(QStringLiteral("brightness")).toInt(), 42);
    }

    void schemaEndpoint()
    {
        FakeController controller;
        controller.m_configs[1] = QJsonObject{};
        Daqster::ControlPlaneRouter router(&controller);
        const auto r = router.route("GET", QStringLiteral("/api/nodes/1/schema"), {});
        QCOMPARE(r.status, 200);
        QCOMPARE(r.body.value(QStringLiteral("type")).toString(), QStringLiteral("object"));

        const auto missing = router.route("GET", QStringLiteral("/api/nodes/2/schema"), {});
        QCOMPARE(missing.status, 404);
    }

    void startStop()
    {
        FakeController controller;
        controller.m_configs[1] = QJsonObject{};
        Daqster::ControlPlaneRouter router(&controller);

        QCOMPARE(router.route("POST", QStringLiteral("/api/nodes/1/start"), {}).status, 200);
        QCOMPARE(router.route("POST", QStringLiteral("/api/nodes/1/stop"), {}).status, 200);
        QCOMPARE(controller.m_startCalls, 1);
        QCOMPARE(controller.m_stopCalls, 1);
    }

    void connectionsRoundTrip()
    {
        FakeController controller;
        Daqster::ControlPlaneRouter router(&controller);

        QJsonObject body;
        body.insert(QStringLiteral("outNodeId"), 1);
        body.insert(QStringLiteral("outPortIndex"), 0);
        body.insert(QStringLiteral("inNodeId"), 2);
        body.insert(QStringLiteral("inPortIndex"), 0);

        QCOMPARE(router.route("POST", QStringLiteral("/api/connections"), body).status, 200);

        const auto list = router.route("GET", QStringLiteral("/api/connections"), {});
        QCOMPARE(list.body.value(QStringLiteral("connections")).toArray().size(), 1);

        QCOMPARE(router.route("DELETE", QStringLiteral("/api/connections"), body).status, 200);
        const auto empty = router.route("GET", QStringLiteral("/api/connections"), {});
        QCOMPARE(empty.body.value(QStringLiteral("connections")).toArray().size(), 0);
    }

    void connectionRejectedIs400()
    {
        FakeController controller;
        controller.m_rejectConnections = true;
        Daqster::ControlPlaneRouter router(&controller);

        QJsonObject body;
        body.insert(QStringLiteral("outNodeId"), 1);
        body.insert(QStringLiteral("outPortIndex"), 0);
        body.insert(QStringLiteral("inNodeId"), 2);
        body.insert(QStringLiteral("inPortIndex"), 0);
        const auto r = router.route("POST", QStringLiteral("/api/connections"), body);
        QCOMPARE(r.status, 400);
        QCOMPARE(r.body.value(QStringLiteral("error")).toString(),
                 QStringLiteral("connection not possible"));
    }

    void connectionMissingFieldsIs400()
    {
        FakeController controller;
        Daqster::ControlPlaneRouter router(&controller);
        const auto r = router.route("POST", QStringLiteral("/api/connections"),
                                    QJsonObject{{QStringLiteral("outNodeId"), 1}});
        QCOMPARE(r.status, 400);
    }

    void saveFlow()
    {
        FakeController controller;
        Daqster::ControlPlaneRouter router(&controller);
        const auto r = router.route("POST", QStringLiteral("/api/flow/save"),
                                    QJsonObject{{QStringLiteral("path"), QStringLiteral("/tmp/x.flow")}});
        QCOMPARE(r.status, 200);
        QCOMPARE(controller.m_savedPath, QStringLiteral("/tmp/x.flow"));

        const auto noPath = router.route("POST", QStringLiteral("/api/flow/save"), {});
        QCOMPARE(noPath.status, 400);
    }

    void reloadFlowRejectsBadShape()
    {
        FakeController controller;
        Daqster::ControlPlaneRouter router(&controller);

        QJsonObject bad;
        bad.insert(QStringLiteral("nodes"), QStringLiteral("nope"));
        const auto r = router.route("POST", QStringLiteral("/api/flow"), bad);
        QCOMPARE(r.status, 400);

        QJsonObject good;
        good.insert(QStringLiteral("nodes"), QJsonArray{});
        QCOMPARE(router.route("POST", QStringLiteral("/api/flow"), good).status, 200);
        QVERIFY(controller.m_reloaded);
    }

    void invalidBodyRejectedForMutation()
    {
        FakeController controller;
        Daqster::ControlPlaneRouter router(&controller);
        const auto r = router.route("POST", QStringLiteral("/api/nodes"), {}, false);
        QVERIFY(r.handled);
        QCOMPARE(r.status, 400);
        QCOMPARE(r.body.value(QStringLiteral("error")).toString(),
                 QStringLiteral("invalid JSON body"));
    }

    void unknownPathIsNotHandled()
    {
        FakeController controller;
        Daqster::ControlPlaneRouter router(&controller);
        QVERIFY(!router.route("GET", QStringLiteral("/api/nope"), {}).handled);
        QVERIFY(!router.route("GET", QStringLiteral("/"), {}).handled);
    }

    void methodNotAllowed()
    {
        FakeController controller;
        Daqster::ControlPlaneRouter router(&controller);
        QCOMPARE(router.route("POST", QStringLiteral("/api/health"), {}).status, 405);
        QCOMPARE(router.route("DELETE", QStringLiteral("/api/nodes"), {}).status, 405);
    }

    void schemaBuilderPrefersProvided()
    {
        const QJsonObject defaults{{QStringLiteral("brightness"), 0}};
        QJsonObject provided;
        provided.insert(QStringLiteral("type"), QStringLiteral("object"));
        provided.insert(QStringLiteral("properties"),
                        QJsonObject{{QStringLiteral("brightness"),
                                     QJsonObject{{QStringLiteral("minimum"), -100}}}});

        const QJsonObject schema =
            Daqster::nodeConfigSchema(defaults, QStringLiteral("VideoEffect"), provided);
        QCOMPARE(schema.value(QStringLiteral("title")).toString(), QStringLiteral("VideoEffect"));
        QVERIFY(schema.contains(QStringLiteral("$schema")));
        QCOMPARE(schema.value(QStringLiteral("properties")).toObject()
                     .value(QStringLiteral("brightness")).toObject()
                     .value(QStringLiteral("minimum")).toInt(), -100);
    }

    void schemaBuilderFallsBack()
    {
        const QJsonObject defaults{{QStringLiteral("brightness"), 0},
                                   {QStringLiteral("mode"), QStringLiteral("x")}};
        const QJsonObject schema =
            Daqster::nodeConfigSchema(defaults, QStringLiteral("Type"), QJsonObject());
        const QJsonObject properties = schema.value(QStringLiteral("properties")).toObject();
        QCOMPARE(properties.value(QStringLiteral("brightness")).toObject()
                     .value(QStringLiteral("type")).toString(), QStringLiteral("number"));
        QCOMPARE(properties.value(QStringLiteral("mode")).toObject()
                     .value(QStringLiteral("type")).toString(), QStringLiteral("string"));
    }

    void schemaBuilderPreservesProvidedTitleAndSchema()
    {
        QJsonObject provided;
        provided.insert(QStringLiteral("title"), QStringLiteral("Custom"));
        provided.insert(QStringLiteral("$schema"), QStringLiteral("draft-07"));
        const QJsonObject schema =
            Daqster::nodeConfigSchema(QJsonObject{}, QStringLiteral("Ignored"), provided);
        QCOMPARE(schema.value(QStringLiteral("title")).toString(), QStringLiteral("Custom"));
        QCOMPARE(schema.value(QStringLiteral("$schema")).toString(), QStringLiteral("draft-07"));
    }
};

QTEST_GUILESS_MAIN(ControlPlaneRouterTest)
#include "test_control_plane_router.moc"
