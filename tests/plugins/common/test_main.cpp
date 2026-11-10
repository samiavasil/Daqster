// Shared main() for daqster_common_tests.
//
// Three test classes, one QCoreApplication — the same shape as
// tests/plugins/nodeditor_nodes/test_lifecycle_main.cpp. QCoreApplication (not
// QApplication): the lifetime primitives under test own no widgets, but the
// event dispatcher is load-bearing — QThread::finished → deleteLater and
// QProcess::finished → deleteLater are delivered through it, so without an
// application object the "the worker/process deletes itself" half of the
// contract would never be exercised.

#include "test_dependency_manager.h"
#include "test_local_server_service.h"
#include "test_process_owner.h"
#include "test_thread_owner.h"
#include "test_websocket_client.h"

#include <QtTest>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    int status = 0;

    {
        ThreadOwnerTest threadOwner;
        status |= QTest::qExec(&threadOwner, argc, argv);
    }
    {
        ProcessOwnerTest processOwner;
        status |= QTest::qExec(&processOwner, argc, argv);
    }
    {
        // REQ-SW-PL-081: the local server supervisor (free port → start →
        // `/health` → ownership file → bounded stop), against a real stub
        // process — no model, no GPU.
        TestLocalServerService localServerService;
        status |= QTest::qExec(&localServerService, argc, argv);
    }

    {
        // REQ-SW-PL-087: the framework WebSocket client (RFC 6455 over
        // Qt::Network), against an in-test server — no Qt WebSockets.
        TestWebSocketClient webSocketClient;
        status |= QTest::qExec(&webSocketClient, argc, argv);
    }

    {
        // REQ-SW-PL-088: declarative dependency management — manifest
        // validation, checksum verification, adopt + lockfile round-trip.
        // No network: install() is covered through its validation half.
        TestDependencyManager dependencyManager;
        status |= QTest::qExec(&dependencyManager, argc, argv);
    }

    return status;
}
