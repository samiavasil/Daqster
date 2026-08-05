#include "RequirementsParser.h"

#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>
#include <QFile>

#include "LogCategories.h"

namespace Daqster {

namespace {

// True when every character of s is a digit (and s is non-empty). Used by
// generateNextId() to enforce an exact prefix-family match: the remainder
// after "REQ-<PREFIX>-" must be a pure number so typed families (REQ-SW-PL-*,
// REQ-SW-FW-*, ...) never pollute each other's max+1 scan.
bool numberStrIsDigits(const QString &s)
{
    if (s.isEmpty())
        return false;
    for (const QChar &c : s) {
        if (!c.isDigit())
            return false;
    }
    return true;
}

// Matches a reference with a trailing parenthesized annotation, e.g.
//   "REQ-SW-PL-013 (публично)"  ->  bare "REQ-SW-PL-013", hint "публично"
// Group 1 = bare ID (no spaces), group 2 = annotation text.
const QRegularExpression &annotationRe()
{
    static const QRegularExpression re(
        QStringLiteral("^([^\\s(]+)\\s*\\(([^()]*)\\)$"));
    return re;
}

// Splits one "Родител:" / "Зависи от:" item into a bare ID and (when present)
// its annotation text. The bare ID is what references are resolved by; the
// annotation is preserved in Requirement::dependencyHints for repo checks.
QString bareReferenceId(const QString &item, QString *hintOut)
{
    const QRegularExpressionMatch match = annotationRe().match(item.trimmed());
    if (!match.hasMatch()) {
        if (hintOut)
            hintOut->clear();
        return item.trimmed();
    }
    if (hintOut)
        *hintOut = match.captured(2).trimmed();
    return match.captured(1).trimmed();
}

// Appends a reference to a list of bare IDs and, when an annotation was
// present, records the hint keyed by the bare ID.
void appendReference(const QString &item, QStringList &bareIds,
                     QHash<QString, QString> &hints)
{
    const QString trimmed = item.trimmed();
    if (trimmed.isEmpty() || trimmed == QStringLiteral("—"))
        return;
    QString hint;
    const QString bare = bareReferenceId(trimmed, &hint);
    if (bare.isEmpty() || bare == QStringLiteral("—"))
        return;
    if (!hint.isEmpty() && !hints.contains(bare))
        hints.insert(bare, hint);
    bareIds.append(bare);
}

QString sectionOf(const QString &baseDir, const QString &absolutePath)
{
    const QString dir = QDir(baseDir).absolutePath();
    const QString sep = QDir::separator();
    // baseDir may point at the repo root (contains DevelopmentProcess/
    // requirements) or at the DevelopmentProcess/requirements/ directory itself.
    const QStringList candidates = {
        dir + sep + QString::fromUtf8(kRequirementsSubdir),
        dir
    };
    for (const QString &candidate : candidates) {
        const QString base = candidate + sep;
        if (absolutePath.startsWith(base + QStringLiteral("active")))
            return QStringLiteral("active");
        if (absolutePath.startsWith(base + QStringLiteral("archive")))
            return QStringLiteral("archive");
    }
    return QString();
}

void collectMarkdownFiles(const QString &dirPath, QVector<QFileInfo> &out)
{
    QDir dir(dirPath);
    const QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QFileInfo &entry : entries) {
        if (entry.isDir()) {
            collectMarkdownFiles(entry.absoluteFilePath(), out);
        } else if (entry.suffix().compare(QStringLiteral("md"), Qt::CaseInsensitive) == 0) {
            out.append(entry);
        }
    }
}

} // namespace

QVector<Requirement> RequirementsParser::parseDirectory(const QString &baseDir)
{
    QVector<Requirement> result;
    QVector<QFileInfo> files;
    collectMarkdownFiles(baseDir, files);

    for (const QFileInfo &fileInfo : files) {
        const QString section = sectionOf(baseDir, fileInfo.absoluteFilePath());
        Requirement req;
        if (parseFile(fileInfo, section, req)) {
            result.append(req);
        }
    }

    std::sort(result.begin(), result.end(),
              [](const Requirement &a, const Requirement &b) {
                  return a.id < b.id;
              });

    return result;
}

QString RequirementsParser::repoForId(const QString &id)
{
    // The typed public scheme is REQ-SW-<TYPE>-<NN>; everything else under
    // REQ-<PREFIX>-<NN> is treated as private (PLG/AI/SEC/DOC...).
    if (id.startsWith(QStringLiteral("REQ-SW-"), Qt::CaseInsensitive))
        return QStringLiteral("public");
    if (id.startsWith(QStringLiteral("REQ-"), Qt::CaseInsensitive))
        return QStringLiteral("private");
    return QStringLiteral("other");
}

QVector<Requirement> RequirementsParser::parseDirectories(const QVector<RequirementRoot> &roots)
{
    QVector<Requirement> result;
    // Roots are deduplicated by canonical path (discoverRepoRoots), but the
    // same physical file can still be reached via multiple roots — e.g. a root
    // at the repo top level plus a root at DevelopmentProcess/requirements, or
    // two sibling roots where one is a symlink. Dedup by canonical FILE path so
    // each file is parsed exactly once.
    QSet<QString> seenFiles;
    for (const RequirementRoot &root : roots) {
        QVector<Requirement> parsed = parseDirectory(root.repoRoot);
        for (Requirement &req : parsed) {
            const QFileInfo fileInfo(req.filePath);
            const QString canonical = fileInfo.canonicalFilePath();
            const QString key = canonical.isEmpty() ? fileInfo.absoluteFilePath()
                                                    : canonical;
            if (seenFiles.contains(key))
                continue;
            seenFiles.insert(key);
            req.repo = repoForId(req.id);
            result.append(req);
        }
    }

    // Stable sort so ties inside one ID family keep their per-root order
    // (parseDirectory already sorts by ID within a root).
    std::stable_sort(result.begin(), result.end(),
                     [](const Requirement &a, const Requirement &b) {
                         const int byId = QString::compare(a.id, b.id, Qt::CaseInsensitive);
                         if (byId != 0)
                             return byId < 0;
                         return QString::compare(a.repo, b.repo, Qt::CaseInsensitive) < 0;
                     });
    return result;
}

QStringList RequirementsParser::discoverRepoRoots()
{
    QStringList roots;

    // 1. Walk up from the application binary to the primary root (the first
    //    directory containing DevelopmentProcess/requirements) — the
    //    historical single-root discovery.
    QDir dir(QCoreApplication::applicationDirPath());
    while (!QDir(dir.filePath(QString::fromUtf8(kRequirementsSubdir))).exists()
           && dir.cdUp()) {
    }
    const QString primary =
        QDir(dir.filePath(QString::fromUtf8(kRequirementsSubdir))).exists()
        ? dir.absolutePath()
        : QCoreApplication::applicationDirPath();
    roots.append(primary);

    // 2. Scan the parent directory of the primary root for SIBLING
    //    directories that also contain DevelopmentProcess/requirements.
    QDir parent(primary);
    if (parent.cdUp()) {
        const QFileInfoList entries =
            parent.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &entry : entries) {
            if (QDir(entry.absoluteFilePath())
                    .exists(QString::fromUtf8(kRequirementsSubdir))) {
                roots.append(entry.absoluteFilePath());
            }
        }
    }

    // 3. Deduplicate canonical absolute paths and sort.
    QStringList canonical;
    QSet<QString> seen;
    for (const QString &root : roots) {
        const QString canon = QDir(root).canonicalPath();
        if (canon.isEmpty() || seen.contains(canon))
            continue;
        seen.insert(canon);
        canonical.append(canon);
    }
    std::sort(canonical.begin(), canonical.end());
    return canonical;
}

bool RequirementsParser::writeRequirement(const Requirement &req)
{
    QFile file(req.filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qCWarning(lcFramework) << "RequirementsParser: cannot write" << req.filePath;
        return false;
    }
    QTextStream out(&file);
    out << req.rawContent;
    file.close();
    return true;
}

void RequirementsParser::setCriterionChecked(Requirement &req, int index, bool done)
{
    if (index < 0 || index >= req.acceptanceCriteria.size())
        return;

    QStringList lines = req.rawContent.split(QStringLiteral("\n"));
    const QString marker = QStringLiteral("- [");

    int matched = 0;
    for (QString &line : lines) {
        if (line.startsWith(marker)) {
            if (matched == index) {
                line = QStringLiteral("- [%1] %2")
                           .arg(done ? QStringLiteral("x") : QStringLiteral(" "))
                           .arg(req.acceptanceCriteria.at(index));
                req.criteriaDone[index] = done;
                break;
            }
            ++matched;
        }
    }
    req.rawContent = lines.join(QStringLiteral("\n"));
}

void RequirementsParser::setStatusLine(Requirement &req, const QString &status)
{
    QStringList lines = req.rawContent.split(QStringLiteral("\n"));
    bool found = false;
    QStringList updated;
    for (const QString &line : lines) {
        if (!found && line.trimmed().startsWith(QStringLiteral("- **Статус:**"))) {
            updated.append(QStringLiteral("- **Статус:** %1").arg(status));
            found = true;
        } else {
            updated.append(line);
        }
    }
    if (!found) {
        int insertAt = 0;
        while (insertAt < updated.size()
               && !updated.at(insertAt).trimmed().startsWith(QStringLiteral("## ")))
            ++insertAt;
        updated.insert(insertAt, QStringLiteral("- **Статус:** %1").arg(status));
    }
    req.rawContent = updated.join(QStringLiteral("\n"));
    req.status = status;
}

void RequirementsParser::setDependenciesLine(Requirement &req,
                                             const QStringList &dependencies)
{
    QStringList valueParts;
    for (const QString &dep : dependencies) {
        if (dep.isEmpty() || dep == QStringLiteral("—"))
            continue;
        valueParts.append(dep);
    }
    const QString newLine = valueParts.isEmpty()
        ? QStringLiteral("- **Зависи от:** —")
        : QStringLiteral("- **Зависи от:** %1").arg(valueParts.join(QStringLiteral(", ")));

    QStringList lines = req.rawContent.split(QStringLiteral("\n"));
    bool found = false;
    QStringList updated;
    for (const QString &line : lines) {
        if (!found && line.trimmed().startsWith(QStringLiteral("- **Зависи от:**"))) {
            updated.append(newLine);
            found = true;
        } else {
            updated.append(line);
        }
    }
    if (!found) {
        int insertAt = 0;
        while (insertAt < updated.size()
               && !updated.at(insertAt).trimmed().startsWith(QStringLiteral("## ")))
            ++insertAt;
        updated.insert(insertAt, newLine);
    }
    req.rawContent = updated.join(QStringLiteral("\n"));
    req.dependencies = valueParts;
}

QString RequirementsParser::generateNextId(const QString &baseDir, const QString &prefix)
{
    const QString prefixStr = QStringLiteral("REQ-%1-").arg(prefix);
    const QString padded = QStringLiteral("%1").arg(0, 3, 10, QLatin1Char('0'));
    int maxNumber = padded.toInt();

    const QVector<Requirement> requirements = parseDirectory(baseDir);
    for (const Requirement &req : requirements) {
        if (!req.id.startsWith(prefixStr))
            continue;
        const QString numberStr = req.id.mid(prefixStr.length());
        // Exact prefix-family match: the remainder after "REQ-<PREFIX>-" must
        // be a pure number. This keeps typed families isolated — e.g. a query
        // for "SW-PL" counts "REQ-SW-PL-001" but never "REQ-SW-FW-001", and a
        // query for "SW" never counts "REQ-SW-PL-001".
        if (numberStr.isEmpty() || !numberStrIsDigits(numberStr))
            continue;
        bool ok = false;
        const int number = numberStr.toInt(&ok);
        if (ok && number > maxNumber)
            maxNumber = number;
    }
    return QStringLiteral("REQ-%1-%2")
        .arg(prefix, QStringLiteral("%1").arg(maxNumber + 1, 3, 10, QLatin1Char('0')));
}

QString RequirementsParser::activeDirectory(const QString &baseDir)
{
    const QString reqDir =
        QDir(baseDir).absoluteFilePath(QString::fromUtf8(kRequirementsSubdir));
    if (QDir(reqDir).exists())
        return QDir(reqDir).filePath(QStringLiteral("active"));
    return QDir(baseDir).filePath(QStringLiteral("active"));
}

QString RequirementsParser::archiveDirectory(const QString &baseDir)
{
    const QString reqDir =
        QDir(baseDir).absoluteFilePath(QString::fromUtf8(kRequirementsSubdir));
    if (QDir(reqDir).exists())
        return QDir(reqDir).filePath(QStringLiteral("archive"));
    return QDir(baseDir).filePath(QStringLiteral("archive"));
}

bool RequirementsParser::moveToArchive(const QString &filePath)
{
    const QFileInfo info(filePath);
    const QStringList dirParts = info.absolutePath().split(QDir::separator());
    const int activeIndex = dirParts.lastIndexOf(QStringLiteral("active"));
    if (activeIndex < 0)
        return false; // file is not inside an active/ directory

    // Replace the "active" path segment with "archive", preserving any nested
    // subdirectory: <...>/active/framework/ -> <...>/archive/framework/.
    QStringList targetParts = dirParts;
    targetParts[activeIndex] = QStringLiteral("archive");
    const QString archiveDir = targetParts.join(QDir::separator());
    QDir().mkpath(archiveDir);
    const QString target = QDir(archiveDir).filePath(info.fileName());
    if (target == filePath)
        return false;
    if (QFile::exists(target) && !QFile::remove(target))
        return false;
    return QFile::rename(filePath, target);
}

bool RequirementsParser::moveToActive(const QString &filePath)
{
    const QFileInfo info(filePath);
    const QStringList dirParts = info.absolutePath().split(QDir::separator());
    const int archiveIndex = dirParts.lastIndexOf(QStringLiteral("archive"));
    if (archiveIndex < 0)
        return false; // file is not inside an archive/ directory

    // Replace the "archive" path segment with "active", preserving any nested
    // subdirectory: <...>/archive/framework/ -> <...>/active/framework/.
    QStringList targetParts = dirParts;
    targetParts[archiveIndex] = QStringLiteral("active");
    const QString activeDir = targetParts.join(QDir::separator());
    QDir().mkpath(activeDir);
    const QString target = QDir(activeDir).filePath(info.fileName());
    if (target == filePath)
        return false;
    if (QFile::exists(target) && !QFile::remove(target))
        return false;
    return QFile::rename(filePath, target);
}

bool RequirementsParser::parseFile(const QFileInfo &fileInfo, const QString &section,
                                   Requirement &out)
{
    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    out.rawContent = in.readAll();
    file.close();

    out.filePath = fileInfo.absoluteFilePath();
    out.fileName = fileInfo.fileName();
    out.section = section;

    const QStringList lines = out.rawContent.split(QStringLiteral("\n"));

    // Title: "# REQ-FW-001: Some Title" (3-segment) or
    //        "# REQ-SW-PL-001: Some Title" (typed, 4-segment)
    // Capture groups: 1 = full ID, 2 = optional type segment, 3 = title.
    const QRegularExpression titleRe(
        QStringLiteral("^#\\s+(REQ-[A-Z]+(-[A-Z]+)?-\\d{3})\\s*:\\s*(.*)$"));
    // Metadata: "- **Статус:** ACTIVE" (bold wraps "Ключ:" together)
    const QRegularExpression metaRe(QStringLiteral("^\\s*-\\s*\\*\\*([^*]+):\\*\\*\\s*(.*)$"));
    // Acceptance criterion: "- [ ] text" / "- [x] text"
    const QRegularExpression criterionRe(QStringLiteral("^\\s*-\\s*\\[([ xX])\\]\\s*(.*)$"));

    bool inDescription = false;
    bool inTraceability = false;
    QStringList descriptionLines;
    QStringList traceabilityLines;

    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("## "))) {
            inDescription = (trimmed == QStringLiteral("## Описание"));
            inTraceability = (trimmed == QStringLiteral("## Проследимост"));
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("### ")))
            continue;

        const QRegularExpressionMatch titleMatch = titleRe.match(line);
        if (titleMatch.hasMatch()) {
            out.id = titleMatch.captured(1);
            out.title = titleMatch.captured(3).trimmed(); // group 2 = optional type
            continue;
        }

        const QRegularExpressionMatch metaMatch = metaRe.match(line);
        if (metaMatch.hasMatch()) {
            const QString key = metaMatch.captured(1).trimmed();
            const QString value = metaMatch.captured(2).trimmed();
            bool handled = true;
            if (key == QStringLiteral("Статус"))
                out.status = value;
            else if (key == QStringLiteral("Приоритет"))
                out.priority = value;
            else if (key.contains(QStringLiteral("Отговорник")))
                out.assignee = value;
            else if (key == QStringLiteral("Дата"))
                out.date = value;
            else if (key == QStringLiteral("Родител")) {
                QString hint;
                const QString bare = bareReferenceId(value, &hint);
                out.parentId = (bare == QStringLiteral("—")) ? QString() : bare;
                if (!bare.isEmpty() && bare != QStringLiteral("—") && !hint.isEmpty())
                    out.dependencyHints.insert(bare, hint);
            }
            else if (key == QStringLiteral("Зависи от")) {
                out.dependencies.clear();
                const QStringList parts = value.split(QLatin1Char(','));
                for (const QString &part : parts)
                    appendReference(part, out.dependencies, out.dependencyHints);
            } else {
                handled = false; // unknown metadata (e.g. under Проследимост)
                // Traceability keys are captured into structured fields BUT
                // deliberately left with handled == false: their lines must
                // also stay part of the raw traceability text below.
                if (key == QStringLiteral("Коммити"))
                    out.commits = value;
                else if (key == QStringLiteral("Код"))
                    out.code = value;
                else if (key == QStringLiteral("Тестове"))
                    out.tests = value;
                else if (key == QStringLiteral("Документация"))
                    out.docs = value;
            }
            if (handled)
                continue;
        }

        const QRegularExpressionMatch criterionMatch = criterionRe.match(line);
        if (criterionMatch.hasMatch()) {
            out.acceptanceCriteria.append(criterionMatch.captured(2).trimmed());
            out.criteriaDone.append(criterionMatch.captured(1).toLower() == QStringLiteral("x"));
            continue;
        }

        if (inDescription)
            descriptionLines.append(line);
        else if (inTraceability)
            traceabilityLines.append(line);
    }

    out.description = descriptionLines.join(QStringLiteral("\n")).trimmed();
    out.traceability = traceabilityLines.join(QStringLiteral("\n")).trimmed();
    return !out.id.isEmpty();
}

PhaseStatus phaseStatus(const Requirement &req)
{
    PhaseStatus result;
    result.architecture = !req.docs.trimmed().isEmpty()
                          && req.docs.trimmed() != QStringLiteral("—");
    result.implementation = !req.code.trimmed().isEmpty()
                            && req.code.trimmed() != QStringLiteral("—");
    result.testing = !req.tests.trimmed().isEmpty()
                     && req.tests.trimmed() != QStringLiteral("—");
    return result;
}

} // namespace Daqster
