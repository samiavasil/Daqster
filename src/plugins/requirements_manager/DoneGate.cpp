#include "DoneGate.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include "RequirementsValidator.h"

namespace Daqster {

namespace {

constexpr char kReviewSubdir[] = "DevelopmentProcess/plans/reviews";

QString readAll(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QTextStream in(&file);
    return in.readAll();
}

} // namespace

QStringList DoneGate::citedPaths(const Requirement &req)
{
    // One extraction rule for the manager, the gate and the traceability CLI
    // (RequirementsValidator::citedCodePaths).
    return RequirementsValidator::citedCodePaths(req);
}

void DoneGate::evaluateLocal(Requirement &req, const QVector<RequirementRoot> &repoRoots)
{
    req.allCriteriaDone = !req.criteriaDone.isEmpty()
                          && !req.criteriaDone.contains(false);
    req.reviewRecord = findReviewRecord(repoRoots, req.id);
    req.reviewVerdict = req.reviewRecord.isEmpty()
                            ? QString()
                            : lastReviewVerdict(readAll(req.reviewRecord));
    req.acceptingReview = !req.reviewRecord.isEmpty()
                          && isAcceptingVerdict(req.reviewVerdict);

    // The code half is NOT answered here: git is a process and a wait inside the
    // plugin is forbidden (check_no_event_loops.cmake). A cited path leaves the
    // half pending — pending refuses, it never reads as "in develop".
    req.missingDevelopPaths.clear();
    req.codeBranch.clear();
    req.codeCheckError.clear();
    const bool cites = !citedPaths(req).isEmpty();
    req.codeInDevelop = !cites;   // nothing cited: nothing to verify, nothing claimed
    req.codeCheckPending = cites; // something cited: DoneGateCheck will answer
    req.doneCandidate = isDoneCandidate(req);
}

void DoneGate::applyCodeAnswer(Requirement &req, const CodeAnswer &answer)
{
    const QStringList cited = citedPaths(req);
    if (!answer.answered || answer.paths != cited) {
        // No answer yet, or an answer about a path list the file no longer
        // cites: refusing is the only safe reading of "not known".
        req.codeInDevelop = false;
        req.codeCheckPending = !cited.isEmpty();
        req.doneCandidate = isDoneCandidate(req);
        return;
    }

    req.codeCheckPending = false;
    req.missingDevelopPaths.clear();
    req.codeBranch.clear();
    req.codeCheckError.clear();
    if (cited.isEmpty()) {
        req.codeInDevelop = true;
    } else if (!answer.error.isEmpty()) {
        // A check that could not run is a refusal with the reason attached, not
        // the silence that reads as "everything resolves" (REQ-SW-PL-107 AC2).
        req.codeInDevelop = false;
        req.codeCheckError = answer.error;
    } else if (answer.inDevelop) {
        req.codeInDevelop = true;
    } else if (answer.missingPaths.isEmpty()) {
        req.codeInDevelop = false;
        req.codeCheckError = QStringLiteral("проверката не върна нито един липсващ път");
    } else {
        req.codeInDevelop = false;
        req.missingDevelopPaths = answer.missingPaths;
        req.codeBranch = answer.branch;
    }
    req.doneCandidate = isDoneCandidate(req);
}

DoneGate::CodeAnswer DoneGate::codeAnswerOf(const Requirement &req)
{
    CodeAnswer answer;
    answer.paths = citedPaths(req);
    answer.answered = !req.codeCheckPending;
    answer.inDevelop = req.codeInDevelop;
    answer.missingPaths = req.missingDevelopPaths;
    answer.branch = req.codeBranch;
    answer.error = req.codeCheckError;
    return answer;
}

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
    if (req.codeCheckPending)
        return QStringLiteral("проверката дали кодът е в `develop` още тече");
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

} // namespace Daqster
