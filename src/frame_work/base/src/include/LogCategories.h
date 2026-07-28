#ifndef LOGCATEGORIES_H
#define LOGCATEGORIES_H

#include <QLoggingCategory>
#include <QString>
#include <QVector>

// ═══════════════════════════════════════════════════════════
// Framework core categories
// ═══════════════════════════════════════════════════════════
Q_DECLARE_LOGGING_CATEGORY(lcFramework)
Q_DECLARE_LOGGING_CATEGORY(lcRegistry)
Q_DECLARE_LOGGING_CATEGORY(lcDiscovery)
Q_DECLARE_LOGGING_CATEGORY(lcPersistence)
Q_DECLARE_LOGGING_CATEGORY(lcShutdown)
Q_DECLARE_LOGGING_CATEGORY(lcProcess)

// ═══════════════════════════════════════════════════════════
// Application categories
// ═══════════════════════════════════════════════════════════
Q_DECLARE_LOGGING_CATEGORY(lcApp)

// ═══════════════════════════════════════════════════════════
// Plugin categories (add new plugins here)
// ═══════════════════════════════════════════════════════════
Q_DECLARE_LOGGING_CATEGORY(lcNodeEditor)
Q_DECLARE_LOGGING_CATEGORY(lcDemoNodes)
Q_DECLARE_LOGGING_CATEGORY(lcAiStudio)
Q_DECLARE_LOGGING_CATEGORY(lcLlama)
Q_DECLARE_LOGGING_CATEGORY(lcCoinTrader)

// ═══════════════════════════════════════════════════════════
// Helper: get all category names for UI/debug console
// ═══════════════════════════════════════════════════════════
namespace Daqster {
namespace Log {

struct CategoryInfo {
    const char *name;
    const char *description;
};

// Returns array of all registered categories with descriptions
// Used by DebugConsoleWidget to populate checkboxes
QVector<CategoryInfo> allCategories();

// Default filter rules (all OFF)
QString defaultFilterRules();

} // namespace Log
} // namespace Daqster

#endif // LOGCATEGORIES_H
