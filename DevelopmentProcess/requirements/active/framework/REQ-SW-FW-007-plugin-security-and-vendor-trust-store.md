# REQ-SW-FW-007: Plugin Security & Vendor Trust Store

- **Статус:** ACTIVE (roadmap — не е планирана за текущия спринт; имплементацията се планира отделно)
- **Приоритет:** P2
- **Отговорник (роля):** Implementation
- **Дата:** 2026-08-06
- **Родител:** —
- **Зависи от:** REQ-SW-FW-001 (архив: archive/framework/REQ-SW-FW-001-plugin-manager-core.md), REQ-SW-FW-002 (архив: archive/framework/REQ-SW-FW-002-plugin-discovery-persistence-and-registry.md)

## Описание

MD5 hash в персистентността на плъгините (`PluginPersistence::computeFileHash()`,
REQ-SW-FW-002 AC3) удостоверява само **целостта на файла** (дали се е променил), но
**не** гарантира автентичност на доставчика — всеки може да компилира плъгин и да го
зарежда. Това изискване добавя криптографска верификация на вендорите към
framework-а, изпълнявана **преди** плъгинът да бъде зареден през `QPluginManager`
(load path-ът от REQ-SW-FW-001 AC1):

1. **Vendor Trust Store** — компонент на framework-а (`frame_work/base`), който
   съхранява публични ключове на доверени доставчици (PEM), с add/remove/list
   управление и персистентност през `PluginPersistence`/`QSettings` (аналогично на
   hash персистентността от REQ-SW-FW-002 AC3).
2. **Signature Verification** — плъгините се подписват дигитално (частен ключ на
   доставчика); при зареждане подписът се верифицира срещу публичния ключ от Trust
   Store, като допълва съществуващия MD5 hash (целост + автентичност).
3. **Security Policy**:
   - *Strict* (Production) — зареждат се само плъгини с валиден подпис от доверен
     доставчик; unsigned/untrusted се отказват.
   - *Permissive* (Development) — неподписаните се зареждат с предупреждение
     (warning) в GUI и `qCWarning` лог.
4. Политиката е конфигурируема през `QSettings`/потребителски настройки, разделно
   за Qt5/Qt6 конфигурационните файлове (както REQ-SW-FW-001 AC4).

**Бележка (companion):** това е framework-страната (публичното repo) на частната
плъгин-сигурност в DaqsterAiStudio (code signing & vendor trust store за частните
плъгини). Съгласно конвенцията public→private референции не се записват в
публичните файлове; private→public посоката (Зависи от) се поддържа в частното
дърво.

## Acceptance Criteria

- [ ] 1. `VendorTrustStore` (frame_work/base) поддържа регистър на доверени
       доставчици: публични ключове (PEM) могат да се добавят, премахват и
       изброяват; състоянието се персистира през `PluginPersistence`/`QSettings`.
- [ ] 2. Верификацията на подписа се изпълнява ПРЕДИ зареждане на библиотеката:
       load path-ът на `QPluginManager`/`QPluginLoaderExt` проверява подписа на
       файла срещу Trust Store.
- [ ] 3. Strict режим: unsigned/untrusted плъгин се отказва (НЕ се зарежда) с
       лог-съобщение; зареждат се само плъгини с валиден подпис от доверен
       доставчик.
- [ ] 4. Permissive режим: unsigned/untrusted плъгин се зарежда, но се маркира
       визуално в GUI (`QPluginListView`) и се логва предупреждение (`qCWarning`).
- [ ] 5. Политиката (Strict/Permissive) е конфигурируема през
       `QSettings`/потребителски настройки, разделно за Qt5/Qt6
       конфигурационните файлове.
- [ ] 6. Подписът допълва съществуващия MD5 hash (REQ-SW-FW-002 AC3) — няма
       регресия на hash персистентността (целост + автентичност).

## Проследимост

- **Коммити:** — (ACTIVE roadmap — имплементацията се планира отделно)
- **Код:** (plan) `src/frame_work/base/src/` — `QPluginManager.cpp` load path,
  `discovery/PluginDiscovery.{h,cpp}`, `persistence/PluginPersistence.{h,cpp}`,
  нов `VendorTrustStore.{h,cpp}`
- **Документация:** `docs/Architecture/framework/QPluginManager.md` (секция
  "Future Roadmap: Plugin Security & Vendor Verification", ~ред 174);
  `docs/Architecture/README.md` (секция "11. Future Enhancements → Security →
  Code signing", ~ред 329)
- **Тестове:** — (roadmap; при имплементация: Qt5 + Qt6 builds, unit тестове,
  headless smoke test по RDD-PROCESS.md)
