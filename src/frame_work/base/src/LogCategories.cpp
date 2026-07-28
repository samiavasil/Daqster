#include "include/LogCategories.h"
#include <QVector>
#include <algorithm>

// ═══════════════════════════════════════════════════════════
// Framework core — auto-registered in global registry
// ═══════════════════════════════════════════════════════════
DAQSTER_LOGGING_CATEGORY(lcFramework,    "daqster.framework",           "Framework core (QPluginManager, etc.)")
DAQSTER_LOGGING_CATEGORY(lcRegistry,     "daqster.framework.registry",  "Plugin registry")
DAQSTER_LOGGING_CATEGORY(lcDiscovery,    "daqster.framework.discovery", "Plugin discovery and loading")
DAQSTER_LOGGING_CATEGORY(lcPersistence,  "daqster.framework.persistence","Plugin persistence/settings")
DAQSTER_LOGGING_CATEGORY(lcShutdown,     "daqster.framework.shutdown",  "Shutdown handlers")
DAQSTER_LOGGING_CATEGORY(lcProcess,      "daqster.framework.process",   "Process management")

// ═══════════════════════════════════════════════════════════
// Application
// ═══════════════════════════════════════════════════════════
DAQSTER_LOGGING_CATEGORY(lcApp,          "daqster.app",                 "Daqster application")

// ═══════════════════════════════════════════════════════════
// Plugins
// ═══════════════════════════════════════════════════════════
DAQSTER_LOGGING_CATEGORY(lcNodeEditor,   "daqster.plugin.nodeeditor",   "Node Editor IDE plugin")
DAQSTER_LOGGING_CATEGORY(lcDemoNodes,    "daqster.plugin.demo",         "Demo nodes plugin")
DAQSTER_LOGGING_CATEGORY(lcAiStudio,     "daqster.plugin.aistudio",     "AI Studio plugin")
DAQSTER_LOGGING_CATEGORY(lcLlama,        "daqster.plugin.aistudio.llama","LlamaEngine (LLM inference)")
DAQSTER_LOGGING_CATEGORY(lcCoinTrader,   "daqster.plugin.cointrader",   "Qt Coin Trader plugin")

// ═══════════════════════════════════════════════════════════
// Registry implementation
// ═══════════════════════════════════════════════════════════
namespace Daqster {
namespace Log {

CategoryRegistry &CategoryRegistry::instance()
{
    static CategoryRegistry s_registry;
    return s_registry;
}

void CategoryRegistry::registerCategory(const char *name, const char *description)
{
    // Avoid duplicates (same category in multiple translation units)
    for (const auto &c : m_categories) {
        if (qstrcmp(c.name, name) == 0)
            return;
    }
    CategoryInfo info{name, description};
    m_categories.append(info);
}

QVector<CategoryInfo> CategoryRegistry::all() const
{
    return m_categories;
}

QVector<CategoryInfo> allCategories()
{
    QVector<CategoryInfo> cats = CategoryRegistry::instance().all();
    // Sort by name for consistent UI order
    std::sort(cats.begin(), cats.end(), [](const CategoryInfo &a, const CategoryInfo &b) {
        return qstrcmp(a.name, b.name) < 0;
    });
    return cats;
}

QString defaultFilterRules()
{
    // All categories OFF by default — user enables via Debug Console
    return QString();
}

} // namespace Log
} // namespace Daqster
