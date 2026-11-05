#include "SceneProblemsPanel.h"

#include <QKeyEvent>
#include <QListWidgetItem>
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

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(2);
    layout->addWidget(m_header);
    layout->addWidget(m_list);

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
    {
        // Filling must not emit activation for every added row.
        const QSignalBlocker blocker(m_list);
        m_list->clear();
        m_nodeIds.clear();

        for (const Daqster::SceneOpenProblem& problem : problems) {
            auto* item = new QListWidgetItem(textForProblem(problem), m_list);
            item->setData(kNodeIdRole, problem.nodeId);
            if (!problem.hint.isEmpty())
                item->setToolTip(problem.hint);
            m_nodeIds << problem.nodeId;
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
