# Upstream Management Guide

Този документ описва как да управлявате upstream tracking за external библиотеките в Daqster проекта.

## 📚 **External Libraries**

### **NodeEditor**
- **Upstream Repository:** https://github.com/paceholder/nodeeditor
- **Your Fork:** https://github.com/samiavasil/nodeeditor
- **Current Status:** 182 commits behind upstream, 4 commits ahead
- **Branch:** `node_editor_plugin` (custom branch with your modifications)

### **QtRest**
- **Upstream Repository:** https://github.com/kafeg/qtrest
- **Your Fork:** https://github.com/samiavasil/qtrest
- **Current Status:** 27 commits behind upstream, 0 commits ahead
- **Branch:** `master` (tracking upstream)

## 🛠️ **Upstream Management Script**

Използвайте `tools/manage_upstream.sh` скрипта за лесно управление на upstream updates:

### **Основни команди:**

```bash
# Покажи текущото състояние
./tools/manage_upstream.sh status

# Провери за нови промени
./tools/manage_upstream.sh check

# Изтегли последните промени от upstream
./tools/manage_upstream.sh fetch

# Слей промени от upstream
./tools/manage_upstream.sh merge nodeeditor
./tools/manage_upstream.sh merge qtrest
./tools/manage_upstream.sh merge all

# Cherry-pick конкретен commit
./tools/manage_upstream.sh cherry-pick <commit-hash>
```

## 📋 **Workflow за Upstream Updates**

### **1. Редовна проверка (месечно):**
```bash
# Провери за нови промени
./tools/manage_upstream.sh check

# Ако има нови промени, изтегли ги
./tools/manage_upstream.sh fetch
```

### **2. Анализ на промените:**
```bash
# Виж детайлно какво е ново в NodeEditor
cd src/external_libs/nodeeditor
git log HEAD..upstream/master --oneline -10
git diff HEAD..upstream/master --stat

# Виж детайлно какво е ново в QtRest
cd src/external_libs/qtrest_lib/qtrest
git log HEAD..upstream/master --oneline -10
git diff HEAD..upstream/master --stat
```

### **3. Избор на стратегия за update:**

#### **A. Пълен merge (за QtRest):**
```bash
# QtRest е по-малко модифициран, може да се merge-не директно
./tools/manage_upstream.sh merge qtrest
```

#### **B. Cherry-pick (за NodeEditor):**
```bash
# NodeEditor има много custom модификации
# Cherry-pick само нужните commits
./tools/manage_upstream.sh cherry-pick <commit-hash>
```

#### **C. Създаване на integration branch:**
```bash
# За големи updates, създай отделен branch
cd src/external_libs/nodeeditor
git checkout -b integration-upstream-$(date +%Y%m%d)
git merge upstream/master
# Разреши конфликтите и тествай
```

## ⚠️ **Важни съображения**

### **NodeEditor специфики:**
- **Много custom модификации** - 4 commits ahead of upstream
- **182 commits behind** - значителни промени в upstream
- **Препоръка:** Cherry-pick само security updates и critical bug fixes
- **Избягвай:** Major version updates без внимателен анализ

### **QtRest специфики:**
- **По-малко модификации** - 0 commits ahead
- **27 commits behind** - умерени промени
- **Препоръка:** Може да се merge-ва по-често
- **Внимание:** Проверявай за Qt6 compatibility

## 🔧 **Ръчни операции**

### **Добавяне на upstream remote:**
```bash
# NodeEditor
cd src/external_libs/nodeeditor
git remote add upstream https://github.com/paceholder/nodeeditor.git

# QtRest
cd src/external_libs/qtrest_lib/qtrest
git remote add upstream https://github.com/kafeg/qtrest.git
```

### **Fetch upstream changes:**
```bash
git fetch upstream
```

### **Cherry-pick specific commit:**
```bash
git cherry-pick <commit-hash>
```

### **Merge upstream changes:**
```bash
git merge upstream/master
```

## 📊 **Monitoring и Alerts**

### **GitHub Notifications:**
- Настройте notifications за upstream repositories
- Следете за security advisories
- Проверявайте за major version releases

### **Automated Checks:**
```bash
# Добавете в cron за месечна проверка
0 0 1 * * /path/to/Daqster/tools/manage_upstream.sh check
```

## 🚨 **Troubleshooting**

### **Merge conflicts:**
```bash
# Виж конфликтите
git status
git diff

# Разреши конфликтите ръчно
# След това:
git add .
git commit
```

### **Cherry-pick conflicts:**
```bash
# Отмени cherry-pick
git cherry-pick --abort

# Или разреши конфликтите
git add .
git cherry-pick --continue
```

### **Upstream remote not found:**
```bash
# Провери remote-ите
git remote -v

# Добави upstream ако липсва
git remote add upstream <upstream-url>
```

## 📝 **Best Practices**

1. **Редовни проверки** - поне веднъж месечно
2. **Тестване** - винаги тествайте след updates
3. **Backup** - създавайте backup преди големи промени
4. **Документиране** - записвайте какво сте cherry-pick-нали
5. **Security first** - приоритизирайте security updates

## 🔗 **Полезни линкове**

- [Git Cherry-pick Guide](https://git-scm.com/docs/git-cherry-pick)
- [Git Merge Strategies](https://git-scm.com/docs/merge-strategies)
- [GitHub Fork Management](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/working-with-forks)
- [NodeEditor Repository](https://github.com/paceholder/nodeeditor)
- [QtRest Repository](https://github.com/kafeg/qtrest)
