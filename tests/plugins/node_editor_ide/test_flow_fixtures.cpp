#include "test_flow_fixtures.h"

#include "FlowUiSection.h"
#include "LayoutTree.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using FlowUi::LayoutNode;
using FlowUi::LayoutSection;

namespace {

// Fixtures that are SUPPOSED to be broken. They are the negative inputs of
// RuntimeShell::loadFlow (REQ-SW-PL-048 AC6) and of the tolerant scene loader:
//   - malformed.flow   truncated JSON → loadFlow must reject it before touching
//                      the graph model
//   - invalid_test.flow / missing_node.flow — well-formed JSON naming a
//                      "BogusUnregisteredNode", i.e. a model-name that no
//                      registry can resolve. loadSceneTolerant skips those
//                      nodes and keeps the rest.
// They are exempt from the "every fixture is valid / every node is named"
// assertions below — that exemption IS the assertion, in everyFixtureIsValidJson
// and everyNodeCarriesAModelName.
const QStringList kNegativeFixtures = {
    QStringLiteral("malformed.flow"),
    QStringLiteral("invalid_test.flow"),
    QStringLiteral("missing_node.flow"),
};

// The five scenarios scripts/run_perf_report.sh (private repo) measures. The
// harness resolves them as tests/data/<name>.flow in the PUBLIC repo and
// aborts the whole run if one is missing, so a rename here silently kills ten
// measurements.
const QStringList kPerfScenarios = {
    QStringLiteral("empty_scene"),
    QStringLiteral("video_1view"),
    QStringLiteral("video_2views"),
    QStringLiteral("video_4views"),
    QStringLiteral("video_effect_chain"),
};

// Fixtures that are SUPPOSED to have no nodes. empty_scene.flow is the
// baseline measurement of the perf harness (a runtime with no graph at all), so
// an empty node list is its whole purpose.
const QStringList kEmptySceneFixtures = {
    QStringLiteral("empty_scene.flow"),
};

/// Endpoints of one connection, normalised to (outNodeId, inNodeId).
///
/// The shipped fixtures carry connections in TWO encodings:
///
///   object  {"outNodeId":0,"outPortIndex":0,"inNodeId":1,"inPortIndex":0}
///   array   [outNodeId, outPortIndex, inNodeId, inPortIndex]
///
/// DataFlowGraphModel::saveConnections() writes the object form (via
/// QtNodes::toJson), and loadConnections() reads only the object form
/// (QtNodes::fromJson calls connection.toObject(), which yields an EMPTY object
/// for a JSON array). So every array-form fixture loads with ZERO connections —
/// see arrayFormConnectionsAreNotUnderstoodByTheLoader() for the pinned list.
///
/// Normalising both here is what lets the referential-integrity check below
/// still cover the array-form fixtures' node ids, instead of reading -1 for
/// every endpoint and reporting a bogus "unknown node" failure.
QPair<int, int> connectionEndpoints(const QJsonValue &value)
{
    if (value.isObject()) {
        const QJsonObject conn = value.toObject();
        return {conn.value(QStringLiteral("outNodeId")).toInt(-1),
                conn.value(QStringLiteral("inNodeId")).toInt(-1)};
    }
    if (value.isArray()) {
        const QJsonArray conn = value.toArray();
        if (conn.size() != 4)
            return {-1, -1};
        // Array order matches QtNodes' stream operator<< above: outNode,
        // outPort, inNode, inPort.
        return {conn.at(0).toInt(-1), conn.at(2).toInt(-1)};
    }
    return {-1, -1};
}

bool isArrayFormConnection(const QJsonValue &value)
{
    return value.isArray();
}

/// Every node id referenced by a layout tree, recursively (panels, grid cells,
/// split/tab children). Docks are handled by the caller.
void collectNodeIds(const LayoutNode &node, QList<int> &out)
{
    switch (node.type) {
    case LayoutNode::Type::Panel:
        if (node.nodeId != QtNodes::InvalidNodeId)
            out.append(node.nodeId);
        break;
    case LayoutNode::Type::Grid:
        for (const auto &cell : node.cells) {
            if (cell.has_value())
                out.append(static_cast<int>(*cell));
        }
        break;
    case LayoutNode::Type::Split:
    case LayoutNode::Type::Tab:
        for (const LayoutNode &child : node.children)
            collectNodeIds(child, out);
        break;
    }
}

} // namespace

QString TestFlowFixtures::fixturesDir()
{
    return QStringLiteral(DAQSTER_FLOW_FIXTURES_DIR);
}

QStringList TestFlowFixtures::fixtureFiles() const
{
    QDir dir(fixturesDir());
    QStringList files = dir.entryList({QStringLiteral("*.flow")}, QDir::Files, QDir::Name);
    return files;
}

QJsonObject TestFlowFixtures::loadFixture(const QString &fileName)
{
    QFile file(QDir(fixturesDir()).filePath(fileName));
    if (!file.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }
    const QByteArray data = file.readAll();
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return QJsonObject();
    return doc.object();
}

void TestFlowFixtures::initTestCase()
{
    // Guards against the whole class passing vacuously: if the compile-time
    // fixtures directory were wrong or empty, every loop below would iterate
    // over nothing and report PASS.
    const QDir dir(fixturesDir());
    QVERIFY2(dir.exists(), qPrintable(QStringLiteral("fixtures dir missing: %1").arg(fixturesDir())));
    const QStringList files = fixtureFiles();
    QVERIFY2(files.size() >= 20,
             qPrintable(QStringLiteral("only %1 fixtures found in %2 — expected the full set")
                            .arg(files.size())
                            .arg(fixturesDir())));
}

void TestFlowFixtures::everyFixtureIsValidJson()
{
    for (const QString &fileName : fixtureFiles()) {
        if (kNegativeFixtures.contains(fileName))
            continue;
        QFile file(QDir(fixturesDir()).filePath(fileName));
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(fileName));
        const QByteArray data = file.readAll();
        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        QVERIFY2(err.error == QJsonParseError::NoError,
                 qPrintable(QStringLiteral("%1: %2 at offset %3")
                                .arg(fileName, err.errorString())
                                .arg(err.offset)));
        QVERIFY2(doc.isObject(), qPrintable(QStringLiteral("%1: top level is not an object").arg(fileName)));
    }
}

void TestFlowFixtures::everyFixtureHasNodes()
{
    for (const QString &fileName : fixtureFiles()) {
        if (kNegativeFixtures.contains(fileName))
            continue;
        const QJsonObject scene = loadFixture(fileName);
        QVERIFY2(scene.contains(QStringLiteral("nodes")), qPrintable(fileName));
        const QJsonArray nodes = scene.value(QStringLiteral("nodes")).toArray();
        if (kEmptySceneFixtures.contains(fileName)) {
            QVERIFY2(nodes.isEmpty(),
                     qPrintable(QStringLiteral("%1 is the perf harness's empty baseline — "
                                               "adding nodes to it changes what it measures")
                                    .arg(fileName)));
            continue;
        }
        QVERIFY2(!nodes.isEmpty(), qPrintable(QStringLiteral("%1: empty node list").arg(fileName)));

        QSet<int> ids;
        for (const QJsonValue &value : nodes) {
            const QJsonObject node = value.toObject();
            const int id = node.value(QStringLiteral("id")).toInt(-1);
            QVERIFY2(id >= 0, qPrintable(QStringLiteral("%1: node without a usable id").arg(fileName)));
            QVERIFY2(!ids.contains(id),
                     qPrintable(QStringLiteral("%1: duplicate node id %2").arg(fileName).arg(id)));
            ids.insert(id);
        }
    }
}

void TestFlowFixtures::everyNodeCarriesAModelName()
{
    for (const QString &fileName : fixtureFiles()) {
        // The two bogus-name fixtures are exempt by design; malformed.flow has
        // no parseable scene at all.
        if (kNegativeFixtures.contains(fileName))
            continue;
        const QJsonArray nodes = loadFixture(fileName).value(QStringLiteral("nodes")).toArray();
        for (const QJsonValue &value : nodes) {
            const QJsonObject node = value.toObject();
            const QJsonObject internal = node.value(QStringLiteral("internal-data")).toObject();
            const QString modelName = internal.value(QStringLiteral("model-name")).toString();
            // DataFlowGraphModel::loadNode resolves the type from exactly this
            // key. A node without it loads as an empty type and is skipped
            // together with every connection touching it — silent data loss
            // with no save-time warning (see test_model_name_serialization).
            QVERIFY2(!modelName.isEmpty(),
                     qPrintable(QStringLiteral("%1: node %2 has no internal-data/model-name")
                                    .arg(fileName)
                                    .arg(node.value(QStringLiteral("id")).toInt())));
        }
    }
}

void TestFlowFixtures::everyConnectionEndpointExists()
{
    for (const QString &fileName : fixtureFiles()) {
        if (kNegativeFixtures.contains(fileName))
            continue;
        const QJsonObject scene = loadFixture(fileName);
        QSet<int> ids;
        const QJsonArray nodes = scene.value(QStringLiteral("nodes")).toArray();
        for (const QJsonValue &value : nodes)
            ids.insert(value.toObject().value(QStringLiteral("id")).toInt(-1));

        const QJsonArray connections = scene.value(QStringLiteral("connections")).toArray();
        for (const QJsonValue &value : connections) {
            const QPair<int, int> endpoints = connectionEndpoints(value);
            const int outId = endpoints.first;
            const int inId = endpoints.second;
            QVERIFY2(ids.contains(outId),
                     qPrintable(QStringLiteral("%1: connection from unknown node %2").arg(fileName).arg(outId)));
            QVERIFY2(ids.contains(inId),
                     qPrintable(QStringLiteral("%1: connection into unknown node %2").arg(fileName).arg(inId)));
        }
    }
}

void TestFlowFixtures::arrayFormFixtureSetIsPinned_KnownDefect()
{
    // Pinned defect (found 2026-10-05): 22 of the 43 shipped fixtures.
    //
    // DataFlowGraphModel::loadConnections() does
    //     ConnectionId connId = fromJson(connection.toObject());
    // and QtNodes::fromJson reads the keys "outNodeId"/"outPortIndex"/
    // "inNodeId"/"inPortIndex". A JSON *array* connection stringifies through
    // toObject() to an EMPTY object, so every endpoint resolves to
    // InvalidNodeId and connectionPossible() rejects the edge. The fixture
    // loads its nodes and silently loses its entire graph — the nodes appear,
    // unconnected, with no warning.
    //
    // The list below is the exact affected set as of this commit. Asserting it
    // does two things: it puts the debt in the test suite instead of only in a
    // comment, and it makes the set impossible to grow unnoticed.
    //
    // If this test FAILS with a diff, that is progress, not a regression:
    // someone converted fixtures to the object form. Delete the converted
    // names from the list. Fixing the loader itself means teaching
    // QtNodes::fromJson the array form, which lives in the nodeeditor GIT
    // SUBMODULE and cannot be changed from this repository; the workaround is
    // to re-save each scene from the app, which writes the object form.
    const QStringList expected = {
        QStringLiteral("test_arithmetic.flow"),
        QStringLiteral("test_audio_source_display.flow"),
        QStringLiteral("test_camera_source.flow"),
        QStringLiteral("test_daq_display.flow"),
        QStringLiteral("test_file_playback.flow"),
        QStringLiteral("test_file_record.flow"),
        QStringLiteral("test_gamepad.flow"),
        QStringLiteral("test_generic_display.flow"),
        QStringLiteral("test_gpu_monitor.flow"),
        QStringLiteral("test_jack_detect.flow"),
        QStringLiteral("test_llama_console.flow"),
        QStringLiteral("test_modulo.flow"),
        QStringLiteral("test_network_loopback.flow"),
        QStringLiteral("test_number_basic.flow"),
        QStringLiteral("test_pcap.flow"),
        QStringLiteral("test_pluto_sdr.flow"),
        QStringLiteral("test_stream_source.flow"),
        QStringLiteral("test_system_monitor.flow"),
        QStringLiteral("test_video_custom_shader.flow"),
        QStringLiteral("test_video_effect_chain.flow"),
        QStringLiteral("test_video_file_to_output.flow"),
        QStringLiteral("test_video_frame_sampler.flow"),
    };

    QStringList found;
    for (const QString &fileName : fixtureFiles()) {
        if (kNegativeFixtures.contains(fileName))
            continue;
        const QJsonArray connections =
            loadFixture(fileName).value(QStringLiteral("connections")).toArray();
        for (const QJsonValue &value : connections) {
            if (isArrayFormConnection(value)) {
                found.append(fileName);
                break;
            }
        }
    }
    found.sort();

    QCOMPARE(found, expected);
}

void TestFlowFixtures::v1NodeUiReferencesExistingNode()
{
    for (const QString &fileName : fixtureFiles()) {
        const QJsonObject scene = loadFixture(fileName);
        const QJsonObject ui = scene.value(QStringLiteral("ui")).toObject();
        if (ui.isEmpty() || ui.value(QStringLiteral("version")).toInt(1) >= 2)
            continue; // no ui section, or schema v2 (covered separately)

        QSet<int> ids;
        const QJsonArray nodes = scene.value(QStringLiteral("nodes")).toArray();
        for (const QJsonValue &value : nodes)
            ids.insert(value.toObject().value(QStringLiteral("id")).toInt(-1));

        const FlowUi::UiSection section = FlowUi::UiSection::fromJson(ui);
        for (auto it = section.nodes.constBegin(); it != section.nodes.constEnd(); ++it) {
            QVERIFY2(ids.contains(static_cast<int>(it.key())),
                     qPrintable(QStringLiteral("%1: ui.nodes has entry %2 but no such node in the scene")
                                    .arg(fileName)
                                    .arg(it.key())));
        }
    }
}

void TestFlowFixtures::v1WorkspaceReferencesExistingWorkspace()
{
    for (const QString &fileName : fixtureFiles()) {
        const QJsonObject scene = loadFixture(fileName);
        const QJsonObject ui = scene.value(QStringLiteral("ui")).toObject();
        if (ui.isEmpty() || ui.value(QStringLiteral("version")).toInt(1) >= 2)
            continue;

        const FlowUi::UiSection section = FlowUi::UiSection::fromJson(ui);
        QSet<int> workspaceIds;
        for (const FlowUi::WorkspaceUi &ws : section.workspaces)
            workspaceIds.insert(ws.id);

        for (auto it = section.nodes.constBegin(); it != section.nodes.constEnd(); ++it) {
            // Workspace 0 exists implicitly in RuntimeShell::arrangeWorkspaces
            // (a missing list gets a default workspace), so only a nonzero
            // reference can dangle.
            if (it.value().workspace == 0)
                continue;
            QVERIFY2(workspaceIds.contains(it.value().workspace),
                     qPrintable(QStringLiteral("%1: node %2 points at workspace %3, which is not declared")
                                    .arg(fileName)
                                    .arg(it.key())
                                    .arg(it.value().workspace)));
        }
    }
}

void TestFlowFixtures::versionDispatchMatchesFixtureShape()
{
    // RuntimeShell.cpp:163 dispatches on `ui.version >= 2` → LayoutSection,
    // else → UiSection. NodeEditorIdeObject applies the same rule. A fixture
    // whose declared version disagrees with its own shape takes the wrong
    // branch and gets either no layout or an empty one, with no error.
    for (const QString &fileName : fixtureFiles()) {
        const QJsonObject scene = loadFixture(fileName);
        const QJsonObject ui = scene.value(QStringLiteral("ui")).toObject();
        if (ui.isEmpty())
            continue;

        const int version = ui.value(QStringLiteral("version")).toInt(1);
        if (version >= 2) {
            const LayoutSection layout = LayoutSection::fromJson(ui);
            QVERIFY2(!layout.workspaces.empty(),
                     qPrintable(QStringLiteral("%1: ui.version=%2 but no workspace parses — "
                                               "the layout engine would build an empty workspace")
                                    .arg(fileName)
                                    .arg(version)));
            for (const FlowUi::WorkspaceLayout &ws : layout.workspaces) {
                QVERIFY2(ws.root.type != LayoutNode::Type::Panel || ws.root.nodeId != QtNodes::InvalidNodeId,
                         qPrintable(QStringLiteral("%1: workspace %2 has an empty root — the whole "
                                                   "workspace renders as a blank panel")
                                        .arg(fileName)
                                        .arg(ws.id)));
            }
        } else {
            const FlowUi::UiSection section = FlowUi::UiSection::fromJson(ui);
            QVERIFY2(!section.workspaces.empty(),
                     qPrintable(QStringLiteral("%1: ui.version=%2 but no workspace parses — "
                                               "arrangeWorkspaces would build nothing")
                                    .arg(fileName)
                                    .arg(version)));
        }
    }
}

void TestFlowFixtures::v2LayoutReferencesExistingNodes()
{
    for (const QString &fileName : fixtureFiles()) {
        const QJsonObject scene = loadFixture(fileName);
        const QJsonObject ui = scene.value(QStringLiteral("ui")).toObject();
        if (ui.isEmpty() || ui.value(QStringLiteral("version")).toInt(1) < 2)
            continue;

        QSet<int> ids;
        const QJsonArray nodes = scene.value(QStringLiteral("nodes")).toArray();
        for (const QJsonValue &value : nodes)
            ids.insert(value.toObject().value(QStringLiteral("id")).toInt(-1));

        const LayoutSection layout = LayoutSection::fromJson(ui);
        for (const FlowUi::WorkspaceLayout &ws : layout.workspaces) {
            QList<int> referenced;
            collectNodeIds(ws.root, referenced);
            for (const FlowUi::DockLayout &dock : ws.docks) {
                if (dock.node != QtNodes::InvalidNodeId)
                    referenced.append(static_cast<int>(dock.node));
            }
            for (int nodeId : referenced) {
                QVERIFY2(ids.contains(nodeId),
                         qPrintable(QStringLiteral("%1: workspace %2 references node %3, "
                                                   "which is not in the scene — deembedNodeWidget "
                                                   "returns nullptr and the cell stays blank")
                                        .arg(fileName)
                                        .arg(ws.id)
                                        .arg(nodeId)));
            }
        }
    }
}

void TestFlowFixtures::video4viewsGridFixtureBuildsItsDeclaredGrid()
{
    // The only schema-v2 fixture. Its numbers are asserted rather than merely
    // validated: it is the reference for what a v2 flow looks like, and it is
    // the fixture that exercises RuntimeShell::arrangeLayout at all.
    const QString fileName = QStringLiteral("video_4views_grid.flow");
    const QJsonObject scene = loadFixture(fileName);
    QVERIFY(!scene.isEmpty());

    const LayoutSection layout =
        LayoutSection::fromJson(scene.value(QStringLiteral("ui")).toObject());
    QCOMPARE(layout.version, 2);
    QCOMPARE(layout.workspaces.size(), std::size_t(1));

    const FlowUi::WorkspaceLayout &ws = layout.workspaces.front();
    QCOMPARE(ws.root.type, LayoutNode::Type::Grid);
    QCOMPARE(ws.root.rows, 2);
    QCOMPARE(ws.root.cols, 2);
    QCOMPARE(ws.root.cells.size(), std::size_t(4));
    for (const auto &cell : ws.root.cells)
        QVERIFY(cell.has_value());

    // One dock holding the source node, pinned right — the grid itself holds
    // only the four VideoOutput nodes.
    QCOMPARE(ws.docks.size(), std::size_t(1));
    QCOMPARE(ws.docks.front().area, FlowUi::DockLayout::Area::Right);
    const int dockNode = static_cast<int>(ws.docks.front().node);
    QVERIFY(dockNode != static_cast<int>(QtNodes::InvalidNodeId));
    for (const auto &cell : ws.root.cells) {
        QVERIFY(cell.has_value());
        QVERIFY2(static_cast<int>(*cell) != dockNode,
                 "the dock node must not also occupy a grid cell — deembedNodeWidget "
                 "re-parents the widget, so the cell would end up holding a dead pointer");
    }
}

void TestFlowFixtures::malformedFixtureStaysMalformed()
{
    // RuntimeShell::loadFlow (REQ-SW-PL-048 AC6) rejects a scene whose JSON
    // does not parse, BEFORE the graph model sees anything. The only input
    // that exercises that branch is this fixture — so if somebody "repairs"
    // it, the branch becomes untested rather than fixed, and nothing here
    // fails loudly. Assert the breakage on purpose.
    const QString fileName = QStringLiteral("malformed.flow");
    QVERIFY2(fixtureFiles().contains(fileName),
             "malformed.flow is gone — RuntimeShell::loadFlow's JSON-rejection "
             "branch has no fixture left to exercise it");

    QFile file(QDir(fixturesDir()).filePath(fileName));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QJsonParseError err{};
    QJsonDocument::fromJson(file.readAll(), &err);
    QVERIFY2(err.error != QJsonParseError::NoError,
             "malformed.flow now parses — see the comment above before changing this assertion");
}

void TestFlowFixtures::perfHarnessScenariosExist()
{
    for (const QString &scenario : kPerfScenarios) {
        const QString fileName = scenario + QStringLiteral(".flow");
        const QStringList files = fixtureFiles();
        QVERIFY2(files.contains(fileName),
                 qPrintable(QStringLiteral("perf scenario fixture missing: %1 — the private "
                                           "run_perf_report.sh aborts the whole run without it")
                                .arg(fileName)));
        // And it must be parseable, or the measurement is INVALID (the harness
        // treats "no [PERF] lines" as a failed run, not as a zero).
        const QJsonObject scene = loadFixture(fileName);
        QVERIFY2(!scene.isEmpty(),
                 qPrintable(QStringLiteral("%1: does not parse — the perf measurement would be "
                                           "reported as ERROR").arg(fileName)));
    }
}