#include "RequirementsValidator.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

#include <functional>

namespace Daqster {

namespace {

// Statuses that assert the work has landed: they may not stay in active/.
// Mirrors ARCHIVED_STATUSES in scripts/validate-traceability.sh (private repo).
const QStringList &archivedStatuses()
{
    static const QStringList list = {QStringLiteral("DONE"),
                                     QStringLiteral("CANCELLED")};
    return list;
}

// IMPLEMENTED belongs to neither zone: the code is on a branch, so the file may
// sit in active/ next to its siblings or be filed in archive/. Both are honest;
// only ACTIVE-in-archive/ and DONE-in-active/ are contradictions.
// Mirrors EITHER_ZONE_STATUSES in the same script.
const QStringList &eitherZoneStatuses()
{
    static const QStringList list = {QStringLiteral("IMPLEMENTED")};
    return list;
}

// The ID grammar of a requirement reference — the same one the parser resolves
// references with. A header citing something else ("ADR-AI-013") is a link of a
// different kind, not a mistyped requirement.
const QRegularExpression &requirementIdRe()
{
    static const QRegularExpression re(
        QStringLiteral("^REQ-[A-Z]+(-[A-Z]+)?-\\d{3}$"));
    return re;
}

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

    // --- Status vocabulary -------------------------------------------------
    // One lifecycle vocabulary, shared with the traceability CLI
    // (scripts/validate-traceability.sh → VOCAB): a status outside it is a
    // defect, not a new lifecycle. Two invalid statuses (FUTURE, PLANNED) once
    // lived in the tree with no visible consequence, which is exactly why this
    // is checked. An empty status is already reported as "status is missing"
    // above and is not repeated here.
    const QStringList vocabulary = statusVocabulary();
    for (const Requirement &req : requirements) {
        if (req.status.trimmed().isEmpty())
            continue;
        const QString token = normalizedStatus(req.status);
        if (token.isEmpty()) {
            issues.append({req.id, QStringLiteral("status"),
                           QStringLiteral("не може да се прочете статус от '%1' "
                                          "(позволени: %2)")
                               .arg(req.status, vocabulary.join(QLatin1Char(' '))),
                           Severity::Error});
            continue;
        }
        if (!vocabulary.contains(token)) {
            issues.append({req.id, QStringLiteral("status"),
                           QStringLiteral("невалиден статус '%1' (позволени: %2)")
                               .arg(token, vocabulary.join(QLatin1Char(' '))),
                           Severity::Error});
            continue;
        }
        // --- Status vs zone (CLI §3 parity) -------------------------------
        // RDD-PROCESS defines one lifecycle: ACTIVE -> DONE (move to archive).
        // The zone check is what catches "ACTIVE while sitting in archive/" and
        // "DONE still in active/" — the latter is the one real defect the two
        // validators now report identically (REQ-SW-PL-079).
        const QString zone = req.section.trimmed().toLower();
        const bool archived = archivedStatuses().contains(token);
        const bool either = eitherZoneStatuses().contains(token);
        if (zone == QStringLiteral("archive") && !archived && !either) {
            issues.append({req.id, QStringLiteral("status"),
                           QStringLiteral("статус '%1' в archive/ (очаква се %2 или %3)")
                               .arg(token,
                                    archivedStatuses().join(QStringLiteral(", ")),
                                    eitherZoneStatuses().join(QStringLiteral(", "))),
                           Severity::Error});
        } else if (zone == QStringLiteral("active") && archived) {
            issues.append({req.id, QStringLiteral("status"),
                           QStringLiteral("статус '%1' в active/ (очаква се ACTIVE, "
                                          "BACKLOG или %2)")
                               .arg(token, eitherZoneStatuses().join(QStringLiteral(", "))),
                           Severity::Error});
        }
    }

    // --- Acceptance criteria vs DONE (CLI §4 parity) ----------------------
    // A DONE requirement asserts the work landed, so it may not carry an
    // unaddressed criterion. An unchecked criterion annotated with ⛔ (proven
    // blocker) or ⏸ (blocked by environment) is a *documented* deferral — a
    // real state, reported as a Warning and left to a human decision.
    for (const Requirement &req : requirements) {
        if (normalizedStatus(req.status) != QStringLiteral("DONE"))
            continue;
        int open = 0;
        int deferred = 0;
        for (int i = 0; i < req.acceptanceCriteria.size(); ++i) {
            if (i < req.criteriaDone.size() && req.criteriaDone.at(i))
                continue;
            const QString &text = req.acceptanceCriteria.at(i);
            if (text.contains(QStringLiteral("⛔"))
                || text.contains(QStringLiteral("⏸")))
                ++deferred;
            else
                ++open;
        }
        if (open > 0) {
            issues.append({req.id, QStringLiteral("acceptanceCriteria"),
                           QStringLiteral("%1 незакрити AC без маркер при статус DONE")
                               .arg(open),
                           Severity::Error});
        }
        if (deferred > 0) {
            issues.append({req.id, QStringLiteral("acceptanceCriteria"),
                           QStringLiteral("%1 AC са ⏸/⛔ при статус DONE — виж CND-046")
                               .arg(deferred),
                           Severity::Warning});
        }
    }

    // --- Parent references (Родител) --------------------------------------
    // A "Родител:" value may cite several IDs, and may cite a non-requirement
    // document (ADR-AI-013). The tree uses the first requirement ID; every
    // listed reference is resolved here so none can rot unseen.
    for (const Requirement &req : requirements) {
        QStringList parents = req.parentIds;
        if (parents.isEmpty() && !req.parentId.trimmed().isEmpty())
            parents.append(req.parentId); // hand-built records (callers, tests)
        for (const QString &parentId : parents) {
            const QString id = parentId.trimmed();
            if (id.isEmpty())
                continue;
            if (id == req.id) {
                issues.append({req.id, QStringLiteral("parentId"),
                               QStringLiteral("requirement references itself as parent"),
                               Severity::Error});
                continue;
            }
            if (!requirementIdRe().match(id).hasMatch()) {
                issues.append({req.id, QStringLiteral("parentId"),
                               QStringLiteral("референцията '%1' не е изискване "
                                              "(REQ-…), не се резолвира тук")
                                   .arg(id),
                               Severity::Warning});
                continue;
            }
            if (!findById(id)) {
                issues.append({req.id, QStringLiteral("parentId"),
                               QStringLiteral("parent '%1' does not exist (dangling reference)")
                                   .arg(id),
                               Severity::Error});
            }
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
            if (!requirementIdRe().match(dep).hasMatch()) {
                issues.append({req.id, QStringLiteral("dependencies"),
                               QStringLiteral("референцията '%1' не е изискване "
                                              "(REQ-…), не се резолвира тук")
                                   .arg(dep),
                               Severity::Warning});
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

    // --- Child references (Деца) ------------------------------------------
    // The inverse of "Родител:". Nothing else reads it, so without this check a
    // stale child link would never be noticed.
    for (const Requirement &req : requirements) {
        for (const QString &child : req.childIds) {
            if (child == req.id)
                continue; // reported by the parent/graph checks
            if (!requirementIdRe().match(child).hasMatch()) {
                issues.append({req.id, QStringLiteral("childIds"),
                               QStringLiteral("референцията '%1' не е изискване "
                                              "(REQ-…), не се резолвира тук")
                                   .arg(child),
                               Severity::Warning});
                continue;
            }
            if (!findById(child)) {
                issues.append({req.id, QStringLiteral("childIds"),
                               QStringLiteral("child '%1' does not exist (dangling reference)")
                                   .arg(child),
                               Severity::Error});
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
            const bool isParent = it.key() == req.parentId
                || req.parentIds.contains(it.key());
            const QString field = isParent
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
        QStringList parents = requirements.at(u).parentIds;
        if (parents.isEmpty() && !requirements.at(u).parentId.trimmed().isEmpty())
            parents.append(requirements.at(u).parentId);
        for (const QString &parentId : parents) {
            const auto it = idToIndex.constFind(parentId.trimmed().toLower());
            if (it == idToIndex.constEnd() || it.value() == u)
                continue;
            parentAdjacency[u].append(it.value());
        }
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

QStringList RequirementsValidator::statusVocabulary()
{
    // Order and membership mirror `readonly VOCAB` in
    // scripts/validate-traceability.sh. Changing one without the other makes
    // the two validators disagree again, which is the defect this fixes.
    return {QStringLiteral("ACTIVE"), QStringLiteral("IMPLEMENTED"),
            QStringLiteral("DONE"), QStringLiteral("CANCELLED"),
            QStringLiteral("BACKLOG")};
}

QString RequirementsValidator::normalizedStatus(const QString &rawStatus)
{
    // "DONE НА КЛОНА" is IMPLEMENTED, not DONE: the code sits on a branch and
    // nothing is in develop. The CLI maps it the same way, so both validators
    // read the same state from the same file.
    static const QRegularExpression doneOnBranchRe(
        QStringLiteral("^DONE\\s+НА\\s+КЛОНА"));

    // A field of the value, optionally wrapped in punctuation the author used
    // for emphasis — "(ACTIVE)", "**ACTIVE**", "ACTIVE,". Inner lowercase
    // letters disqualify a field, so prose ("на", "клон") can never be read as
    // a status.
    static const QRegularExpression fieldTokenRe(
        QStringLiteral("^[^A-Za-z]*([A-Z]{3,15})[^A-Za-z]*$"));

    QString text = rawStatus;
    text.remove(QLatin1Char('*')).remove(QLatin1Char('`')).remove(QLatin1Char(':'));

    if (doneOnBranchRe.match(text.trimmed()).hasMatch())
        return QStringLiteral("IMPLEMENTED");

    // The status is the FIRST readable all-caps field; whatever follows it is
    // rationale ("ACTIVE — AC1–AC13 имплементирани").
    const QStringList fields =
        text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const QString &field : fields) {
        const QRegularExpressionMatch match = fieldTokenRe.match(field);
        if (match.hasMatch())
            return match.captured(1);
    }
    return QString();
}

} // namespace Daqster
