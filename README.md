# DZP Player Locker (`@DZP_PlayerLocker`)

Персональное внутриигровое хранилище для **DayZ 1.29** (build 1.29.163047).
**Полная перезапись старого мода под API 1.29** — версия `1.1.0`.

- На карте стоит **невидимая точка-куб 1×1** в настраиваемых координатах (п. 1.1).
- Курсор на точке → стандартный селектор действий: **F — «Открыть хранилище»** (п. 1.2),
  **колесо мыши — «Расширить хранилище на 10»** (п. 1.3). Кастомных UI нет (п. 1.3/2.4).
- «Открыть» спавнит **личный невидимый ящик** на 1.5 м перед игроком и открывает
  **штатный TAB-инвентарь** (cargo слева, как у WoodenCrate) (п. 1.2, 4.2).
- Хранилище **персональное, серверное**: 10 стартовых слотов, +10 за покупку, до 100 (п. 1.4).
- Валюта — `SmallStone` (1 камень = 1 шаг = 10 слотов, настраивается) (п. 1.4).
- Предметы видны **только владельцу**; данные лежат на сервере (п. 1.5).
- Конфиг и пер-плеер JSON создаются автоматически (п. 1.6–1.7).

---

## Структура репозитория

```text
@DZP_PlayerLocker/
├── mod.cpp                          # v1.1.0 (корень мода, НЕ внутри PBO)
├── DZP_PlayerLocker.pbo             # собранный архив (13 записей, ~55–60 КБ)
└── src/DZP_PlayerLocker/            # исходники для сборки PBO
    ├── config.cpp                   # CfgPatches / CfgMods / CfgVehicles
    ├── $PBOPREFIX$                  # DZP_PlayerLocker
    └── Scripts/
        ├── 3_Game/DZP_PlayerLocker.c
        ├── 4_World/
        │   ├── DZP_ActionRegistration.c
        │   ├── DZP_LlockerPoint.c
        │   ├── DZP_OpenStorageAction.c
        │   ├── DZP_ExpandStorageAction.c
        │   ├── DZP_StorageContainer.c
        │   ├── DZP_StorageManager.c
        │   └── DZP_PlayerStorageData.c
        └── 5_Mission/
            ├── MissionServer.c
            └── MissionGameplay.c
tools/build_pbo.py                   # сборка и проверка PBO
```

`config.cpp → CfgMods.defs.files[]` подключает ровно три каталога:
`DZP_PlayerLocker/scripts/3_game`, `.../scripts/4_world`, `.../scripts/5_mission`.

---

## Установка

1. Скопируйте папку `@DZP_PlayerLocker` в `steamapps/common/DayZ Server/` (и клиенту — в `.../DayZ/`).
2. Запуск сервера: `-mod=@DZP_PlayerLocker` (в серверный `.cmd`/`serverConfig`).
3. Клиент тоже должен запускаться с `-mod=@DZP_PlayerLocker` (папка на **обеих** сторонах).
4. При первом старте сервер создаст:
   - `$profile:PlayerLocker/config.json`;
   - `$profile:PlayerLocker/data/`.

---

## Конфиг (`$profile:PlayerLocker/config.json`)

Создаётся автоматически при первом старте (п. 1.6):

```json
{
  "enabled": true,
  "pointX": 7500.0,
  "pointY": 500.0,
  "pointZ": 7500.0,
  "currencyItem": "SmallStone",
  "pricePerStep": 1,
  "slots": {
    "initial": 10,
    "step": 10,
    "max": 100
  }
}
```

| Поле | Значение |
|---|---|
| `enabled` | `false` — точка не спавнится, действия недоступны |
| `pointX/Y/Z` | координаты невидимой точки (Y — высота, с которой объект опускается на поверхность) |
| `currencyItem` | classname валюты (по умолчанию `SmallStone`) |
| `pricePerStep` | цена одного расширения на `slots.step` слотов (по умолчанию `1`) |
| `slots.initial` | стартовые слоты (кратно 10, 10..100) |
| `slots.step` | прибавка за покупку (кратно 10) |
| `slots.max` | максимум (кратно 10, ≤100) |

Значения слотов округляются до кратных 10 — габариты ящика задаются
классами `DZP_LlockerBox_10 … DZP_LlockerBox_100` (5×2 … 10×10 клеток).

---

## Данные игроков (`$profile:PlayerLocker/data/<UID>.json`)

```json
{
  "slots": 20,
  "items": [
    { "classname": "BandageDressing", "quantity": 0.0, "health": 100.0 }
  ]
}
```

Сохранение: при **покупке**, при **выходе** игрока, **каждые 5 минут**,
при **закрытии** ящика (отход ≥15 м) и при **остановке миссии** (п. 1.7).
После рестарта сервера содержимое восстанавливается при следующем открытии.

---

## Пользовательский сценарий (п. 4)

1. **Старт сервера** → чтение конфига → спавн `DZP_LlockerPoint`
   (`SetAllowDamage(false)`, `SetInvisible(true)`).
2. **F на точке** → сервер спавнит личный ящик в 1.5 м перед игроком и пишет
   в чат `ChatMP` («Хранилище открыто…»); клиент через ~800 мс
   (`CallLater`) вызывает штатный `ShowInventory()` → TAB.
   Ящик в секции «вокруг», клик — cargo слева.
3. **Колесо мыши на точке** → «Расширить хранилище на 10» → сервер:
   - проверяет максимум (отказ с причиной в чате);
   - **списывает ровно N** `SmallStone` (при нехватке — отказ в чате с причиной);
   - увеличивает слоты на `step` (до `max`), пересоздаёт ящик большего габарита
     с сохранением содержимого, сохраняет файл, пишет в чат успех.
4. **Отход ≥15 м** (проверка раз в 2 с) → ящик сохраняется и удаляется,
   игроку приходит сообщение. **Выход игрока** → сохранение файла.
5. Предметы других игроков недоступны: свой ящик спавнится только своему UID.

---

## Логи RPT

Префикс всех строк: `[DZP_PlayerLocker]` (п. 1.10), пишется в server RPT:

```text
[DZP_PlayerLocker] config.json not found, created with defaults: $profile:PlayerLocker/config.json
[DZP_PlayerLocker] config: enabled=true, point=[7500 500 7500], currency=SmallStone, pricePerStep=1, slots initial=10 step=10 max=100
[DZP_PlayerLocker] locker point spawned
[DZP_PlayerLocker] session created for 76561198000000000, slots=10
[DZP_PlayerLocker] storage opened for 76561198000000000
[DZP_PlayerLocker] expanded 76561198000000000: 10 -> 20, paid 1 SmallStone
[DZP_PlayerLocker] expand refused (max) for 76561198000000000
[DZP_PlayerLocker] saved 76561198000000000 (logout), slots=20, items=7
[DZP_PlayerLocker] player data saved: $profile:PlayerLocker/data/76561198000000000.json
```

Ошибки спавна помечаются `ERROR:`, восстановления — `WARNING:`.

---

## Сборка PBO

```bash
python3 tools/build_pbo.py
```

Скрипт упаковывает ровно 12 файлов из `src/DZP_PlayerLocker` + properties-запись
`prefix=DZP_PlayerLocker` и проверяет:

```text
  records (Vers + files) : 13 (Vers=1, files=12)
  entry present          : scripts/4_world/dzp_actionregistration.c -> OK
  prefix                 : DZP_PlayerLocker
  size                   : 56283 bytes (55.0 KB)
  SHA1 trailer           : OK
VERIFY OK: 13 records, prefix=DZP_PlayerLocker, size and SHA1 OK
```

Формат: запись `Vers` первой, файлы uncompressed (`method=0`), terminator,
данные подряд, хвост `0x00 + SHA1(заголовок‖данные)` — как в ванидных DayZ-архивах.

---

## Реализация под API DayZ 1.29

| Вопрос | Решение (сверено с dayzexplorer **1.29.163047** и `BohemiaInteractive/DayZ-Script-Diff`) |
|---|---|
| Регистрация действий | `modded class ActionConstructor` → `RegisterActions(TTypenameArray)`; без этого `AddAction` молчит (п. 2.2) |
| Действия | `ActionInteractBase`; `CCINone` + `CCTCursor`; `CMD_ACTIONMOD_INTERACTONCE`, `STANCEMASK_ALL`, `CursorIcons.None` (п. 2.1) |
| Второй пункт меню | стандартный wheel-селектор 1.29; текст собирается в `ActionCondition` из netsync-поля точки (п. 2.4) |
| Открытие инвентаря | клиент: `GetGame().GetMission().ShowInventory()` через `CallLater(…, 800)` (2-арг. форма, п. 2.5/2.9) |
| Невидимость | `Entity.SetInvisible(true)` в `EEInit()` — локальный рендер на каждой машине (п. 1.1) |
| Ящик | `Container_Base` без Open/Close (п. 2.6); спавн `CreateObjectEx(..., ECE_PLACE_ON_SURFACE)`, удаление `ObjectDelete` (п. 2.7) |
| Валюта | `GetQuantity()` → **float**; у ванильного `SmallStone` нет `varQuantity` (несложенный) → подсчёт `HasQuantity() ? GetQuantity() : 1`, списание `SetQuantity()` / `ObjectDelete()` поштучно (п. 2.11, крит. 6.4) |
| Сеть | `RegisterNetSyncVariableInt("m_ExpandStep", …)` + `SetSynchDirty()` (п. 2.12) |
| UID | `PlayerDisconnected(PlayerBase, PlayerIdentity, string uid)` (п. 2.13) |
| Чат | `GetGame().ChatMP(player, text, "colorAction")` (п. 2.8/1.9) |
| Сервер/клиент | серверные вызовы в `5_Mission` — `#ifdef SERVER`, клиентские — `#ifndef SERVER` (п. 2.16) |
| UI | **нет** кастомного UI и stringtable — все строки захардкожены (п. 1.3, запрет §7) |

Соглашения соблюдены: папки `src/DZP_PlayerLocker/...` (источники внутри `@DZP_PlayerLocker`),
`mod.cpp` v1.1.0, PBO содержит только §3-структуру, тексты действий — ровно
«Открыть хранилище» / «Расширить хранилище на 10» (шаг подставляется из конфига).

---

## Changelog

### 1.1.0 — полная перезапись под DayZ 1.29 API

- Переписаны все скрипты под API **1.29.163047** (устаревшие вызовы удалены).
- Действия зарегистрированы через `modded ActionConstructor` (иначе в 1.29 не работают).
- `SetInvisible(true)` в `EEInit()` — точка и ящик невидимы на сервере и клиентах.
- Открытие — только штатный `ShowInventory()` через `CallLater(800)` (кастомных меню в 1.29 нет).
- Расширение — второй пункт wheel-селектора, текст из netsync-шага конфига.
- Валюта: учтено, что ванильный `SmallStone` несложенный — списание ровно N штук через
  `EnumerateInventory(PREORDER)` + `SetQuantity`/`ObjectDelete`.
- Пер-плеер `data/<UID>.json`: сохранение при покупке, выходе, каждые 5 минут,
  закрытии ящика и остановке миссии; автозакрытие при отходе ≥15 м.
- `$profile:PlayerLocker/config.json` создаётся автоматически с умолчаниями.
- Логи `[DZP_PlayerLocker]` в server RPT; уведомления — `ChatMP`/`colorAction`.
- Сборка PBO — `tools/build_pbo.py` (13 записей, SHA1-проверка).
