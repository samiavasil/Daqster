#ifndef SAMPLEDECIMATORNODE_H
#define SAMPLEDECIMATORNODE_H

// SPDX-License-Identifier: MIT

#include "nodeditor_nodes_core_export.h"

#include "Dsp/FirDecimator.h"
#include "Processing/Audio/AudioDspSupport.h"
#include "capabilities/INodeConfigSchema.h"

#include <QtNodes/NodeDelegateModel>

#include <QJsonObject>
#include <QString>
#include <QVector>

#include <memory>

/**
 * @brief Integer rate reduction `fs → fs/M` with an anti-alias kernel
 *        (REQ-SW-PL-092).
 *
 * Decimation is not "drop M−1 of every M samples": everything above the output
 * Nyquist folds back permanently, so `FirDecimator` filters first and keeps
 * every M-th **filtered** sample. `bypassFilter` exists for tests and
 * deliberately skips the FIR — the aliasing it lets through is the thing the
 * default path prevents.
 *
 * Coefficients, in order: `filterRef` (`<DEPS>/filters/<ref>.json`), then
 * `filterPath` (scene-relative / `${VAR}` allowed), then the built-in
 * anti-alias kernel for M. `coefficientStatus()` says which one ran and the
 * first 8 hex digits of its sha256, so two runs can be compared.
 *
 * **Timebase.** The published `firstIndex` is the block's position in the
 * **output** stream: the input stream's start re-anchored at the new rate
 * (`firstIndex_in / M`) plus the number of output samples already produced.
 * That is what `SampledData::isContiguousWith()` compares, so consecutive
 * output blocks are continuous even though they are M times shorter than the
 * input. The input-side mapping (`output k ↔ input k·M`) is the contract's
 * `inputIndexForOutput()` and is **not** folded into `firstIndex` — one index
 * domain per block, otherwise continuity would be measured in units that do not
 * match the block.
 *
 * Mono per channel: one engine per input channel, identical kernels. Each block
 * is published as normalized FLOAT32 (see `AudioDspSupport`).
 */
class NODEDITOR_NODES_CORE_EXPORT SampleDecimatorNode : public QtNodes::NodeDelegateModel,
                                                        public Daqster::INodeConfigSchema
{
    Q_OBJECT

public:
    SampleDecimatorNode();
    ~SampleDecimatorNode() override;

    QString caption() const override { return QStringLiteral("Sample decimator"); }
    bool captionVisible() const override { return true; }
    QString name() const override { return QStringLiteral("SampleDecimatorNode"); }

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
    int factor() const { return m_factor; }
    QString filterRef() const { return m_filterRef; }
    QString filterPath() const { return m_filterPath; }
    bool bypassFilter() const { return m_bypassFilter; }

    /// Where the kernel came from plus its sha256 prefix — the status line.
    QString coefficientStatus() const { return m_coefficientStatus; }

    /// Output rate implied by the last resolved input rate (0 = unknown yet).
    double outputSampleRate() const { return m_outputRate; }

    /// Input sample index that output sample @p outputIndex maps to, or
    /// `kUnknownIndex` for a negative index.
    ///
    /// `outputIndex · M` — exposed because the mapping is part of the contract
    /// (and the published `firstIndex` lives in the *output* domain, so the two
    /// are deliberately different units).
    qint64 inputIndexForOutput(qint64 outputIndex) const;

public slots:
    void setFactor(int factor);
    void setFilterRef(const QString &filterRef);
    void setFilterPath(const QString &filterPath);
    void setBypassFilter(bool bypass);

signals:
    void configChanged();
    void statusChanged(const QString &status);

private:
    /// Resolve coefficients and configure one engine per channel.
    bool ensureConfigured(int channels);
    /// Apply M to one block (filtered, or bypassed for tests).
    QVector<QVector<float>> decimate(const QVector<QVector<float>> &channels);
    void setError(const QString &message);
    void invalidate();

    int m_factor = 3;
    QString m_filterRef;
    QString m_filterPath;
    bool m_bypassFilter = false;

    bool m_configured = false;
    QVector<FirDecimator> m_engines;
    QString m_coefficientStatus;

    AudioDspSupport::StreamFingerprint m_fingerprint;
    double m_outputRate = 0.0;
    qint64 m_origin = SampledData::kUnknownIndex;
    qint64 m_outBase = SampledData::kUnknownIndex;  ///< output index of the stream's start
    qint64 m_outEmitted = 0;   ///< global output samples produced
    qint64 m_inSeen = 0;       ///< global input samples consumed (bypass path)

    std::shared_ptr<SampledData> m_output;
    QtNodes::NodeValidationState m_validation;
};

#endif // SAMPLEDECIMATORNODE_H
