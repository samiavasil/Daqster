#pragma once

#include <QtTest>

// Test class for SampledStreamDescriptor (REQ-SW-PL-022 AC 4): the canonical
// self-describing descriptor for sampled streams (audio, DAQ, sensors) that
// consolidates the legacy GenericStreamConfig and QDevIOStreamConfig into a
// single type with a runtime `domain` discriminator. Declared in a header so
// it shares the demo_nodeditor_nodes test binary through QTest::qExec in
// test_main.cpp (same pattern as SampledDataTest).
class SampledStreamDescriptorTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstruction_saneDefaults();
    void sampleRate_doubleSemantics();
    void channels_perChannelNameAndType();
    void sampleTypeEnum_allTenValues();
    void endianness_field();
    void amplitude_unit_scale_offset();
    void domain_discriminator();
    void sourceAndTimestamp_fields();
    void sampleTypeByteSize_allTypes();
    void sampleTypeName_allTypes();
    void bytesPerFrame_mixedLayout();
};
