#pragma once

#include <QtTest>

// Test class for the shipped .flow fixtures in tests/data (REQ-SW-PL-049
// AC6, REQ-SW-PL-048 AC7).
//
// Every fixture is a contract: the perf harness (scripts/run_perf_report.sh in
// the private repo) launches Daqster with DAQSTER_AUTOSTART_FLOW=<fixture>, the
// runtime shell parses it, and the layout engine materialises widgets from its
// "ui" section. A fixture that parses into an empty section, or that references
// a node id that is not in the file, does not fail loudly at load time — the
// node is silently skipped as "<unnamed>" and every connection touching it
// disappears. That is exactly the class of defect the model-name serialization
// test guards on the save side, seen from the fixture side.
//
// These tests are QtCore-only (JSON + the two section models) but share the
// QApplication binary — see test_main.cpp.
class TestFlowFixtures : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // AC1/AC2 — shape of the "ui" section for the v1 schema.
    void everyFixtureHasNodes();
    void everyNodeCarriesAModelName();
    void everyConnectionEndpointExists();

    // Known defect, pinned so it stays visible and cannot grow. See the
    // implementation for why it cannot be fixed from this repository.
    void arrayFormFixtureSetIsPinned_KnownDefect();

    void v1NodeUiReferencesExistingNode();
    void v1WorkspaceReferencesExistingWorkspace();

    // The version dispatch itself (RuntimeShell.cpp:163 /
    // NodeEditorIdeObject) — a fixture whose version disagrees with its own
    // shape would take the wrong branch and get no layout at all.
    void versionDispatchMatchesFixtureShape();

    // AC1/AC4 — schema-v2 layout tree fixtures.
    void v2LayoutReferencesExistingNodes();

    // The perf harness names these five scenarios explicitly; a rename or a
    // deletion silently turns a whole measurement into "flow file missing".
    void perfHarnessScenariosExist();

    // Negative fixtures must STAY broken, or the validation path loses the
    // input that exercises it.
    void malformedFixtureStaysMalformed();

    // LayoutEngine is only ever handed a tree that parsed into something;
    // assert the shape of the one schema-v2 fixture rather than only its
    // referential integrity.
    void video4viewsGridFixtureBuildsItsDeclaredGrid();

    void everyFixtureIsValidJson();

private:
    /// Parses one fixture. Returns an empty object when the file cannot be
    /// opened or does not hold a JSON object — callers assert on that.
    QJsonObject loadFixture(const QString &fileName);

    QStringList fixtureFiles() const;
    static QString fixturesDir();
};