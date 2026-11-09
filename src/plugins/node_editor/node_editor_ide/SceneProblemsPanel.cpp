#include "SceneProblemsPanel.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidgetItem>
#include <QMetaObject>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

/// Data role that carries the node id of a row (QString, may be empty).
constexpr int kNodeIdRole = Qt::UserRole + 1;

} // namespace

SceneProblemsPanel::SceneProblemsPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("sceneProblemsPanel"));

    m_header = new QToolButton(this);
    m_header->setObjectName(QStringLiteral("sceneProblemsHeader"));
    m_header->setCheckable(true);
    m_header->setChecked(false);
    m_header->setAutoRaise(true);
    m_header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_header->setArrowType(Qt::RightArrow);
    m_header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_header, &QToolButton::toggled, this, &SceneProblemsPanel::toggleCollapsed);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("sceneProblemsList"));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setWordWrap(true);
    connect(m_list, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* item) {
                if (item != nullptr)
                    activateRow(m_list->row(item));
            });
    // Enter is not `itemDoubleClicked`: handled in eventFilter() so a platform
    // that activates on a single click cannot turn selection into navigation.
    m_list->installEventFilter(this);

    // The failure of a fixer lands here, not in a modal: the user keeps the
    // list (and the node) in front of them while reading the reason.
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("sceneProblemsStatus"));
    m_status->setWordWrap(true);
    m_status->setVisible(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(2);
    layout->addWidget(m_header);
    layout->addWidget(m_list);
    layout->addWidget(m_status);

    setCollapsed(true);
    setVisible(false);
}

SceneProblemsPanel::~SceneProblemsPanel() = default;

QString SceneProblemsPanel::textForProblem(const Daqster::SceneOpenProblem& problem)
{
    QString text;
    if (!problem.nodeName.isEmpty()) {
        text = problem.nodeName;
        if (!problem.nodeId.isEmpty())
            text += QStringLiteral(" (") + problem.nodeId + QLatin1Char(')');
        text += QStringLiteral(": ");
    }
    text += problem.message;
    if (!problem.hint.isEmpty())
        text += QStringLiteral(" — ") + problem.hint;
    return text;
}

void SceneProblemsPanel::setProblems(const QVector<Daqster::SceneOpenProblem>& problems)
{
    m_problems = problems;
    setStatus(QString());

    {
        // Filling must not emit activation for every added row.
        const QSignalBlocker blocker(m_list);
        m_list->clear();
        m_nodeIds.clear();

        for (int i = 0; i < problems.size(); ++i) {
            const Daqster::SceneOpenProblem& problem = problems.at(i);
            auto* item = new QListWidgetItem(textForProblem(problem), m_list);
            item->setData(kNodeIdRole, problem.nodeId);
            if (!problem.hint.isEmpty())
                item->setToolTip(problem.hint);
            m_nodeIds << problem.nodeId;
            installRowWidget(item, i, problem);
        }
    }

    m_header->setText(problems.isEmpty()
                          ? QString()
                          : tr("Проблеми в сцената (%1)").arg(problems.size()));

    // A clean scene gets no panel at all; a scene with problems gets the header
    // (collapsed — the list opens on request).
    setVisible(!problems.isEmpty());
    if (!problems.isEmpty())
        setCollapsed(true);
}

int SceneProblemsPanel::problemCount() const
{
    return m_list != nullptr ? m_list->count() : 0;
}

QString SceneProblemsPanel::headerText() const
{
    return m_header != nullptr ? m_header->text() : QString();
}

QStringList SceneProblemsPanel::nodeIds() const
{
    return m_nodeIds;
}

bool SceneProblemsPanel::isCollapsed() const
{
    return m_list == nullptr || m_list->isHidden();
}

void SceneProblemsPanel::setCollapsed(bool collapsed)
{
    if (m_list == nullptr)
        return;

    m_list->setVisible(!collapsed);
    if (m_header != nullptr) {
        const QSignalBlocker blocker(m_header);
        m_header->setChecked(!collapsed);
        m_header->setArrowType(collapsed ? Qt::RightArrow : Qt::DownArrow);
    }
}

void SceneProblemsPanel::toggleCollapsed()
{
    setCollapsed(!m_header->isChecked());
}

void SceneProblemsPanel::activateRow(int row)
{
    if (row < 0 || row >= m_nodeIds.size())
        return;
    emit problemActivated(m_nodeIds.at(row));
}

// ── "Поправи" (REQ-SW-PL-085) ───────────────────────────────────────────────

void SceneProblemsPanel::setFixHandler(
    std::function<QString(const Daqster::SceneOpenProblem&)> label,
    std::function<bool(const Daqster::SceneOpenProblem&, QString*)> fix)
{
    m_fixLabel = std::move(label);
    m_fix = std::move(fix);

    // Rebuild so the buttons appear (or disappear) on the rows already shown.
    setProblems(m_problems);
}

QString SceneProblemsPanel::statusText() const
{
    return m_status != nullptr && !m_status->isHidden() ? m_status->text() : QString();
}

QToolButton* SceneProblemsPanel::fixButtonAt(int row) const
{
    if (row < 0 || row >= m_list->count())
        return nullptr;
    QWidget* widget = m_list->itemWidget(m_list->item(row));
    return widget != nullptr ? widget->findChild<QToolButton*>() : nullptr;
}

void SceneProblemsPanel::installRowWidget(QListWidgetItem* item, int index,
                                          const Daqster::SceneOpenProblem& problem)
{
    if (!m_fixLabel || !m_fix)
        return;

    const QString label = m_fixLabel(problem);
    if (label.isEmpty())
        return; // no fixer handles this row — keep the plain text row

    auto* row = new QWidget(m_list);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(4);

    auto* text = new QLabel(item->text(), row);
    text->setObjectName(QStringLiteral("sceneProblemsRowText"));
    text->setWordWrap(true);
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    rowLayout->addWidget(text, 1);

    auto* button = new QToolButton(row);
    button->setObjectName(QStringLiteral("sceneProblemsFixButton"));
    button->setText(label);
    button->setToolTip(label);
    rowLayout->addWidget(button, 0, Qt::AlignTop);
    connect(button, &QToolButton::clicked, this, [this, index]() { runFix(index); });

    m_list->setItemWidget(item, row);
    item->setSizeHint(row->sizeHint());
}

void SceneProblemsPanel::runFix(int index)
{
    if (!m_fix || index < 0 || index >= m_problems.size())
        return;

    // The list can be rebuilt by the host while the fix runs (setProblems()
    // clears the rows), so work on a copy and never touch the items here.
    const Daqster::SceneOpenProblem problem = m_problems.at(index);
    QString error;
    const bool ok = m_fix(problem, &error);

    if (!ok) {
        setStatus(error.isEmpty() ? tr("Поправката се провали — причината не е известна")
                                  : error);
        return;
    }

    setStatus(QString());

    // The host refreshes the panel from the scene, which clears the list and
    // deletes the button we are currently inside — let the click unwind first.
    QMetaObject::invokeMethod(
        this, [this, problem]() { emit fixRequested(problem); },
        Qt::QueuedConnection);
}

void SceneProblemsPanel::setStatus(const QString& text)
{
    if (m_status == nullptr)
        return;
    m_status->setText(text);
    m_status->setVisible(!text.isEmpty());
}

bool SceneProblemsPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_list && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            activateRow(m_list->currentRow());
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
