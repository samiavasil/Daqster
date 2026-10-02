#pragma once

#include <QtTest>

/// Regression tests for the "model-name" serialization invariant.
///
/// The node graph serializer resolves a node's type from
/// `<node>["internal-data"]["model-name"]` (DataFlowGraphModel.cpp). That key
/// is written by the BASE NodeDelegateModel::save(). A model that overrides
/// save() and builds a fresh QJsonObject instead of starting from the base
/// result silently drops the key — the node still saves without complaint, but
/// on load it resolves to an empty type, is reported as "<unnamed>", is
/// skipped, and every connection touching it is dropped with it.
///
/// That is a silent data-loss defect, so it is asserted directly here for
/// every model that overrides save() without any heavy state setup.
class ModelNameSerializationTest : public QObject
{
    Q_OBJECT

private slots:
    // Each case: construct the model, call save(), and require that
    // "model-name" is present and equal to the model's name().
    void audioSource_savesModelName();
    void gpuMonitor_savesModelName();
    void jackDetect_savesModelName();
    void pcap_savesModelName();
    void numberDisplay_savesModelName();
    void arithmeticLogic_savesModelName();
    void modulo_savesModelName();

    // The dead "name" key must be gone: it duplicated the type and invited
    // the mistake that caused this defect.
    void save_doesNotWriteBareNameKey();

    // The keys the models' own load() implementations read must survive
    // alongside "model-name" — starting from the base object must not have
    // displaced them.
    void modelSpecificKeysSurvive();
};
