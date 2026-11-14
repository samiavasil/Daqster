# DependencyManager — декларативно управление на външни артефакти (as-built)

Родител: [Архитектура](./README.md) · REQ-SW-PL-088

## Какво прави и къде живее

Външните артефакти (бинари, библиотеки, тегла, гласове, изходен код) се
описват в **един манифест** — `dependencies.json` — вместо да се въвеждат ръчно
в скриптове, env променливи и абсолютни пътища. Мениджърът прави скучната
половина:

```
манифест → проверка (status) → (install | adopt) → lockfile
```

Той не знае нищо за AI, каталози или нодове: `kind` е данни, а `provides` е
информационно (връзването с `ModelCatalog`/`ServerCatalog` е работа на
консуматора, виж REQ-AI-023).

| Какво | Къде |
|---|---|
| хедър (схема + договор) | `src/plugins/common/Dependencies/DependencyManager.h` |
| имплементация | `src/plugins/common/Dependencies/DependencyManager.cpp` |
| общата библиотека | `DaqsterCommonLibrary` (`Qt::Core` + `Qt::Network`) |
| пространство от имена | `Daqster::DependencyManager`, `Daqster::DependencySpec`, `Daqster::DependencyStatus` |
| тестовете | `tests/plugins/common/test_dependency_manager.{h,cpp}` |
| лог категория | `daqster.dependencies` |

## Манифест `dependencies.json`

```json
{
  "version": 1,
  "dependencies": [
    {
      "id": "piper",
      "kind": "binary",
      "version": "1.2.0",
      "url": "https://example.com/piper_linux_x86_64.tar.gz",
      "sha256": "<64 lowercase hex>",
      "size": 12345678,
      "file": "piper",
      "provides": ["binaries/piper"],
      "executable": true
    }
  ]
}
```

| Поле | Значение |
|---|---|
| `id` | уникален; може да е вложен (`voice/bg_BG-dimitar-medium`) |
| `kind` | `binary` \| `library` \| `model` \| `voice` \| `source` |
| `version` | свободен текст (`0.8.0`, `8642541b2`, `2023-09`) |
| `url` | само `https://`; празен = **само adopt** |
| `sha256` | 64 малки hex знака; **задължителен** при непразен `url` |
| `size` | байтове; `0` = неизвестен (проверява се само checksum) |
| `file` | име на файла в `<root>/<id>/<version>/` |
| `provides` | роли/видове в каталога — информационно |
| `executable` | информационно |

Валидацията е **all-or-nothing**: при липсващо поле, повторен `id`,
непознат `kind`, не-https `url` или невалиден/липсващ `sha256` целият
манифест се отхвърля и предишните `specs()` остават непокътнати.

## Инсталационно дърво и lockfile

```
<root>/
├── deps.lock.json
├── piper/1.2.0/piper
└── voice/bg_BG-dimitar-medium/2023-09/bg_BG-dimitar-medium.onnx
```

`deps.lock.json` се пише от `install()`/`adopt()` и записва за всяко id:
`{id, version, path, sha256, adopted}`. `status()` първо гледа lockfile-а (за да
провери осиновен файл където реално живее), после `<root>/<id>/<version>/<file>`.

## Атомарна инсталация

`install()` сваля през `QNetworkAccessManager` в `QSaveFile` (временен файл в
**същата** директория), смята sha256 **в движение** и едва след съвпадение прави
`commit()` (атомарен `rename`). При грешка, отказ или разминаване се вика
`cancelWriting()` — **не остава partial файл**. Единствено `install` за дадено
`id` тече в даден момент. Няма `waitFor*()` и няма shell-out.

## Adopt-first миграция

Днес артефактите вече са на диска (`../daqster_dependencies`, `../llama.cpp`,
`../llama_models`, `/mnt/Data/whisper-models`). `adopt(spec, existingPath)`
проверява размер + sha256 и записва **абсолютния** път в lockfile-а, без да
копира нищо. Така миграцията не изисква повторно теглене, а `status()` започва
да пази и вече съществуващите файлове. Спецификация само с `id`/`version`/`file`
(без `url` и без `sha256`) е валидна adopt-only: изчисленият при adopt sha256 се
записва и става очакваната стойност за следващите проверки (trust-on-first-use).

`install()` при празен `url` отказва с ясен текст и препраща към `adopt()`.

## Сигурност

- **Само HTTPS.** `http://`, `file://` и големи пътища се отхвърлят с грешка,
  която казва какво е получено. Пренасочванията са
  `NoLessSafeRedirectPolicy` (https → http е забранено), а след края се
  проверява и финалната схема.
- **sha256 е задължителен** за всяко теглене и се проверява **преди**
  преместването на файла на мястото му. Липсващ/невалиден checksum = отказ,
  не „ще мине и без него“.
- **Без post-install shell** и без изпълнение на съдържание от манифеста.
- `id`/`version`/`file` не могат да съдържат `..`, празни сегменти, `\` или
  `:` — манифестът не може да излезе извън `<root>`.
- Lockfile-ът пази локални пътища и не бива да се commit-ва, когато сочи към
  машиноспецифични adopt-нати файлове.

## Какво още не е тук

- `update`/`rollback` (нова `<version>/` директория + diff) — следваща фаза.
- `source` build рецепти — те са частни (AI) и не живеят в общия мениджър.
- Офлайн кеш по sha256 — фаза 3 от плана (`deps-cache/`).
