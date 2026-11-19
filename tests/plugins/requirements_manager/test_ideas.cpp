#include <QtTest>
#include <QTemporaryDir>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableView>
#include <QTextBrowser>
#include <QTimer>

#include "IdeaReader.h"
#include "IdeaModel.h"
#include "RequirementsWidget.h"
#include "RequirementsParser.h"
#include "RequirementsValidator.h"

using namespace Daqster;

namespace {

//!< Writes one file, creating its directory.
QString writeFile(const QString &filePath, const QString &content)
{
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();
    file.write(content.toUtf8());
    file.close();
    return filePath;
}

//!< A requirement file the parser and validator both accept.
QString requirementMarkdown(const QString &id, const QString &status)
{
    return QStringLiteral("# %1: Fixture requirement\n"
                          "\n"
                          "- **Статус:** %2\n"
                          "- **Приоритет:** High\n"
                          "- **Отговорник (роля):** Implementation\n"
                          "- **Дата:** 2026-11-16\n"
                          "- **Родител:** —\n"
                          "- **Зависи от:** —\n"
                          "\n"
                          "## Описание\n"
                          "\n"
                          "Описание.\n"
                          "\n"
                          "## Acceptance Criteria\n"
                          "\n"
                          "- [ ] един критерий\n"
                          "\n"
                          "## Проследимост\n"
                          "\n"
                          "- **Коммити:** —\n")
        .arg(id, status);
}

QString ideasDirFor(const QString &root)
{
    return QDir(root).filePath(QStringLiteral("DevelopmentProcess/plans/ideas"));
}

QString activeDirFor(const QString &root)
{
    return QDir(root).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
}

//!< Builds a repo root with one active requirement.
QString makeRequirementsRoot(const QString &root)
{
    writeFile(QDir(activeDirFor(root)).filePath(QStringLiteral("REQ-SW-PL-900-fixture.md")),
              requirementMarkdown(QStringLiteral("REQ-SW-PL-900"), QStringLiteral("ACTIVE")));
    return root;
}

//!< Three idea files with three different header shapes + one malformed file.
void writeIdeasTree(const QString &root)
{
    const QString dir = ideasDirFor(root);

    // Shape 1: plain keys, `·` separators.
    writeFile(QDir(dir).filePath(QStringLiteral("IDEA-001-zeta.md")),
              QStringLiteral("# IDEA-001 — Zeta\n"
                             "\n"
                             "> Статус: идея · Добавена: 2026-10-01 · Следващ преглед: 2027-01-01\n"
                             "\n"
                             "## Защо\n"
                             "\n"
                             "Тяло на идеята Zeta.\n"));

    // Shape 2: bold keys, `|` separators.
    writeFile(QDir(dir).filePath(QStringLiteral("IDEA-002-alpha.md")),
              QStringLiteral("# IDEA-002 — Alpha\n"
                             "\n"
                             "> **Статус:** узряла | **Добавена:** 2026-10-02 | **Следващ преглед:** 2027-02-02\n"
                             "\n"
                             "## Защо\n"
                             "\n"
                             "Тяло на идеята Alpha.\n"));

    // Shape 3: fields in another order, mixed separators.
    writeFile(QDir(dir).filePath(QStringLiteral("IDEA-003-mu.md")),
              QStringLiteral("# IDEA-003 — Mu\n"
                             "\n"
                             "> Добавена: 2026-10-03 | Статус: паркирана · Следващ преглед: 2027-03-03\n"
                             "\n"
                             "## Защо\n"
                             "\n"
                             "Тяло на идеята Mu.\n"));

    // Malformed: announces an idea in its name, has no `# ` title → skipped.
    writeFile(QDir(dir).filePath(QStringLiteral("IDEA-004-broken.md")),
              QStringLiteral("Този файл няма заглавие.\n"));

    // Not an idea at all (the register's README) → skipped quietly.
    writeFile(QDir(dir).filePath(QStringLiteral("README.md")),
              QStringLiteral("# Идеи — регистър\n"
                             "\n"
                             "> Статус: завършен · Обновено: 2026-11-16\n"));
}

//!< Polls for the modal dialog a slot opens, then saves or rejects it.
void answerNextDialog(bool accept)
{
    QTimer *timer = new QTimer(qApp);
    timer->setInterval(10);
    QObject::connect(timer, &QTimer::timeout, timer, [timer, accept]() {
        QDialog *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        timer->stop();
        timer->deleteLater();
        if (!accept) {
            dialog->reject();
            return;
        }
        // "Create Requirement" is the dialog box's Save button; clicking it is
        // the real accept path (it writes the file before accepting).
        if (QDialogButtonBox *box = dialog->findChild<QDialogButtonBox *>()) {
            if (QPushButton *save = box->button(QDialogButtonBox::Save)) {
                save->click();
                return;
            }
        }
        dialog->accept();
    });
    timer->start();
}

QString readAll(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(file.readAll());
}

} // namespace

/**
 * @brief Tests for the read-only "Идеи" panel (REQ-SW-PL-095).
 *
 * The panel is structurally outside the requirements pipeline: these tests sit
 * in their own binary precisely because they prove it stays that way.
 */
class TestIdeas : public QObject
{
    Q_OBJECT

private slots:
    void readerParsesHeaderShapesAndSkipsMalformed();
    void readerCountsAttachments();
    void ideasTabHiddenWithoutIdeas();
    void ideasTabShowsRowsAndSorts();
    void ideasSearchFilters();
    void promotionWritesRequirementAndPromotionLine();
    void promotionCancelWritesNothing();
    void validatorCountsUnchangedWithIdeas();
};

void TestIdeas::readerParsesHeaderShapesAndSkipsMalformed()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = dir.filePath(QStringLiteral("repo"));
    writeIdeasTree(root);

    const QVector<Idea> ideas = IdeaReader::readFromRoots({root});
    QCOMPARE(ideas.size(), 3); // IDEA-004 (malformed) and README are skipped

    QCOMPARE(ideas.at(0).id, QStringLiteral("IDEA-001"));
    QCOMPARE(ideas.at(0).title, QStringLiteral("Zeta"));
    QCOMPARE(ideas.at(0).status, QStringLiteral("идея"));
    QCOMPARE(ideas.at(0).added, QStringLiteral("2026-10-01"));
    QCOMPARE(ideas.at(0).nextReview, QStringLiteral("2027-01-01"));

    QCOMPARE(ideas.at(1).id, QStringLiteral("IDEA-002"));
    QCOMPARE(ideas.at(1).title, QStringLiteral("Alpha"));
    QCOMPARE(ideas.at(1).status, QStringLiteral("узряла"));
    QCOMPARE(ideas.at(1).added, QStringLiteral("2026-10-02"));
    QCOMPARE(ideas.at(1).nextReview, QStringLiteral("2027-02-02"));

    QCOMPARE(ideas.at(2).id, QStringLiteral("IDEA-003"));
    QCOMPARE(ideas.at(2).title, QStringLiteral("Mu"));
    QCOMPARE(ideas.at(2).status, QStringLiteral("паркирана"));
    QCOMPARE(ideas.at(2).added, QStringLiteral("2026-10-03"));
    QCOMPARE(ideas.at(2).nextReview, QStringLiteral("2027-03-03"));

    // The body is kept for the preview (single read, no second parser).
    QVERIFY(ideas.at(0).rawContent.contains(QStringLiteral("Тяло на идеята Zeta.")));
}

void TestIdeas::readerCountsAttachments()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = dir.filePath(QStringLiteral("repo"));
    writeIdeasTree(root);

    const QString attachments =
        QDir(ideasDirFor(root)).filePath(QStringLiteral("IDEA-001/attachments"));
    writeFile(QDir(attachments).filePath(QStringLiteral("spec.md")), QStringLiteral("a"));
    writeFile(QDir(attachments).filePath(QStringLiteral("drafts/plan.md")), QStringLiteral("b"));

    const QVector<Idea> ideas = IdeaReader::readFromRoots({root});
    QCOMPARE(ideas.size(), 3);
    QCOMPARE(ideas.at(0).attachmentCount, 2);
    QCOMPARE(ideas.at(1).attachmentCount, 0);
    QCOMPARE(ideas.at(2).attachmentCount, 0);
}

void TestIdeas::ideasTabHiddenWithoutIdeas()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir.filePath(QStringLiteral("repo")));

    RequirementsWidget widget;
    widget.openDirectory(root);

    QTabWidget *tabs = widget.findChild<QTabWidget *>();
    QVERIFY(tabs);
    const int index = tabs->indexOf(widget.findChild<QWidget *>(QStringLiteral("ideasPanel")));
    QVERIFY(index >= 0);
    QVERIFY(!tabs->isTabVisible(index));
}

void TestIdeas::ideasTabShowsRowsAndSorts()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir.filePath(QStringLiteral("repo")));
    writeIdeasTree(root);

    RequirementsWidget widget;
    widget.openDirectory(root);

    QTabWidget *tabs = widget.findChild<QTabWidget *>();
    QVERIFY(tabs);
    const int index = tabs->indexOf(widget.findChild<QWidget *>(QStringLiteral("ideasPanel")));
    QVERIFY(index >= 0);
    QVERIFY(tabs->isTabVisible(index));

    QTableView *table = widget.findChild<QTableView *>(QStringLiteral("ideasTable"));
    QVERIFY(table);
    QAbstractItemModel *model = table->model();
    QCOMPARE(model->rowCount(), 3);
    QCOMPARE(model->columnCount(), int(IdeaTableModel::ColumnCount));
    QVERIFY(table->isSortingEnabled());

    auto column = [model](int row, int col) {
        return model->index(row, col).data(Qt::DisplayRole).toString();
    };

    // Sorted by ID: the model's register order.
    QCOMPARE(column(0, IdeaTableModel::IdColumn), QStringLiteral("IDEA-001"));
    QCOMPARE(column(2, IdeaTableModel::IdColumn), QStringLiteral("IDEA-003"));

    // Titles are sorted independently of the IDs.
    table->sortByColumn(IdeaTableModel::TitleColumn, Qt::AscendingOrder);
    QCOMPARE(column(0, IdeaTableModel::IdColumn), QStringLiteral("IDEA-002")); // Alpha
    QCOMPARE(column(1, IdeaTableModel::IdColumn), QStringLiteral("IDEA-003")); // Mu
    QCOMPARE(column(2, IdeaTableModel::IdColumn), QStringLiteral("IDEA-001")); // Zeta

    table->sortByColumn(IdeaTableModel::StatusColumn, Qt::AscendingOrder);
    QCOMPARE(column(0, IdeaTableModel::StatusColumn), QStringLiteral("идея"));
    QCOMPARE(column(1, IdeaTableModel::StatusColumn), QStringLiteral("паркирана"));
    QCOMPARE(column(2, IdeaTableModel::StatusColumn), QStringLiteral("узряла"));
}

void TestIdeas::ideasSearchFilters()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir.filePath(QStringLiteral("repo")));
    writeIdeasTree(root);

    RequirementsWidget widget;
    widget.openDirectory(root);

    QTableView *table = widget.findChild<QTableView *>(QStringLiteral("ideasTable"));
    QLineEdit *search = widget.findChild<QLineEdit *>(QStringLiteral("ideasSearch"));
    QVERIFY(table);
    QVERIFY(search);
    QAbstractItemModel *model = table->model();
    QCOMPARE(model->rowCount(), 3);

    search->setText(QStringLiteral("alpha"));
    QCoreApplication::processEvents();
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(model->index(0, IdeaTableModel::IdColumn).data(Qt::DisplayRole).toString(),
             QStringLiteral("IDEA-002"));

    search->setText(QStringLiteral("IDEA-003"));
    QCoreApplication::processEvents();
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(model->index(0, IdeaTableModel::IdColumn).data(Qt::DisplayRole).toString(),
             QStringLiteral("IDEA-003"));

    search->setText(QStringLiteral("няма такова"));
    QCoreApplication::processEvents();
    QCOMPARE(model->rowCount(), 0);

    search->clear();
    QCoreApplication::processEvents();
    QCOMPARE(model->rowCount(), 3);
}

void TestIdeas::promotionWritesRequirementAndPromotionLine()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir.filePath(QStringLiteral("repo")));
    writeIdeasTree(root);

    const QString ideaPath =
        QDir(ideasDirFor(root)).filePath(QStringLiteral("IDEA-001-zeta.md"));
    const QString ideaBefore = readAll(ideaPath);

    RequirementsWidget widget;
    widget.show();
    widget.openDirectory(root);

    QTableView *table = widget.findChild<QTableView *>(QStringLiteral("ideasTable"));
    QPushButton *promote =
        widget.findChild<QPushButton *>(QStringLiteral("promoteIdeaButton"));
    QVERIFY(table);
    QVERIFY(promote);
    QVERIFY(!promote->isEnabled()); // nothing selected yet

    table->setCurrentIndex(table->model()->index(0, 0));
    QCoreApplication::processEvents();
    QVERIFY(promote->isEnabled());

    const QStringList before = QDir(activeDirFor(root)).entryList(
        {QStringLiteral("*.md")}, QDir::Files, QDir::Name);

    answerNextDialog(true);
    promote->click();

    const QStringList after = QDir(activeDirFor(root)).entryList(
        {QStringLiteral("*.md")}, QDir::Files, QDir::Name);
    QCOMPARE(after.size(), before.size() + 1);

    QString createdPath;
    for (const QString &name : after) {
        if (!before.contains(name))
            createdPath = QDir(activeDirFor(root)).filePath(name);
    }
    QVERIFY(!createdPath.isEmpty());

    const QString created = readAll(createdPath);
    QVERIFY2(created.contains(QStringLiteral("from_idea: IDEA-001")), qPrintable(created));
    QVERIFY(created.contains(QStringLiteral("Zeta")));

    // The idea gained one header line; the rest of the file is untouched.
    const QString ideaAfter = readAll(ideaPath);
    QVERIFY(ideaAfter.contains(QStringLiteral("> **Промотирана:**")));
    QVERIFY(ideaAfter.contains(QStringLiteral("Тяло на идеята Zeta.")));
    QVERIFY(ideaAfter.startsWith(QStringLiteral("# IDEA-001 — Zeta")));
    QVERIFY(ideaAfter.contains(QStringLiteral("2026-10-01")));
    QVERIFY(ideaAfter.size() > ideaBefore.size());
}

void TestIdeas::promotionCancelWritesNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir.filePath(QStringLiteral("repo")));
    writeIdeasTree(root);

    const QString ideaPath =
        QDir(ideasDirFor(root)).filePath(QStringLiteral("IDEA-001-zeta.md"));
    const QString ideaBefore = readAll(ideaPath);
    const QStringList activeBefore = QDir(activeDirFor(root)).entryList(
        {QStringLiteral("*.md")}, QDir::Files, QDir::Name);

    RequirementsWidget widget;
    widget.show();
    widget.openDirectory(root);

    QTableView *table = widget.findChild<QTableView *>(QStringLiteral("ideasTable"));
    QPushButton *promote =
        widget.findChild<QPushButton *>(QStringLiteral("promoteIdeaButton"));
    QVERIFY(table);
    QVERIFY(promote);
    table->setCurrentIndex(table->model()->index(0, 0));
    QCoreApplication::processEvents();

    answerNextDialog(false);
    promote->click();

    QCOMPARE(readAll(ideaPath), ideaBefore);
    QCOMPARE(QDir(activeDirFor(root)).entryList({QStringLiteral("*.md")}, QDir::Files,
                                                QDir::Name),
             activeBefore);
}

void TestIdeas::validatorCountsUnchangedWithIdeas()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir.filePath(QStringLiteral("repo")));

    const QVector<Requirement> withoutIdeas = RequirementsParser::parseDirectory(root);
    const QVector<RequirementsValidator::Issue> issuesWithout =
        RequirementsValidator::validate(withoutIdeas);

    writeIdeasTree(root);

    const QVector<Requirement> withIdeas = RequirementsParser::parseDirectory(root);
    const QVector<RequirementsValidator::Issue> issuesWith =
        RequirementsValidator::validate(withIdeas);

    // The idea files live outside DevelopmentProcess/requirements, so neither
    // the parsed set nor a single validator finding may change.
    QCOMPARE(withIdeas.size(), withoutIdeas.size());
    QCOMPARE(issuesWith.size(), issuesWithout.size());
    QCOMPARE(RequirementsValidator::countSeverity(issuesWith,
                                                  RequirementsValidator::Severity::Error),
             RequirementsValidator::countSeverity(issuesWithout,
                                                  RequirementsValidator::Severity::Error));
}

QTEST_MAIN(TestIdeas)

#include "test_ideas.moc"
