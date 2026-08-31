#ifndef LOGCATEGORIES_H
#define LOGCATEGORIES_H

#include <QLoggingCategory>
#include <QString>
#include <QVector>
#include "build_cfg.h"

// ═══════════════════════════════════════════════════════════
// Framework core categories
// ═══════════════════════════════════════════════════════════
FRAME_WORKSHARED_EXPORT const QLoggingCategory &lcFramework();
FRAME_WORKSHARED_EXPORT const QLoggingCategory &lcRegistry();
FRAME_WORKSHARED_EXPORT const QLoggingCategory &lcDiscovery();
FRAME_WORKSHARED_EXPORT const QLoggingCategory &lcPersistence();
FRAME_WORKSHARED_EXPORT const QLoggingCategory &lcShutdown();
FRAME_WORKSHARED_EXPORT const QLoggingCategory &lcProcess();
FRAME_WORKSHARED_EXPORT const QLoggingCategory &lcPerf();

// ═══════════════════════════════════════════════════════════
// Application categories
// ═══════════════════════════════════════════════════════════
FRAME_WORKSHARED_EXPORT const QLoggingCategory &lcApp();

// ═══════════════════════════════════════════════════════════
// Plugin categories (add new plugins here)
// ═══════════════════════════════════════════════════════════
FRAME_WORKSHARED_EXPORT const QLoggingCategory &lcNodeEditor();
FRAME_WORKSHARED_EXPORT const QLoggingCategory &lcDemoNodes();
FRAME_WORKSHARED_EXPORT const QLoggingCategory &lcAiStudio();
FRAME_WORKSHARED_EXPORT const QLoggingCategory &lcLlama();
FRAME_WORKSHARED_EXPORT const QLoggingCategory &lcCoinTrader();

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
class FRAME_WORKSHARED_EXPORT CategoryRegistry {
public:
    static CategoryRegistry &instance();
    void registerCategory(const char *name, const char *description);
    QVector<CategoryInfo> all() const;

private:
    CategoryRegistry() = default;
    QVector<CategoryInfo> m_categories;
};

/// Static helper — one instance per DAQSTER_LOGGING_CATEGORY per translation unit.
struct FRAME_WORKSHARED_EXPORT StaticRegistrar {
    StaticRegistrar(const char *name, const char *desc) {
        CategoryRegistry::instance().registerCategory(name, desc);
    }
};

// Returns all registered categories (from the global registry)
FRAME_WORKSHARED_EXPORT QVector<CategoryInfo> allCategories();

// Default filter rules (all OFF)
FRAME_WORKSHARED_EXPORT QString defaultFilterRules();

} // namespace Log
} // namespace Daqster

/// Define a logging category AND auto-register it in the global registry.
/// Use in .cpp files instead of Q_LOGGING_CATEGORY.
/// Example: DAQSTER_LOGGING_CATEGORY(lcMyPlugin, "daqster.plugin.myplugin", "My Plugin")
#define DAQSTER_LOGGING_CATEGORY(name, nameStr, desc) \
    Q_LOGGING_CATEGORY(name, nameStr) \
    static const ::Daqster::Log::StaticRegistrar DAQSTER_CAT_AUTO_##name(nameStr, desc);

#endif // LOGCATEGORIES_H
