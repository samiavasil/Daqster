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
// Automatic Category Registry
// ═══════════════════════════════════════════════════════════
// Categories defined with DAQSTER_LOGGING_CATEGORY macro
// are automatically registered at static init time.
// DebugConsoleWidget reads from this registry dynamically.
// ═══════════════════════════════════════════════════════════

namespace Daqster {
namespace Log {

struct CategoryInfo {
    const char *name;
    const char *description;
};

/// Singleton registry collecting all DAQSTER_LOGGING_CATEGORY entries.
/// Thread-safe after static init (read-only during runtime).
class CategoryRegistry {
public:
    static CategoryRegistry &instance();
    void registerCategory(const char *name, const char *description);
    QVector<CategoryInfo> all() const;

private:
    CategoryRegistry() = default;
    QVector<CategoryInfo> m_categories;
};

/// Static helper — one instance per DAQSTER_LOGGING_CATEGORY per translation unit.
struct StaticRegistrar {
    StaticRegistrar(const char *name, const char *desc) {
        CategoryRegistry::instance().registerCategory(name, desc);
    }
};

// Returns all registered categories (from the global registry)
QVector<CategoryInfo> allCategories();

// Default filter rules (all OFF)
QString defaultFilterRules();

} // namespace Log
} // namespace Daqster

/// Define a logging category AND auto-register it in the global registry.
/// Use in .cpp files instead of Q_LOGGING_CATEGORY.
/// Example: DAQSTER_LOGGING_CATEGORY(lcMyPlugin, "daqster.plugin.myplugin", "My Plugin")
#define DAQSTER_LOGGING_CATEGORY(name, nameStr, desc) \
    Q_LOGGING_CATEGORY(name, nameStr) \
    static const ::Daqster::Log::StaticRegistrar DAQSTER_CAT_AUTO_##name(nameStr, desc);

#endif // LOGCATEGORIES_H
