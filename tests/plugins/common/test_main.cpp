// Shared main() for daqster_common_tests.
//
// Two test classes, one QCoreApplication — the same shape as
// tests/plugins/nodeditor_nodes/test_lifecycle_main.cpp. QCoreApplication (not
// QApplication): the lifetime primitives under test own no widgets, but the
// event dispatcher is load-bearing — QThread::finished → deleteLater and
// QProcess::finished → deleteLater are delivered through it, so without an
// application object the "the worker/process deletes itself" half of the
// contract would never be exercised.

#include "test_process_owner.h"
#include "test_thread_owner.h"

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

    return status;
}
