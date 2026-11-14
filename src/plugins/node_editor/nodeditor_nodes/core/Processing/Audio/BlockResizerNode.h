#ifndef BLOCKRESIZERNODE_H
#define BLOCKRESIZERNODE_H

// SPDX-License-Identifier: MIT

#include "nodeditor_nodes_core_export.h"

#include "Processing/Audio/AudioDspSupport.h"
#include "capabilities/INodeConfigSchema.h"

#include <QtNodes/NodeDelegateModel>

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <memory>

/**
 * @brief Re-package a sampled stream into fixed-size blocks (REQ-SW-PL-092).
 *
 * This node changes **how many samples arrive together**, never the signal: the
 * bytes are copied verbatim, so no sample type, no precision and no rate is
 * touched. It is the "display / FFT wants 1024 samples, the source delivers
 * whatever the device chose" operation from ADR-AI-016 §4.
 *
 * Two ways to say the size:
 *   - `blockSamples` — frames per output block (0 = passthrough);
 *   - `blockMs`      — milliseconds per block, converted to frames **once**,
 *                      when the input rate is first known. The hot path never
 *                      sees milliseconds (ADR-AI-016 §2);
 *   - both set → `blockSamples` wins and a warning says so, because "I set two
 *     sizes" has one honest answer and it must not be picked silently.
 *
 * `firstIndex` is preserved: the carry remembers where it starts in the stream,
 * so the output blocks are contiguous even though the input blocks are not the
 * same size. A gap in the input timebase discards the carry — a block that
 * glues samples from both sides of a hole would be a lie.
 *
 * 1 in / 1 out, no widget (`embeddedWidget()` is nullptr), live parameters
 * (`x-restart: false`): a size change takes effect on the next block.
 */
class NODEDITOR_NODES_CORE_EXPORT BlockResizerNode : public QtNodes::NodeDelegateModel,
                                                     public Daqster::INodeConfigSchema
{
    Q_OBJECT

public:
    BlockResizerNode();
    ~BlockResizerNode() override;

    QString caption() const override { return QStringLiteral("Block resizer"); }
    bool captionVisible() const override { return true; }
    QString name() const override { return QStringLiteral("BlockResizerNode"); }

    /// The port shape is constant and the body shows no payload — both opt-outs
    /// keep a 100 blocks/s stream from repainting the scene.
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

    // ── Config (read by the GUI widget / tests) ────────────────────────────
    int blockSamples() const { return m_blockSamples; }
    double blockMs() const { return m_blockMs; }

public slots:
    /// Set the output block size in frames; ≤0 means passthrough.
    void setBlockSamples(int blockSamples);
    /// Set the output block size in milliseconds; ≤0 means "not configured".
    void setBlockMs(double blockMs);

signals:
    /// The effective size or the last error changed (widget / status line).
    void configChanged(int blockSamples, double blockMs);
    /// A one-line reason when the node cannot process the stream.
    void statusChanged(const QString &status);

private:
    /// Resolve the effective block size on the first block; false on error.
    bool resolveTarget(const SampledStreamDescriptor &descriptor);
    /// Append the block and emit every complete output block.
    void appendAndEmit(const SampledData &input);
    int pendingFrames() const;
    void setError(const QString &message);
    void resetState();

    int m_blockSamples = 0;
    double m_blockMs = 0.0;

    /// Frames per emitted block; 0 = passthrough (nothing to re-package).
    int m_targetFrames = 0;
    bool m_targetResolved = false;
    bool m_warnedBothSizes = false;

    QByteArray m_pending;        ///< carry: frames not yet emitted
    qint64 m_pendingFirstIndex = -1;
    SampledStreamDescriptor m_descriptor;
    int m_frameBytes = 0;
    AudioDspSupport::StreamFingerprint m_fingerprint;

    std::shared_ptr<SampledData> m_output;
    QtNodes::NodeValidationState m_validation;
};

#endif // BLOCKRESIZERNODE_H
