#include <QtTest>
#include <QCoreApplication>

#include "test_video_transform_ops.h"
#include "test_stream_url_validator.h"
#include "test_sampled_data.h"
#include "test_fft_util.h"
#include "test_audio_buffer_to_sampled.h"
#include "test_video_perf_badge.h"

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
    {
        FftUtilTest fftUtil;
        status |= QTest::qExec(&fftUtil, argc, argv);
    }
    {
        AudioBufferToSampledTest audioBufferToSampled;
        status |= QTest::qExec(&audioBufferToSampled, argc, argv);
    }
    {
        TestVideoPerfBadge videoPerfBadge;
        status |= QTest::qExec(&videoPerfBadge, argc, argv);
    }
    return status;
}
