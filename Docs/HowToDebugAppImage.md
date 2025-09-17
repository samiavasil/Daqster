# Как да дебъгваме Daqster AppImage

Този документ описва различните начини за дебъгване на Daqster приложението, когато то е пакетирано като AppImage.

## Съдържание

1. [Debug AppImage (Най-лесно)](#debug-appimage-най-лесно)
2. [GDB с AppImage](#gdb-с-appimage)
3. [Valgrind за memory debugging](#valgrind-за-memory-debugging)
4. [Debug с environment variables](#debug-с-environment-variables)
5. [Extract AppImage за дебъг](#extract-appimage-за-дебъг)
6. [Debug с Qt Creator](#debug-с-qt-creator)
7. [Debug с strace](#debug-с-strace)
8. [Debug с log файлове](#debug-с-log-файлове)
9. [Debug с breakpoints](#debug-с-breakpoints)
10. [Препоръки](#препоръки)

---

## Debug AppImage (Най-лесно) 🎯

**Най-лесният начин** за дебъгване е да използваш Debug AppImage-а, който се създава автоматично в CI.

### Какво е Debug AppImage?

- ✅ **Debug символи** - включени всички debug информация
- ✅ **Stack traces** - детайлни stack traces при crashes
- ✅ **Source mapping** - връзка към source code
- ✅ **GDB ready** - може да се дебъгва с GDB
- ❌ **По-голям размер** - заради debug символите
- ❌ **По-бавно изпълнение** - debug оптимизации

### Използване:

```bash
# Debug AppImage има debug символи
./Daqster-Debug-x86_64.AppImage

# Можеш да го дебъгваш директно с GDB
gdb ./Daqster-Debug-x86_64.AppImage
```

---

## GDB с AppImage 🔧

GDB (GNU Debugger) е най-мощният инструмент за дебъгване на C++ приложения.

### Основни команди:

```bash
# Стартирай AppImage с GDB
gdb ./Daqster-x86_64.AppImage

# В GDB:
(gdb) set environment LD_LIBRARY_PATH=/tmp/.mount_Daqster*/usr/lib
(gdb) run
# или с конкретен plugin
(gdb) run --args QtCoinTrader
```

### Полезни GDB команди:

```bash
# Постави breakpoint
(gdb) break main
(gdb) break QPluginManager::LoadPlugin

# Следвай изпълнението
(gdb) step
(gdb) next
(gdb) continue

# Покажи stack trace
(gdb) bt

# Покажи променливи
(gdb) print variable_name
(gdb) info locals

# Излез
(gdb) quit
```

---

## Valgrind за memory debugging 🧠

Valgrind е отличен инструмент за откриване на memory leaks и други memory проблеми.

### Основни команди:

```bash
# Проверка за memory leaks
valgrind --leak-check=full ./Daqster-x86_64.AppImage

# Или с конкретен plugin
valgrind --leak-check=full ./Daqster-x86_64.AppImage QtCoinTrader

# Детайлна проверка
valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./Daqster-x86_64.AppImage

# Проверка за race conditions
valgrind --tool=helgrind ./Daqster-x86_64.AppImage
```

### Интерпретация на резултатите:

- **Definitely lost** - сигурни memory leaks
- **Indirectly lost** - косвени memory leaks
- **Possibly lost** - възможни memory leaks
- **Still reachable** - memory, който все още е достъпен

---

## Debug с environment variables 🔧

Можеш да включиш debug output чрез environment variables.

### Qt Debug:

```bash
# Включи Qt plugin debugging
QT_DEBUG_PLUGINS=1 ./Daqster-x86_64.AppImage

# Включи Qt logging
QT_LOGGING_RULES="*=true" ./Daqster-x86_64.AppImage

# Включи QML debugging
QML_IMPORT_TRACE=1 ./Daqster-x86_64.AppImage
```

### Daqster Debug:

```bash
# Включи нашите debug съобщения
DAQSTER_DEBUG=1 ./Daqster-x86_64.AppImage

# Или с конкретен plugin
DAQSTER_DEBUG=1 ./Daqster-x86_64.AppImage QtCoinTrader
```

### Други полезни variables:

```bash
# Покажи всички environment variables
env | grep -E "(QT|DAQSTER)"

# Включи verbose output
VERBOSE=1 ./Daqster-x86_64.AppImage
```

---

## Extract AppImage за дебъг 📦

Можеш да извлечеш AppImage в директория и да дебъгваш директно изпълнимия файл.

### Стъпки:

```bash
# 1. Извлечи AppImage
./Daqster-x86_64.AppImage --appimage-extract

# 2. Сега имаш squashfs-root/ директория
ls -la squashfs-root/

# 3. Дебъгвай директно изпълнимия файл
gdb ./squashfs-root/usr/bin/Daqster

# 4. Настрой environment variables
export LD_LIBRARY_PATH=$(pwd)/squashfs-root/usr/lib
export QT_PLUGIN_PATH=$(pwd)/squashfs-root/usr/lib/plugins
export DAQSTER_PLUGIN_DIR=$(pwd)/squashfs-root/usr/lib/daqster/plugins
```

### Предимства:

- ✅ **По-лесно дебъгване** - директно с GDB
- ✅ **По-бързо** - няма overhead от AppImage
- ✅ **По-гъвкаво** - можеш да променяш файлове

---

## Debug с Qt Creator 🛠️

Qt Creator е отличен IDE за дебъгване на Qt приложения.

### Стъпки:

1. **Extract AppImage**:
   ```bash
   ./Daqster-x86_64.AppImage --appimage-extract
   ```

2. **Отвори в Qt Creator**:
   - File → Open File or Project
   - Избери `squashfs-root/usr/bin/Daqster`

3. **Настрой debug configuration**:
   - Run → Debug
   - Настрой environment variables
   - Добави breakpoints

4. **Настрой environment**:
   ```bash
   export LD_LIBRARY_PATH=/path/to/squashfs-root/usr/lib
   export QT_PLUGIN_PATH=/path/to/squashfs-root/usr/lib/plugins
   export DAQSTER_PLUGIN_DIR=/path/to/squashfs-root/usr/lib/daqster/plugins
   ```

### Полезни функции:

- **Breakpoints** - постави breakpoints в кода
- **Step through** - следвай изпълнението стъпка по стъпка
- **Variable inspection** - проверявай стойностите на променливите
- **Call stack** - виж call stack-а

---

## Debug с strace 🔍

strace проследява системните извиквания на приложението.

### Основни команди:

```bash
# Проследи всички системни извиквания
strace ./Daqster-x86_64.AppImage

# Проследи само file операции
strace -e trace=file ./Daqster-x86_64.AppImage

# Проследи с конкретен plugin
strace -e trace=file ./Daqster-x86_64.AppImage QtCoinTrader

# Запиши в файл
strace -o daqster_trace.log ./Daqster-x86_64.AppImage
```

### Полезни опции:

```bash
# Покажи само грешки
strace -e trace=file -e trace=network -e trace=process

# Покажи timing информация
strace -t ./Daqster-x86_64.AppImage

# Покажи PID на процесите
strace -p ./Daqster-x86_64.AppImage
```

---

## Debug с log файлове 📝

Можеш да запишеш debug output в файлове за по-лесно анализиране.

### Основни команди:

```bash
# Пренасочи debug output към файл
./Daqster-x86_64.AppImage 2>&1 | tee daqster_debug.log

# Или с конкретен plugin
./Daqster-x86_64.AppImage QtCoinTrader 2>&1 | tee plugin_debug.log

# С timestamp
./Daqster-x86_64.AppImage 2>&1 | while IFS= read -r line; do echo "$(date): $line"; done | tee daqster_timestamped.log
```

### Анализиране на логовете:

```bash
# Търси за грешки
grep -i error daqster_debug.log

# Търси за warnings
grep -i warning daqster_debug.log

# Покажи последните 50 реда
tail -50 daqster_debug.log

# Покажи редове с конкретен текст
grep "Plugin" daqster_debug.log
```

---

## Debug с breakpoints ⏸️

Можеш да поставиш breakpoints в кода за по-детайлно дебъгване.

### В GDB:

```bash
gdb ./Daqster-x86_64.AppImage
(gdb) break main
(gdb) break QPluginManager::LoadPlugin
(gdb) break ApplicationsManager::StartApplication
(gdb) run
```

### В Qt Creator:

1. Отвори source файловете
2. Постави breakpoints с кликване вляво от номера на реда
3. Стартирай debug режим

### Условни breakpoints:

```bash
# Breakpoint само когато условие е изпълнено
(gdb) break QPluginManager::LoadPlugin if pluginName == "QtCoinTrader"

# Breakpoint след определен брой извиквания
(gdb) break QPluginManager::LoadPlugin
(gdb) ignore 1 5  # Игнорирай първите 5 извиквания
```

---

## Препоръки 💡

### За най-лесно дебъгване:

1. **Използвай Debug AppImage** - има всички debug символи
2. **Extract AppImage** - за по-лесно дебъгване
3. **Добави debug код** - за по-детайлна информация
4. **Използвай environment variables** - за включване на debug output

### За production debugging:

1. **Използвай log файлове** - за проследяване на проблеми
2. **Използвай strace** - за системни проблеми
3. **Използвай Valgrind** - за memory проблеми

### За development:

1. **Използвай Qt Creator** - за интерактивно дебъгване
2. **Използвай GDB** - за по-дълбоко дебъгване
3. **Използвай breakpoints** - за точково дебъгване

---

## Полезни команди за дебъгване

### Проверка на AppImage:

```bash
# Провери AppImage
file Daqster-x86_64.AppImage

# Покажи съдържанието
./Daqster-x86_64.AppImage --appimage-extract
ls -la squashfs-root/

# Провери библиотеките
ldd squashfs-root/usr/bin/Daqster
```

### Проверка на plugins:

```bash
# Провери дали plugins са намерени
./Daqster-x86_64.AppImage 2>&1 | grep -i plugin

# Провери plugin paths
./Daqster-x86_64.AppImage 2>&1 | grep -i "search paths"
```

### Проверка на environment:

```bash
# Покажи environment variables
env | grep -E "(QT|DAQSTER|APPIMAGE)"

# Покажи mounted AppImage
ls -la /tmp/.mount_*
```

---

## Заключение

Debug AppImage-ът е най-добрият избор за дебъгване, но всички методи имат своето място в различни ситуации. Избери този, който най-добре отговаря на твоите нужди!

За въпроси или проблеми, моля отвори issue в GitHub repository-то.
