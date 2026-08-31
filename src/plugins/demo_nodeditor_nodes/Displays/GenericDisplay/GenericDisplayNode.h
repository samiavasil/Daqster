#ifndef GENERICDISPLAYNODE_H
#define GENERICDISPLAYNODE_H

#include "DaqDisplayNode.h"

/**
 * @brief Legacy alias of the DAQ Display (REQ-SW-PL-022).
 *
 * Old saved graphs reference the "GenericDisplay" model name. Keeping this
 * thin subclass preserves saved-graph loading (AC 8) while inheriting the
 * real DataPlot rendering of DaqDisplayNode — the old updateTimeChart() /
 * updateFFTChart() stubs are replaced by the inherited slot pipeline (AC 6).
 */
class GenericDisplayNode : public DaqDisplayNode
{
    Q_OBJECT

public:
    GenericDisplayNode() = default;

    QString caption() const override
    { return QStringLiteral("Generic Display"); }

    QString name() const override
    { return QStringLiteral("GenericDisplay"); }

    /// Inherits the DaqDisplayNode opt-out: the node BODY does not depend on
    /// data — widget content self-repaints via Qt.
    bool dataArrivalChangesWidget() const override { return false; }
};

#endif // GENERICDISPLAYNODE_H