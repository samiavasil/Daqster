#include "SceneOpen.h"

// Qt first: capabilities/INodeProvider.h ends with Q_DECLARE_INTERFACE, which
// needs the Qt headers to be in scope.
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "QPluginManager.h"
#include "capabilities/INodeProvider.h"
#include "capabilities/ISceneOpening.h"
#include "LogCategories.h"

namespace Daqster {

namespace {

/// One problem as the host sees it, without any of the parts being empty.
QString problemLine(const SceneOpenProblem& problem)
{
    QString subject;
    if (!problem.nodeName.isEmpty())
        subject = problem.nodeName;
    if (!problem.nodeId.isEmpty()) {
        if (subject.isEmpty())
            subject = problem.nodeId;
        else
            subject += QStringLiteral(" (%1)").arg(problem.nodeId);
    }

    QString line;
    if (!subject.isEmpty())
        line = subject + QStringLiteral(": ");
    line += problem.message;
    if (!problem.hint.isEmpty())
        line += QStringLiteral(" — ") + problem.hint;
    return line;
}

} // namespace

SceneOpenReport checkSceneOpening(const QJsonObject& scene, const QString& scenePath,
                                  const std::vector<INodeProvider*>& providers)
{
    SceneOpenReport report;
    for (INodeProvider* provider : providers) {
        if (provider == nullptr)
            continue;
        auto* opening = dynamic_cast<ISceneOpening*>(provider);
        if (opening == nullptr)
            continue;   // the hook is optional

        ++report.providersAsked;
        // The 2-arg virtual is always called: for a plugin that only knows the
        // 1-arg form, ISceneOpening's default implementation forwards to it, so
        // old plugins behave exactly as they did (REQ-SW-PL-082).
        const SceneOpenVerdict verdict = opening->onSceneOpening(scene, scenePath);
        if (verdict.accepted())
            continue;   // acceptance is silent: hosts ask twice on the happy path

        // A refusal is not a crash: it is a plugin saying "this scene cannot run
        // here". Every problem is kept, in the plugin's own words.
        for (const SceneOpenProblem& problem : verdict.problems) {
            qCWarning(lcNodeEditor).noquote() << "SceneOpen: scene refused —" << problemLine(problem);
            report.problems.append(problem);
        }
    }
    return report;
}

SceneOpenReport checkSceneOpening(const QJsonObject& scene,
                                  const std::vector<INodeProvider*>& providers)
{
    return checkSceneOpening(scene, QString(), providers);
}

SceneOpenReport checkSceneOpening(const QJsonObject& scene, const QString& scenePath)
{
    QPluginManager* pluginManager = QPluginManager::instance();
    if (pluginManager == nullptr)
        return SceneOpenReport{};

    std::vector<INodeProvider*> providers;
    const auto discovered = pluginManager->nodeProviders();
    providers.reserve(static_cast<size_t>(discovered.size()));
    for (INodeProvider* provider : discovered)
        providers.push_back(provider);
    return checkSceneOpening(scene, scenePath, providers);
}

SceneOpenReport checkSceneOpening(const QJsonObject& scene)
{
    return checkSceneOpening(scene, QString());
}

QJsonArray SceneOpenReport::problemsJson() const
{
    QJsonArray array;
    for (const SceneOpenProblem& problem : problems) {
        array.append(QJsonObject{{QStringLiteral("nodeId"), problem.nodeId},
                                 {QStringLiteral("nodeName"), problem.nodeName},
                                 {QStringLiteral("message"), problem.message},
                                 {QStringLiteral("hint"), problem.hint}});
    }
    return array;
}

QString sceneOpeningMessage(const SceneOpenReport& report)
{
    if (report.accepted())
        return QString();

    QStringList lines;
    lines.reserve(report.problems.size());
    for (const SceneOpenProblem& problem : report.problems)
        lines.append(QStringLiteral("  - ") + problemLine(problem));

    QString message = QStringLiteral("Scene cannot be opened — %1 problem(s) reported "
                                     "by the loaded plugins:\n")
                          .arg(report.problems.size());
    message += lines.join(QLatin1Char('\n'));
    message += QStringLiteral("\n\nNothing was loaded: the scene that was already open "
                              "(if any) was left untouched.");
    return message;
}

} // namespace Daqster
