#include "RequirementsValidator.h"

#include <QHash>
#include <QRegularExpression>
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

    // --- Duplicate IDs across repos (REQ-SW-PL-012) ----------------------
    // With a merged multi-repo vector, the same bare ID can appear twice (e.g.
    // a public REQ-SW-PL-013 and a private duplicate). The old silent
    // last-wins lookup hid this; now every duplicate occurrence is an Error.
    QHash<QString, int> idToIndex;
    QHash<QString, QStringList> idOwners; // lowercase ID -> repos seen so far
    for (int i = 0; i < requirements.size(); ++i) {
        const Requirement &req = requirements.at(i);
        const QString key = req.id.toLower();
        if (idToIndex.contains(key)) {
            QStringList repos = idOwners.value(key);
            const QString repoLabel = req.repo.trimmed().isEmpty()
                ? QStringLiteral("(unknown)")
                : req.repo;
            if (!repos.contains(repoLabel))
                repos.append(repoLabel);
            idOwners.insert(key, repos);
            issues.append({req.id, QStringLiteral("id"),
                           QStringLiteral("duplicate requirement ID '%1' — shared by %2")
                               .arg(req.id, repos.join(QStringLiteral(", "))),
                           Severity::Error});
        } else {
            idToIndex.insert(key, i);
            idOwners.insert(key, {req.repo.trimmed().isEmpty()
                                      ? QStringLiteral("(unknown)")
                                      : req.repo});
        }
    }

    const auto findById = [&](const QString &id) -> const Requirement * {
        const auto it = idToIndex.constFind(id.toLower());
        if (it == idToIndex.constEnd())
            return nullptr;
        return &requirements.at(it.value());
    };

    // --- ID format (typed scheme) -----------------------------------------
    // The ID is a validated format-model, not semantics: public requirements
    // follow "REQ-SW-<TYPE>-<NN>" (FW/APP/PL/BLD), private ones follow
    // "REQ-<PREFIX>-<NN>" (PLG/AI/SEC/DOC). Anything else gets a Warning so
    // malformed IDs stay visible without breaking the tree.
    const QRegularExpression idFormatRe(
        QStringLiteral("^REQ-[A-Z]+(-[A-Z]+)?-\\d{3}$"));
    for (const Requirement &req : requirements) {
        if (!idFormatRe.match(req.id).hasMatch()) {
            issues.append({req.id, QStringLiteral("id"),
                           QStringLiteral("ID '%1' не следва схемата REQ-<PREFIX>-<NN>")
                               .arg(req.id),
                           Severity::Warning});
        }
    }

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

    // --- Cross-repo annotation hints (REQ-SW-PL-012) ---------------------
    // An annotation like "REQ-SW-PL-013 (частно)" claims the referenced
    // requirement lives in the private repo. When the hint's implied repo does
    // not match the resolved requirement's actual repo, flag a Warning (the
    // reference still resolves; only the annotation is wrong).
    const auto impliedRepo = [](const QString &hint) -> QString {
        if (hint.contains(QStringLiteral("публично"), Qt::CaseInsensitive))
            return QStringLiteral("public");
        if (hint.contains(QStringLiteral("частно"), Qt::CaseInsensitive))
            return QStringLiteral("private");
        return QString();
    };

    auto checkHintRepo = [&](const Requirement &req, const QString &field,
                             const QString &bareId, const QString &hint,
                             const Requirement *target) {
        if (!target || hint.trimmed().isEmpty())
            return;
        const QString implied = impliedRepo(hint);
        if (implied.isEmpty())
            return;
        const QString actual = target->repo.trimmed().isEmpty()
            ? QStringLiteral("(unknown)")
            : target->repo;
        if (QString::compare(implied, actual, Qt::CaseInsensitive) != 0) {
            issues.append({req.id, field,
                           QStringLiteral("reference '%1' is annotated as %2 but resolves to "
                                          "a %3 requirement")
                               .arg(bareId, hint.trimmed(), actual),
                           Severity::Warning});
        }
    };

    for (const Requirement &req : requirements) {
        for (auto it = req.dependencyHints.constBegin();
             it != req.dependencyHints.constEnd(); ++it) {
            const QString field = (it.key() == req.parentId)
                ? QStringLiteral("parentId")
                : QStringLiteral("dependencies");
            checkHintRepo(req, field, it.key(), it.value(), findById(it.key()));
        }
    }

    // --- Cycles in the dependency graph (DFS back-edge detection) ---------
    const int n = requirements.size();
    QVector<QVector<int>> depAdjacency(n);
    for (int u = 0; u < n; ++u) {
        for (const QString &dep : requirements.at(u).dependencies) {
            const auto it = idToIndex.constFind(dep.toLower());
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
        const auto it = idToIndex.constFind(requirements.at(u).parentId.toLower());
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
