#pragma once

#include <QtWidgets/QWidget>

#include <QStringList>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class QToolBox;

class AudioOutputNode;

/**
 * @brief GUI widget of the Audio Output node (REQ-SW-PL-076, core/gui split of
 * REQ-SW-PL-053).
 *
 * Shows the four persisted fields of `AudioOutputNode::Config` — device,
 * volume, buffer size and enabled — plus the engine's status line. The device
 * is chosen by the **stable** `AudioCompat::deviceId()`, never by an index: the
 * enumeration order shifts between sessions (a USB headset plugged in), so an
 * index would silently point at another device after a restart.
 *
 * The widget is a view, not a second copy of the state: every change goes
 * through `AudioOutputNode::setConfig()`, and `reloadFromModel()` re-reads the
 * model (a `load()` from a `.flow` file or a REST `POST /api/nodes/{id}/config`
 * reaches the fields through `configChanged`). Controls are blocked while
 * being filled — a value that came from the model is not a user edit and must
 * not be written back, which would loop through `applyToModel()`.
 */
class AudioOutputWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @param model The node model this widget edits (not owned).
     * @param parent Parent widget (ownership of the memory).
     */
    explicit AudioOutputWidget(AudioOutputNode* model, QWidget* parent = nullptr);
    ~AudioOutputWidget() override = default;

    /// Rebuild the device list and highlight @p selectedIndex.
    ///
    /// @param descriptions Device names, first entry being the "System default"
    ///                     pseudo-entry
    /// @param ids          Matching `AudioCompat::deviceId()` values, stored as
    ///                     the combo's item data ("" for the default entry)
    /// @param selectedIndex Index to highlight; ignored when out of range
    void setDevices(const QStringList& descriptions, const QStringList& ids,
                    int selectedIndex);

public slots:
    /// Model → widget: re-read every field from `AudioOutputNode::config()`.
    void reloadFromModel();

    /// Model → widget: status line text (from `AudioOutputNode::statusText()`).
    void setStatus(const QString& status);

private:
    void buildUi();

    /// Widget → model: one `Config`, started from the model's current one so a
    /// field this widget does not show is never reset to a default.
    void applyToModel();

    /// Volume / buffer / device are meaningless while disabled — grey them out
    /// instead of letting a user tune a sink that will not be opened.
    void updateControlStates();

    AudioOutputNode* m_model = nullptr;

    QToolBox* m_sections = nullptr;
    QComboBox* m_deviceCombo = nullptr;
    QDoubleSpinBox* m_volumeBox = nullptr;
    QSpinBox* m_bufferBox = nullptr;
    QCheckBox* m_enabledBox = nullptr;
    QLabel* m_statusLabel = nullptr;
};
