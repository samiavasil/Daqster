#include "RequirementsParser.h"

#include <QDir>
#include <QRegularExpression>
#include <QTextStream>
#include <QFile>

#include "LogCategories.h"

namespace Daqster {

namespace {

QString sectionOf(const QString &baseDir, const QString &absolutePath)
{
    const QString dir = QDir(baseDir).absolutePath();
    const QString sep = QDir::separator();
    // baseDir may point at the repo root (contains requirements/) or at the
    // requirements/ directory itself.
    const QStringList candidates = {
        dir + sep + QStringLiteral("requirements"),
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
    const QString reqDir = QDir(baseDir).absoluteFilePath(QStringLiteral("requirements"));
    if (QDir(reqDir).exists())
        return QDir(reqDir).filePath(QStringLiteral("active"));
    return QDir(baseDir).filePath(QStringLiteral("active"));
}

QString RequirementsParser::archiveDirectory(const QString &baseDir)
{
    const QString reqDir = QDir(baseDir).absoluteFilePath(QStringLiteral("requirements"));
    if (QDir(reqDir).exists())
        return QDir(reqDir).filePath(QStringLiteral("archive"));
    return QDir(baseDir).filePath(QStringLiteral("archive"));
}

bool RequirementsParser::moveToArchive(const QString &filePath)
{
    const QFileInfo info(filePath);
    const QString dir = info.absolutePath();
    const QString sep = QDir::separator();
    if (!dir.endsWith(sep + QStringLiteral("active")))
        return false;

    const QString archiveDir =
        dir.left(dir.length() - QStringLiteral("active").length())
        + QStringLiteral("archive");
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
    const QString dir = info.absolutePath();
    const QString sep = QDir::separator();
    if (!dir.endsWith(sep + QStringLiteral("archive")))
        return false;

    const QString activeDir =
        dir.left(dir.length() - QStringLiteral("archive").length())
        + QStringLiteral("active");
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

    // Title: "# REQ-FW-001: Some Title"
    const QRegularExpression titleRe(QStringLiteral("^#\\s+(REQ-[A-Z]+-\\d+)\\s*:\\s*(.*)$"));
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
            out.title = titleMatch.captured(2).trimmed();
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
            else if (key == QStringLiteral("Родител"))
                out.parentId = (value == QStringLiteral("—")) ? QString() : value;
            else if (key == QStringLiteral("Зависи от")) {
                out.dependencies.clear();
                const QStringList parts = value.split(QLatin1Char(','));
                for (const QString &part : parts) {
                    const QString dep = part.trimmed();
                    if (dep.isEmpty() || dep == QStringLiteral("—"))
                        continue;
                    out.dependencies.append(dep);
                }
            } else {
                handled = false; // unknown metadata (e.g. under Проследимост)
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

} // namespace Daqster
