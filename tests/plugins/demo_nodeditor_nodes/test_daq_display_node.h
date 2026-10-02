#pragma once

#include <QtTest>

/// GUI tests for DaqDisplayNode save()/restore() round-trip (REQ-SW-PL-023 §6,
/// REQ-SW-PL-025 §4). QTEST_MAIN in test_daq_display_node.cpp provides a
/// QApplication main; the binary runs headless via the offscreen platform
/// plugin (QT_QPA_PLATFORM=offscreen set by the CTest ENVIRONMENT property).
class DaqDisplayNodeTest : public QObject
{
    Q_OBJECT

private slots:
    void save_restore_v1Compat();
    void save_restore_v2_ringSeconds();
    void save_restore_multipleCards();
    void restore_v1_fileDefaults();
    void save_restore_mode_physical();

    // Worker-owned rolling ring buffer (REQ-SW-PL-025 AC 3/AC 4), exercised
    // through the friend-declared static appendRingBlock() + ComputeState.
    void ringBuffer_rollingWindowDropsOldest();
    void ringBuffer_descriptorChangeReset();

    // Display-world consolidation: GenericDisplayNode is a thin DaqDisplayNode
    // subclass (name/caption only) with behavioral parity (save/restore), and
    // the "AudioDisplay" registry key resolves to a DaqDisplayNode-derived
    // alias (not the QDevIO obsolete node).
    void genericDisplay_aliasBehavior();
    void registry_aliasResolution();

    // The widget rebuilds its whole chart tree on plotCardsChanged. That signal
    // used to fire on EVERY incoming sample (refresh() ran from setInData()),
    // so an audio stream tore down and rebuilt the QtCharts tree per buffer and
    // pinned the GUI thread at ~99% — the app froze and had to be killed.
    void plotCardsChanged_notFiredPerSample();
    void plotCardsChanged_firedWhenDescriptorChanges();

    // DaqDisplayWidget::rebuildCards() drained its card list with
    //   while (!m_cards.isEmpty()) destroyCard(m_cards.last());
    // but destroyCard() only zeroes the entry — it never removed it, and
    // isEmpty() tests the size. The loop therefore never terminated and the
    // GUI thread spun at 100% forever on the first rebuild of a non-empty
    // model. A rebuild of a populated model must return and keep the card
    // count stable across repeated rebuilds.
    void widget_rebuildCards_terminatesAndIsStable();
};
