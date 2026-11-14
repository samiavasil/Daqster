#ifndef SAMPLERESAMPLERNODE_H
#define SAMPLERESAMPLERNODE_H

// SPDX-License-Identifier: MIT

#include "nodeditor_nodes_core_export.h"

#include "Dsp/PolyphaseResampler.h"
#include "Processing/Audio/AudioDspSupport.h"
#include "capabilities/INodeConfigSchema.h"

#include <QtNodes/NodeDelegateModel>

#include <QJsonObject>
#include <QString>
#include <QVector>

#include <memory>

/**
 * @brief Rational rate conversion `fs → fs·up/down` (REQ-SW-PL-092).
 *
 * 44.1 kHz → 48 kHz is `up = 160, down = 147`; the actual conversion is a
 * polyphase FIR, so there is no interpolate-then-decimate waste and the output
 * is bit-identical whether the input arrives in one block or many
 * (`PolyphaseResampler` carries integer state only).
 *
 * Coefficients: `filterRef` (`<DEPS>/filters/<ref>.json`) → `filterPath`
 * (scene-relative / `${VAR}`) → the built-in anti-alias kernel for `up/down`.
 * `coefficientStatus()` records which one ran and its sha256 prefix.
 *
 * **Timebase.** The published `firstIndex` is the block's position in the
 * **output** stream (the stream re-anchored at the new rate plus the output
 * samples already produced), which is the unit `isContiguousWith()` compares —
 * so consecutive output blocks are continuous. The input-side mapping
 * `inputIndexForOutput(k) = ⌊k·down/up⌋` is exactly the position the polyphase
 * engine evaluates (`q = ⌊i·M/L⌋`), is exposed for consumers that need the
 * original timeline, and is asserted in the tests.
 *
 * One engine per input channel; output blocks are normalized FLOAT32.
 */
class NODEDITOR_NODES_CORE_EXPORT SampleResamplerNode : public QtNodes::NodeDelegateModel,
                                                        public Daqster::INodeConfigSchema
{
    Q_OBJECT

public:
    SampleResamplerNode();
    ~SampleResamplerNode() override;

    QString caption() const override { return QStringLiteral("Sample resampler"); }
    bool captionVisible() const override { return true; }
    QString name() const override { return QStringLiteral("SampleResamplerNode"); }

    bool dataArrivalChangesGeometry() const override { return false; }
    bool dataArrivalChangesWidget() const override { return false; }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;
    QJsonObject configSchema() const override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex portIndex) override;

    QWidget *embeddedWidget() override { return nullptr; }

    QtNodes::NodeValidationState validationState() const override;

    // ── Config ─────────────────────────────────────────────────────────────
    int up() const { return m_up; }
    int down() const { return m_down; }
    QString filterRef() const { return m_filterRef; }
    QString filterPath() const { return m_filterPath; }

    QString coefficientStatus() const { return m_coefficientStatus; }
    double outputSampleRate() const { return m_outputRate; }

    /// Input sample index that output sample @p outputIndex maps to.
    ///
    /// `⌊outputIndex · down / up⌋` — exposed because the mapping is part of the
    /// contract and a test asserts it directly instead of inferring it.
    qint64 inputIndexForOutput(qint64 outputIndex) const;

public slots:
    void setUp(int up);
    void setDown(int down);
    void setFilterRef(const QString &filterRef);
    void setFilterPath(const QString &filterPath);

signals:
    void configChanged();
    void statusChanged(const QString &status);

private:
    bool ensureConfigured(int channels);
    void setError(const QString &message);
    void invalidate();
    void restartMapping();

    int m_up = 160;
    int m_down = 147;
    QString m_filterRef;
    QString m_filterPath;

    bool m_configured = false;
    QVector<PolyphaseResampler> m_engines;
    QString m_coefficientStatus;

    AudioDspSupport::StreamFingerprint m_fingerprint;
    double m_outputRate = 0.0;
    qint64 m_origin = SampledData::kUnknownIndex;
    qint64 m_outBase = SampledData::kUnknownIndex;  ///< output index of the stream's start
    qint64 m_outEmitted = 0;

    std::shared_ptr<SampledData> m_output;
    QtNodes::NodeValidationState m_validation;
};

#endif // SAMPLERESAMPLERNODE_H
