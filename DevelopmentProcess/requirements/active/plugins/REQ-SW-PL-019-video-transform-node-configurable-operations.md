# REQ-SW-PL-019: Video Transform Node — Configurable Operations (VideoTransformNode)

- **Статус:** DONE
- **Приоритет:** P2
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-06
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-018

## Описание

`VideoModifierNode` (REQ-SW-PL-018, комити `e5b7309`/`ef1b66c`) е фиксиран demo
node с една операция (размяна на R↔B каналите). Това изискване го надгражда до
**общ video transform node** с конфигурируеми операции:

1. **Преименуване:** `VideoModifierNode` → **`VideoTransformNode`** (регистрация
   `"VideoModifier"` → `"VideoTransform"`). Старите saved scenes с `"VideoModifier"`
   се чупят — прието (няма production сцени).
2. **8 базови операции** (винаги налични, QImage-базирани, без външни
   зависимости): RGB Channel Swap (R↔B, без параметри), Grayscale (без
   параметри), Invert (без параметри), Brightness (slider −100..+100),
   Contrast (slider 0..200%), Blur (radius slider 0..10), Flip (combo
   horizontal/vertical), Sepia (без параметри).
3. **Опционален OpenCV (compile-time auto-detect):** `find_package(OpenCV QUIET)`
   в CMakeLists-а на plugin-а; ако е открит — `HAVE_OPENCV` + `OpenCVTransforms.cpp`
   с 3 допълнителни операции: GaussianBlur (kernel slider), Canny (2 threshold
   sliders), Threshold (value slider). QImage↔cv::Mat конверсията е в този файл.
   Ако OpenCV липсва — plugin-ът се build-ва нормално с 8-те базови операции;
   OpenCV операциите се появяват в списъка само когато `HAVE_OPENCV` е компилиран.
4. **Архитектура:** op engine (per-op `QImage + params → QImage` функции,
   static/namespace стил по модела на `VideoCompat`), widget = operation combo +
   `QStackedWidget` с по една страница параметри за операция, model = тънък
   controller с `save()`/`load()`, персистиращи текущата операция + параметрите.

## Acceptance Criteria

- [x] 1. **Преименуване + регистрация.** `Sources/Video/VideoModifierNode.{h,cpp}`
       → `Sources/Video/VideoTransformNode.{h,cpp}` — клас `VideoTransformNode`
       (`NodeDelegateModel`), `caption() = "Video Transform"`,
       `name() = "VideoTransform"`; `DemoNodeEditorNodesObject::registerNodes()`
       регистрира `VideoTransformNode` под категория "Video" (заменя
       `registerModel<VideoModifierNode>`); `CMakeLists.txt` обновява
       `VIDEO_NODES` списъка. Старите сцени с `"VideoModifier"` не се зареждат —
       прието.
- [x] 2. **8 базови операции (QImage, без външни зависимости).** Op engine с
       per-op функции `QImage + params → QImage` (namespace/static стил):
       RGB Channel Swap (R↔B, без параметри), Grayscale (без параметри),
       Invert (без параметри), Brightness (slider −100..+100), Contrast
       (slider 0..200%), Blur (radius slider 0..10), Flip (combo
       horizontal/vertical), Sepia (без параметри). Операциите се появяват
       динамично в списъка; всяка обработва входящия кадър и емитира
       модифицирано `ImageData`.
- [x] 3. **Widget.** Embedded widget: operation combo + `QStackedWidget` с по
       една страница параметри за операция; смяна на операцията превключва
       страницата и пре-обработва текущия кадър с новите параметри.
- [x] 4. **save()/load().** Моделът персистира текущата операция + параметрите
       в `QJsonObject`; `load()` възстановява състоянието на widget-а.
- [x] 5. **Опционален OpenCV (compile-time auto-detect).** `find_package(OpenCV
       QUIET)` в `CMakeLists.txt`; при открит OpenCV — `HAVE_OPENCV` +
       `OpenCVTransforms.cpp` с 3 операции: GaussianBlur (kernel slider), Canny
       (2 threshold sliders), Threshold (value slider); QImage↔cv::Mat
       конверсията е в този файл. При липсващ OpenCV plugin-ът се build-ва
       нормално с 8-те базови операции; OpenCV операциите присъстват в списъка
       само при `HAVE_OPENCV`.
- [x] 6. **Tests.** Unit тестове за op engine-а (`TestVideoTransformOps`,
        известни пиксели за swap/invert/grayscale/brightness/contrast/blur/
        flip/sepia + null-input contract; param ranges и clamping) — добавени
        2026-08-07 след вдигане на standing инструкцията (комит `bac4503`).
        Qt5 + Qt6 builds; съществуващата suite остава зелена.

## Проследимост

- **Коммити:** `e574c63` (feat), `efc05c2` (docs: създаване на изискването),
  `0cf6d19` (fix: Qt5 vertical flip), `bac4503` (test: video suite) —
  branch `feat/phase3-graph-matrix`
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/Video/` (`VideoTransformNode.{h,cpp}`,
  op engine — напр. `VideoTransformOps.{h,cpp}`, `OpenCVTransforms.cpp` —
  conditional при `HAVE_OPENCV`), `DemoNodeEditorNodesObject.cpp` (registerNodes),
  `CMakeLists.txt` (OpenCV auto-detect)
- **Документация:** `docs/plugins/demo_nodeditor_nodes/README.md` (VideoTransformNode — операции секция)
- **Тестове:** Qt5 (5.15.2) + Qt6 (6.9.2) builds PASS и за двата варианта
  (с/без OpenCV): OpenCV path (DAQSTER_USE_OPENCV=ON, OpenCV 4.6.0 dev
  инсталиран за тази верификация) и fallback (DAQSTER_USE_OPENCV=OFF — 8-те
  базови операции, без OpenCV sources/links); `demo_nodeditor_nodes_tests` —
  `TestVideoTransformOps` **16/16 PASS** и на двете версии (swap, grayscale,
  invert, brightness clamps, contrast, blur 0/uniform/radius-1, flip h/v,
  sepia, null input); fix `0cf6d19` — Qt5 вертикален flip (`mirrored()`);
  app smoke (Qt5/Qt6, DISPLAY=:0) — приложенията стартират без crash.

## Бележки по имплементацията (план)

- **CMake структура.** `find_package(OpenCV QUIET)` се добавя в
  `demo_nodeditor_nodes/CMakeLists.txt` (преди `create_plugin`); при
  `OpenCV_FOUND` — `OpenCVTransforms.cpp` се добавя към source списъка и
  `HAVE_OPENCV`/`${OpenCV_INCLUDE_DIRS}`/`${OpenCV_LIBS}` се подават през
  поддържаните `COMPILE_DEFINITIONS`/`INCLUDE_DIRECTORIES`/`LINK_LIBRARIES`
  параметри на `create_plugin()`. Като защита заглавният файл също пази
  `#include <opencv2/opencv.hpp>` зад `#ifdef HAVE_OPENCV`.
- **Крос-платформена сигурност.** `find_package(OpenCV QUIET)` е безопасен и на
  Windows (официалният OpenCV installer доставя CMake config); `OpenCV_LIBS`
  са пълни пътища. Runtime DLL-ите на OpenCV трябва да са на `PATH` само когато
  се ползват OpenCV операциите — извън обхват, отбелязано за deployment.
- **QImage↔cv::Mat gotcha-та.** cv::Mat е BGR по подразбиране, QImage е RGB —
  да се работи през `QImage::Format_RGB888` (същия модел като
  `FrameToTensorNode.cpp`) и изрично `cv::COLOR_RGB2GRAY`/`COLOR_RGB2BGR`;
  `bytesPerLine` на QImage може да надвишава `width*3` (32-bit alignment) — при
  конструкция на `cv::Mat` от QImage buffer да се подаде `bytesPerLine` като
  step; ARGB32 да се конвертира преди това; `cv::Mat`, обвиващ QImage bits, не
  трябва да надживява QImage-а (резултатът се копира обратно в нов QImage);
  Grayscale изход (Canny/Threshold) да се емитира като RGB32 grayscale за
  еднородност на пайплайна.
- **Регистрация/save-load.** Промяната на `name()` сменя registry key-а —
  стари flow файлове с `"VideoModifier"` не се резолвват (прието). Новите
  save/load ключове: `"operation"`, `"brightness"`, `"contrast"`,
  `"blurRadius"`, `"flipMode"`, `"gaussianKernel"`, `"cannyLow"`,
  `"cannyHigh"`, `"thresholdValue"`.
- Изходът на node-а остава `ImageData` ("image") — частният AI Studio pipeline
  (`FrameToTensorNode`, REQ-AI-006) продължава да работи без промени.

## Бележка

Изискването е създадено **преди** имплементацията (2026-08-06) по одобрен
дизайн (rename + 8 базови операции + опционален OpenCV). Имплементацията и
верификацията са завършени на 2026-08-06: AC 1–5 са mark-нати `[x]`
(имплементация + Qt5/Qt6 builds и с двата варианта — с/без OpenCV; app smoke
без crash). AC 6 (Tests) е mark-нат на 2026-08-07 след вдигане на standing
инструкцията (комит `bac4503`); по пътя беше открит и поправен реален Qt5
bug: вертикален flip (`0cf6d19`, `mirrored()` call). Всички AC 1–6 са `[x]`;
статус → **DONE**; файлът остава в `active/` (архивирането е отделно
решение, по прецедента на PL-009/010/016).
