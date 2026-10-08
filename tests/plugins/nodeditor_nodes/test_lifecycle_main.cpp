// Shared main() for demo_nodeditor_lifecycle_tests.
//
// The target deliberately reuses MODEL_NAME_TEST_SOURCES, which brings in
// test_model_name_serialization.cpp — and that file's own QTEST_MAIN. Two mains
// cannot coexist in one binary, and whichever loses silently keeps its test
// class unregistered: the binary links, ctest reports green, and the tests that
// were supposed to run never execute. That is exactly what happened the first
// time this target was built, so the two classes are run explicitly here and
// test_model_name_serialization.cpp's QTEST_MAIN stays as the main() of the
// OTHER binary that shares those sources.
//
// QCoreApplication, not QApplication: the models under test own no widget
// (REQ-SW-PL-051) and are idle until start().

#include "test_model_name_serialization.h"
#include "test_thread_lifecycle.h"

#include <QtTest>

int main(int argc, char *argv[])
{
    int status = 0;

    {
        ModelNameSerializationTest modelName;
        status |= QTest::qExec(&modelName, argc, argv);
    }
    {
        TestThreadLifecycle lifecycle;
        status |= QTest::qExec(&lifecycle, argc, argv);
    }

    return status;
}
