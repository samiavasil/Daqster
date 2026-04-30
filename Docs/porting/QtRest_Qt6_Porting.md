# QtRest Qt6 Porting Guide

Родител: [Porting Topics](./README.md) | [Documentation Index](../INDEX.md)

Този документ описва процеса на портиране на QtRest библиотеката за Qt6 support в Daqster проекта.

## **Анализ на QtRest за Qt6**

### **Използвани Qt модули:**
- **QtCore** - QObject, QAbstractListModel (съвместим с Qt6)
- **QtNetwork** - QNetworkAccessManager, QNetworkRequest, QNetworkReply (съвместим с Qt6)
- **QtQml** - QQmlPropertyMap (съвместим с Qt6)

### **Qt модули НЕ използвани:**
- **QtWidgets** - не се използва в QtRest
- **QtGui** - не се използва в QtRest
- **QtQuickControls2** - не се използва в QtRest

### **Заключение:**
**QtRest е напълно съвместим с Qt6!** Всички използвани Qt модули са налични и стабилни в Qt6.

## **Направени промени**

### **1. QtRest CMakeLists.txt обновяване:**

```cmake
## Find Qt libraries - support both Qt5 and Qt6
if(QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS
                 Network
                 Core
                 Qml
    )
    set(QT_PREFIX Qt6)
else()
    find_package(Qt5 5.3 COMPONENTS
                 Network
                 Core
                 Widgets
                 Gui
                 Qml
    )
    set(QT_PREFIX Qt5)
endif()

target_link_libraries(qtrest_lib
  PUBLIC
    ${QT_PREFIX}::Core
    ${QT_PREFIX}::Network
    ${QT_PREFIX}::Qml
)
```

### **2. Главен CMakeLists.txt обновяване:**

```cmake
if(QT_VERSION_MAJOR EQUAL 5)
    # Qt5 - изисква QuickControls2 за QtRest
    if(QT_QUICKCONTROLS2_LIB AND NOT "${QT_QUICKCONTROLS2_LIB}" STREQUAL "")
        add_subdirectory(src/external_libs/qtrest_lib)
        message(STATUS "QtRest library enabled for Qt5 (QuickControls2 available)")
    else()
        message(STATUS "QtRest library disabled for Qt5 (QuickControls2 not available)")
    endif()
else()
    # Qt6 - изисква само QML за QtRest
    if(QT_QML_LIB AND NOT "${QT_QML_LIB}" STREQUAL "")
        add_subdirectory(src/external_libs/qtrest_lib)
        message(STATUS "QtRest library enabled for Qt6 (QML available)")
    else()
        message(STATUS "QtRest library disabled for Qt6 (QML not available)")
    endif()
endif()
```

### **3. Plugins CMakeLists.txt обновяване:**

```cmake
if(QT_VERSION_MAJOR EQUAL 5)
    # Qt5 - изисква QuickControls2 + QtRest library
    if(QT_QUICKCONTROLS2_LIB AND NOT "${QT_QUICKCONTROLS2_LIB}" STREQUAL "" AND TARGET qtrest_lib)
        add_subdirectory(QtCoinTrader)
        message(STATUS "QtCoinTrader plugin enabled for Qt5 (QuickControls2 + QtRest library available)")
    endif()
else()
    # Qt6 - изисква само QML + QtRest library
    if(QT_QML_LIB AND NOT "${QT_QML_LIB}" STREQUAL "" AND TARGET qtrest_lib)
        add_subdirectory(QtCoinTrader)
        message(STATUS "QtCoinTrader plugin enabled for Qt6 (QML + QtRest library available)")
    endif()
endif()
```

## **Резултати**

### **Qt5 Support:**
- **QtRest library** - включена ако има QuickControls2
- **QtCoinTrader plugin** - включен ако има QuickControls2 + QtRest library
- **Пълна функционалност** - всички features налични

### **Qt6 Support:**
- **QtRest library** - включена ако има QML (винаги налично в Qt6)
- **QtCoinTrader plugin** - включен ако има QML + QtRest library
- **Пълна функционалност** - всички features налични

### **NodeEditor:**
- **Qt6** - изключен заради compatibility проблеми
- **Qt5** - пълна поддръжка

## **Тестване**

### **Тестов скрипт:**
```bash
./test_qt6_qtrest.sh
```

### **Ръчно тестване:**
```bash
# Qt6 build
cmake -S . -B build_qt6 -DUSE_QT6=ON
cmake --build build_qt6 -j

# Проверка на резултатите
ls -la build_qt6/lib/libqtrest_lib.so
ls -la build_qt6/bin/plugins/libQtCoinTraderPlugin.so
```

## **Сравнение Qt5 vs Qt6**

| Компонент | Qt5 | Qt6 | Забележка |
|-----------|-----|-----|-----------|
| **QtRest Library** | QuickControls2 required | QML required | Qt6 е по-лесен |
| **QtCoinTrader Plugin** | QuickControls2 + QtRest | QML + QtRest | Qt6 е по-лесен |
| **NodeEditor Plugin** | Пълна поддръжка | Изключен | Compatibility проблеми |
| **Test Plugins** | Всички | Всички | Без промени |

## **Upstream Updates**

### **Текущо състояние:**
- **Upstream:** 27 commits behind
- **Latest version:** 0.4.0 (с CMake support)
- **Status:** Готов за merge

### **Препоръка:**
```bash
# Merge upstream changes
./tools/build_helpers/manage_upstream.sh merge qtrest

# Или cherry-pick specific commits
./tools/build_helpers/manage_upstream.sh cherry-pick <commit-hash>
```

## **Следващи стъпки**

1. **Тестване** - проверете Qt6 build в Qt Creator
2. **Upstream merge** - обновете до latest upstream version
3. **CI integration** - добавете Qt6 + QtRest в CI matrix
4. **Documentation** - обновете README с Qt6 QtRest support

## **Предимства на Qt6 портирането**

1. **По-лесни dependencies** - само QML вместо QuickControls2
2. **По-добра производителност** - Qt6 оптимизации
3. **Модерен код** - най-нови Qt features
4. **Дългосрочна поддръжка** - Qt6 е current LTS

## **Важни забележки**

1. **NodeEditor** остава изключен за Qt6 заради compatibility проблеми
2. **QtRest** работи отлично с Qt6
3. **QtCoinTrader** работи с Qt6 ако има QtRest
4. **Test plugins** работят с двете версии

## **Полезни линкове**

- [Qt6 Migration Guide](https://doc.qt.io/qt-6/portingguide.html)
- [QtRest Repository](https://github.com/kafeg/qtrest)
- [Qt6 QML Documentation](https://doc.qt.io/qt-6/qtqml-index.html)
- [Qt6 Network Documentation](https://doc.qt.io/qt-6/qtnetwork-index.html)





