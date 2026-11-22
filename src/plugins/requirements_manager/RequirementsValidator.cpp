#include "RequirementsValidator.h"

#include "RequirementDates.h"

#include <QHash>
#include <QDir>
#include <QFileInfo>
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

// ── Споменатите кодови пътища (REQ-SW-PL-103) ───────────────────────────────
namespace {

// The three traceability fields that carry paths. "Код:" is the code itself;
// "Тестове:"/"Документация:" are citations of the same kind — a stale path in
// any of them misleads the reader the same way.
QStringList citedPathFields(const Requirement &req)
{
    return {req.code, req.tests, req.docs};
}

// Repository top-level directories a cited path may start with. A token such as
// "DecisionNode.*" or "0/N" is prose or a number, not a path, and treating it as
// one buries the real findings under noise.
bool looksLikeRepoToplevel(const QString &path)
{
    static const QStringList prefixes = {
        QStringLiteral("src/"),     QStringLiteral("tests/"),
        QStringLiteral("cmake/"),   QStringLiteral("scripts/"),
        QStringLiteral("docs/"),    QStringLiteral("tools/"),
        QStringLiteral("DevelopmentProcess/"), QStringLiteral("external_libs/"),
        QStringLiteral(".github/"), QStringLiteral("plugins/")};
    for (const QString &prefix : prefixes) {
        if (path.startsWith(prefix))
            return true;
    }
    return false;
}

// {a,b} -> a, b (cartesian product over every brace group), the same expansion
// the CLI does: `{h,cpp}` and `{core,gui}` are how the requirement texts
// actually cite several files in one token.
QStringList expandBraces(const QString &token)
{
    static const QRegularExpression braceRe(QStringLiteral("\\{([^{}]*)\\}"));
    const QRegularExpressionMatch match = braceRe.match(token);
    if (!match.hasMatch())
        return {token};
    QStringList out;
    const QString head = token.left(match.capturedStart());
    const QString tail = token.mid(match.capturedEnd());
    for (const QString &alt : match.captured(1).split(QLatin1Char(',')))
        for (const QString &rest : expandBraces(tail))
            out.append(head + alt + rest);
    return out;
}

QStringList pathTokensOf(const QString &text)
{
    static const QRegularExpression spanRe(QStringLiteral("`([^`\\n]+)`"));
    static const QRegularExpression stripTailRe(QStringLiteral("[:#].*$"));
    static const QString forbidden = QStringLiteral(" *<>|\"'()$;&");
    QStringList out;
    auto it = spanRe.globalMatch(text);
    while (it.hasNext()) {
        QString span = it.next().captured(1);
        if (!looksLikeRepoToplevel(span))
            continue;
        bool bad = false;
        for (const QChar c : forbidden) {
            if (span.contains(c)) { bad = true; break; }
        }
        if (bad || span.contains(QStringLiteral("..")))
            continue;
        span.remove(stripTailRe);   // foo.cpp:36-38 / CMakeLists.txt#anchor
        for (const QString &p : expandBraces(span)) {
            const QString trimmed = p.trimmed();
            if (!trimmed.isEmpty())
                out.append(trimmed);
        }
    }
    return out;
}

// <root>/DevelopmentProcess/requirements/... -> <root>. The file path is the
// only anchor a parsed requirement carries, and the layout is fixed.
QString repoRootOf(const Requirement &req)
{
    static const QString marker = QStringLiteral("/DevelopmentProcess/");
    const QString path = QDir::fromNativeSeparators(req.filePath);
    const int at = path.indexOf(marker);
    return at > 0 ? path.left(at) : QString();
}

// Repository roots to resolve against: the requirement's own repo plus its
// sibling repos that also carry a requirements tree. Public requirements are
// read from the private tree's copies while their code lives in `../daqster`,
// so the sibling is part of the resolution — the same classes the CLI resolves
// (the develop tree and the private tree).
QStringList candidateRoots(const Requirement &req, const QStringList &explicitRoots)
{
    if (!explicitRoots.isEmpty())
        return explicitRoots;
    const QString root = repoRootOf(req);
    if (root.isEmpty())
        return {};
    QStringList roots{root};
    const QDir parent = QFileInfo(root).dir();
    const QFileInfoList siblings = parent.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &sibling : siblings) {
        if (sibling.absoluteFilePath() == root)
            continue;
        if (QDir(sibling.absoluteFilePath()).exists(QStringLiteral("DevelopmentProcess/requirements")))
            roots.append(sibling.absoluteFilePath());
    }
    return roots;
}

bool pathResolves(const QString &path, const QStringList &roots)
{
    for (const QString &root : roots) {
        if (QFileInfo::exists(QDir(root).filePath(path)))
            return true;
    }
    return false;
}

} // namespace

QStringList RequirementsValidator::citedCodePaths(const Requirement &req)
{
    static const QRegularExpression struckRe(QStringLiteral("~~[^~]*~~"));
    QString text = req.rawContent;
    text.remove(struckRe);
    return pathTokensOf(text);
}

std::optional<RequirementsValidator::Severity>
RequirementsValidator::pathSeverityForStatus(const QString &rawStatus){
    const QString token = normalizedStatus(rawStatus);
    if (token == QStringLiteral("IMPLEMENTED") || token == QStringLiteral("DONE"))
        return Severity::Error;
    if (token == QStringLiteral("ACTIVE") || token == QStringLiteral("BACKLOG"))
        return Severity::Warning;
    // DRAFT is a proposal: its files need not exist yet, and CANCELLED work was
    // never written. Neither is graded.
    return std::nullopt;
}

QVector<RequirementsValidator::Issue>
RequirementsValidator::validateCitedPaths(const QVector<Requirement> &requirements,
                                          const QStringList &repoRoots){
    QVector<Issue> issues;
    for (const Requirement &req : requirements) {
        const auto severity = pathSeverityForStatus(req.status);
        if (!severity)
            continue;
        const QStringList roots = candidateRoots(req, repoRoots);
        if (roots.isEmpty())
            continue;   // no file behind the record — nothing to resolve against
        for (const QString &field : citedPathFields(req)) {
            for (const QString &path : pathTokensOf(field)) {
                if (pathResolves(path, roots))
                    continue;
                const QString message = *severity == Severity::Error
                    ? QStringLiteral("цитираният код '%1' не съществува").arg(path)
                    : QStringLiteral("цитираният код '%1' още не е имплементиран (%2)")
                          .arg(path, normalizedStatus(req.status));
                issues.append({req.id, QStringLiteral("code"), message, *severity});
            }
        }
    }
    std::stable_sort(issues.begin(), issues.end(),
                     [](const Issue &a, const Issue &b) {
                         if (a.severity != b.severity)
                             return a.severity == Severity::Error;
                         return a.id < b.id;
                     });
    return issues;
}

// ── Връзка идея ↔ изискване (REQ-SW-PL-098) ──────────────────────────────

namespace {

//!< One "idea-link" finding; every rule reports through here, so the field name
//!< and the severity cannot drift between the three of them.
RequirementsValidator::Issue ideaLinkIssue(const QString &id, const QString &message,
                                           const QString &filePath)
{
    return {id, QStringLiteral("idea-link"), message,
            RequirementsValidator::Severity::Error, filePath};
}

//!< Direction 1: a requirement citing an idea must find a recorded promotion.
QVector<RequirementsValidator::Issue>
citationIssues(const QVector<Requirement> &requirements,
               const QHash<QString, const Idea *> &ideaById)
{
    const QStringList recorded = RequirementsValidator::ideaPromotionStatuses();
    QVector<RequirementsValidator::Issue> issues;
    for (const Requirement &req : requirements) {
        for (const QString &cited : RequirementsValidator::citedIdeaIds(req)) {
            const Idea *idea = ideaById.value(cited, nullptr);
            if (!idea) {
                issues.append(ideaLinkIssue(
                    req.id,
                    QStringLiteral("изискването цитира %1, но няма такъв файл в "
                                   "plans/ideas/").arg(cited),
                    req.filePath));
                continue;
            }
            const QString status = idea->status.trimmed();
            if (recorded.contains(status.toLower()))
                continue;
            issues.append(ideaLinkIssue(
                req.id,
                QStringLiteral("изискването цитира идея %1 („%2“), а тя не носи запис за "
                               "промоция — %3")
                    .arg(cited,
                         status.isEmpty() ? QStringLiteral("без статус") : status, req.id),
                req.filePath));
        }
    }
    return issues;
}

//!< Direction 2: "промотирана" must name a requirement that exists.
QVector<RequirementsValidator::Issue>
promotionIssues(const QVector<Idea> &ideas, const QSet<QString> &requirementIds)
{
    QVector<RequirementsValidator::Issue> issues;
    for (const Idea &idea : ideas) {
        // Two keys, two different requirements: the reader keeps the first and
        // would drop the second in silence, which is exactly the "renamed or
        // moved" drift the field exists to catch (REQ-SW-PL-098 review R1-3).
        if (!idea.promotedKeyId.isEmpty() && !idea.requirementKeyId.isEmpty()
            && idea.promotedKeyId.compare(idea.requirementKeyId, Qt::CaseInsensitive) != 0) {
            issues.append(ideaLinkIssue(
                idea.id,
                QStringLiteral("идея %1 носи две различни връзки — „Промотирана: %2“ и "
                               "„Изискване: %3“ — едната е остаряла")
                    .arg(idea.id, idea.promotedKeyId, idea.requirementKeyId),
                idea.path));
        }
        const QString promoted = idea.promotedId.trimmed();
        if (promoted.isEmpty() || requirementIds.contains(promoted.toUpper()))
            continue;   // an idea without a requirement is the normal state
        issues.append(ideaLinkIssue(
            promoted,
            QStringLiteral("идея %1 е отбелязана като промотирана в %2, но такова "
                           "изискване няма").arg(idea.id, promoted),
            idea.path));
    }
    return issues;
}

} // namespace

QStringList RequirementsValidator::ideaPromotionStatuses()
{
    // Mirrors RECORDED in scripts/check-idea-promotion.sh. A second list would
    // be a second answer to "is the promotion recorded?".
    return {QStringLiteral("промотирана"), QStringLiteral("реализирана"),
            QStringLiteral("слята"), QStringLiteral("завършен"),
            QStringLiteral("исторически")};
}

QStringList RequirementsValidator::citedIdeaIds(const Requirement &req)
{
    static const QRegularExpression struckRe(QStringLiteral("~~[^~]*~~"));
    static const QRegularExpression ideaRe(QStringLiteral("\\b(IDEA-[0-9]+)"));
    QString text = req.rawContent;
    text.remove(struckRe);

    QStringList ids;
    QRegularExpressionMatchIterator it = ideaRe.globalMatch(text);
    while (it.hasNext()) {
        const QString id = it.next().captured(1).toUpper();
        if (!ids.contains(id))
            ids.append(id);
    }
    return ids;
}

QVector<RequirementsValidator::Issue>
RequirementsValidator::validateIdeaLinks(const QVector<Requirement> &requirements,
                                         const QVector<Idea> &ideas)
{
    QHash<QString, const Idea *> ideaById;   // upper-case IDEA-NNN -> idea
    for (const Idea &idea : ideas)
        ideaById.insert(idea.id.trimmed().toUpper(), &idea);

    QSet<QString> requirementIds;            // upper-case REQ-*
    for (const Requirement &req : requirements)
        requirementIds.insert(req.id.trimmed().toUpper());

    QVector<Issue> issues = citationIssues(requirements, ideaById);
    issues += promotionIssues(ideas, requirementIds);

    std::stable_sort(issues.begin(), issues.end(),
                     [](const Issue &a, const Issue &b) {
                         if (a.severity != b.severity)
                             return a.severity == Severity::Error;
                         return a.id < b.id;
                     });
    return issues;
}

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
        // REQ-SW-PL-101: "Създадено:" is the field, "Дата:" the legacy alias, so
        // one accessor answers both and the two warnings stay distinct
        // (REQ-SW-PL-100): "missing" and "not a date" are different repairs.
        const QString created = createdDate(req);
        if (created.trimmed().isEmpty()) {
            issues.append({req.id, QStringLiteral("date"),
                           QStringLiteral("creation date is missing"),
                           Severity::Warning});
        } else if (!RequirementDates::isValidIsoDate(created)) {
            // REQ-SW-PL-100: the field is a date, not free text. A Warning (not
            // an Error) so a legacy file with a sloppy value stays valid — it
            // is only invisible, and being invisible is what let the field rot.
            issues.append({req.id, QStringLiteral("date"),
                           QStringLiteral("датата '%1' не е YYYY-MM-DD")
                               .arg(created.trimmed()),
                           Severity::Warning});
        }
        if (req.description.trimmed().isEmpty())
            issues.append({req.id, QStringLiteral("description"),
                           QStringLiteral("description is empty"),
                           Severity::Warning});
        if (req.acceptanceCriteria.isEmpty())
            issues.append({req.id, QStringLiteral("acceptanceCriteria"),
                           QStringLiteral("no acceptance criteria defined"),
                           Severity::Warning});
    }

    // --- Template generation (REQ-SW-PL-008) ------------------------------
    // One canonical template: the description section is "## Накратко" (v2).
    // The two older generations stay recognised (their text is a real
    // description, not a missing one) but are reported, otherwise v1 never
    // retires: 88 of the 129 files on the real tree still say "## Описание".
    // A file with no description heading at all follows no template at all —
    // that is an Error, not a deviation, and it must not be silenced.
    for (const Requirement &req : requirements) {
        const QString version = req.templateVersion.trimmed();
        if (version.isEmpty())
            continue; // hand-built record (no .md behind it) — nothing to grade
        if (version == RequirementTemplate::canonicalVersion())
            continue;

        const QString canonical = QStringLiteral("%1 (## %2)")
                                      .arg(RequirementTemplate::canonicalVersion(),
                                           RequirementTemplate::canonicalHeading());
        if (version == QStringLiteral("unknown")) {
            issues.append({req.id, QStringLiteral("template"),
                           QStringLiteral("няма секция за описание — каноничният "
                                          "шаблон иска ## %1")
                               .arg(RequirementTemplate::canonicalHeading()),
                           Severity::Error});
            continue;
        }
        const QString heading = req.templateHeading.trimmed().isEmpty()
            ? RequirementTemplate::headingForVersion(version)
            : req.templateHeading.trimmed();
        issues.append({req.id, QStringLiteral("template"),
                       QStringLiteral("шаблон %1 (## %2) — каноничният е %3")
                           .arg(version, heading, canonical),
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
        //
        // DRAFT (REQ-SW-PL-099) is a proposal awaiting the owner's approval: it
        // is legal only in active/, because that is where it is approved. A
        // DRAFT under archive/ is a contradiction of the same shape as
        // DONE-in-active/, and it says so in its own words rather than falling
        // through to the generic archive message.
        const QString zone = req.section.trimmed().toLower();
        const bool archived = archivedStatuses().contains(token);
        const bool either = eitherZoneStatuses().contains(token);
        if (zone == QStringLiteral("archive") && token == QStringLiteral("DRAFT")) {
            issues.append({req.id, QStringLiteral("status"),
                           QStringLiteral("статус '%1' в archive/ — черновата се "
                                          "одобрява в active/, не се архивира")
                               .arg(token),
                           Severity::Error});
        } else if (zone == QStringLiteral("archive") && !archived && !either) {
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

    // --- Revision vs approval (REQ-SW-PL-106) ----------------------------
    // The approval is pinned to a number, so "the file grew past what the human
    // accepted" is a fact, not a guess. It blocks exactly the statuses that mean
    // "work in progress" — ACTIVE and IMPLEMENTED; DRAFT is blocked by its own
    // status and the archived end states have nothing left to implement. A file
    // with no approval line (approvedRevision < 0) is legacy and silent.
    for (const Requirement &req : requirements) {
        const QString token = normalizedStatus(req.status);
        if (token != QStringLiteral("ACTIVE") && token != QStringLiteral("IMPLEMENTED"))
            continue;
        if (req.approvedRevision < 0 || req.revision <= req.approvedRevision)
            continue;
        issues.append({req.id, QStringLiteral("revision"),
                       QStringLiteral("ревизия %1 не е одобрена (одобрена е ревизия %2) "
                                      "— имплементацията е блокирана")
                           .arg(QString::number(req.revision),
                                QString::number(req.approvedRevision)),
                       Severity::Error});
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
    // Explicit grading of a reference by the target's status (REQ-SW-PL-008):
    //   DONE / IMPLEMENTED / ACTIVE / BACKLOG -> nothing: a normal state. A DONE
    //       dependency is the end state the lifecycle asks for, so archiving it
    //       is not a defect (warning on it punished 4 correct filings);
    //   CANCELLED -> Warning: the dependency was dropped, so a live requirement
    //       cannot be built on it;
    //   archived with an unreadable status -> Warning naming the file: the
    //       reference points at a record nobody can grade;
    //   not in the tree -> Error: a broken reference is never silence.
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
                               QStringLiteral("зависи от несъществуващо изискване '%1'")
                                   .arg(dep),
                               Severity::Error});
                continue;
            }
            const QString token = normalizedStatus(target->status);
            if (token == QStringLiteral("CANCELLED")) {
                issues.append({req.id, QStringLiteral("dependencies"),
                               QStringLiteral("зависи от отказано изискване '%1'")
                                   .arg(dep),
                               Severity::Warning});
            } else if (target->section == QStringLiteral("archive")
                       && !vocabulary.contains(token)) {
                // Archived, yet its status cannot be read at all: the reference
                // resolves to a record the vocabulary does not grade. Name the
                // file so a human can open it.
                const QString file = target->fileName.trimmed().isEmpty()
                    ? QFileInfo(target->filePath).fileName()
                    : target->fileName;
                issues.append({req.id, QStringLiteral("dependencies"),
                               QStringLiteral("зависи от архивирано изискване '%1' с "
                                              "неразпознат статус (файл %2)")
                                   .arg(dep, file),
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

    // --- Attach the source file to every issue that names an ID -----------
    // Filled in one post-pass instead of at each creation site: the copyable
    // report addresses an issue by file (REQ-SW-PL-001), and the requirement
    // set is the only place the mapping lives.
    QHash<QString, QString> idToFile;
    for (const Requirement &req : requirements) {
        if (!req.id.isEmpty() && !idToFile.contains(req.id.toLower()))
            idToFile.insert(req.id.toLower(), req.filePath);
    }
    for (Issue &issue : issues) {
        if (!issue.id.isEmpty())
            issue.filePath = idToFile.value(issue.id.toLower());
    }

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
    // DRAFT leads the list because it is where a requirement starts: an agent
    // writes it, the owner accepts it, and only then may it be implemented
    // (REQ-SW-PL-099).
    return {QStringLiteral("DRAFT"), QStringLiteral("ACTIVE"),
            QStringLiteral("IMPLEMENTED"), QStringLiteral("DONE"),
            QStringLiteral("CANCELLED"), QStringLiteral("BACKLOG")};
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
