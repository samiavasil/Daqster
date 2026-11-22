#include <QtTest>
#include <QTemporaryDir>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableView>
#include <QTreeView>
#include <QTextBrowser>
#include <QTimer>

#include "IdeaReader.h"
#include "IdeaModel.h"
#include "DependencyGraphWidget.h"
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
    void readerParsesBothPromotionKeys();
    void readerKeepsBothKeysApart();
    void ideasTabHiddenWithoutIdeas();
    void ideasTabShowsRowsAndSorts();
    void ideasSearchFilters();
    void promotionColumnShowsRequirementAndStatus();
    void promotionColumnClickOpensRequirement();
    void requirementPreviewShowsIdeaOrigin();
    void graphDrawsIdeaAsDashedNode();
    void graphKeepsIdeaWhenFilterChanges();
    void managerReportIncludesIdeaLinkError();
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

//!< Writes an idea promoted to @p requirementId (one more row in the fixture).
QString writePromotedIdea(const QString &root, const QString &requirementId)
{
    return writeFile(
        QDir(ideasDirFor(root)).filePath(QStringLiteral("IDEA-005-promoted.md")),
        QStringLiteral("# IDEA-005 — Promoted\n"
                       "\n"
                       "> Статус: промотирана · Добавена: 2026-10-05 · "
                       "Следващ преглед: 2027-05-05\n"
                       "> Промотирана: %1\n"
                       "\n"
                       "## Защо\n"
                       "\n"
                       "Тяло на промотираната идея.\n")
            .arg(requirementId));
}

void TestIdeas::readerParsesBothPromotionKeys()
{
    // REQ-SW-PL-098: one link, one home — the idea's header. The register uses
    // two names for it (IDEA-002 carries "Изискване:", the REQ names
    // "Промотирана:"), and both must feed Idea::promotedId.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir.filePath(QStringLiteral("repo")));
    writeIdeasTree(root);
    writePromotedIdea(root, QStringLiteral("REQ-SW-PL-900"));
    writeFile(QDir(ideasDirFor(root)).filePath(QStringLiteral("IDEA-007-req.md")),
              QStringLiteral("# IDEA-007 — Req key\n"
                             "\n"
                             "> Статус: промотирана · Добавена: 2026-10-07\n"
                             "> Изискване: REQ-SW-PL-901 („промотирана“ 2026-11-22)\n"
                             "\n"
                             "## Защо\n\nТяло.\n"));

    const QVector<Idea> ideas = IdeaReader::readFromRoots({root});
    QCOMPARE(ideas.size(), 5); // IDEA-001..003 + IDEA-005 + IDEA-007
    for (const Idea &idea : ideas) {
        if (idea.id == QStringLiteral("IDEA-005"))
            QCOMPARE(idea.promotedId, QStringLiteral("REQ-SW-PL-900"));
        else if (idea.id == QStringLiteral("IDEA-007"))
            QCOMPARE(idea.promotedId, QStringLiteral("REQ-SW-PL-901"));
        else
            QVERIFY2(idea.promotedId.isEmpty(), qPrintable(idea.id));
    }
}

void TestIdeas::readerKeepsBothKeysApart()
{
    // Review R1-3 (REQ-SW-PL-098): "Промотирана:" and "Изискване:" are two
    // spellings of one link, but a file can carry both — and then one of them is
    // stale. promotedId stays "first key wins" (the tab shows one value); the
    // two fields are what lets the validator report the disagreement instead of
    // dropping the loser in silence.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir.filePath(QStringLiteral("repo")));
    writeFile(QDir(ideasDirFor(root)).filePath(QStringLiteral("IDEA-008-both.md")),
              QStringLiteral("# IDEA-008 — Both keys\n"
                             "\n"
                             "> Статус: промотирана · Добавена: 2026-10-08\n"
                             "> Промотирана: REQ-SW-PL-900\n"
                             "> Изискване: REQ-SW-PL-902\n"
                             "\n"
                             "## Защо\n\nТяло.\n"));

    const QVector<Idea> ideas = IdeaReader::readFromRoots({root});
    QCOMPARE(ideas.size(), 1);
    const Idea &idea = ideas.first();
    QCOMPARE(idea.promotedId, QStringLiteral("REQ-SW-PL-900")); // first in the file
    QCOMPARE(idea.promotedKeyId, QStringLiteral("REQ-SW-PL-900"));
    QCOMPARE(idea.requirementKeyId, QStringLiteral("REQ-SW-PL-902"));
}

void TestIdeas::promotionColumnShowsRequirementAndStatus()
{
    // AC4: `REQ · статус` for a promoted idea, empty for the rest. The empty
    // cell means "not promoted yet", not "broken".
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir.filePath(QStringLiteral("repo")));
    writeIdeasTree(root);
    writePromotedIdea(root, QStringLiteral("REQ-SW-PL-900"));

    RequirementsWidget widget;
    widget.openDirectory(root);

    QTableView *table = widget.findChild<QTableView *>(QStringLiteral("ideasTable"));
    QVERIFY(table);
    QAbstractItemModel *model = table->model();
    QCOMPARE(model->rowCount(), 4);

    auto cell = [model](int row, int col, int role) {
        return model->index(row, col).data(role).toString();
    };
    // Sorted by ID: IDEA-001, 002, 003, 005.
    QCOMPARE(cell(3, IdeaTableModel::IdColumn, Qt::DisplayRole), QStringLiteral("IDEA-005"));
    QCOMPARE(cell(3, IdeaTableModel::RequirementColumn, Qt::DisplayRole),
             QStringLiteral("REQ-SW-PL-900 · ACTIVE"));
    QCOMPARE(cell(3, IdeaTableModel::RequirementColumn, IdeaTableModel::RequirementRole),
             QStringLiteral("REQ-SW-PL-900"));
    QCOMPARE(cell(0, IdeaTableModel::RequirementColumn, Qt::DisplayRole), QString());
    QCOMPARE(cell(0, IdeaTableModel::RequirementColumn, IdeaTableModel::RequirementRole),
             QString());
    // The header names the column; the promoted cell looks like a link.
    QCOMPARE(model->headerData(IdeaTableModel::RequirementColumn, Qt::Horizontal,
                               Qt::DisplayRole).toString(),
             QStringLiteral("Изискване"));
    QVERIFY(model->index(3, IdeaTableModel::RequirementColumn).data(Qt::ForegroundRole).isValid());
    QVERIFY(!model->index(0, IdeaTableModel::RequirementColumn)
                 .data(Qt::ForegroundRole).isValid());
}

void TestIdeas::promotionColumnClickOpensRequirement()
{
    // AC4: "клик отваря изискването" — a click on the cell switches to the
    // requirements tab and selects the requirement there.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir.filePath(QStringLiteral("repo")));
    writeIdeasTree(root);
    writePromotedIdea(root, QStringLiteral("REQ-SW-PL-900"));

    RequirementsWidget widget;
    widget.show();
    widget.openDirectory(root);

    QTabWidget *tabs = widget.findChild<QTabWidget *>();
    QTableView *table = widget.findChild<QTableView *>(QStringLiteral("ideasTable"));
    QTreeView *tree = widget.findChild<QTreeView *>(QStringLiteral("requirementsTree"));
    QVERIFY(tabs);
    QVERIFY(table);
    QVERIFY(tree);
    tabs->setCurrentIndex(tabs->indexOf(widget.findChild<QWidget *>(QStringLiteral("ideasPanel"))));
    QCoreApplication::processEvents();

    const QModelIndex cell = table->model()->index(3, IdeaTableModel::RequirementColumn);
    table->scrollTo(cell);
    QCoreApplication::processEvents();
    const QRect rect = table->visualRect(cell);
    QVERIFY(rect.isValid());
    QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());
    QCoreApplication::processEvents();

    QCOMPARE(tabs->currentIndex(), 0);
    const QModelIndex current = tree->currentIndex();
    QVERIFY(current.isValid());
    QCOMPARE(current.data(Qt::DisplayRole).toString(), QStringLiteral("REQ-SW-PL-900"));

    // A click on a column that is not the link changes nothing.
    const QModelIndex idCell = table->model()->index(0, IdeaTableModel::IdColumn);
    QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                      table->visualRect(idCell).center());
    QCoreApplication::processEvents();
    QCOMPARE(tree->currentIndex().data(Qt::DisplayRole).toString(),
             QStringLiteral("REQ-SW-PL-900"));
}

void TestIdeas::requirementPreviewShowsIdeaOrigin()
{
    // AC5: the requirement's details name the idea it grew out of — read from
    // the idea's header, never written into the requirement's file.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir.filePath(QStringLiteral("repo")));
    RequirementsWidget widget;
    widget.openDirectory(root);
    QTreeView *tree = widget.findChild<QTreeView *>(QStringLiteral("requirementsTree"));
    QTextBrowser *preview =
        widget.findChild<QTextBrowser *>(QStringLiteral("requirementPreview"));
    QVERIFY(tree);
    QVERIFY(preview);

    //!< The tree row of a requirement ID (the tree is sectioned).
    auto treeRow = [tree](const QString &id) {
        QAbstractItemModel *model = tree->model();
        for (int section = 0; section < model->rowCount(); ++section) {
            const QModelIndex sectionIndex = model->index(section, 0);
            for (int row = 0; row < model->rowCount(sectionIndex); ++row) {
                const QModelIndex candidate = model->index(row, 0, sectionIndex);
                if (candidate.data(Qt::DisplayRole).toString() == id)
                    return candidate;
            }
        }
        return QModelIndex();
    };

    // Before the idea exists: the row is there and says "—" instead of hiding.
    QModelIndex target = treeRow(QStringLiteral("REQ-SW-PL-900"));
    QVERIFY(target.isValid());
    tree->setCurrentIndex(target);
    QCoreApplication::processEvents();
    QVERIFY2(preview->document()->toPlainText().contains(QStringLiteral("Произход: —")),
             qPrintable(preview->document()->toPlainText()));

    writeIdeasTree(root);
    writePromotedIdea(root, QStringLiteral("REQ-SW-PL-900"));
    widget.openDirectory(root);
    QCoreApplication::processEvents();

    target = treeRow(QStringLiteral("REQ-SW-PL-900"));
    QVERIFY(target.isValid());
    tree->setCurrentIndex(target);
    QCoreApplication::processEvents();

    const QString plain = preview->document()->toPlainText();
    QVERIFY2(plain.contains(QStringLiteral("Произход: IDEA-005")), qPrintable(plain));
}

void TestIdeas::graphDrawsIdeaAsDashedNode()
{
    // AC6: the idea is a separate (dashed) node joined to its requirement, and
    // the solid nodes stay requirements only.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir.filePath(QStringLiteral("repo")));
    writeIdeasTree(root);
    writePromotedIdea(root, QStringLiteral("REQ-SW-PL-900"));

    RequirementsWidget widget;
    widget.openDirectory(root);

    DependencyGraphWidget *graph = widget.findChild<DependencyGraphWidget *>();
    QVERIFY(graph);
    DependencyGraphScene *scene = graph->graphScene();
    QVERIFY(scene);

    int ideaNodes = 0;
    int requirementNodes = 0;
    for (DependencyGraphNodeItem *item : scene->nodeItems()) {
        if (item->isIdea()) {
            ++ideaNodes;
            QCOMPARE(item->requirementId(), QStringLiteral("IDEA-005"));
        } else {
            ++requirementNodes;
        }
    }
    QCOMPARE(ideaNodes, 1);          // only the promoted idea is drawn
    QCOMPARE(requirementNodes, 1);   // REQ-SW-PL-900 — solid nodes are requirements

    int promotionEdges = 0;
    for (DependencyGraphEdgeItem *edge : scene->edgeItems()) {
        if (edge->kind() != GraphEdge::IdeaPromotion)
            continue;
        ++promotionEdges;
        QCOMPARE(edge->fromNode()->requirementId(), QStringLiteral("IDEA-005"));
        QCOMPARE(edge->toNode()->requirementId(), QStringLiteral("REQ-SW-PL-900"));
    }
    QCOMPARE(promotionEdges, 1);
}

void TestIdeas::graphKeepsIdeaWhenFilterChanges()
{
    // Review R1-1 (REQ-SW-PL-098): re-applying the view filters used to hand the
    // graph an EMPTY idea vector, so every dashed idea node and its promotion
    // edge disappeared on the first filter or search — while the ideas tab still
    // listed them. The graph claimed "no ideas", which is a lie the reader has no
    // reason to doubt.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir.filePath(QStringLiteral("repo")));
    writeIdeasTree(root);
    writePromotedIdea(root, QStringLiteral("REQ-SW-PL-900"));

    RequirementsWidget widget;
    widget.openDirectory(root);

    DependencyGraphWidget *graph = widget.findChild<DependencyGraphWidget *>();
    QVERIFY(graph);
    DependencyGraphScene *scene = graph->graphScene();
    QVERIFY(scene);

    const auto ideaNodeCount = [scene]() {
        int count = 0;
        for (DependencyGraphNodeItem *item : scene->nodeItems()) {
            if (item->isIdea())
                ++count;
        }
        return count;
    };
    QCOMPARE(ideaNodeCount(), 1);   // baseline: the promoted idea is drawn

    // 1. A search that keeps the requirement: the idea stays, because it is the
    //    requirement's origin, not a search hit of its own.
    QLineEdit *search = widget.findChild<QLineEdit *>(QStringLiteral("requirementsSearch"));
    QVERIFY(search);
    search->setText(QStringLiteral("REQ-SW-PL-900"));
    QCoreApplication::processEvents();
    QCOMPARE(ideaNodeCount(), 1);

    // 2. The repo filter takes the other branch of applyViewFilters() (a
    //    discovered-root entry filters by file-path prefix) and must keep it too.
    search->clear();
    QCoreApplication::processEvents();
    QComboBox *repoFilter =
        widget.findChild<QComboBox *>(QStringLiteral("requirementsRepoFilter"));
    QVERIFY(repoFilter);
    const int rootEntry = repoFilter->findData(QFileInfo(root).canonicalFilePath());
    QVERIFY(rootEntry >= 0);
    repoFilter->setCurrentIndex(rootEntry);
    QCoreApplication::processEvents();
    QCOMPARE(ideaNodeCount(), 1);

    // Both filters together are the same code path once more.
    search->setText(QStringLiteral("900"));
    QCoreApplication::processEvents();
    QCOMPARE(ideaNodeCount(), 1);
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

void TestIdeas::managerReportIncludesIdeaLinkError()
{
    // AC1–AC3 end to end: the manager's own report (the counter above the
    // details) carries the promotion finding — nobody has to run the script.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir.filePath(QStringLiteral("repo")));
    const QString reqPath = QDir(activeDirFor(root))
                                .filePath(QStringLiteral("REQ-SW-PL-900-fixture.md"));

    auto validationText = [](RequirementsWidget &widget) {
        for (QLabel *label : widget.findChildren<QLabel *>()) {
            if (label->text().contains(QStringLiteral("Validation")))
                return label->text();
        }
        return QString();
    };

    RequirementsWidget widget;
    widget.openDirectory(root);
    const QString before = validationText(widget);
    QVERIFY2(!before.contains(QStringLiteral("error")), qPrintable(before));

    // The requirement now cites an idea whose status records no promotion.
    writeFile(reqPath, requirementMarkdown(QStringLiteral("REQ-SW-PL-900"),
                                           QStringLiteral("ACTIVE"))
                            + QStringLiteral("\n## Произход\n\n- изведено от IDEA-003\n"));
    writeIdeasTree(root); // IDEA-003 is "паркирана"
    widget.openDirectory(root);
    QCoreApplication::processEvents();

    const QString after = validationText(widget);
    QVERIFY2(after.contains(QStringLiteral("1 error")), qPrintable(after));
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
