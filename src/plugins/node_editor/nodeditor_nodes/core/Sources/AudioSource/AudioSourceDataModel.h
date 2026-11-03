#ifndef AUDIOSOURCEDATAMODEL_H
#define AUDIOSOURCEDATAMODEL_H

#include "AudioCompat.h"
#include "AudioStartStop.h"
#include "MicCaptureWorker.h"
#include "NodeDataTypes/SampledData.h"
#include "shared/IStoppable.h"
#include "shared/IStartable.h"

#include <Threading/ThreadOwner.h>

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/internal/Definitions.hpp>

#include <functional>
#include <memory>

class AudioSourceDataModel : public QtNodes::NodeDelegateModel, public Daqster::IStoppable, public Daqster::IStartable
{
    Q_OBJECT

public:
    AudioSourceDataModel();

    virtual
    ~AudioSourceDataModel() override;

public:

    QString
    caption() const override
    { return QStringLiteral("AudioSource Source"); }

    bool
    captionVisible() const override
    { return false; }

    QString
    name() const override
    { return QStringLiteral("AudioSource"); }

public:

    QJsonObject
    save() const override;

    /*void
restore(QJsonObject const &p) override;
*/

public:

    unsigned int
    nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType
    dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData>
    outData(QtNodes::PortIndex const port) override;

    void
    setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex const port) override;

    /// Core model has no QtWidgets dependency — the config UI is created by the
    /// GUI plugin and wired through NodeWidgetFactory (REQ-SW-PL-051). The
    /// accessors below hand the UI non-owning pointers to the live
    /// device/format state, exactly as the model used to pass them itself.
    QWidget *
    embeddedWidget() override { return nullptr; }

    QAudioDeviceInfo* deviceInfo() { return &m_DevInfo; }
    QAudioFormat* audioFormat() { return &m_FormatAudio; }

    /// Stop the capture thread cleanly. Idempotent — safe to call multiple
    /// times (REQ-SW-PL-050).
    ///
    /// After it returns the worker is gone and nothing may reach it again
    /// (REQ-SW-PL-078): the scene close stops the nodes first and only then
    /// deletes the connections, so outputConnectionDeleted() → setCaptureEnabled()
    /// runs *after* this and must not touch a freed worker.
    void stop() override;

    /// Start capture programmatically (runtime autoStart, REQ-SW-PL-048).
    void start() override;

    QtNodes::ConnectionPolicy portConnectionPolicy(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override
    {
        Q_UNUSED(portType);
        Q_UNUSED(portIndex);
        return QtNodes::ConnectionPolicy::One;
    }

    void outputConnectionCreated(QtNodes::ConnectionId const &) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &) override;

public slots:
    /// Start/Stop command from AudioSourceDataModelUI (called by
    /// NodeWidgetFactory).
    void onUiStart(AudioStartStop start);

    /// Device/format selection made in the UI. The UI holds non-owning
    /// pointers to m_DevInfo / m_FormatAudio, so the state is stored here and
    /// then forwarded to the capture worker.
    void onAudioConnectionChanged(QAudioDeviceInfo devInfo,
                                  QAudioFormat formatAudio);

private slots:
    void onSamplesReady(std::shared_ptr<SampledData> data);

private:
    void setCaptureEnabled(bool enabled);

    /// Queue a call on the capture thread through the owner.
    ///
    /// The owner raises its latch before it joins the thread, and `invoke()`
    /// refuses work after that; the QPointer is captured **by value** so a
    /// deferred call never dereferences a worker that stop() already dropped
    /// (REQ-SW-PL-078 — the graph deletes connections after a node's stop()).
    ///
    /// @param call What to run on the worker thread.
    /// @return false when the capture worker is stopping/stopped (the normal
    ///         "the scene is closed" answer, not an error).
    bool onCaptureThread(std::function<void(MicCaptureWorker *)> call);

    /// Owns the capture thread and its worker (REQ-SW-PL-050). `alive()` is the
    /// single predicate every worker-bound call checks; stop() is idempotent,
    /// latches before quit() and joins with a deadline.
    Daqster::ThreadOwner<MicCaptureWorker> m_capture;
    QAudioDeviceInfo m_DevInfo;
    QAudioFormat m_FormatAudio;
    std::shared_ptr<SampledData> m_lastData;
    int m_connectionCount = 0;
};

#endif // AUDIOSOURCEDATAMODEL_H
