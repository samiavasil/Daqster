#include <QtTest>
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>

#include "DaqDisplayNode.h"
#include "test_daq_display_node.h"

using PT = DaqDisplayNode::PlotCard::ProcessingType;
using DM = DaqDisplayNode::PlotCard::DecodeMode;

// ── save/restore round-trip: v1-compatible format ───────────────────────────
//
// Create a node, add one TimeDomain card, save() → JSON → restore() to a fresh
// node, then verify the restored card has the same title, processing type, and
// channel index. v2 fields (mode, unitAxes) should be at their defaults since
// the original node was created with defaults.
void DaqDisplayNodeTest::save_restore_v1Compat()
{
    DaqDisplayNode node1;
    node1.addPlotCard(QStringLiteral("Test1"), PT::TimeDomain, 0, DM::Normalized, true);
    QCOMPARE(node1.m_cards.size(), 1);

    const QJsonObject json = node1.save();

    DaqDisplayNode node2;
    node2.restore(json);

    QCOMPARE(node2.m_cards.size(), 1);
    QCOMPARE(node2.m_cards.at(0).title, QStringLiteral("Test1"));
    QCOMPARE(node2.m_cards.at(0).processingType, PT::TimeDomain);
    QCOMPARE(node2.m_cards.at(0).channelIndex, 0);
    QCOMPARE(node2.m_cards.at(0).mode, DM::Normalized);
    QCOMPARE(node2.m_cards.at(0).unitAxes, true);
}

// ── save/restore ringSeconds (v2 field) ─────────────────────────────────────
//
// The default is 10.0 s. Changing it to 20.0 must survive a round-trip.
void DaqDisplayNodeTest::save_restore_v2_ringSeconds()
{
    DaqDisplayNode node1;
    QCOMPARE(node1.m_ringSeconds, 10.0);

    node1.m_ringSeconds = 20.0;
    const QJsonObject json = node1.save();
    QVERIFY(json.contains(QStringLiteral("ringSeconds")));
    QCOMPARE(json.value(QStringLiteral("ringSeconds")).toDouble(), 20.0);

    DaqDisplayNode node2;
    node2.restore(json);
    QCOMPARE(node2.m_ringSeconds, 20.0);
}

// ── save/restore multiple cards ─────────────────────────────────────────────
//
// Three cards (1 time + 2 FFT, different channels and modes), round-trip all
// properties.
void DaqDisplayNodeTest::save_restore_multipleCards()
{
    DaqDisplayNode node1;
    node1.addPlotCard(QStringLiteral("Time Plot"), PT::TimeDomain, 0, DM::Normalized, true);
    node1.addPlotCard(QStringLiteral("FFT Plot 1"), PT::FrequencySpectrum, 1,
                      DM::Normalized, true);
    node1.addPlotCard(QStringLiteral("FFT Plot 2"), PT::FrequencySpectrum, 2,
                      DM::Physical, false);
    QCOMPARE(node1.m_cards.size(), 3);

    const QJsonObject json = node1.save();

    DaqDisplayNode node2;
    node2.restore(json);
    QCOMPARE(node2.m_cards.size(), 3);

    // Card 0: TimeDomain, channel 0, Normalized, unitAxes=true
    QCOMPARE(node2.m_cards.at(0).title, QStringLiteral("Time Plot"));
    QCOMPARE(node2.m_cards.at(0).processingType, PT::TimeDomain);
    QCOMPARE(node2.m_cards.at(0).channelIndex, 0);
    QCOMPARE(node2.m_cards.at(0).mode, DM::Normalized);
    QCOMPARE(node2.m_cards.at(0).unitAxes, true);

    // Card 1: FrequencySpectrum, channel 1, Normalized
    QCOMPARE(node2.m_cards.at(1).title, QStringLiteral("FFT Plot 1"));
    QCOMPARE(node2.m_cards.at(1).processingType, PT::FrequencySpectrum);
    QCOMPARE(node2.m_cards.at(1).channelIndex, 1);
    QCOMPARE(node2.m_cards.at(1).mode, DM::Normalized);
    QCOMPARE(node2.m_cards.at(1).unitAxes, true);

    // Card 2: FrequencySpectrum, channel 2, Physical, unitAxes=false
    QCOMPARE(node2.m_cards.at(2).title, QStringLiteral("FFT Plot 2"));
    QCOMPARE(node2.m_cards.at(2).processingType, PT::FrequencySpectrum);
    QCOMPARE(node2.m_cards.at(2).channelIndex, 2);
    QCOMPARE(node2.m_cards.at(2).mode, DM::Physical);
    QCOMPARE(node2.m_cards.at(2).unitAxes, false);
}

// ── restore from v1-format JSON (backward compat) ───────────────────────────
//
// A v1 JSON has no ringSeconds, no per-card mode, no per-card unitAxes. After
// restore(), all v2 fields must hold their documented defaults.
void DaqDisplayNodeTest::restore_v1_fileDefaults()
{
    // Construct a v1-format JSON: one card, no v2 fields.
    QJsonObject v1Json;
    QJsonArray plots;
    QJsonObject plot;
    plot[QStringLiteral("title")] = QStringLiteral("Legacy Plot");
    plot[QStringLiteral("processing")] = QStringLiteral("time");
    plot[QStringLiteral("channel")] = 0;
    plots.append(plot);
    v1Json[QStringLiteral("plots")] = plots;

    DaqDisplayNode node;
    node.restore(v1Json);

    QCOMPARE(node.m_cards.size(), 1);
    QCOMPARE(node.m_cards.at(0).title, QStringLiteral("Legacy Plot"));
    QCOMPARE(node.m_cards.at(0).processingType, PT::TimeDomain);
    QCOMPARE(node.m_cards.at(0).channelIndex, 0);

    // v2 defaults (AC 7) — undocumented fields fall back to their defaults.
    QCOMPARE(node.m_cards.at(0).mode, DM::Normalized);
    QCOMPARE(node.m_cards.at(0).unitAxes, true);
    QCOMPARE(node.m_ringSeconds, 10.0);
}

// ── save/restore physical decode mode ───────────────────────────────────────
//
// A card with DecodeMode::Physical must round-trip the mode string "physical".
void DaqDisplayNodeTest::save_restore_mode_physical()
{
    DaqDisplayNode node1;
    node1.addPlotCard(QStringLiteral("Physical Plot"), PT::TimeDomain, 0,
                      DM::Physical, true);

    const QJsonObject json = node1.save();
    const QJsonArray plots = json.value(QStringLiteral("plots")).toArray();
    QCOMPARE(plots.size(), 1);
    QCOMPARE(plots.at(0).toObject().value(QStringLiteral("mode")).toString(),
             QStringLiteral("physical"));

    DaqDisplayNode node2;
    node2.restore(json);

    QCOMPARE(node2.m_cards.size(), 1);
    QCOMPARE(node2.m_cards.at(0).title, QStringLiteral("Physical Plot"));
    QCOMPARE(node2.m_cards.at(0).mode, DM::Physical);
}

QTEST_MAIN(DaqDisplayNodeTest)
