#include <QtTest>
#include <QCoreApplication>

#include "test_video_transform_ops.h"
#include "test_stream_url_validator.h"
#include "test_sampled_data.h"

// Shared main for the demo_nodeditor_nodes video test classes. Each class is
// declared in its own header so a single binary can run all of them through
// QTest::qExec. This mirrors what QTEST_GUILESS_MAIN expands to for a single
// class (QCoreApplication + AA_Use96Dpi + qExec), keeping the tests headless.
// QImage is a QtGui value class and does not require a QGuiApplication.
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);

    int status = 0;
    {
        TestVideoTransformOps videoOps;
        status |= QTest::qExec(&videoOps, argc, argv);
    }
    {
        TestStreamUrlValidator streamUrl;
        status |= QTest::qExec(&streamUrl, argc, argv);
    }
    {
        SampledDataTest sampledData;
        status |= QTest::qExec(&sampledData, argc, argv);
    }
    return status;
}
