#include "RequirementsValidator.h"

#include <QHash>
#include <QSet>

#include <functional>

namespace Daqster {

namespace {

// DFS helper: reports back-edge cycles in a directed graph.
// adjacency[u] returns the list of target requirement indexes for node u.
// For every back edge u -> v (v currently on the DFS stack) an elementary
// cycle chain [v, ..., u, v] is appended to issues once (deduplicated).
void collectCycles(const QVector<Requirement> &requirements,
                   const QHash<QString, int> &idToIndex,
                   const QVector<QVector<int>> &adjacency,
                   const QString &field,
                   QVector<RequirementsValidator::Issue> &issues)
{
    const int n = requirements.size();
    QVector<int> state(n, 0); // 0 = unvisited, 1 = on stack, 2 = done
    QVector<int> stack;
    QSet<QString> reportedChains;

    std::function<void(int)> dfs = [&](int u) {
        state[u] = 1;
        stack.append(u);
        for (const int v : adjacency.at(u)) {
            if (state[v] == 1) {
                QStringList chain;
                const int start = stack.indexOf(v);
                for (int k = start; k < stack.size(); ++k)
                    chain.append(requirements.at(stack.at(k)).id);
                chain.append(requirements.at(v).id);

                const QString chainKey = chain.join(QLatin1Char('|'));
                if (reportedChains.contains(chainKey))
                    continue;
                reportedChains.insert(chainKey);

                const QString message = QStringLiteral("cycle detected: %1")
                                            .arg(chain.join(QStringLiteral(" → ")));
                issues.append({requirements.at(v).id, field, message,
                               RequirementsValidator::Severity::Error});
            } else if (state[v] == 0) {
                dfs(v);
            }
        }
        stack.removeLast();
        state[u] = 2;
    };

    for (int u = 0; u < n; ++u) {
        if (state[u] == 0)
            dfs(u);
    }
}

} // namespace

QVector<RequirementsValidator::Issue> RequirementsValidator::validate(
    const QVector<Requirement> &requirements)
{
    QVector<Issue> issues;

    QHash<QString, int> idToIndex;
    for (int i = 0; i < requirements.size(); ++i)
        idToIndex.insert(requirements.at(i).id, i);

    const auto findById = [&](const QString &id) -> const Requirement * {
        const auto it = idToIndex.constFind(id);
        if (it == idToIndex.constEnd())
            return nullptr;
        return &requirements.at(it.value());
    };

    // --- Missing / incomplete fields --------------------------------------
    for (const Requirement &req : requirements) {
        if (req.title.trimmed().isEmpty())
            issues.append({req.id, QStringLiteral("title"),
                           QStringLiteral("title is empty"),
                           Severity::Warning});
        if (req.status.trimmed().isEmpty())
            issues.append({req.id, QStringLiteral("status"),
                           QStringLiteral("status is missing"),
                           Severity::Warning});
        if (req.priority.trimmed().isEmpty())
            issues.append({req.id, QStringLiteral("priority"),
                           QStringLiteral("priority is missing"),
                           Severity::Warning});
        if (req.date.trimmed().isEmpty())
            issues.append({req.id, QStringLiteral("date"),
                           QStringLiteral("creation date is missing"),
                           Severity::Warning});
        if (req.description.trimmed().isEmpty())
            issues.append({req.id, QStringLiteral("description"),
                           QStringLiteral("description is empty"),
                           Severity::Warning});
        if (req.acceptanceCriteria.isEmpty())
            issues.append({req.id, QStringLiteral("acceptanceCriteria"),
                           QStringLiteral("no acceptance criteria defined"),
                           Severity::Warning});
    }

    // --- Parent references (Родител) --------------------------------------
    for (const Requirement &req : requirements) {
        if (req.parentId.trimmed().isEmpty())
            continue;
        if (req.parentId == req.id) {
            issues.append({req.id, QStringLiteral("parentId"),
                           QStringLiteral("requirement references itself as parent"),
                           Severity::Error});
            continue;
        }
        const Requirement *parent = findById(req.parentId);
        if (!parent) {
            issues.append({req.id, QStringLiteral("parentId"),
                           QStringLiteral("parent '%1' does not exist (dangling reference)")
                               .arg(req.parentId),
                           Severity::Error});
        }
    }

    // --- Dependency references (Зависи от) --------------------------------
    for (const Requirement &req : requirements) {
        for (const QString &dep : req.dependencies) {
            if (dep == req.id) {
                issues.append({req.id, QStringLiteral("dependencies"),
                               QStringLiteral("requirement depends on itself"),
                               Severity::Error});
                continue;
            }
            const Requirement *target = findById(dep);
            if (!target) {
                issues.append({req.id, QStringLiteral("dependencies"),
                               QStringLiteral("dependency '%1' does not exist (dangling reference)")
                                   .arg(dep),
                               Severity::Error});
            } else if (target->section == QStringLiteral("archive")
                       && req.section != QStringLiteral("archive")) {
                issues.append({req.id, QStringLiteral("dependencies"),
                               QStringLiteral("depends on archived requirement '%1'")
                                   .arg(dep),
                               Severity::Warning});
            }
        }
    }

    // --- Cycles in the dependency graph (DFS back-edge detection) ---------
    const int n = requirements.size();
    QVector<QVector<int>> depAdjacency(n);
    for (int u = 0; u < n; ++u) {
        for (const QString &dep : requirements.at(u).dependencies) {
            const auto it = idToIndex.constFind(dep);
            if (it == idToIndex.constEnd() || it.value() == u)
                continue; // dangling / self references are reported above
            depAdjacency[u].append(it.value());
        }
    }
    collectCycles(requirements, idToIndex, depAdjacency,
                  QStringLiteral("dependencies"), issues);

    // --- Cycles in the parent graph (tree would otherwise nest forever) ---
    QVector<QVector<int>> parentAdjacency(n);
    for (int u = 0; u < n; ++u) {
        if (requirements.at(u).parentId.trimmed().isEmpty())
            continue;
        const auto it = idToIndex.constFind(requirements.at(u).parentId);
        if (it == idToIndex.constEnd() || it.value() == u)
            continue;
        parentAdjacency[u].append(it.value());
    }
    collectCycles(requirements, idToIndex, parentAdjacency,
                  QStringLiteral("parentId"), issues);

    // --- Sort: errors first, then by requirement ID -----------------------
    std::stable_sort(issues.begin(), issues.end(),
                     [](const Issue &a, const Issue &b) {
                         if (a.severity != b.severity)
                             return a.severity == Severity::Error;
                         return a.id < b.id;
                     });

    return issues;
}

int RequirementsValidator::countSeverity(const QVector<Issue> &issues, Severity severity)
{
    int count = 0;
    for (const Issue &issue : issues) {
        if (issue.severity == severity)
            ++count;
    }
    return count;
}

} // namespace Daqster
