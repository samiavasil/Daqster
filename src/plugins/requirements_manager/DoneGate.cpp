#include "DoneGate.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>

#include "RequirementsValidator.h"

namespace Daqster {

namespace {

constexpr char kReviewSubdir[] = "DevelopmentProcess/plans/reviews";
constexpr int kGitTimeoutMs = 10000;

QString readAll(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QTextStream in(&file);
    return in.readAll();
}

// One `git` run in @p root; arguments are plain words (no shell).
bool runGit(const QString &root, const QStringList &arguments, QString *output)
{
    QProcess git;
    git.setWorkingDirectory(root);
    git.start(QStringLiteral("git"), arguments);
    if (!git.waitForStarted(kGitTimeoutMs) || !git.waitForFinished(kGitTimeoutMs))
        return false;
    if (output)
        *output = QString::fromUtf8(git.readAllStandardOutput());
    return git.exitStatus() == QProcess::NormalExit && git.exitCode() == 0;
}

// True when `develop` exists in @p root as a commit — a repo without the branch
// is a different failure ("нищо не се проверява"), not "the code is missing".
bool hasDevelop(const QString &root)
{
    return runGit(root, {QStringLiteral("rev-parse"), QStringLiteral("--verify"),
                         QStringLiteral("--quiet"),
                         QStringLiteral("develop^{commit}")},
                  nullptr);
}

// The three answers a repo can give about `develop`: it has the branch, it is a
// repo without it, or it is not a repo at all. Collapsing the last two into
// "no" is what let a missing repo read as "the code is in develop".
enum class Develop { Present, Branchless, RepoMissing };

Develop developState(const QString &root)
{
    if (hasDevelop(root))
        return Develop::Present;
    const bool isRepo = runGit(root, {QStringLiteral("rev-parse"),
                                      QStringLiteral("--git-dir")},
                               nullptr);
    return isRepo ? Develop::Branchless : Develop::RepoMissing;
}

// Every path of @p paths that `develop` does not carry, in one process:
// --batch-check reads "<rev>:<path>" lines and answers "<id> missing" for the
// absent ones, so the price is per requirement, not per path.
QStringList pathsMissingFromDevelop(const QString &root, const QStringList &paths)
{
    QProcess cat;
    cat.setWorkingDirectory(root);
    cat.start(QStringLiteral("git"),
              {QStringLiteral("cat-file"), QStringLiteral("--batch-check")});
    if (!cat.waitForStarted(kGitTimeoutMs))
        return paths;
    QByteArray input;
    for (const QString &path : paths)
        input += QByteArray("develop:") + path.toUtf8() + '\n';
    cat.write(input);
    cat.closeWriteChannel();
    if (!cat.waitForFinished(kGitTimeoutMs))
        return paths;

    const QStringList answers = QString::fromUtf8(cat.readAllStandardOutput())
                                    .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QStringList missing;
    for (int i = 0; i < paths.size(); ++i) {
        // A path git cannot even name (empty answer) is missing, not resolved:
        // silence must never read as a pass.
        if (i >= answers.size() || answers.at(i).trimmed().endsWith(QStringLiteral(" missing")))
            missing.append(paths.at(i));
    }
    return missing;
}

// The branch a repo has the path on instead of develop (its checked-out one).
QString currentBranch(const QString &root)
{
    QString out;
    if (!runGit(root, {QStringLiteral("rev-parse"), QStringLiteral("--abbrev-ref"),
                       QStringLiteral("HEAD")}, &out))
        return QString();
    return out.trimmed();
}

// The root whose working tree carries @p path, empty when none does.
QString worktreeRootWith(const QVector<RequirementRoot> &roots, const QString &path)
{
    for (const RequirementRoot &root : roots) {
        if (QFileInfo::exists(QDir(root.repoRoot).filePath(path)))
            return root.repoRoot;
    }
    return QString();
}

// The paths of @p paths that NO loaded repo's develop carries. One batch per
// repo, and a path an earlier repo solved is not asked about again: the price
// is per requirement, not per path. A repo without `develop` is not asked at
// all — its silence would read as "missing", which is a different fact.
// @p answered tells the caller whether any repo could answer at all.
QStringList pathsMissingEverywhere(const QVector<RequirementRoot> &roots,
                                   QStringList paths, bool *answered)
{
    *answered = false;
    for (const RequirementRoot &root : roots) {
        if (paths.isEmpty() || !hasDevelop(root.repoRoot))
            continue;
        *answered = true;
        paths = pathsMissingFromDevelop(root.repoRoot, paths);
    }
    return paths;
}

// Why the develop check could not run — the sentence the owner reads instead of
// a silent pass. Never says "in develop" when there is no develop to ask.
QString whyNoDevelop(const QVector<RequirementRoot> &roots)
{
    if (roots.isEmpty())
        return QStringLiteral("няма заредено репо");
    QStringList reasons;
    for (const RequirementRoot &root : roots) {
        switch (developState(root.repoRoot)) {
        case Develop::Present:
            continue;
        case Develop::Branchless:
            reasons.append(QStringLiteral("няма клон `develop` в %1").arg(root.repoRoot));
            break;
        case Develop::RepoMissing:
            reasons.append(QStringLiteral("%1 не е git репо").arg(root.repoRoot));
            break;
        }
    }
    return reasons.join(QStringLiteral("; "));
}

// Fills the three code fields of @p req from the cited @p paths.
//
// "Cannot check" and "checked, all there" must never answer the same: a repo
// without `develop` (or no repo at all) is a REFUSAL carrying the reason, not a
// pass (REQ-SW-PL-107 AC2).
void verifyCode(Requirement &req, const QVector<RequirementRoot> &roots,
                const QStringList &paths)
{
    bool answered = false;
    const QStringList missing = pathsMissingEverywhere(roots, paths, &answered);
    if (!answered) {
        req.codeInDevelop = false;
        req.codeCheckError = whyNoDevelop(roots);
        return;
    }
    if (missing.isEmpty()) {
        req.codeInDevelop = true;
        return;
    }
    const QString carrier = worktreeRootWith(roots, missing.first());
    req.missingDevelopPaths = missing;
    req.codeBranch = carrier.isEmpty() ? QString() : currentBranch(carrier);
    req.codeInDevelop = false;
}

} // namespace

QString DoneGate::lastReviewVerdict(const QString &content)
{
    const QStringList lines = content.split(QLatin1Char('\n'));
    bool inRounds = false;
    QString verdict;
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed == QStringLiteral("## Кръгове")) {
            inRounds = true;
            continue;
        }
        if (!inRounds)
            continue;
        if (trimmed.startsWith(QStringLiteral("## ")))
            break;
        if (!trimmed.startsWith(QLatin1Char('|')))
            continue;
        // A row is `| a | b | c | d | verdict |`; the cells are the parts
        // between the pipes, so the last column is the second-to-last part.
        const QStringList cells = trimmed.split(QLatin1Char('|'));
        if (cells.size() < 3)
            continue;
        const QString cell = cells.at(cells.size() - 2).trimmed();
        if (cell.isEmpty() || cell == QStringLiteral("присъда"))
            continue;   // the header row names the column, it is not a verdict
        if (cell.count(QLatin1Char('-')) == cell.size() && cell.size() > 1)
            continue;   // the |---|---| separator
        verdict = cell;
    }
    return verdict;
}

bool DoneGate::isAcceptingVerdict(const QString &verdict)
{
    const QString token = verdict.trimmed();
    return token == QStringLiteral("ПРИЕТО")
           || token == QStringLiteral("ПРИЕТО С БЕЛЕЖКИ");
}

QString DoneGate::findReviewRecord(const QVector<RequirementRoot> &repoRoots,
                                   const QString &id)
{
    for (const RequirementRoot &root : repoRoots) {
        const QDir reviewsDir(QDir(root.repoRoot).filePath(QString::fromUtf8(kReviewSubdir)));
        if (!reviewsDir.exists())
            continue;
        const QStringList files = reviewsDir.entryList({QStringLiteral("*.md")},
                                                       QDir::Files, QDir::Name);
        for (const QString &name : files) {
            // README.md is the template, not a record of any requirement.
            if (name.compare(QStringLiteral("README.md"), Qt::CaseInsensitive) == 0)
                continue;
            const QString path = reviewsDir.absoluteFilePath(name);
            if (readAll(path).contains(id, Qt::CaseInsensitive))
                return path;
        }
    }
    return QString();
}

QStringList DoneGate::missingPreconditions(const Requirement &req)
{
    QStringList missing;
    missing.append(criteriaGaps(req));
    const QString reviewGap = reviewGapFor(req);
    if (!reviewGap.isEmpty())
        missing.append(reviewGap);
    const QString codeGap = codeGapFor(req);
    if (!codeGap.isEmpty())
        missing.append(codeGap);
    const QString zoneGap = zoneGapFor(req);
    if (!zoneGap.isEmpty())
        missing.append(zoneGap);
    return missing;
}

QStringList DoneGate::criteriaGaps(const Requirement &req)
{
    if (req.criteriaDone.isEmpty())
        return {QStringLiteral("няма нито един AC — договорът липсва")};
    QStringList gaps;
    for (int i = 0; i < req.criteriaDone.size(); ++i) {
        if (req.criteriaDone.at(i))
            continue;
        gaps.append(QStringLiteral("AC%1 не е отметнат: %2")
                        .arg(QString::number(i + 1),
                             req.acceptanceCriteria.value(i).left(70)));
    }
    return gaps;
}

QString DoneGate::reviewGapFor(const Requirement &req)
{
    if (req.reviewRecord.isEmpty()) {
        return QStringLiteral("няма ревю запис в plans/reviews/, който да назовава %1")
            .arg(req.id);
    }
    if (isAcceptingVerdict(req.reviewVerdict))
        return QString();
    const QString verdict = req.reviewVerdict.isEmpty()
                                ? QStringLiteral("няма ред за кръг")
                                : req.reviewVerdict;
    return QStringLiteral("последната присъда в %1 е „%2“ — не е приемане")
        .arg(QFileInfo(req.reviewRecord).fileName(), verdict);
}

QString DoneGate::codeGapFor(const Requirement &req)
{
    if (req.codeInDevelop)
        return QString();
    // A check that could not run says so: it is a refusal with a reason, never
    // the silence that reads as "everything resolves".
    if (!req.codeCheckError.isEmpty())
        return QStringLiteral("не мога да проверя дали кодът е в develop: %1")
            .arg(req.codeCheckError);
    const QString paths = req.missingDevelopPaths.join(QStringLiteral(", "));
    if (req.codeBranch.isEmpty())
        return QStringLiteral("кодът още не е в `develop`: %1").arg(paths);
    return QStringLiteral("кодът още не е в `develop` (на клона `%1` е): %2")
        .arg(req.codeBranch, paths);
}

QString DoneGate::zoneGapFor(const Requirement &req)
{
    if (RequirementsValidator::normalizedStatus(req.status) == QStringLiteral("DONE")
        && req.section != QStringLiteral("archive")) {
        return QStringLiteral("статус DONE, но файлът не е в archive/");
    }
    return QString();
}

bool DoneGate::isDoneCandidate(const Requirement &req)
{
    return RequirementsValidator::normalizedStatus(req.status)
               == QStringLiteral("IMPLEMENTED")
           && missingPreconditions(req).isEmpty();
}

void DoneGate::evaluate(Requirement &req, const QVector<RequirementRoot> &repoRoots,
                        bool forceCodeCheck)
{
    req.allCriteriaDone = !req.criteriaDone.isEmpty()
                          && !req.criteriaDone.contains(false);
    req.reviewRecord = findReviewRecord(repoRoots, req.id);
    req.reviewVerdict = req.reviewRecord.isEmpty()
                            ? QString()
                            : lastReviewVerdict(readAll(req.reviewRecord));
    req.acceptingReview = !req.reviewRecord.isEmpty()
                          && isAcceptingVerdict(req.reviewVerdict);

    // git answers only where the answer can change a decision: a non-IMPLEMENTED
    // requirement is not a candidate, and the tree must not pay two processes
    // per row for a status that can never be marked.
    const bool implemented = RequirementsValidator::normalizedStatus(req.status)
                             == QStringLiteral("IMPLEMENTED");
    req.missingDevelopPaths.clear();
    req.codeBranch.clear();
    req.codeCheckError.clear();
    req.codeInDevelop = true;   // nothing cited: nothing to verify, and nothing claimed
    if (forceCodeCheck || implemented) {
        const QStringList paths = RequirementsValidator::citedCodePaths(req);
        if (!paths.isEmpty())
            verifyCode(req, repoRoots, paths);
    }
    req.doneCandidate = isDoneCandidate(req);
}

} // namespace Daqster
