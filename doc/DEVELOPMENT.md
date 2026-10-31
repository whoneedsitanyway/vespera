# Разработка

## Требования

- macOS (сборка проверена на Apple Silicon, Apple Clang).
- Command Line Tools (или Xcode) — `xcode-select -p`.
- CMake ≥ 3.22 и Ninja.

## Тулчейн

JUCE подтягивается через CMake `FetchContent` в `build/_deps/JUCE-src`
(версия зафиксирована в `CMakeLists.txt` — сейчас **8.0.6**).

Можно переиспользовать существующий checkout JUCE и пропускают загрузку:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DVESPERA_JUCE_DIR=/path/to/JUCE
```

Если cmake/ninja не установлены в систему, их удобно держать в локальном venv
(по образцу Lunara — `.tools/`, который в `.gitignore`):

```bash
cd ~/Vespera
python3 -m venv .tools
.tools/bin/pip install --upgrade pip cmake ninja
```

## Конфигурация и сборка

```bash
cd ~/Vespera
<cmake> -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_MAKE_PROGRAM="<ninja>"
<cmake> --build build --config Release
```

Например, с локальным venv:

```bash
.tools/bin/cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_MAKE_PROGRAM="$HOME/Vespera/.tools/bin/ninja"
.tools/bin/cmake --build build --config Release
```

Повторная сборка после правок кода — только вторая команда.
После правок `CMakeLists.txt` — снова обе.

Собираются и плагин, и все офлайн-тесты (таргеты `VesperaGeneratorTest`,
`VesperaSynthTest`, `VesperaPluginTest`).

## Установка

`COPY_PLUGIN_AFTER_BUILD TRUE` в `CMakeLists.txt` автоматически копирует
артефакты:

- VST3 → `~/Library/Audio/Plug-Ins/VST3/Vespera.vst3`
- AU → `~/Library/Audio/Plug-Ins/Components/Vespera.component`
- Standalone → `build/Vespera_artefacts/Release/Standalone/Vespera.app`

## Проверка AU

```bash
auval -v aumu Vspa Strv      # должно быть: AU VALIDATION SUCCEEDED
```

Коды: тип `aumu` (музыкальный инструмент), подтип `Vspa` (`PLUGIN_CODE`),
производитель `Strv` (`PLUGIN_MANUFACTURER_CODE`).

## Карта кода

```
Source/
  PluginProcessor.h/.cpp      процессор: параметры, секвенция, цепь, randomize, MIDI-экспорт
  PluginEditor.h/.cpp         UI: секции, раскладка, привязки, тема, COMPACT, MIDI-drag
  PresetManager.h             файловые пресеты (save/load XML-состояния APVTS)
  FactoryPresets.h            вшитые заводские пресеты (XML-строки, версия 8)
  DebugLogger.h/.cpp          «чёрный ящик»: снапшоты уровней, аномалии, лог-файл
  dsp/
    Scale.h                   лады, квантование, транспозиция по ступеням
    Chord.h                   диатонические аккорды из ступеней лада
    NotePool.h                пул последних нот + Latch
    PatternEngine.h           генератор паттернов (контуры, density, mutate, chord mask)
    PolySynth.h               полисинтез (24 голоса, 2 осциллятора, PolyBLEP, ADSR)
    ResonantFilter.h          TPT state-variable фильтр (LP/BP/HP) + env follower
    Lfo.h                   источник модуляции (7 форм, free/sync)
    Character.h               drive / crush / noise / wow / fold / ring
    Limiter.h                 выходной пиковый лимитер (вкл/выкл, мягкий потолок)
  ui/
    VesperaLookAndFeel.h/.cpp LookAndFeel, темы, IconButton
    PatternDisplay.h/.cpp     визуализация пула, шагов и playhead
Tests/
  GeneratorTest.cpp           офлайн-тесты стройблоков (лады, пул, паттерн, character, LFO, лимитер, rate)
  SynthTest.cpp               голоса: звук/релиз, конечность, voice stealing
  PluginTest.cpp              реальный процессор + редактор: генерация, пресеты,
                              состояние, MIDI-экспорт, Play Sync, UI-блокировки
doc/                          эта документация
```

## Тесты

Все офлайн-тесты собираются вместе с плагином (`cmake --build build`) или
поимённо: `--target VesperaGeneratorTest VesperaSynthTest VesperaPluginTest`.
Таргеты создаются хелперами `vespera_add_dsp_test` (заголовочные модули,
быстрая сборка) и `vespera_add_plugin_test` (реальный процессор + редактор)
в `CMakeLists.txt` — новый тест достаточно добавить одной строкой.

Запуск всех тестов:

```bash
cd ~/Vespera
for t in GeneratorTest SynthTest PluginTest; do
    "./build/Vespera${t}_artefacts/Release/Vespera${t}" || exit 1
done
```

Что проверяют:

- `VesperaGeneratorTest` — квантование всегда попадает в ступень лада; пул
  держит последние ноты и слушает `latch`; паттерн использует только ноты пула
  и остаётся в ладу; `density = 0` даёт тишину; `mutate` эволюционирует;
  reverse-контур зеркалится; структура фразы (мотив A B A C повторяется,
  скачки ограничены, каденция на устойчивой ступени, октава на секцию);
  аккорды диатоничны и упорядочены; character (шум гейтится, crush меняет
  сигнал, fold/ring ограничены); LFO в диапазоне и varies; лимитер держит
  горячий вход и не трогает тихий; таблица `rate` согласована с именами.
- `VesperaSynthTest` — ноты звучат и уходят в тишину; экстремальные настройки
  конечны и ограничены; voice stealing не превышает 24 голоса.
- `VesperaPluginTest` — реальный процессор: генерация audible/ограниченного
  звука, реквантование при смене Key, транспозиция по ступеням, reverse в
  дисплее, CLEAR, RANDOM (не трогает `mutate`/`noise`, всегда audible,
  засевает пул из нового лада, varied), плотный рендер не клипит,
  `keyLock`, **экспорт MIDI** (запись → чтение → note-on), заводские пресеты
  (список/установка/загрузка/фолбэк/дефолт), round-trip состояния, поведение
  редактора (дефолтный пресет, переоткрытие окна, восстановленный проект,
  снапшот отрисовки), блокировка ручки RATE при LFO Sync.

## Отладка / чёрный ящик

Редкие баги (скачок громкости, писк, шум) ловятся через `DebugLogger`:
аудио-поток пишет снапшоты в lock-free FIFO, фоновый поток ведёт лог
`~/Library/Application Support/Vespera/vespera_debug.log`. При аномалии в лог
выгружается история последних 256 блоков. Открыть лог — кнопка **LOG** в хедере
(красный бейдж = аномалия записана). Тестовые сборки (`VESPERA_TEST_BUILD=1`)
пишут во временный каталог и не затирают пользовательский журнал.
Подробнее — в [ARCHITECTURE.md](ARCHITECTURE.md), раздел «Диагностика».

## CI

GitHub Actions: `.github/workflows/windows.yml` собирает **VST3 для Windows
x64** на `windows-latest` при push/PR в `master` (и по `workflow_dispatch`) и
складывает артефакт `Vespera-VST3-Windows-x64`. Сборка использует `FetchContent`
для JUCE, поэтому отдельных шагов установки фреймворка не нужно.

macOS-сборка (VST3 + AU + Standalone) и прогон офлайн-тестов выполняются
локально: CI-раннер для macOS в этом проекте не настроен.

## Как добавить параметр

1. Добавить id в `vespera::ids` (`PluginProcessor.h`).
2. Создать параметр в `createParameterLayout()` (`PluginProcessor.cpp`) —
   хелперы `addFloat` / `addInt` / `addBool` / `addChoice`.
3. Добавить указатель в `RawParams` (`PluginProcessor.h`) и биндинг в
   `bindRawParams()` — в `processBlock` параметр читается как
   `rawParams.<имя>->load()` (без строковых лукапов в audio-потоке).
4. Добавить контрол в редакторе (`addKnob`/`addCombo`/`ToggleButton`) и в `resized()`.
5. Если параметр попадает в журнал — добавить поле в `BlockSnapshot`
   (`DebugLogger.h`) и его вывод в `formatLine()`.
6. Обновить [PARAMETERS.md](PARAMETERS.md).

## Полезно

- `randomizeParameters()` — пример программного изменения параметров через
  `setValueNotifyingHost` (`setReal` конвертирует реальное значение в 0..1).
- `buildSeedNotes()` / `publishSeed()` / `applyPendingSeed()` — безопасная
  передача данных из message-потока в audio-поток (SpinLock + атомарный счётчик).
- `getRateNames()` / `rateInBeats()` — таблица делений шага.
- `getContourNames()` — таблица контуров генератора.
- `Lfo::getNames()` — формы модуляции.
