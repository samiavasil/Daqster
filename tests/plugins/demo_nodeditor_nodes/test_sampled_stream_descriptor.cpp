#include <QtTest>
#include <QString>
#include <QVector>

#include "NodeDataTypes/SampledStreamDescriptor.h"
#include "test_sampled_stream_descriptor.h"

// SampledStreamDescriptor unit tests (REQ-SW-PL-022 AC 4). Every expectation
// below mirrors the defaults and inline helpers declared in
// SampledStreamDescriptor.h — nothing beyond the header API is assumed.
// Enumerators are compared through static_cast<int> so the explicit numeric
// values of the consolidation contract are pinned down portably.

void SampledStreamDescriptorTest::defaultConstruction_saneDefaults()
{
    const SampledStreamDescriptor d;

    QCOMPARE(d.sampleRate, 0.0);
    QVERIFY(d.channels.isEmpty());
    QCOMPARE(d.totalChannels(), 0);
    QCOMPARE(d.bytesPerFrame(), 0);
    QCOMPARE(static_cast<int>(d.endianness), static_cast<int>(SampleEndian::LittleEndian));
    QCOMPARE(d.unit, QStringLiteral("V"));
    QCOMPARE(d.amplitudeScale, 1.0);
    QCOMPARE(d.amplitudeOffset, 0.0);
    QVERIFY(d.domain.isEmpty());
    QVERIFY(d.deviceId.isEmpty());
    QVERIFY(d.sourceName.isEmpty());
    QCOMPARE(d.firstSampleTimestamp, qint64(0));
    QCOMPARE(d.expectedBufferSeconds, 0.0);
}

void SampledStreamDescriptorTest::sampleRate_doubleSemantics()
{
    // Audio rates are integral, but the field is double so sub-Hz DAQ
    // streams (e.g., 0.1 Hz logging) stay representable without scaling.
    SampledStreamDescriptor audio;
    audio.sampleRate = 44100.0;
    QCOMPARE(audio.sampleRate, 44100.0);

    SampledStreamDescriptor daq;
    daq.sampleRate = 0.1;
    QVERIFY(qAbs(daq.sampleRate - 0.1) < 1e-12);

    SampledStreamDescriptor vibration;
    vibration.sampleRate = 51200.5;
    QVERIFY(qAbs(vibration.sampleRate - 51200.5) < 1e-9);
}

void SampledStreamDescriptorTest::channels_perChannelNameAndType()
{
    SampledStreamDescriptor d;
    d.channels = {
        StreamChannelDescriptor { QStringLiteral("Left"), SampleType::FLOAT32 },
        StreamChannelDescriptor { QStringLiteral("Right"), SampleType::FLOAT32 },
        StreamChannelDescriptor { QStringLiteral("Ch0"), SampleType::INT16 },
    };

    QCOMPARE(d.totalChannels(), 3);
    QCOMPARE(d.channels.at(0).name, QStringLiteral("Left"));
    QCOMPARE(static_cast<int>(d.channels.at(0).sampleType), static_cast<int>(SampleType::FLOAT32));
    QCOMPARE(d.channels.at(1).name, QStringLiteral("Right"));
    QCOMPARE(static_cast<int>(d.channels.at(2).sampleType), static_cast<int>(SampleType::INT16));

    // A 16-channel DAQ stream must be representable (REQ-SW-PL-022 AC 4).
    SampledStreamDescriptor daq;
    for (int i = 0; i < 16; ++i)
        daq.channels.append({ QStringLiteral("Ch%1").arg(i), SampleType::INT24 });
    QCOMPARE(daq.totalChannels(), 16);
    QCOMPARE(daq.channels.at(15).name, QStringLiteral("Ch15"));
}

void SampledStreamDescriptorTest::sampleTypeEnum_allTenValues()
{
    // Extended from the legacy 6-type enum: 8/16/24/32-bit signed/unsigned
    // integers plus 32/64-bit IEEE 754 floats — values are part of the
    // consolidation contract and must not shift.
    QCOMPARE(static_cast<int>(SampleType::INT8), 0);
    QCOMPARE(static_cast<int>(SampleType::UINT8), 1);
    QCOMPARE(static_cast<int>(SampleType::INT16), 2);
    QCOMPARE(static_cast<int>(SampleType::UINT16), 3);
    QCOMPARE(static_cast<int>(SampleType::INT24), 4);
    QCOMPARE(static_cast<int>(SampleType::UINT24), 5);
    QCOMPARE(static_cast<int>(SampleType::INT32), 6);
    QCOMPARE(static_cast<int>(SampleType::UINT32), 7);
    QCOMPARE(static_cast<int>(SampleType::FLOAT32), 8);
    QCOMPARE(static_cast<int>(SampleType::FLOAT64), 9);
}

void SampledStreamDescriptorTest::endianness_field()
{
    const SampledStreamDescriptor d;
    QCOMPARE(static_cast<int>(d.endianness), static_cast<int>(SampleEndian::LittleEndian));

    SampledStreamDescriptor big;
    big.endianness = SampleEndian::BigEndian;
    QCOMPARE(static_cast<int>(big.endianness), static_cast<int>(SampleEndian::BigEndian));
}

void SampledStreamDescriptorTest::amplitude_unit_scale_offset()
{
    const SampledStreamDescriptor d;
    QCOMPARE(d.unit, QStringLiteral("V"));
    QCOMPARE(d.amplitudeScale, 1.0);
    QCOMPARE(d.amplitudeOffset, 0.0);

    // Raw ADC counts: scale converts LSB -> volts, offset re-centers a
    // bipolar ADC whose mid-scale sits at 0 counts.
    SampledStreamDescriptor raw;
    raw.unit = QStringLiteral("raw");
    raw.amplitudeScale = 0.00030518;
    raw.amplitudeOffset = -5.0;
    QCOMPARE(raw.unit, QStringLiteral("raw"));
    QVERIFY(qAbs(raw.amplitudeScale - 0.00030518) < 1e-9);
    QCOMPARE(raw.amplitudeOffset, -5.0);
}

void SampledStreamDescriptorTest::domain_discriminator()
{
    // One unified descriptor type: `domain` discriminates at runtime instead
    // of separate per-domain classes ("audio", "daq", "ecg", ...).
    SampledStreamDescriptor audio;
    audio.domain = QStringLiteral("audio");
    QCOMPARE(audio.domain, QStringLiteral("audio"));

    SampledStreamDescriptor daq;
    daq.domain = QStringLiteral("daq");
    QCOMPARE(daq.domain, QStringLiteral("daq"));

    SampledStreamDescriptor ecg;
    ecg.domain = QStringLiteral("ecg");
    QCOMPARE(ecg.domain, QStringLiteral("ecg"));

    // Default is an empty domain; producers set it explicitly.
    QVERIFY(SampledStreamDescriptor{}.domain.isEmpty());
}

void SampledStreamDescriptorTest::sourceAndTimestamp_fields()
{
    const SampledStreamDescriptor d;
    QVERIFY(d.deviceId.isEmpty());
    QVERIFY(d.sourceName.isEmpty());
    QCOMPARE(d.firstSampleTimestamp, qint64(0));

    SampledStreamDescriptor s;
    s.deviceId = QStringLiteral("alsa:hw:0");
    s.sourceName = QStringLiteral("microphone-front");
    s.firstSampleTimestamp = qint64(1724300000123456LL); // µs since epoch
    QCOMPARE(s.deviceId, QStringLiteral("alsa:hw:0"));
    QCOMPARE(s.sourceName, QStringLiteral("microphone-front"));
    QCOMPARE(s.firstSampleTimestamp, qint64(1724300000123456LL));
}

void SampledStreamDescriptorTest::sampleTypeByteSize_allTypes()
{
    QCOMPARE(sampleTypeByteSize(SampleType::INT8), 1);
    QCOMPARE(sampleTypeByteSize(SampleType::UINT8), 1);
    QCOMPARE(sampleTypeByteSize(SampleType::INT16), 2);
    QCOMPARE(sampleTypeByteSize(SampleType::UINT16), 2);
    QCOMPARE(sampleTypeByteSize(SampleType::INT24), 3);
    QCOMPARE(sampleTypeByteSize(SampleType::UINT24), 3);
    QCOMPARE(sampleTypeByteSize(SampleType::INT32), 4);
    QCOMPARE(sampleTypeByteSize(SampleType::UINT32), 4);
    QCOMPARE(sampleTypeByteSize(SampleType::FLOAT32), 4);
    QCOMPARE(sampleTypeByteSize(SampleType::FLOAT64), 8);
}

void SampledStreamDescriptorTest::sampleTypeName_allTypes()
{
    QCOMPARE(sampleTypeName(SampleType::INT8), QStringLiteral("int8"));
    QCOMPARE(sampleTypeName(SampleType::UINT8), QStringLiteral("uint8"));
    QCOMPARE(sampleTypeName(SampleType::INT16), QStringLiteral("int16"));
    QCOMPARE(sampleTypeName(SampleType::UINT16), QStringLiteral("uint16"));
    QCOMPARE(sampleTypeName(SampleType::INT24), QStringLiteral("int24"));
    QCOMPARE(sampleTypeName(SampleType::UINT24), QStringLiteral("uint24"));
    QCOMPARE(sampleTypeName(SampleType::INT32), QStringLiteral("int32"));
    QCOMPARE(sampleTypeName(SampleType::UINT32), QStringLiteral("uint32"));
    QCOMPARE(sampleTypeName(SampleType::FLOAT32), QStringLiteral("float32"));
    QCOMPARE(sampleTypeName(SampleType::FLOAT64), QStringLiteral("float64"));
}

void SampledStreamDescriptorTest::bytesPerFrame_mixedLayout()
{
    // Interleaved layout [ch0_s0][ch1_s0]...[chN_s0][ch0_s1]...: one frame is
    // the sum of the per-channel sample sizes — mixed-type DAQ frames included.
    SampledStreamDescriptor d;
    d.channels = {
        StreamChannelDescriptor { QStringLiteral("Ch0"), SampleType::INT16 },   // 2
        StreamChannelDescriptor { QStringLiteral("Ch1"), SampleType::FLOAT32 }, // 4
        StreamChannelDescriptor { QStringLiteral("Ch2"), SampleType::UINT8 },   // 1
        StreamChannelDescriptor { QStringLiteral("Ch3"), SampleType::INT24 },   // 3
    };
    QCOMPARE(d.bytesPerFrame(), 10);

    SampledStreamDescriptor stereo64;
    stereo64.channels = {
        StreamChannelDescriptor { QStringLiteral("L"), SampleType::FLOAT64 },
        StreamChannelDescriptor { QStringLiteral("R"), SampleType::FLOAT64 },
    };
    QCOMPARE(stereo64.bytesPerFrame(), 16);

    QCOMPARE(SampledStreamDescriptor{}.bytesPerFrame(), 0);
}

// No QTEST_GUILESS_MAIN here: this test class shares the demo_nodeditor_nodes
// test binary whose main lives in test_main.cpp.
