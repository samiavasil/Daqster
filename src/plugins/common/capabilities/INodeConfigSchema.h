#pragma once

#include <QJsonObject>

namespace Daqster {

/**
 * @brief Optional self-description of a node model's configuration.
 *
 * REQ-SW-PL-063 AC10. `NodeDelegateModel::save()` gives the control plane the
 * *shape* of a node's config (field names + JSON types), but not the machine
 * readable metadata a remote UI needs: which values are valid (`enum`),
 * allowed ranges (`minimum`/`maximum`), defaults, or which fields only apply
 * for a given mode (`visibleWhen`).
 *
 * A node model implements this interface to provide a richer JSON Schema
 * Draft-7 fragment for exactly the object its `save()` returns. When the model
 * does not implement it, the control plane falls back to the best-effort
 * schema derived from `save()` — so implementation is incremental and optional.
 *
 * The interface is intentionally plain (non-QObject) and lives in the
 * widget-free core, so it works in headless mode too. Discovery is a
 * `dynamic_cast` on the model instance, mirroring IStartable/IStoppable.
 */
class INodeConfigSchema
{
public:
    virtual ~INodeConfigSchema() = default;

    /// Draft-7 fragment describing this node's `save()` output. The control
    /// plane adds `$schema` and `title` if they are missing.
    virtual QJsonObject configSchema() const = 0;
};

} // namespace Daqster
