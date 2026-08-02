#pragma once

#include <QString>
#include <QVector>
#include "RequirementsParser.h"

class QIODevice;

namespace Daqster {

/**
 * @brief Exports the Traceability Matrix in Markdown / CSV / JSON (REQ-SW-010).
 *
 * Every exporter is a pure static function that writes to a caller-provided
 * QIODevice (testable via QBuffer, used by the widget with a real QFile).
 *
 *   - exportMarkdown: table matching the repo's traceability-matrix.md style.
 *   - exportCsv:      RFC 4180 — fields containing a comma, quote or newline
 *                     are quoted, embedded quotes are doubled.
 *   - exportJson:     QJsonArray of objects with all structured fields,
 *                     including acceptance criteria and their done flags.
 *   - buildSummary:   human-readable metrics report (AC4): counts by status,
 *                     completion % (checked / total acceptance criteria),
 *                     count with dependencies, dangling references, cycles.
 */
class MatrixExporter
{
public:
    //!< Signature shared by the three exporters (used by the widget's
    //!< export buttons to call the right one via one helper).
    using ExportFunction = bool (*)(const QVector<Requirement> &requirements,
                                    QIODevice &device);

    static bool exportMarkdown(const QVector<Requirement> &requirements, QIODevice &device);
    static bool exportCsv(const QVector<Requirement> &requirements, QIODevice &device);
    static bool exportJson(const QVector<Requirement> &requirements, QIODevice &device);

    /**
     * @brief Builds the completion/status/dependency metrics report.
     *
     * "Cycles" counts requirements that participate in at least one dependency
     * cycle (a requirement that can reach itself through "Зависи от:" edges).
     * "Dangling references" counts unresolved parentId/dependency references
     * (case-insensitive, mirroring the rest of the tooling).
     */
    static QString buildSummary(const QVector<Requirement> &requirements);
};

} // namespace Daqster
