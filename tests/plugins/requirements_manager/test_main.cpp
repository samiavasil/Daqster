#include <QtTest>
#include <QCoreApplication>

#include "test_parser.h"
#include "test_model.h"
#include "test_validator.h"
#include "test_graph.h"
#include "test_graph_layout.h"
#include "test_search.h"

// Shared main for the Requirements Manager test classes. Each class is
// declared in its own header so a single binary can run all of them through
// QTest::qExec. This mirrors what QTEST_GUILESS_MAIN expands to for a single
// class (QCoreApplication + AA_Use96Dpi + qExec), keeping the tests headless.
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);

    int status = 0;
    {
        TestParser parser;
        status |= QTest::qExec(&parser, argc, argv);
    }
    {
        TestModel model;
        status |= QTest::qExec(&model, argc, argv);
    }
    {
        TestValidator validator;
        status |= QTest::qExec(&validator, argc, argv);
    }
    {
        TestGraph graph;
        status |= QTest::qExec(&graph, argc, argv);
    }
    {
        TestGraphLayout graphLayout;
        status |= QTest::qExec(&graphLayout, argc, argv);
    }
    {
        TestSearchEngine searchEngine;
        status |= QTest::qExec(&searchEngine, argc, argv);
    }
    return status;
}
