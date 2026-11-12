# Layout на зависимостите — един корен, видовете са подпапки (as-built)

Родител: [Архитектура](./README.md) · REQ-SW-PL-091 · произвежда се от
REQ-SW-PL-089 (`DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-089-delivery-infrastructure.md`
в частното repo `DaqsterAiStudio`)

## Дървото

```
<DEPS>/llama.cpp/build/bin/{llama-server,llama-mtmd-cli,llama-tts}
<DEPS>/whisper.cpp/build/bin/whisper-server
<DEPS>/piper/piper              (+ espeak-ng-data до него)
<DEPS>/piper-voices/<voice>.onnx (+ .onnx.json)
<DEPS>/sd-server
<DEPS>/models/{llm,asr,vlm,embedding,tts,sd}/…
```

`<DEPS>` е стойността на токена `DAQSTER_DEPS` (машинна таблица
`~/.config/Daqster/scene-variables.json` → среда). Единственото място, което
знае подпътищата, е `Daqster::SceneLayout`
(`src/plugins/common/Scene/SceneLayout.{h,cpp}`): `llamaBuildRoot()`,
`llamaBinDir()`, `modelsDir()`, `modelsDirFor(kind)`, `piperBin()`,
`piperVoicesDir()`, `whisperBin()`, `sdServer()`.

## Правилото

**Един корен; видът е подпапка.** Няма втори токен на вид
(`DAQSTER_LLM_DIR`, `DAQSTER_ASR_DIR`…): нов вид е нова подпапка в `models/`,
не нова променлива. Причината е измерена — дотук всеки консуматор слепваше
своя подпът и вторият консуматор можеше да слепи друг.

`DAQSTER_LLAMA` и `DAQSTER_LLAMA_MODELS` остават **derived**: `SceneVariables`
ги отговаря от корена (`<DEPS>/llama.cpp/build` и `<DEPS>/models`), но едва
след override → таблица → среда. Изрична стойност печели, тоест машина,
настроена преди това правило, продължава да работи; машина, която задава само
`DAQSTER_DEPS`, вече не задава нищо друго. Липсващ корен дава **празно** +
причина, не `/<подпът>`.

## Кои записи може да са симлинкове и защо

| Запис | Може ли симлинк | Защо |
|---|---|---|
| `llama.cpp/build` | да | `llama.cpp` се строи **на място** извън корена (2 GB) и се линква, вместо да се копира |
| `piper-voices` | да | гласовете се теглят отделно и живеят на партицията с място |
| `models/*` | да | теглата са 30 GB; `fetch.sh` има правило за размер и `/mnt/Builder` е пълен |
| `whisper.cpp/build/bin` | не | строи се заедно с whisper.cpp, но `bin/` може да е симлинк към него |
| `piper/piper`, `sd-server` | не | самодостатъчни бинарни файлове с `RUNPATH=$ORIGIN` |

`SceneLayout` отговаря за **пътища**, не за съществуване: счупен симлинк не бива
да превръща отговора в „не е конфигурирано" — който консумира пътя, проверява
файла и казва кой липсва (същата конвенция като `scripts/run-*-flow.sh`).

## Кой произвежда това дърво

Бъдещият инсталатор (`REQ-SW-PL-089`, виж пътя по-горе) трябва да произведе
**точно** това дърво: един корен, фиксирани подпътища, симлинкове там, където
артефактът е built-in-place или на друга партиция. Дотук то се сглобява ръчно и
затова `DAQSTER_LLAMA` беше настройка; след инсталатора е следствие.
