#ifndef AUDIOSTARTSTOP_H
#define AUDIOSTARTSTOP_H

/**
 * @brief Shared Start/Stop command enum for the audio source nodes.
 *
 * REQ-SW-PL-051 (core/gui split): this used to be nested in
 * AudioSourceDataModelUI, which now lives in the GUI plugin. The core models
 * (AudioSourceDataModel, AudioSourceDataModelObsolete) and the obsolete worker
 * still need the type, so it moved to a dependency-free core header that both
 * sides include. Keeping one definition preserves the single contract both
 * nodes wire to.
 */
enum AudioStartStop {
    ASDM_STOP,
    ASDM_START,
    ASDM_RELOAD,
};

#endif // AUDIOSTARTSTOP_H
