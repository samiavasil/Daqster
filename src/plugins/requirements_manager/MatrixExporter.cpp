#include "MatrixExporter.h"

#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QTextStream>

namespace Daqster {

namespace {

// "—" is used for empty matrix cells, matching traceability-matrix.md.
QString cell(const QString &value)
{
    return value.trimmed().isEmpty() ? QStringLiteral("—") : value;
}

// Backtick-wrapped cell for the Markdown table (IDs / commit hashes / code
// locations are code-flavored in traceability-matrix.md).
QString backticked(const QString &value)
{
    return value.trimmed().isEmpty() ? QStringLiteral("—")
                                     : QStringLiteral("`%1`").arg(value);
}

// RFC 4180: a field is quoted when it contains a comma, quote, CR or LF;
// embedded quotes are doubled.
QString csvEscape(const QString &field)
{
    const bool needsQuotes = field.contains(QLatin1Char(','))
                             || field.contains(QLatin1Char('"'))
                             || field.contains(QLatin1Char('\n'))
                             || field.contains(QLatin1Char('\r'));
    if (!needsQuotes)
        return field;

    QString escaped = field;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

int resolveIndex(const QVector<Requirement> &requirements, const QString &id)
{
    for (int i = 0; i < requirements.size(); ++i) {
        if (QString::compare(requirements.at(i).id, id, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

// Counts requirements that participate in at least one dependency cycle: a
// requirement that can reach itself again by following "Зависи от:" edges.
int cyclicRequirementCount(const QVector<Requirement> &requirements)
{
    const int n = requirements.size();
    QVector<QVector<int>> outgoing(n);
    for (int i = 0; i < n; ++i) {
        for (const QString &dep : requirements.at(i).dependencies) {
            const int j = resolveIndex(requirements, dep);
            if (j >= 0)
                outgoing[i].append(j);
        }
    }

    int cyclic = 0;
    for (int start = 0; start < n; ++start) {
        QVector<bool> seen(n, false);
        seen[start] = true;
        QVector<int> stack = outgoing.at(start);
        bool found = false;
        while (!stack.isEmpty()) {
            const int u = stack.takeLast();
            if (u == start) { // reached the start again -> cycle member
                found = true;
                break;
            }
            if (seen.at(u))
                continue;
            seen[u] = true;
            for (const int v : outgoing.at(u)) {
                if (v == start) {
                    found = true;
                    break;
                }
                if (!seen.at(v))
                    stack.append(v);
            }
            if (found)
                break;
        }
        if (found)
            ++cyclic;
    }
    return cyclic;
}

} // namespace

bool MatrixExporter::exportMarkdown(const QVector<Requirement> &requirements,
                                    QIODevice &device)
{
    QString content;
    QTextStream out(&content);

    // Non-ASCII literals MUST be QStringLiteral: QTextStream's
    // operator<<(const char*) uses fromLocal8Bit (Latin-1 in a C locale),
    // which would mangle the Cyrillic/em-dash text before toUtf8().
    out << QStringLiteral("# Traceability Matrix — Requirements Manager & Framework Tools\n")
        << QStringLiteral("\n")
        << QStringLiteral("Матрица за проследимост на изискванията за **Requirements Manager** инструмент (`REQ-SW-<TYPE>-*`).\n")
        << QStringLiteral("\n")
        << QStringLiteral("## Изисквания (`REQ-SW-<TYPE>-*`)\n")
        << QStringLiteral("\n")
        << QStringLiteral("| REQ ID | Repo | Заглавие | Статус | Родител | Зависи от | Коммит(и) | Код | Тестове |\n")
        << QStringLiteral("|--------|------|----------|--------|---------|-----------|-----------|-----|---------|\n");

    for (const Requirement &req : requirements) {
        out << "| `" << req.id << "` | " << cell(req.repo) << " | " << req.title << " | " << req.status
            << " | " << cell(req.parentId) << " | "
            << cell(req.dependencies.join(QStringLiteral(", "))) << " | "
            << backticked(req.commits) << " | " << backticked(req.code)
            << " | " << cell(req.tests) << " |\n";
    }
    return device.write(content.toUtf8()) >= 0;
}

bool MatrixExporter::exportCsv(const QVector<Requirement> &requirements,
                               QIODevice &device)
{
    QString content;
    QTextStream out(&content);

    const QStringList headers = {
        QStringLiteral("ID"),          QStringLiteral("Repo"),
        QStringLiteral("Title"),       QStringLiteral("Status"),
        QStringLiteral("Priority"),    QStringLiteral("Parent"),
        QStringLiteral("Dependencies"), QStringLiteral("Commits"),
        QStringLiteral("Code"),        QStringLiteral("Tests"),
        QStringLiteral("Section")
    };

    QStringList escapedHeaders;
    for (const QString &header : headers)
        escapedHeaders.append(csvEscape(header));
    out << escapedHeaders.join(QLatin1Char(',')) << "\r\n";

    for (const Requirement &req : requirements) {
        const QStringList fields = {
            req.id,
            req.repo,
            req.title,
            req.status,
            req.priority,
            req.parentId,
            req.dependencies.join(QStringLiteral(", ")),
            req.commits,
            req.code,
            req.tests,
            req.section
        };
        QStringList escapedFields;
        for (const QString &field : fields)
            escapedFields.append(csvEscape(field));
        out << escapedFields.join(QLatin1Char(',')) << "\r\n";
    }
    return device.write(content.toUtf8()) >= 0;
}

bool MatrixExporter::exportJson(const QVector<Requirement> &requirements,
                                QIODevice &device)
{
    QJsonArray array;
    for (const Requirement &req : requirements) {
        QJsonArray dependencies;
        for (const QString &dep : req.dependencies)
            dependencies.append(dep);

        QJsonArray criteria;
        for (const QString &criterion : req.acceptanceCriteria)
            criteria.append(criterion);

        QJsonArray criteriaDone;
        for (const bool done : req.criteriaDone)
            criteriaDone.append(done);

        QJsonObject object;
        object.insert(QStringLiteral("id"), req.id);
        object.insert(QStringLiteral("repo"), req.repo);
        object.insert(QStringLiteral("title"), req.title);
        object.insert(QStringLiteral("status"), req.status);
        object.insert(QStringLiteral("priority"), req.priority);
        object.insert(QStringLiteral("assignee"), req.assignee);
        object.insert(QStringLiteral("date"), req.date);
        object.insert(QStringLiteral("parentId"), req.parentId);
        object.insert(QStringLiteral("dependencies"), dependencies);
        object.insert(QStringLiteral("description"), req.description);
        object.insert(QStringLiteral("traceability"), req.traceability);
        object.insert(QStringLiteral("commits"), req.commits);
        object.insert(QStringLiteral("code"), req.code);
        object.insert(QStringLiteral("tests"), req.tests);
        object.insert(QStringLiteral("section"), req.section);
        object.insert(QStringLiteral("fileName"), req.fileName);
        object.insert(QStringLiteral("acceptanceCriteria"), criteria);
        object.insert(QStringLiteral("criteriaDone"), criteriaDone);
        array.append(object);
    }

    const QJsonDocument document(array);
    return device.write(document.toJson(QJsonDocument::Indented)) >= 0;
}

QString MatrixExporter::buildSummary(const QVector<Requirement> &requirements)
{
    int totalAc = 0;
    int checkedAc = 0;
    int withDependencies = 0;
    QMap<QString, int> statusCounts;

    for (const Requirement &req : requirements) {
        totalAc += req.acceptanceCriteria.size();
        const int doneCount =
            qMin(req.criteriaDone.size(), req.acceptanceCriteria.size());
        for (int i = 0; i < doneCount; ++i) {
            if (req.criteriaDone.at(i))
                ++checkedAc;
        }

        QStringList meaningful;
        for (const QString &dep : req.dependencies) {
            if (!dep.trimmed().isEmpty() && dep != QStringLiteral("—"))
                meaningful.append(dep);
        }
        if (!meaningful.isEmpty())
            ++withDependencies;

        const QString status = req.status.trimmed().isEmpty()
            ? QStringLiteral("UNKNOWN")
            : req.status;
        statusCounts[status] += 1;
    }

    // Dangling references: parentId / dependencies that resolve to no existing
    // requirement (case-insensitive, mirroring the graph tooling).
    int dangling = 0;
    for (const Requirement &req : requirements) {
        if (!req.parentId.trimmed().isEmpty()
            && req.parentId != QStringLiteral("—")
            && resolveIndex(requirements, req.parentId) < 0) {
            ++dangling;
        }
        for (const QString &dep : req.dependencies) {
            if (!dep.trimmed().isEmpty() && dep != QStringLiteral("—")
                && resolveIndex(requirements, dep) < 0) {
                ++dangling;
            }
        }
    }

    QString completion;
    if (totalAc > 0) {
        const double percent = 100.0 * static_cast<double>(checkedAc)
                               / static_cast<double>(totalAc);
        completion = QStringLiteral("%1/%2 acceptance criteria (%3%)")
                         .arg(checkedAc)
                         .arg(totalAc)
                         .arg(QString::number(percent, 'f', 1));
    } else {
        completion = QStringLiteral("0/0 acceptance criteria (n/a)");
    }

    QStringList statusParts;
    for (auto it = statusCounts.constBegin(); it != statusCounts.constEnd(); ++it)
        statusParts.append(QStringLiteral("%1 %2").arg(it.key()).arg(it.value()));

    return QStringLiteral(
               "Total requirements: %1\n"
               "Status: %2\n"
               "Completion: %3\n"
               "With dependencies: %4\n"
               "Dangling references: %5\n"
               "Cycles: %6")
        .arg(requirements.size())
        .arg(statusParts.join(QStringLiteral(" | ")))
        .arg(completion)
        .arg(withDependencies)
        .arg(dangling)
        .arg(cyclicRequirementCount(requirements));
}

} // namespace Daqster
