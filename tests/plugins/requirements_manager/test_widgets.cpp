#include <QtTest>
#include <QTemporaryDir>
#include <QTreeView>
#include <QToolBar>
#include <QAction>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QTableView>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QSplitter>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QMessageBox>
#include <QTimer>
#include <QAbstractButton>
#include <QFileInfo>
#include <type_traits>

#include "RequirementsWidget.h"
#include "ValidationDialog.h"
#include "TraceabilityMatrixWidget.h"
#include "TraceabilityMatrixModel.h"
#include "RequirementsModel.h"
#include "RequirementsValidator.h"
#include "ValidationReport.h"
#include "RequirementPreview.h"
#include "RequirementDates.h"

using namespace Daqster;

namespace {

QString writeRequirement(const QString &dirPath, const QString &fileName,
                         const QString &content)
{
    QDir().mkpath(dirPath);
    const QString filePath = QDir(dirPath).filePath(fileName);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return QString();
    file.write(content.toUtf8());
    file.close();
    return filePath;
}

// A requirement whose .md carries a section the parser does NOT model
// ("## Бележки"): it can only reach the preview through a real Markdown render.
QString requirementMarkdown(const QString &id, const QString &status,
                            const QString &date = QStringLiteral("2026-11-16"))
{
    return QStringLiteral(
               "# %1: Fixture requirement\n"
               "\n"
               "- **Статус:** %2\n"
               "- **Приоритет:** High\n"
               "- **Отговорник (роля):** Implementation\n"
               "- **Дата:** %3\n"
               "- **Родител:** —\n"
               "- **Зависи от:** —\n"
               "\n"
               "## Описание\n"
               "\n"
               "Описание на фикстурата.\n"
               "\n"
               "## Acceptance Criteria\n"
               "\n"
               "- [ ] 1. Първи критерий.\n"
               "\n"
               "## Бележки\n"
               "\n"
               "Този раздел съществува само в markdown файла.\n"
               "\n"
               "## Проследимост\n"
               "\n"
               "- **Коммити:** abc123\n")
        .arg(id, status, date);
}

// Builds a temp requirements tree and returns its root (empty on failure).
QString makeRequirementsRoot(QTemporaryDir &dir)
{
    const QString activeDir =
        QDir(dir.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    writeRequirement(activeDir, QStringLiteral("REQ-SW-PL-900-fixture.md"),
                     requirementMarkdown(QStringLiteral("REQ-SW-PL-900"),
                                         QStringLiteral("ACTIVE")));
    writeRequirement(activeDir, QStringLiteral("REQ-SW-PL-901-fixture.md"),
                     requirementMarkdown(QStringLiteral("REQ-SW-PL-901"),
                                         QStringLiteral("ACTIVE")));
    return dir.path();
}

// Same tree plus one DRAFT (REQ-SW-PL-099): the file carries the DRAFT- prefix
// the private repo uses, and its status line is the only difference.
QString makeDraftRequirementsRoot(QTemporaryDir &dir, QString *draftPath = nullptr)
{
    const QString root = makeRequirementsRoot(dir);
    const QString activeDir =
        QDir(root).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString path =
        writeRequirement(activeDir, QStringLiteral("DRAFT-REQ-SW-PL-902-fixture.md"),
                         requirementMarkdown(QStringLiteral("REQ-SW-PL-902"),
                                             QStringLiteral("DRAFT")));
    if (draftPath)
        *draftPath = path;
    return root;
}

// Requirement row in the (proxy-sorted) tree by bare ID.
QModelIndex viewIndexForId(QAbstractItemModel *model, const QString &id)
{
    for (int section = 0; section < model->rowCount(); ++section) {
        const QModelIndex sectionIndex = model->index(section, 0);
        for (int row = 0; row < model->rowCount(sectionIndex); ++row) {
            const QModelIndex candidate = model->index(row, 0, sectionIndex);
            if (candidate.data(Qt::DisplayRole).toString() == id)
                return candidate;
        }
    }
    return QModelIndex();
}

QString readAll(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(file.readAll());
}

} // namespace

// The by-value contract, expressed in the type system (REQ-SW-PL-099): the
// approval reloads m_requirements, so an id that aliases one of its elements
// must not be able to reach the reload. With `const QString &` this assert
// fails to compile — the defect cannot be reintroduced silently.
static_assert(std::is_same<decltype(&RequirementsWidget::acceptDraft),
                           bool (RequirementsWidget::*)(QString, const QString &)>::value,
              "acceptDraft must take the requirement id by value: it reloads the "
              "vector the action's id aliases");

// GUI tests for the Requirements Manager widgets (REQ-SW-PL-001). QTEST_MAIN
// provides a QApplication; the binary runs under the offscreen platform plugin
// (QT_QPA_PLATFORM=offscreen, set by the CTest ENVIRONMENT property).
class TestWidgets : public QObject
{
    Q_OBJECT

private slots:
    void actionBarFitsNarrowWindow();
    void previewRendersWholeMarkdownFile();
    void validationDialogCopiesMarkdown();
    void validationDialogCopiesStatusReport();
    void matrixMetricsPanelCollapsesOnNarrowWindow();
    void matrixTableSortsInView();
    void requirementsTreeSortsInView();
    void detailsSplitterSeparatesPreviewAndCheckboxes();
    void acceptActionEnabledOnlyForDraft();
    void acceptDraftRewritesFileAndCounter();
    void acceptActionApprovesDraftInPlace();
    void previewHeaderShowsCreatedAndAge();
    void overdueDraftIsPaintedInWarningColour();
    void draftCounterTooltipShowsOldestWait();
};

void TestWidgets::actionBarFitsNarrowWindow()
{
    RequirementsWidget widget;
    widget.resize(600, 500);

    QToolBar *bar = widget.findChild<QToolBar *>(QStringLiteral("requirementsActionBar"));
    QVERIFY(bar);

    // Every action survived the move from buttons to toolbar actions.
    QVERIFY(bar->actions().size() >= 11);

    // The old layout put 11 QPushButtons in one QHBoxLayout; their minimum was
    // the sum of their size hints. That is the reference the toolbar must beat
    // comfortably.
    int legacyMinimum = 0;
    for (const QAction *action : bar->actions()) {
        if (action->isSeparator())
            continue;
        QPushButton reference(action->text());
        legacyMinimum += reference.minimumSizeHint().width();
    }
    QVERIFY(legacyMinimum > 600);

    // The window can shrink: the toolbar overflows into an extension menu
    // instead of forcing its full width.
    const int minimum = widget.minimumSizeHint().width();
    qInfo("minimumSizeHint %d px (action bar %d px) vs legacy buttons %d px",
          minimum, bar->minimumSizeHint().width(), legacyMinimum);
    // The bar itself only ever claims one button plus the overflow arrow.
    QVERIFY2(bar->minimumSizeHint().width() < 200,
             qPrintable(QStringLiteral("action bar minimum %1")
                            .arg(bar->minimumSizeHint().width())));
    QVERIFY2(minimum < legacyMinimum / 2,
             qPrintable(QStringLiteral("minimumSizeHint %1 vs legacy %2")
                            .arg(minimum).arg(legacyMinimum)));
    QCOMPARE(widget.width(), 600);
}

void TestWidgets::previewRendersWholeMarkdownFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir);

    RequirementsWidget widget;
    widget.openDirectory(root);

    QTreeView *tree = widget.findChild<QTreeView *>(QStringLiteral("requirementsTree"));
    QVERIFY(tree);
    QAbstractItemModel *model = tree->model();
    QVERIFY(model);

    // Select REQ-SW-PL-900 by walking the sections.
    QModelIndex target;
    for (int section = 0; section < model->rowCount() && !target.isValid(); ++section) {
        const QModelIndex sectionIndex = model->index(section, 0);
        for (int row = 0; row < model->rowCount(sectionIndex); ++row) {
            const QModelIndex candidate = model->index(row, 0, sectionIndex);
            if (candidate.data(Qt::DisplayRole).toString()
                == QStringLiteral("REQ-SW-PL-900")) {
                target = candidate;
                break;
            }
        }
    }
    QVERIFY(target.isValid());

    tree->setCurrentIndex(target);
    QCoreApplication::processEvents();
    QTextBrowser *preview =
        widget.findChild<QTextBrowser *>(QStringLiteral("requirementPreview"));
    QVERIFY(preview);
    const QString plain = preview->document()->toPlainText();

    // The header still carries the parsed fields...
    QVERIFY(plain.contains(QStringLiteral("REQ-SW-PL-900")));
    QVERIFY(plain.contains(QStringLiteral("Status:")));
    // ...and the body is the whole file, including a mid-file section that the
    // parser never extracts.
    QVERIFY2(plain.contains(QStringLiteral("Бележки")), qPrintable(plain));
    QVERIFY(plain.contains(QStringLiteral("Този раздел съществува само в markdown файла.")));
    // The last section of the file is reachable too (nothing truncates it).
    QVERIFY(plain.contains(QStringLiteral("Проследимост")));
}

void TestWidgets::validationDialogCopiesMarkdown()
{
    RequirementsValidator::Issue issue;
    issue.id = QStringLiteral("REQ-SW-PL-900");
    issue.field = QStringLiteral("status");
    issue.message = QStringLiteral("невалиден статус");
    issue.severity = RequirementsValidator::Severity::Error;
    issue.filePath = QStringLiteral("/tmp/req/REQ-SW-PL-900-fixture.md");
    issue.line = 12;

    ValidationDialog dialog({issue});
    const QString markdown = dialog.markdownText();
    QCOMPARE(markdown, QStringLiteral("- error: REQ-SW-PL-900-fixture.md:12 — невалиден статус"));

    QPushButton *copy = nullptr;
    for (QPushButton *button : dialog.findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("Копирай"))
            copy = button;
    }
    QVERIFY(copy);
    QApplication::clipboard()->clear();
    copy->click();
    QCOMPARE(QApplication::clipboard()->text(), markdown);
}

void TestWidgets::validationDialogCopiesStatusReport()
{
    RequirementsValidator::Issue error;
    error.message = QStringLiteral("грешка");
    error.severity = RequirementsValidator::Severity::Error;
    error.filePath = QStringLiteral("/tmp/req/REQ-A.md");

    RequirementsValidator::Issue warning;
    warning.message = QStringLiteral("предупреждение");
    warning.severity = RequirementsValidator::Severity::Warning;
    warning.filePath = QStringLiteral("/tmp/req/REQ-B.md");

    ValidationDialog dialog({error, warning});
    const QString report = dialog.reportText();
    const QStringList lines = report.split(QLatin1Char('\n'));

    QVERIFY2(lines.first().startsWith(QStringLiteral("## Валидация ")), qPrintable(report));
    QVERIFY(report.contains(QStringLiteral("1 грешки, 1 предупреждения")));
    QVERIFY(report.contains(QStringLiteral("- error: REQ-A.md — грешка")));
    QVERIFY(report.contains(QStringLiteral("- warning: REQ-B.md — предупреждение")));

    QPushButton *copyReport = nullptr;
    for (QPushButton *button : dialog.findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("Копирай като справка"))
            copyReport = button;
    }
    QVERIFY(copyReport);
    QApplication::clipboard()->clear();
    copyReport->click();
    QCOMPARE(QApplication::clipboard()->text(), report);
}

void TestWidgets::matrixMetricsPanelCollapsesOnNarrowWindow()
{
    QVector<Requirement> reqs;
    Requirement req;
    req.id = QStringLiteral("REQ-SW-PL-900");
    req.title = QStringLiteral("Fixture");
    req.status = QStringLiteral("ACTIVE");
    req.section = QStringLiteral("active");
    reqs.append(req);

    TraceabilityMatrixWidget widget;
    widget.setRequirements(reqs);
    QVERIFY(!widget.metricsText().isEmpty());
    QVERIFY(widget.metricsText().contains(QStringLiteral("Total requirements")));

    // Narrow: the metrics report folds away so the table keeps the height.
    widget.resize(600, 400);
    widget.show();
    QCoreApplication::processEvents();
    QVERIFY(!widget.metricsExpanded());

    // Wide: it unfolds by itself.
    widget.resize(1200, 400);
    QCoreApplication::processEvents();
    QVERIFY(widget.metricsExpanded());

    // …and a manual toggle wins over the width heuristic from then on.
    QPushButton *toggle = nullptr;
    QPushButton *copy = nullptr;
    for (QPushButton *button : widget.findChildren<QPushButton *>()) {
        if (button->text().startsWith(QStringLiteral("Метрики")))
            toggle = button;
        if (button->text() == QStringLiteral("Копирай"))
            copy = button;
    }
    QVERIFY(toggle);
    QVERIFY(copy);

    toggle->click();
    QVERIFY(!widget.metricsExpanded());
    widget.resize(1400, 400);
    QCoreApplication::processEvents();
    QVERIFY(!widget.metricsExpanded());

    // Copy action puts the same text on the clipboard.
    QApplication::clipboard()->clear();
    copy->click();
    QCOMPARE(QApplication::clipboard()->text(), widget.metricsText());
}

void TestWidgets::matrixTableSortsInView()
{
    QVector<Requirement> reqs;
    for (const QString &id : {QStringLiteral("REQ-SW-PL-002"), QStringLiteral("REQ-SW-PL-001"),
                              QStringLiteral("REQ-SW-PL-003")}) {
        Requirement req;
        req.id = id;
        req.title = id;
        req.status = QStringLiteral("ACTIVE");
        req.section = QStringLiteral("active");
        reqs.append(req);
    }

    TraceabilityMatrixWidget widget;
    widget.setRequirements(reqs);

    QTableView *table = widget.findChild<QTableView *>(QStringLiteral("traceabilityTable"));
    QVERIFY(table);
    QVERIFY(table->isSortingEnabled());

    table->sortByColumn(TraceabilityMatrixModel::IdColumn, Qt::DescendingOrder);
    QCOMPARE(table->model()
                 ->index(0, TraceabilityMatrixModel::IdColumn)
                 .data(Qt::DisplayRole)
                 .toString(),
             QStringLiteral("REQ-SW-PL-003"));

    table->sortByColumn(TraceabilityMatrixModel::IdColumn, Qt::AscendingOrder);
    QCOMPARE(table->model()
                 ->index(0, TraceabilityMatrixModel::IdColumn)
                 .data(Qt::DisplayRole)
                 .toString(),
             QStringLiteral("REQ-SW-PL-001"));
}

void TestWidgets::requirementsTreeSortsInView()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir);

    RequirementsWidget widget;
    widget.openDirectory(root);

    QTreeView *tree = widget.findChild<QTreeView *>(QStringLiteral("requirementsTree"));
    QVERIFY(tree);
    QVERIFY(tree->isSortingEnabled());

    tree->sortByColumn(RequirementsModel::IdColumn, Qt::DescendingOrder);
    QAbstractItemModel *model = tree->model();

    // The section rows stay at the top level, the requirements sort inside.
    QCOMPARE(model->index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("active"));
    const QModelIndex section = model->index(0, 0);
    QVERIFY(model->rowCount(section) >= 2);
    QCOMPARE(model->index(0, 0, section).data(Qt::DisplayRole).toString(),
             QStringLiteral("REQ-SW-PL-901"));
    QCOMPARE(model->index(1, 0, section).data(Qt::DisplayRole).toString(),
             QStringLiteral("REQ-SW-PL-900"));
}

void TestWidgets::detailsSplitterSeparatesPreviewAndCheckboxes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir);

    RequirementsWidget widget;
    widget.openDirectory(root);
    widget.resize(900, 700);
    widget.show();
    QCoreApplication::processEvents();

    QTreeView *tree = widget.findChild<QTreeView *>(QStringLiteral("requirementsTree"));
    QVERIFY(tree);
    QAbstractItemModel *model = tree->model();
    QModelIndex target;
    for (int section = 0; section < model->rowCount() && !target.isValid(); ++section) {
        const QModelIndex sectionIndex = model->index(section, 0);
        for (int row = 0; row < model->rowCount(sectionIndex); ++row) {
            const QModelIndex candidate = model->index(row, 0, sectionIndex);
            if (candidate.data(Qt::DisplayRole).toString()
                == QStringLiteral("REQ-SW-PL-900")) {
                target = candidate;
                break;
            }
        }
    }
    QVERIFY(target.isValid());
    tree->setCurrentIndex(target);
    QCoreApplication::processEvents();

    // The vertical splitter lives INSIDE the horizontal tree ↔ details
    // splitter; nesting must not flatten the outer one (REQ-SW-PL-095).
    QSplitter *details =
        widget.findChild<QSplitter *>(QStringLiteral("requirementDetailsSplitter"));
    QVERIFY(details);
    QCOMPARE(details->orientation(), Qt::Vertical);
    QCOMPARE(details->count(), 2);
    QVERIFY(!details->childrenCollapsible());
    QVERIFY(details->handle(1));
    QCOMPARE(details->handleWidth(), 6);

    QWidget *top = details->widget(0);
    QWidget *bottom = details->widget(1);
    QVERIFY(top);
    QVERIFY(bottom);
    QVERIFY2(top->height() >= 80 && bottom->height() >= 80,
             qPrintable(QStringLiteral("initial heights %1 / %2")
                            .arg(top->height()).arg(bottom->height())));
    // Preview owns the bigger share by default (~2/3 vs ~1/3).
    QVERIFY(top->height() > bottom->height());

    // Dragging (here: setSizes) really moves the boundary: the default is
    // ~2/3 for the preview, so shrinking it must shrink the widget.
    const int topBefore = top->height();
    const int bottomBefore = bottom->height();
    details->setSizes({150, 350});
    QCoreApplication::processEvents();
    QVERIFY2(top->height() != topBefore || bottom->height() != bottomBefore,
             qPrintable(QStringLiteral("heights unchanged: %1 / %2")
                            .arg(top->height()).arg(bottom->height())));
    QVERIFY2(bottom->height() > top->height(),
             qPrintable(QStringLiteral("after setSizes 150/350: %1 / %2")
                            .arg(top->height()).arg(bottom->height())));
    details->setSizes({400, 120});
    QCoreApplication::processEvents();
    QVERIFY2(top->height() > bottom->height(),
             qPrintable(QStringLiteral("after setSizes 400/120: %1 / %2")
                            .arg(top->height()).arg(bottom->height())));

    // The outer, horizontal (tree ↔ details) splitter still holds the panel
    // that now contains the vertical one — the nesting did not flatten it.
    QSplitter *outer = qobject_cast<QSplitter *>(details->parentWidget()->parentWidget());
    QWidget *detailsPanel = details->parentWidget();
    QVERIFY(outer);
    QCOMPARE(outer->orientation(), Qt::Horizontal);
    QVERIFY(outer->indexOf(detailsPanel) >= 0);

    // …and the checkbox half did not lose its signal in the splitter: a tick
    // still travels through onCriterionToggled to the file on disk.
    // The bottom half IS the checkbox list, no wrapper in between.
    QListWidget *criteria = qobject_cast<QListWidget *>(bottom);
    QVERIFY(criteria);
    QVERIFY(criteria->count() >= 1);
    QListWidgetItem *criterion = criteria->item(0);
    QVERIFY(criterion->checkState() == Qt::Unchecked);
    criterion->setCheckState(Qt::Checked);
    QCoreApplication::processEvents();
    const QString criteriaFile = QDir(root).filePath(
        QStringLiteral("DevelopmentProcess/requirements/active/REQ-SW-PL-900-fixture.md"));
    QFile written(criteriaFile);
    QVERIFY(written.open(QIODevice::ReadOnly | QIODevice::Text));
    QVERIFY2(QString::fromUtf8(written.readAll()).contains(QStringLiteral("- [x] 1.")),
             "ticking a criterion must reach the file");

    // Edit swaps preview ↔ editor inside the same pane: the splitter and its
    // checkbox half survive the toggle.
    QToolBar *bar = widget.findChild<QToolBar *>(QStringLiteral("requirementsActionBar"));
    QVERIFY(bar);
    QAction *edit = nullptr;
    for (QAction *action : bar->actions()) {
        if (action->text() == QStringLiteral("Edit"))
            edit = action;
    }
    QVERIFY(edit);
    QPlainTextEdit *editor = widget.findChild<QPlainTextEdit *>();
    QTextBrowser *preview =
        widget.findChild<QTextBrowser *>(QStringLiteral("requirementPreview"));
    QVERIFY(editor);
    QVERIFY(preview);
    QVERIFY(!editor->isVisible());
    edit->trigger();
    QCoreApplication::processEvents();
    QVERIFY(editor->isVisible());
    QVERIFY(!preview->isVisible());
    QCOMPARE(details->count(), 2);
    QCOMPARE(details->widget(0), top);
}

void TestWidgets::acceptActionEnabledOnlyForDraft()
{
    // REQ-SW-PL-099: the approval action exists for exactly one transition.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeDraftRequirementsRoot(dir);

    RequirementsWidget widget;
    widget.openDirectory(root);

    QTreeView *tree = widget.findChild<QTreeView *>(QStringLiteral("requirementsTree"));
    QVERIFY(tree);
    QAction *accept = widget.findChild<QAction *>(QStringLiteral("acceptDraftAction"));
    QVERIFY(accept);

    const QModelIndex draft = viewIndexForId(tree->model(), QStringLiteral("REQ-SW-PL-902"));
    const QModelIndex active = viewIndexForId(tree->model(), QStringLiteral("REQ-SW-PL-900"));
    QVERIFY(draft.isValid());
    QVERIFY(active.isValid());

    tree->setCurrentIndex(active);
    QCoreApplication::processEvents();
    QVERIFY2(!accept->isEnabled(), "ACTIVE is not a draft: nothing to accept");

    tree->setCurrentIndex(draft);
    QCoreApplication::processEvents();
    QVERIFY(accept->isEnabled());

    // Sorting still works with the new status: DRAFT sorts after ACTIVE in the
    // status column, and the section row stays where it is.
    tree->sortByColumn(RequirementsModel::StatusColumn, Qt::AscendingOrder);
    QAbstractItemModel *model = tree->model();
    QCOMPARE(model->index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("active"));
    const QModelIndex section = model->index(0, 0);
    QCOMPARE(model->rowCount(section), 3);
    QCOMPARE(model->index(model->rowCount(section) - 1, 0, section)
                 .data(Qt::DisplayRole).toString(),
             QStringLiteral("REQ-SW-PL-902"));
}

void TestWidgets::acceptDraftRewritesFileAndCounter()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString draftPath;
    const QString root = makeDraftRequirementsRoot(dir, &draftPath);
    QVERIFY(!draftPath.isEmpty());
    const QString before = readAll(draftPath);

    RequirementsWidget widget;
    widget.openDirectory(root);

    QLabel *counter = widget.findChild<QLabel *>(QStringLiteral("draftCountLabel"));
    QVERIFY(counter);
    QCOMPARE(counter->text(), QStringLiteral("чакат одобрение: 1"));

    // The transition itself, without the modal confirmation that belongs to
    // the action's click handler.
    QVERIFY(widget.acceptDraft(QStringLiteral("REQ-SW-PL-902"),
                               QStringLiteral("2026-11-21")));

    const QString after = readAll(draftPath);
    const QString expected =
        QString(before).replace(QStringLiteral("- **Статус:** DRAFT"),
                                QStringLiteral("- **Статус:** ACTIVE\n"
                                               "- **Одобрено:** 2026-11-21, потребител"));
    QCOMPARE(after, expected);

    // The tree was refreshed: the counter says nothing is waiting, and the
    // status column shows the accepted state.
    QCOMPARE(counter->text(), QStringLiteral("чакат одобрение: 0"));
    QTreeView *tree = widget.findChild<QTreeView *>(QStringLiteral("requirementsTree"));
    QVERIFY(tree);
    const QModelIndex row = viewIndexForId(tree->model(), QStringLiteral("REQ-SW-PL-902"));
    QVERIFY(row.isValid());
    QCOMPARE(row.sibling(row.row(), RequirementsModel::StatusColumn)
                 .data(Qt::DisplayRole).toString(),
             QStringLiteral("ACTIVE"));
}

void TestWidgets::acceptActionApprovesDraftInPlace()
{
    // The regression test for the accepted-draft crash (REQ-SW-PL-099): the
    // existing test above passes a *temporary* id, so it never aliases
    // m_requirements and the dangling reference stayed invisible. This one
    // drives the action, i.e. the path where the id really does alias
    // `m_requirements.at(m_currentIndex).id` while acceptDraft() reloads the
    // vector. On the buggy build this aborted with qBadAlloc inside
    // navigateToId() -> QString::arg().
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString draftPath;
    const QString root = makeDraftRequirementsRoot(dir, &draftPath);
    QVERIFY(!draftPath.isEmpty());

    // Give the fixture the draft banner a real draft carries, so the stale-text
    // rule is exercised through the click as well.
    const QString withBanner =
        QString(readAll(draftPath))
            .replace(QStringLiteral("- **Зависи от:** —\n\n"),
                     QStringLiteral("- **Зависи от:** —\n\n"
                                    "> **Чернова — чака одобрение в мениджъра; не се "
                                    "имплементира, докато не стане `ACTIVE`.**\n\n"));
    QVERIFY(withBanner.contains(QStringLiteral("Чернова")));
    QVERIFY(!writeRequirement(
                 QFileInfo(draftPath).absolutePath(), QFileInfo(draftPath).fileName(),
                 withBanner)
                 .isEmpty());

    RequirementsWidget widget;
    widget.openDirectory(root);

    QTreeView *tree = widget.findChild<QTreeView *>(QStringLiteral("requirementsTree"));
    QVERIFY(tree);
    QAction *accept = widget.findChild<QAction *>(QStringLiteral("acceptDraftAction"));
    QVERIFY(accept);

    const QModelIndex draft = viewIndexForId(tree->model(), QStringLiteral("REQ-SW-PL-902"));
    QVERIFY(draft.isValid());
    tree->setCurrentIndex(draft);
    QCoreApplication::processEvents();
    QVERIFY2(accept->isEnabled(), "a DRAFT inside active/ is acceptable");

    // The click asks for confirmation; answer „Да“ as soon as the box is up.
    QTimer answer;
    answer.setInterval(10);
    QObject::connect(&answer, &QTimer::timeout, [&answer]() {
        QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (!box)
            return;
        if (QAbstractButton *yes = box->button(QMessageBox::Yes)) {
            answer.stop();
            yes->click();
        }
    });
    answer.start();
    accept->trigger();
    answer.stop();
    QCoreApplication::processEvents();

    // The file moved DRAFT -> ACTIVE and lost the banner: no crash on the way.
    const QString after = readAll(draftPath);
    QVERIFY2(after.contains(QStringLiteral("- **Статус:** ACTIVE")), qPrintable(after));
    QVERIFY(after.contains(QStringLiteral("- **Одобрено:**")));
    QVERIFY2(!after.contains(QStringLiteral("Чернова")), qPrintable(after));

    // The reloaded tree resolves the same id, and navigation landed on it.
    const QModelIndex accepted =
        viewIndexForId(tree->model(), QStringLiteral("REQ-SW-PL-902"));
    QVERIFY(accepted.isValid());
    QCOMPARE(accepted.sibling(accepted.row(), RequirementsModel::StatusColumn)
                 .data(Qt::DisplayRole).toString(),
             QStringLiteral("ACTIVE"));
    QCOMPARE(tree->currentIndex().data(Qt::DisplayRole).toString(),
             QStringLiteral("REQ-SW-PL-902"));
    QLabel *counter = widget.findChild<QLabel *>(QStringLiteral("draftCountLabel"));
    QVERIFY(counter);
    QCOMPARE(counter->text(), QStringLiteral("чакат одобрение: 0"));
    QVERIFY2(!accept->isEnabled(), "the accepted requirement is no longer a draft");
}

QTEST_MAIN(TestWidgets)

#include "test_widgets.moc"
void TestWidgets::previewHeaderShowsCreatedAndAge()
{
    // REQ-SW-PL-100: the details header pairs the creation date with its age,
    // so a reader never does the arithmetic. The reference day is a parameter,
    // which is what makes "12 дни" assertable.
    Requirement req;
    req.id = QStringLiteral("REQ-SW-PL-900");
    req.title = QStringLiteral("Fixture");
    req.status = QStringLiteral("ACTIVE");
    req.priority = QStringLiteral("High");
    req.assignee = QStringLiteral("Implementation");
    req.date = QStringLiteral("2026-11-09");

    const QString html =
        RequirementPreview::headerHtml(req, {}, {}, QDate(2026, 11, 21));
    QVERIFY2(html.contains(QStringLiteral("Създадено:")), qPrintable(html));
    QVERIFY2(html.contains(QStringLiteral("2026-11-09 · преди 12 дни")),
             qPrintable(html));
    // Status still leads the same line — the date sits next to it, not under it.
    QVERIFY(html.indexOf(QStringLiteral("Status:"))
            < html.indexOf(QStringLiteral("Създадено:")));

    // A requirement without a date says so instead of rendering "преди -1 дни".
    req.date.clear();
    const QString bare =
        RequirementPreview::headerHtml(req, {}, {}, QDate(2026, 11, 21));
    QVERIFY2(bare.contains(QStringLiteral("Създадено:</b> —")), qPrintable(bare));
}

void TestWidgets::overdueDraftIsPaintedInWarningColour()
{
    // A DRAFT nobody approved for longer than two weeks is painted in the
    // warning colour; an old ACTIVE requirement is not.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir);
    const QString activeDir =
        QDir(root).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    writeRequirement(activeDir, QStringLiteral("DRAFT-REQ-SW-PL-903-fixture.md"),
                     requirementMarkdown(QStringLiteral("REQ-SW-PL-903"),
                                         QStringLiteral("DRAFT"),
                                         QStringLiteral("2020-01-01")));

    RequirementsWidget widget;
    widget.openDirectory(root);

    QTreeView *tree = widget.findChild<QTreeView *>(QStringLiteral("requirementsTree"));
    QVERIFY(tree);
    const QModelIndex stale = viewIndexForId(tree->model(), QStringLiteral("REQ-SW-PL-903"));
    const QModelIndex active = viewIndexForId(tree->model(), QStringLiteral("REQ-SW-PL-900"));
    QVERIFY(stale.isValid());
    QVERIFY(active.isValid());

    const QVariant colour = stale.data(Qt::ForegroundRole);
    QVERIFY2(colour.isValid(), "the overdue DRAFT row must be coloured");
    QCOMPARE(colour.value<QColor>(), RequirementsModel::staleDraftColour());
    QVERIFY2(!active.data(Qt::ForegroundRole).isValid(),
             "an old ACTIVE requirement is not waiting for approval");
}

void TestWidgets::draftCounterTooltipShowsOldestWait()
{
    // The counter is a reading, not a number to think about: its tooltip says
    // which draft waited longest and for how many days.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = makeRequirementsRoot(dir);
    const QString activeDir =
        QDir(root).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    writeRequirement(activeDir, QStringLiteral("DRAFT-REQ-SW-PL-904-fixture.md"),
                     requirementMarkdown(QStringLiteral("REQ-SW-PL-904"),
                                         QStringLiteral("DRAFT"),
                                         QStringLiteral("2026-10-01")));
    writeRequirement(activeDir, QStringLiteral("DRAFT-REQ-SW-PL-905-fixture.md"),
                     requirementMarkdown(QStringLiteral("REQ-SW-PL-905"),
                                         QStringLiteral("DRAFT"),
                                         QDate::currentDate().toString(Qt::ISODate)));

    RequirementsWidget widget;
    widget.openDirectory(root);

    QLabel *counter = widget.findChild<QLabel *>(QStringLiteral("draftCountLabel"));
    QVERIFY(counter);
    QCOMPARE(counter->text(), QStringLiteral("чакат одобрение: 2"));

    const QString expectedDays = QString::number(
        RequirementDates::ageInDays(QStringLiteral("2026-10-01"), QDate::currentDate()));
    QVERIFY2(counter->toolTip().contains(QStringLiteral("REQ-SW-PL-904")),
             qPrintable(counter->toolTip()));
    QVERIFY2(counter->toolTip().contains(expectedDays),
             qPrintable(counter->toolTip()));
}
