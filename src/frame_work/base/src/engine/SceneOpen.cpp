#include "SceneOpen.h"

// Qt first: capabilities/INodeProvider.h ends with Q_DECLARE_INTERFACE, which
// needs the Qt headers to be in scope.
#include <QString>

#include "QPluginManager.h"
#include "capabilities/INodeProvider.h"
#include "capabilities/ISceneOpening.h"
#include "LogCategories.h"

namespace Daqster {

SceneOpenReport checkSceneOpening(const QJsonObject& scene,
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
        const SceneOpenVerdict verdict = opening->onSceneOpening(scene);
        if (verdict.accepted())
            continue;   // acceptance is silent: hosts ask twice on the happy path

        // A refusal is not a crash: it is a plugin saying "this scene cannot run
        // here". Every problem is kept, in the plugin's own words.
        for (const QString& problem : verdict.problems) {
            qCWarning(lcNodeEditor) << "SceneOpen: scene refused —" << problem;
            report.problems.append(problem);
        }
    }
    return report;
}

SceneOpenReport checkSceneOpening(const QJsonObject& scene)
{
    QPluginManager* pluginManager = QPluginManager::instance();
    if (pluginManager == nullptr)
        return SceneOpenReport{};

    std::vector<INodeProvider*> providers;
    const auto discovered = pluginManager->nodeProviders();
    providers.reserve(static_cast<size_t>(discovered.size()));
    for (INodeProvider* provider : discovered)
        providers.push_back(provider);
    return checkSceneOpening(scene, providers);
}

QString sceneOpeningMessage(const SceneOpenReport& report)
{
    if (report.accepted())
        return QString();

    QString message = QStringLiteral("Scene cannot be opened — %1 problem(s) reported "
                                     "by the loaded plugins:\n  - ")
                          .arg(report.problems.size());
    message += report.problems.join(QStringLiteral("\n  - "));
    message += QStringLiteral("\n\nNothing was loaded: the scene that was already open "
                              "(if any) was left untouched.");
    return message;
}

} // namespace Daqster
