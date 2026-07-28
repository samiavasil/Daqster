#include "include/LogCategories.h"
#include <QVector>

// ═══════════════════════════════════════════════════════════
// Framework core
// ═══════════════════════════════════════════════════════════
Q_LOGGING_CATEGORY(lcFramework,    "daqster.framework")
Q_LOGGING_CATEGORY(lcRegistry,     "daqster.framework.registry")
Q_LOGGING_CATEGORY(lcDiscovery,    "daqster.framework.discovery")
Q_LOGGING_CATEGORY(lcPersistence,  "daqster.framework.persistence")
Q_LOGGING_CATEGORY(lcShutdown,     "daqster.framework.shutdown")
Q_LOGGING_CATEGORY(lcProcess,      "daqster.framework.process")

// ═══════════════════════════════════════════════════════════
// Application
// ═══════════════════════════════════════════════════════════
Q_LOGGING_CATEGORY(lcApp,          "daqster.app")

// ═══════════════════════════════════════════════════════════
// Plugins
// ═══════════════════════════════════════════════════════════
Q_LOGGING_CATEGORY(lcNodeEditor,   "daqster.plugin.nodeeditor")
Q_LOGGING_CATEGORY(lcDemoNodes,    "daqster.plugin.demo")
Q_LOGGING_CATEGORY(lcAiStudio,     "daqster.plugin.aistudio")
Q_LOGGING_CATEGORY(lcLlama,        "daqster.plugin.aistudio.llama")
Q_LOGGING_CATEGORY(lcCoinTrader,   "daqster.plugin.cointrader")

// ═══════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════
namespace Daqster {
namespace Log {

QVector<CategoryInfo> allCategories()
{
    return {
        {"daqster.framework",           "Framework core (QPluginManager, etc.)"},
        {"daqster.framework.registry",  "Plugin registry"},
        {"daqster.framework.discovery", "Plugin discovery and loading"},
        {"daqster.framework.persistence", "Plugin persistence/settings"},
        {"daqster.framework.shutdown",  "Shutdown handlers"},
        {"daqster.framework.process",   "Process management"},
        {"daqster.app",                 "Daqster application"},
        {"daqster.plugin.nodeeditor",   "Node Editor IDE plugin"},
        {"daqster.plugin.demo",         "Demo nodes plugin"},
        {"daqster.plugin.aistudio",     "AI Studio plugin"},
        {"daqster.plugin.aistudio.llama", "LlamaEngine (LLM inference)"},
        {"daqster.plugin.cointrader",   "Qt Coin Trader plugin"},
    };
}

QString defaultFilterRules()
{
    // All categories OFF by default — user enables via Debug Console
    return QString();
}

} // namespace Log
} // namespace Daqster
