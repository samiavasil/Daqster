# PERF: Video Display Performance Results — 2026-08-13

**Версия на документа:** 1.0 (първоначален снапшот)
**Дата на измерванията:** 2026-08-13
**Repo / branch:** `daqster` (public) — `feat/REQ-SW-PL-027-video-profiling`
**Статус на кода по време на измерванията:** работно дърво по време на
изследването (shadow fix + GL blit + NV12-direct); след това закрепено в
локални комити (вж. `git log` и секция „Ключови изводи").
**Сурови данни:** `/tmp/opencode/glblit/` (логове, perf.data, ps snapshot-и, скриптове)

> Този документ е пълен снапшот на перформанс изследването на video display
> пайплайна (декуплиран detached GL blit прозорец), включително методологията,
> числовите резултати преди/след shadow-fix-а, perf анализа на тясното място,
> направените промени и следващите лостове. Служи за по-късна проверка и за
> репликация на измерванията.

---

## 1. Методология

### 1.1 Харнес (измерителна среда на процеса)

Всички прогони ползват един и същ харнес:

- **Изпълним файл:** `build_qtX/bin/Daqster` (Qt5 или Qt6), модул `NodeEditorIde`.
- **Blocking stdin:** `tail -f /dev/null |` ПРЕДИ стартирането на Daqster —
  **задължително**. Без него `QConsoleListener` (Qt console listener) спinning-ва
  на stdin и idle CPU е ~180% (измерено: `idle_cpu.txt` показва 181-183% при
  празен graph БЕЗ blocking stdin). С blocking stdin idle е ~1-2%.
- **Изолиран HOME:** всеки прогон получава чист `HOME=/tmp/opencode/glblit/<label>_home`
  (първичен QSettings/прозоречни state-ове, без замърсяване между прогоните).
- **Автостарт на video graph:** `DAQSTER_AUTOSTART_VIDEO=1` + `DAQSTER_VIDEO_FILE=<файл>`
  строят `VideoFileSource -> VideoOutput` граф програмно, натискат „Play" и
  включват чекбокса „Perf" (без GUI интеракция). GL blit пътят се включва с
  `DAQSTER_GL_BLIT=1`.
- **Лог конфигурация:** `--log-console-enabled 1 --log-level Info`.
- **Перф метрика (FW-008 + PL-027):** конзолен ред на всеки 5 s:
  `[PERF] video | SW | fmt=<pf> | handle=<h> | fps=<n> | gap=<ms> | present=<ms> | total=<ms> | cpu=<%>`
  — `cpu=` е self-CPU на процеса (Sample CPU, Linux `/proc/self/stat`).
- **Външна проверка:** `ps -o pcpu= -p <pid>` семпли на всеки 2 s за 30-40 s.
- **Perf профилиране:** `perf record -F 499 --call-graph dwarf` върху процеса за
  30-40 s. Perf binary-ят е разопакован в `/tmp/opencode/glblit/perf` (perf_pkg)
  и се стартира с `LD_LIBRARY_PATH=/tmp/opencode/glblit env -u DEBUGINFOD_URLS`
  (избягва DEBUGINFOD мрежовите заявки, които забавят/спират resolution-а).
- **Продължителност на прогон:** 30-40 s (след ~8 s settle време за warm-up на
  playback + GL blit); GLBLIT avg стойностите се четат в steady state
  (frames ≥ 600-750).

### 1.2 Video източник

- **Файл:** `bars_h265_1080p25_x3.mp4` — синтетичен тестов клип:
  - кодек H.265/HEVC, резолюция **1920×1080 (1080p)**, 25 fps,
  - продължителност **120 s** (3× повторение на 40 s bars клип, `concat`),
  - pix_fmt (ffprobe): yuv420p; в `QVideoFrameFormat` на Qt6 декодерът го
    нормализира до **NV12** (1920×1080, ColorSpace_BT601, HandleType NoHandle).
- Реални потребителски файлове (h264/h265, yuvj420p/yuv420p) също се
  нормализират от Qt6 FFmpeg backend-а до NV12 (GLBLIT-FMT проверка,
  `probe_*.log`) — референтният синтетичен файл е представителен.

### 1.3 Метрики

1. **CPU% (основната метрика):** self-CPU на процеса от PERF реда + ps %CPU
   като кръстосана проверка.
2. **present/total:** PERF микро-сегменти (output.present = време за предаване
   на кадъра на display; output.total = цялата обработка на кадъра в
   `VideoOutputNode`).
3. **GLBLIT avg:** самозапис в `VideoGLBlitWidget::logBlitStats()` на всяка
   150-та рамка: `GLBLIT | fmt=<pf> | frames=<n> | avg=<us> | max=<us> | failures=<n>`
   (реално map+upload+draw в paintGL).
4. **fps / gap:** фактически кадров темп и междинна празнина от source-а.

---

## 2. Околна среда

| Параметър | Стойност |
|---|---|
| OS | Linux (X11, DISPLAY=:0) |
| Qt версии | Qt **5.15.2** (`/mnt/Builder/bin/Linux/Qt/5.15.2/gcc_64`) и Qt **6.9.2** (`/mnt/Builder/bin/Linux/Qt/6.9.2/gcc_64`) |
| GPU | **Hybrid**: Intel Iris Xe (iGPU) + NVIDIA **RTX 3070** (dGPU) |
| GL контекст | GL 4.6, Compatibility profile (Qt default, `useCore=0`), `DAQSTER_GL_MATRIX=bt709`, `DAQSTER_GL_RANGE=full` |
| GL blit | `DAQSTER_GL_BLIT=1` (експериментален detached GL прозорец вместо QVideoWidget/QPixmap) |
| Build dirs | `build_qt5/`, `build_qt6/` |
| Отчетна метрика | self-CPU% на `Daqster` процеса (PERF ред) + ps %CPU |

---

## 3. Числени резултати (таблици)

### 3.1 Qt6 — GL blit път (NV12 шейдър)

| Сценарий | CPU% (self, steady) | Забележка |
|---|---|---|
| **Преди shadow fix** | **~36%** (35.0-38.8%; `probe_shadow_before.log`, `perf_qt6_gl.log`) | QGraphicsDropShadowEffect активен |
| **След shadow fix** | **17.1-18.8%** (16.9-22.0%; `probe_shadow_after.log`, `ab_phase12*.txt`, `qt6_gl_postshadow.log`) | ShadowEnabled=false (глобално + per-node) |
| Idle (без video) | **~1-2%** | blocking stdin; празен graph |

`present=0.0ms`, `total=0.0-1.4ms` (след shadow: ~0.0-0.5ms), `fps=25`.

### 3.2 Qt5 — софтуерен (без GL) път

| Сценарий | CPU% (self, steady) | Източник |
|---|---|---|
| **Преди shadow fix** | **57-61%** (55.3-57.6%; `live_qt5_nogl.log`) | QImage → QPixmap + smooth-scale |
| **След shadow fix** | **34.2%** (33.0-36.0% PERF; ps avg ~34.9; `qt5_postshadow_a.out`) | без shadow |
| Idle | **1.1%** (ps avg; `qt5_postshadow_c_ps.txt`) | blocking stdin |

### 3.3 Qt5 — GL blit път (QImage upload, SPIKE)

| Сценарий | CPU% (self, steady) | Източник |
|---|---|---|
| **Преди shadow fix** | **42.8%** (39.8-47.0%; `perf_qt5_gl.log`) | GL blit + shadow |
| **След shadow fix** | **27.9%** (27.0-30.2% PERF; ps avg ~30.3; `qt5_postshadow_b.out`) | GL blit без shadow |

> Забележка: тези стойности са с Qt5 GL пътят, който към момента на измерването
> качва **QImage** (RGBA текстура) — Qt5 все още конвертира NV12→QImage в source.
> След NV12-direct за Qt5 (Variante A, Част 3) конверсията трябва да изчезне от
> hot path-а и CPU да спадне допълнително (~20% или по-малко).

### 3.4 GLBLIT blit cost (самозапис в paintGL)

| Qt | fmt | avg (steady) | fps | failures |
|---|---|---|---|---|
| Qt6 | NV12 | **~308 µs** (frames=750: 308.5 µs; `qt6_gl_postshadow.log`) | 25 | 0 |
| Qt5 | QImage(5) | **~370 µs** (траектория 2621→1266→959→792→585 µs за frames 150-750; `qt5_gl_postshadow.log`) | 25 | 0 |

> GLBLIT avg спада от ~1700-2600 µs (първите 150 кадъра, JIT/текстурна
> инициализация + warm-up) до steady state след ~600+ кадъра.

### 3.5 Qt5 NV12-direct (Variante A, след Част 3) — НОВО

| Метрика | Преди (Qt5 GL + QImage) | След (Qt5 GL + NV12-direct) |
|---|---|---|
| CPU% (self, steady) | **27.9%** | **15.0-16.2%** (PERF cpu=; ps avg ~19.4) |
| GLBLIT avg (steady) | ~585 µs (QImage upload) | **44.5-57.8 µs** (NV12 шейдър) |
| GLBLIT fmt | `QImage(5)` | **`NV12` — без toImage** |
| fps / failures | 25 / 0 | **25 / 0** |
| Qt6 контрола | 17.8-18.2% (непроменено) | 17.8-18.2% (непроменено) |

> Източник: `qt5_gl_nv12direct.out` (40 s прогон, `measure_qt5_nv12.sh`).
> Qt5 `qt_convert_NV12_to_ARGB32` изчезва от hot path-а: source-ът транспортира
> owned NV12 `QVideoFrame` (VideoCompat::frameToOwnedFrame), GL widget-ът го
> качва като YUV текстури и конвертира в шейдъра. Спестяването е **~12 pt CPU**
> (27.9% → ~15.5%) и **10× по-бърз blit** (585 → ~50 µs).

### 3.6 Резюме CPU%

| Път | Qt | Преди shadow | След shadow | След NV12-direct |
|---|---|---|---|---|
| GL blit | Qt6 | ~36% | **17.1-18.8%** | **17.8-18.2%** |
| SW (QPixmap) | Qt5 | 57-61% | **34.2%** | — (пътят е заменен от GL) |
| GL blit | Qt5 | 42.8% | **27.9%** | **15.0-16.2%** |

### 3.7 GL blit DEFAULT за Qt5 + auto-fallback + UI toggle (REQ-SW-PL-021) — НОВО

Потребителско решение: GL blit става **default за Qt5** (най-бързият път), Qt6 остава
на native `QVideoWidget`. Env var `DAQSTER_GL_BLIT` е debug override само за стартиране
(стойността има значение — `=0` форсира софтуерен път, `=1` форсира GL); след старт
чекбоксът **„GPU display"** (Qt5) има последната дума. При недостъпен GL контекст има
auto-fallback към софтуерния път (лог `GL fallback: <причина>`), без crash.

Измервания (2026-08-13, финален binary, `measure_gl_default.sh`, 45 s прогони, bars_h265_1080p25_x3.mp4):

| Сценарий | CPU% (self, steady) | Display | Забележка |
|---|---|---|---|
| **Qt5 default** (без env) | **16.0-17.6%** | GL blit | GLBLIT `fmt=NV12` avg ~68-132 µs (steady), failures=0, fps=25; `ps` ~19-20% |
| **Qt5 `DAQSTER_GL_BLIT=0`** | **33.8-36.0%** | софтуерен | `total=4.4-4.8ms`/кадър (NV12→ARGB32 + QLabel); fps=25 |
| **Qt6 default** (без env) | **17.0-17.4%** | native QVideoWidget | няма GLBLIT редове (GL не е активен) |
| Qt5 `DAQSTER_GL_BLIT=1` | ~19-20% | GL blit (форсиран) | същият път като Qt5 default; измерването паралелно с Qt6 run (contention) |
| Qt6 `DAQSTER_GL_BLIT=1` | ~19-20% | GL blit (форсиран) | A/B пътят работи и на Qt6; NV12, failures=0 |

**GLBLIT-FMT (NV12, без toImage) на Qt5:** временният GLBLIT-FMT probe от по-ранната
сесия не е комитван; сегашният `GLBLIT | fmt=NV12 | ... | failures=0` ред потвърждава
NV12 шейдърния път — липсва суфикс `-> toImage(N)`, който се появява при RGB fallback
(avg ~50-130 µs е несъвместим с per-frame QImage upload ~370-585 µs).

**Auto-fallback тест:** `glPlatformAvailable()` временно форсиран да върне false
(симулация VM/remote без GL) → лог `GL fallback: no GL context can be created —
switching to software display`, GL прозорец не се създава, видеото се показва в
embedded QLabel, чекбоксът се отмаркира; повторното маркиране fallback-ва веднага
(session guard `m_glFailed`). Scratch тестове (не се комитват): `/tmp/opencode/reattach/`.

**Регрес на прозореца (закачи/разкачи/закачи):** scratch тест срещу реалния X11 display
потвърждава и на Qt5 (GL прозорец) и на Qt6 (native QVideoWidget): закачане → прозорец
видим; разкачане → изчезва; повторно закачане → пак се появява. UI toggle „GPU display"
на Qt5: uncheck → GL прозорецът се затваря и следващият кадър минава по софтуерен път;
re-check → GL прозорецът се отваря отново.

---

## 4. Perf анализ на тясното място (perf categories)

### 4.1 Qt5-GL (след shadow, `qt5_gl_postshadow_top.txt`, `perf_qt5_gl.txt`)

| Категория | Дял | Символ/път |
|---|---|---|
| **NV12→ARGB32 конверсия в source** | **~27%** (self 26.78% след shadow; 17.48% преди shadow — делът расте, защото shadow-ът е свален) | `qt_convert_NV12_to_ARGB32` ← `QVideoFrame::image()` ← `VideoFileSourceNode::onFrameAvailable` |
| GL JIT/display (QOpenGL upload/draw) | ~25% | GL пътят в `VideoGLBlitWidget` |
| **nodeeditor repaint (QPixmap scale)** | **~23%** (12.08% `qt_scale_image_32bit` след shadow; 21.87% преди) | `QRasterPaintEngine::drawPixmap` ← `QGraphicsScenePrivate::drawItemHelper` |
| Shadow blur | ~5-12% | `expblur<12,10,true>` (QGraphicsDropShadowEffect) — виж 4.3 |

> Qt5 нямаше NV12-direct: probe кадърът се конвертираше веднага в source
> (`QVideoFrame::image()`), така че CPU-то на конверсията оставаше в hot path-а.
> **След Част 3 (NV12-direct) тази категория изчезва от hot path-а** — source-ът
> транспортира owned NV12 QVideoFrame, GL шейдърът конвертира на GPU (15.0-16.2% CPU).

### 4.2 Qt6-GL (след shadow, `qt6_gl_postshadow_top.txt`, `ivan_full25_self.txt`)

| Категория | Дял | Коментар |
|---|---|---|
| **nodeeditor scene repaint** | **~49%** | `QBezier::toPolygon` (24.9% — curve flattening на connection-ите), image convert (~7.1%), + address-only Qt6Gui символи в 0x2949xx диапазона (drawImage/scene paint, инлайнати) |
| **display/GL (upload+draw)** | **~17%** | `cuMemcpy2DAsync_v2` (2.7%), `QVideoFrame::map` + upload, glDraw |
| **decode (HW)** | **~3.6%** | CUDA HW decode (libcuda), пренебрежимо |

> Qt6 decode НЕ е проблем: HW decode (CUDA) е ~0.5-3.6% от CPU. Bottleneck-ът
> след shadow-fix-а е **repaint на nodeeditor scene-а** (сгъване на кривите на
> connection-ите + scene drawImage) и GL display upload.

### 4.3 Корен на първоначалния проблем (shadow)

- **QGraphicsDropShadowEffect** върху нодовите екземпляри е основният скрит
  консуматор при video playback: при 25 fps всеки кадър тригерира repaint на
  сцената, а shadow-ът прави **expblur** върху node-графиката при всяко рисуване.
- Наблюдавано: Qt6 **36% → 17.6%** CPU само от изключване на shadow-а
  (глобално в `DefaultStyle.json` + per-node гаранции); `expblur` ~5-12% на Qt5.
- Защо точно при video: при статичен граф без video shadow-ът се рисува веднъж;
  при 25 fps scene-ът се инвалидира всеки кадър (входният node показва нов
  кадър) и shadow blur-ът се изпълнява per-repaint на цялата node-графика.

---

## 5. Какво е направено (измененията)

1. **`nodeeditor` submodule `resources/DefaultStyle.json`:** `ShadowEnabled: true → false`
   (глобално изключване на shadow-а по подразбиране). Суровият стил може да го
   включи обратно, ако потребител го изиска.
2. **Per-node гаранции:** `ShadowEnabled=false` директно в конструкторите на
   display нодовете (защита дори ако глобален стил го включи):
   - `VideoOutputNode` (`Sources/Video/VideoOutputNode.cpp`)
   - `DaqDisplayNode` (`Displays/DaqDisplay/DaqDisplayNode.cpp`)
   - `NumberDisplayDataModel` (`node_editor_ide/BuiltInNodes/Displays/NumberDisplay/NumberDisplayDataModel.cpp`)
   - `QDevIoDisplayModelObsolete` (`node_editor_ide/BuiltInNodes/Library/display/QDevIoDisplayModelObsolete.cpp` —
     покрива и `AudioDisplayModelObsolete` по наследство)
3. **GL blit (експериментален detached GL прозорец):** `VideoGLBlitWidget`
   (`Sources/Video/VideoGLBlitWidget.{h,cpp}`) — QOpenGLWidget, който презентира
   кадрите през YUV→RGB шейдър (NV12 / YUV420P) или QImage (RGB fallback).
   Включва се с `DAQSTER_GL_BLIT=1`; параметри `DAQSTER_GL_MATRIX`, `DAQSTER_GL_RANGE`,
   `DAQSTER_GL_FORCE_CORE` за A/B.
4. **NV12-direct за Qt5 (Variante A):** `VideoFrameData` вече не е Qt6-gated;
   Qt5 source-ите транспортират OWNED NV12/YUV420P `QVideoFrame`
   (`VideoCompat::frameToOwnedFrame` — map + memcpy по plane-ове в QByteArray,
   `QAbstractPlanarVideoBuffer` subclass); `VideoOutputNode` има два входа и на
   Qt5 (video-frame@0 → GL, image@1 → SW); `VideoGLBlitWidget` е un-gated за Qt5
   (NV12/YUV420P шейдърен път, QImage fallback за RGB формати). Неподдържан
   формат → invalid frame → source пада на QImage. **Преномерация:** Qt5 порт 0
   стана video-frame (беше image) — стари saved Qt5 графи с image@0 ще загубят
   връзката (документирано в REQ-SW-PL-020/022, append-last модела).
5. **Vsync/swap fix (Qt5 GL):** на този NVIDIA GLX setup `QOpenGLWidget`
   repaint-ът е throttled до ~1 Hz по подразбиране (проверено с минимален
   standalone repro), което задушава видео пайплайна до 1 fps. Фикс:
   `QSurfaceFormat::setSwapInterval(0)` + `setDefaultFormat` в конструктора на
   `VideoGLBlitWidget` → свободни presents → 25 fps.
6. **Task 4 (geometry gate) — ОТХВЪРЛЕН с данни:** опитът да се ограничи
   per-frame repaint чрез geometry gate (проверки `QGraphicsItem`/scene geometry)
   беше тестван и отхвърлен — не даваше съществена полза, а добавяше сложност;
   виж `shot_geom.sh`, `crops_*.png`, `statics_*.png`, `main_*.png`, `raise_*.png`
   (доказателства, че прозорецът/geometry не е източник на допълнителни репейнти
   след shadow-fix-а).

---

## 6. Ключови изводи

1. **Decode не е проблем.** HW decode (Qt6 CUDA) е ~0.5-3.6% CPU; SW FFmpeg
   decode също е нисък (2-3% при 1080p25). Целта „HW decode" от по-ранните фази
   беше грешна посока — истинският разход е display/repaint.
2. **Shadow е коренът на първоначалния CPU скок.** `QGraphicsDropShadowEffect`
   ~46% от CPU-то при video playback (36% → 17.6% на Qt6); изключен глобално +
   per-node.
3. **Bottleneck след shadow = repaint (Qt6) / конверсия (Qt5).**
   - Qt6: nodeeditor scene repaint ~49% (QBezier curve flattening + drawImage);
     display/GL ~17%; decode ~3.6%.
   - Qt5: NV12→ARGB32 конверсия в source ~27%; nodeeditor repaint ~23%; GL ~25%.
4. **Следващи лостове (документирани, не имплементирани):**
   - Qt5 NV12-direct (Variante A) — **ИМПЛЕМЕНТИРАН** (Част 3): Qt5 GL пада от
     27.9% на **15.0-16.2%** CPU; GLBLIT от ~585 µs на ~50 µs; `qt_convert_NV12_to_ARGB32`
     изчезва от hot path-а.
   - Qt6: намаляване на scene repaint-а — connection curve flattening
     (QBezier::toPolygon 24.9%) и drawImage; кандидати: по-груба апроксимация на
     connection-кривите, статичен cache на scene pixmap при video, throttling на
     scene инвалидацията.
   - GPU zero-copy (REQ-SW-PL-021) — HW текстура → GL без CPU map/upload.
5. **Vsync throttling (Qt5 GLX):** открит е environment-level проблем — Qt5
   `QOpenGLWidget` swap-ът е throttled до ~1 Hz на NVIDIA GLX (минимален standalone
   repro), което задушава видео пайплайна до 1 fps. Фикснат с
   `setSwapInterval(0)`. Ако се появи отново 1 fps при video, първо провери това.

---

## 7. Изходни данни и репликация

Всички сурови данни и скриптове са в `/tmp/opencode/glblit/`:

| Файл | Съдържание |
|---|---|
| `probe_shadow_before.log` / `probe_shadow_after.log` | Qt6 GL преди/след shadow (PERF редове) |
| `probe_shadow_final.log`, `probe_shadow_confirm.log` | Qt6 допълнителни потвърждения |
| `qt6_gl_postshadow.log` (+ .data/.top.txt) | Qt6 GL след shadow (PERF + GLBLIT + perf profile) |
| `live_qt5_nogl.log` / `live_qt5_gl.log` | Qt5 преди shadow (без GL / с GL) |
| `qt5_postshadow_a.out`, `qt5_postshadow_b.out`, `qt5_postshadow_c.out` | Qt5 след shadow: SW / GL / idle |
| `qt5_gl_postshadow.log` (+ .data/.top.txt) | Qt5 GL след shadow (perf profile) |
| `qt5_gl_nv12direct.out` | **Qt5 GL след NV12-direct (Част 3)** — 25 fps, 15.0-16.2% CPU, GLBLIT ~50 µs NV12, failures=0 |
| `measure_qt5_nv12.sh` | Скрипт за Qt5 GL NV12-direct прогон |
| `perf_qt5_gl.data/.txt/.log`, `perf_qt6_gl.data/.txt/.log` | perf record/report данни |
| `ivan_full25.data/.log/.self.txt` | Qt6 25 s пълен профил |
| `qt6_gui_raw.txt`, `qt6_gui_addrs.txt` | Qt6Gui адрес-решени символи |
| `measure_qt5_postshadow.sh`, `perf_profile.sh`, `measure_ab.sh` | Скриптове за прогон |
| `idle_cpu.txt`, `idle2_cpu2.txt`, `idle3_cpu.txt` | Idle без/с blocking stdin (~180% / ~1%) |
| `shot_*.sh`, `crops_*.png`, `statics_*.png`, `main_*.png`, `raise_*.png` | Task 4 (geometry gate) доказателства |

**Репликация (пример):**

```bash
# Qt6 GL blit след shadow
tail -f /dev/null | env HOME=/tmp/opencode/glblit/repro_home DISPLAY=:0 \
  LD_LIBRARY_PATH=/mnt/Builder/bin/Linux/Qt/6.9.2/gcc_64/lib:build_qt6/bin \
  DAQSTER_GL_BLIT=1 DAQSTER_AUTOSTART_VIDEO=1 DAQSTER_VIDEO_FILE=/tmp/opencode/glblit/bars_h265_1080p25_x3.mp4 \
  build_qt6/bin/Daqster NodeEditorIde --log-console-enabled 1 --log-level Info

# Qt5 GL blit след shadow
tail -f /dev/null | env HOME=/tmp/opencode/glblit/repro5_home DISPLAY=:0 \
  LD_LIBRARY_PATH=/mnt/Builder/bin/Linux/Qt/5.15.2/gcc_64/lib:build_qt5/bin \
  DAQSTER_GL_BLIT=1 DAQSTER_AUTOSTART_VIDEO=1 DAQSTER_VIDEO_FILE=/tmp/opencode/glblit/bars_h265_1080p25_x3.mp4 \
  build_qt5/bin/Daqster NodeEditorIde --log-console-enabled 1 --log-level Info
```
