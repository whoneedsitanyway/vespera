# MIDI-выход — план реализации

> **Статус:** дизайн-документ, фича **не реализована**.
> **Дата:** 2026-11-09, версия плагина 1.0.0.
> **Связанные файлы:** `CMakeLists.txt`, `Source/PluginProcessor.*`,
> `Source/PluginEditor.*`, `Tests/PluginTest.cpp`, `doc/ARCHITECTURE.md`,
> `doc/PARAMETERS.md`.
>
> Это не пользовательская документация, а рабочий план: что менять, в каком
> порядке и какие подводные камни учесть. Номера строк приведены по состоянию
> на дату выше и могут сместиться после правок.

## 1. Цель и границы

Сейчас Vespera умеет только **экспортировать** паттерн в Standard MIDI File
(`writePatternMidiFile()`) и перетаскивать его в DAW. Нужен **живой MIDI-выход**:
сгенерированные ноты должны в реальном времени уходить на другие инструменты.

Сценарии:

1. **VST3 в DAW:** Vespera стоит на дорожке, её MIDI-выход маршрутизируется на
   другой инструмент (внутренний или внешний).
2. **Standalone:** Vespera отправляет MIDI в виртуальный порт
   (macOS — IAC Driver, Windows — loopMIDI), оттуда — в другую программу.
3. **Экспорт SMF** остаётся как был — для офлайн-работы.

Не входит в задачу:

- MIDI-вход как источник нот (уже есть).
- MIDI clock / start / stop / continue (отдельная задача).
- MPE, poly aftertouch, MIDI 2.0.
- Автоматическая маршрутизация на конкретный инструмент — это делает хост.

## 2. Что есть сейчас

| Место | Факт |
|---|---|
| `CMakeLists.txt:63` | `NEEDS_MIDI_OUTPUT FALSE` |
| `Source/PluginProcessor.h:129` | `producesMidi()` возвращает `false` |
| `Source/PluginProcessor.cpp:655` | `processBlock()` получает `juce::MidiBuffer& midi` |
| `Source/PluginProcessor.cpp:775-798` | входные ноты читаются и квантуются в пул |
| `Source/PluginProcessor.cpp:515-588` | `triggerStep()` — единственное место рождения нот |
| `Source/PluginProcessor.cpp:555, 559, 574` | ноты уходят в `synth.noteOnTimed()` |
| `Source/PluginProcessor.cpp:1306-1419` | `writePatternMidiFile()` — офлайн-экспорт SMF |
| `Source/PluginEditor.h:13-29, 129` | `MidiDragButton` — drag-экспорт файла |

Важно: `midi` — одновременно входной и выходной буфер. Если ничего не делать,
хост увидит входные ноты как выходные (эхо). Это надо контролировать явно.

## 3. Архитектура

```
                 ┌─ NotePool ─ PatternEngine ─ triggerStep ─┬─ PolySynth ─ ... ─ Audio Out
MIDI In ─ ... ───┘                                          │
                                                            └─ MidiOutScheduler ─ MidiBuffer ─ Host / Standalone MidiOutput
```

- `triggerStep()` остаётся единственным местом, где рождаются ноты. Рядом с
  `synth.noteOnTimed()` вызывается `scheduleMidiNoteOn()`, поэтому MIDI и
  внутренний синтез не могут разойтись.
- `MidiOutScheduler` — небольшая структура в процессоре: очередь отложенных
  note-off, retrigger-flush, all-notes-off.
- В конце `processBlock()` буфер отдаётся хосту. В standalone тот же буфер
  забирает `AudioProcessorPlayer` и отправляет в `MidiOutput`.

## 4. Этап 1 — контракт плагина

1. `CMakeLists.txt`: `NEEDS_MIDI_OUTPUT TRUE`.
2. `PluginProcessor.h`: `bool producesMidi() const override { return true; }`.
   - **Всегда `true`**, независимо от параметра `midiOut`. Иначе хост не создаст
     выходную шину при загрузке, и включение «на лету» не заработает.
3. `acceptsMidi()` остаётся `true`, `isMidiEffect()` — `false`.
4. Проверить, что VST3-обёртка создаёт event output bus: в Cubase/Nuendo
   MIDI-выход инструмента должен появиться в списке входов других дорожек.
5. **AU:** MIDI-выход инструмента в Logic/GarageBand не маршрутизируется.
   Это ожидаемое ограничение формата, не баг. Для AU остаются аудио и экспорт
   файла.

## 5. Этап 2 — маршрутизация в `processBlock`

Порядок обработки:

1. Прочитать транспорт (как сейчас, `PluginProcessor.cpp:668-714`).
2. Прочитать `midiOut`, `midiChannel`, `midiSecondChannel`, `midiThru`,
   `localSynth`.
3. Обработать входные события (как сейчас, строки 775-798).
4. Подготовить выход:

```cpp
midiOutBuffer.clear();

if (midiThru)
    midiOutBuffer.addEvents (midi, 0, numSamples, 0);

midi.clear();          // вход не эхо-ится по умолчанию
```

5. Выпустить созревшие note-off из очереди (этап 3).
6. Пройти цикл рендера; в `triggerStep()` добавить сгенерированные события в
   `midiOutBuffer` (только при `midiOut = on`; см. этап 3).
7. В конце блока: `midi.swapWith (midiOutBuffer);`.

`midiThru` работает независимо от `midiOut`: входные ноты уходят наружу, даже
если генератор не отдаёт свои. Это удобно для «сквозного» режима, но требует
явного включения.

Почему member-буфер + `swapWith`:

- `MidiBuffer::addEvent()` может аллоцировать; `ensureSize()` в `prepareToPlay()`
  и обмен буферами убирают аллокации из аудио-потока.
- `swapWith()` не копирует данные, только меняет местами внутренние массивы.
- После обмена в `midiOutBuffer` оказывается старый буфер хоста; на следующем
  блоке он очищается.

`prepareToPlay()`:

```cpp
midiOutBuffer.ensureSize (4096);
pendingNoteOffCount = 0;
```

Оценка размера: один шаг — максимум 4 ноты аккорда + 1 second voice = 5 note-on
и 5 note-off; 16 шагов ≈ 160 событий. 4 КБ — с запасом.

`numSamples == 0`: ранний выход остаётся; буфер не трогаем. Если хост пришлёт
входные события при нулевом блоке, они не эхо-ятся, потому что мы ничего не
добавляем (и не обрабатываем — это существующее поведение).

## 6. Этап 3 — планировщик note-off

### 6.1. Структура

```cpp
struct PendingNoteOff
{
    int note = -1;
    int channel = 1;
    int samplesLeft = 0;   // относительно начала текущего блока
};

std::array<PendingNoteOff, 64> pendingNoteOffs {};
int pendingNoteOffCount = 0;
juce::MidiBuffer midiOutBuffer;
```

`scheduleMidiNoteOn()` — no-op при `midiOut = off`; очередь note-off при этом
всё равно дочищается (см. 6.5). `localSynth` на планировщик не влияет: MIDI
уходит независимо от того, звучит ли внутренний синтез.

### 6.2. Длительность

Для lead, chords и second voice:

```
length = min (samplesPerStep * gate, stepSamples * 0.98)
```

где `stepSamples` — фактическая длина шага с учётом swing (тот же `factor`,
который возвращает `triggerStep()`). Это совпадает с логикой
`writePatternMidiFile()` (`PluginProcessor.cpp:1341-1367`) и гарантирует, что
note-off не окажется позже следующего note-on той же ноты.

Для **Drone** длина особая: `samplesPerStep * (period + 0.9)`, она специально
перекрывает следующий триггер (`SecondVoice.h:210`).

### 6.3. Внутри блока и через границу

- Если `offset + length < numSamples` — note-off добавляется сразу в
  `midiOutBuffer` по offset `offset + length`.
- Иначе — в очередь с `samplesLeft = offset + length - numSamples`.

В начале каждого блока (до `triggerStep()`):

```cpp
for (int i = pendingNoteOffCount; --i >= 0;)
{
    auto& p = pendingNoteOffs[(size_t) i];

    if (p.samplesLeft < numSamples)
    {
        midiOutBuffer.addEvent (juce::MidiMessage::noteOff (p.channel, p.note),
                                juce::jmax (0, p.samplesLeft));
        p = pendingNoteOffs[(size_t) --pendingNoteOffCount];
    }
    else
    {
        p.samplesLeft -= numSamples;
    }
}
```

Порядок «сначала note-off, потом note-on» на одном offset обеспечивается тем,
что очередь выпускается до `triggerStep()`, а `MidiBuffer::addEvent()` при
равном offset ставит новое событие после существующих.

### 6.4. Retrigger

Перед новым note-on для той же пары `(note, channel)` вызвать
`flushPendingOff (note, channel, offset)`: выпустить note-off по offset нового
note-on (или на 1 сэмпл раньше, если `offset > 0`). Это нужно для Drone и для
совпавших нот lead / second voice.

### 6.5. Стоп и выключение

- **Стоп транспорта** (`sequencerRunning` true → false,
  `PluginProcessor.cpp:696-712`): выпустить все pending note-off по offset 0,
  отправить `allNotesOff` (лучше `allSoundOff`) на используемые каналы,
  очистить очередь.
- **Выключение `midiOut`** (on → off): то же самое, затем перестать добавлять
  события.
- **`prepareToPlay()` / `releaseResources()`:** очистить очередь. В
  `releaseResources()` отправить уже некуда, но при следующем `prepareToPlay()`
  очередь пустая — «зависших» событий из прошлой сессии не будет.
- **Переполнение очереди:** если 64 слота заняты — выпустить самый старый
  note-off немедленно (offset 0) и занять его слот. Лучше короткая нота, чем
  зависшая.

### 6.6. Почему не отправлять note-off сразу

`triggerStep()` вызывается в середине блока, а длительность может выходить за
блок. Кроме того, `gate = 1.0` и swing могут дать note-off позже следующего
триггера. Очередь решает оба случая без аллокаций.

### 6.7. Чего не делать

- Не использовать `juce::MidiMessageSequence` в аудио-потоке (аллокации).
- Не пытаться «вычесть» события из `midi` после факта: буфер хоста может быть
  переиспользован.
- Не открывать `MidiOutput` в plugin-сборке: только standalone, иначе двойная
  маршрутизация и обход latency compensation хоста.

## 7. Этап 4 — параметры и UI

### 7.1. Параметры

| Контрол | ID | Тип | Диапазон | Дефолт | Смысл |
|---|---|---|---|---|---|
| MIDI Out | `midiOut` | bool | — | off | Включить живой MIDI-выход |
| MIDI Ch | `midiChannel` | int | 1..16 | 1 | Канал lead / chords |
| 2nd Ch | `midiSecondChannel` | int | 0..16 | 0 | Канал second voice; 0 = как lead |
| MIDI Thru | `midiThru` | bool | — | off | Пропускать входные ноты наружу |
| Local Synth | `localSynth` | bool | — | on | Внутренний синтез; off = только MIDI |

Добавить в `vespera::ids`, `RawParams`, `bindRawParams()`,
`createParameterLayout()`. Для `midiSecondChannel` — `AudioParameterIntAttributes`
со строкой `0 → "Same"`.

Важно:

- `randomizeParameters()` **не трогает** эти параметры (как `chords`,
  `limiter`, `transportSync`).
- Пресеты: новые параметры получат дефолты. `kFactoryPresetVersion` поднимать
  не нужно, если дефолты устраивают; поднимать — только если нужно, чтобы
  старые пресеты явно содержали MIDI-настройки.
- `midiOut` по умолчанию **off**: включение меняет поведение хоста (появляется
  MIDI-выход), не стоит делать это без ведома пользователя.
- `localSynth` по умолчанию **on**: Vespera остаётся инструментом. Комбинация
  `midiOut = off, localSynth = off` даёт тишину — предупредить в tooltip.

### 7.2. UI

- Переименовать кнопку `MIDI` в хедере в `EXPORT`: сейчас это drag-экспорт SMF
  (`PluginEditor.h:129`), после появления живого MIDI имя будет путать.
  Обновить tooltip и `doc/PARAMETERS.md`.
- В полосе OUTPUT добавить ряд MIDI:
  - `MIDI Out` (toggle), `Local Synth` (toggle), `Thru` (toggle);
  - `Ch` (combo 1..16), `2nd Ch` (combo Same, 1..16).
- Полоса OUTPUT сейчас `kOutputStripHeight = 118` (`PluginEditor.h:144`); для
  второго ряда увеличить до ~150 и поправить `resized()`
  (`PluginEditor.cpp:784-804`).
- В COMPACT-режиме (600 px) проверить, что высота не ломает дисплей; при
  необходимости MIDI-ряд переносится в popup-меню.
- Tooltip: «MIDI OUT — send generated notes to another instrument (the host must
  route the plug-in MIDI output)».

### 7.3. DebugLogger (опционально)

Добавить в `BlockSnapshot` поля `midiOutOn`, `midiChannel`, `localSynth`,
`pendingOffs` и вывести в `formatLine()`. Полезно для диагностики зависших нот.

## 8. Этап 5 — Standalone

В DAW маршрутизацию делает хост. В standalone нужно самим открыть
`juce::MidiOutput`.

### 8.1. Вариант A (рекомендуемый): через `AudioProcessorPlayer`

`juce::AudioProcessorPlayer` уже умеет отправлять `MidiBuffer` в `MidiOutput`:

- `player.setMidiOutput (device.get())` — указатель не владеющий;
- после `processBlock()` плеер сам вызывает `sendBlockOfMessages()` /
  `sendBlockOfMessagesNow()`;
- `MidiOutput` открывается на message-потоке и запускается
  `startBackgroundThread()`.

Реализация:

1. В standalone-редакторе получить `StandalonePluginHolder::getInstance()`
   (заголовок `juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h`).
2. Заполнить combo устройствами `MidiOutput::getAvailableDevices()`.
3. При выборе: `MidiOutput::openDevice (identifier)`,
   `startBackgroundThread()`, `holder->player.setMidiOutput (device.get())`.
4. Хранить `std::unique_ptr<MidiOutput>` в редакторе/роутере; при закрытии
   сначала `setMidiOutput (nullptr)`, затем `stopBackgroundThread()` и сброс.
5. Компиляция: shared code может не видеть `StandalonePluginHolder` (модуль
   `juce_audio_plugin_client` линкуется обёрткой). Вынести код в отдельный
   `.cpp`, который собирается только для standalone-таргета, либо проверить
   линковку модуля. Если это неудобно — вариант B.

### 8.2. Вариант B: `MidiOutput` внутри процессора

- Процессор владеет `std::unique_ptr<MidiOutput>`; открытие/закрытие — только
  на message-потоке.
- В `processBlock()` при `wrapperType == wrapperType_Standalone` и открытом
  устройстве: `sendBlockOfMessages (midiOutBuffer,
  Time::getMillisecondCounterHiRes(), currentSampleRate)`.
- Указатель защитить `SpinLock` + `ScopedTryLock` (если не удалось — пропустить
  блок) или держать устройство живым до конца жизни приложения.
- Плюс: не зависит от `StandalonePluginHolder`; минус: дублирует логику плеера
  и требует аккуратного lifetime.

### 8.3. Важный нюанс: Play Sync в standalone

В JUCE 8.0.6 `AudioProcessorPlayer` устанавливает процессору собственный
`AudioPlayHead`, который возвращает `PositionInfo` без `setIsPlaying()` и без
`setBpm()`. То есть `getIsPlaying() == false`, `getBpm() == nullopt`.

Текущий код (`PluginProcessor.cpp:675-694`):

```cpp
transportKnown = true;
transportPlaying = pos->getIsPlaying();   // false
...
sequencerRunning = ! (playSync && transportKnown) || transportPlaying;  // false при playSync=true
```

Значит, в standalone с `Play Sync = on` генератор **не должен играть**. Это надо
проверить на реальной сборке; если подтвердится — для standalone нужен один из
вариантов:

- считать `transportKnown = false`, если хост не сообщил BPM и `isPlaying`
  (free-run, как описано в документации);
- или добавить локальный play/stop и BPM в standalone;
- или в standalone принудительно выключать `Play Sync`.

Это отдельный баг/задача, но без него standalone MIDI-out будет молчать.
Включить в план как prerequisite.

### 8.4. macOS / Windows

- macOS: `IAC Driver` (Audio MIDI Setup) — включить, создать шину; в DAW или
  другой программе выбрать её вход.
- Windows: `loopMIDI` (Tobias Erichsen) или аналогичный виртуальный порт.
- Петля: если выход standalone завернуть на его же вход, сгенерированные ноты
  попадут в NotePool → бесконечный рост и эхо. Минимум — предупредить в
  документации; в идеале — игнорировать ноты, пришедшие с собственного выхода
  (сложно, не для MVP).

## 9. Этап 6 — тесты

Новый файл `Tests/MidiOutTest.cpp` + строка
`vespera_add_plugin_test(VesperaMidiOutTest Tests/MidiOutTest.cpp)` в
`CMakeLists.txt` (или секции в `PluginTest.cpp`, но файл уже больше 2000 строк).

Хелпер:

```cpp
struct MidiCapture
{
    int noteOns = 0, noteOffs = 0;
    std::array<int, 128> held {};             // счётчик удерживаемых нот
    std::vector<std::pair<int, int>> events;  // (offset, note) для отладки
};
```

Собирать события из `midi` после каждого `processBlock()`.

Проверки:

1. `midiOut = false` → после блока в `midi` нет событий (вход не эхо-ится).
2. `midiOut = true`, пул заполнен → есть note-on; все ноты в ладу; velocity в
   `(0, 1]`.
3. Каждый note-on получает note-off за N блоков; в конце нет удерживаемых нот.
4. `Chords = true` → до 4 note-on на шаг, все разные, в ладу.
5. `secondRole = Octave`, `midiSecondChannel = 2` → second voice на канале 2,
   lead — на 1.
6. `midiThru = true` → входная нота есть в выходе; `false` — нет.
7. `localSynth = false`, `monitor = false` → аудио тихое (RMS ≈ 0), но
   MIDI-события есть.
8. Медленный rate + `gate = 1.0` → note-off приходит в следующем блоке
   (проверка очереди).
9. Быстрый rate + `gate = 1.0` → нет наложения одинаковых нот без note-off
   (retrigger flush).
10. Стоп транспорта в середине ноты → all-notes-off / все note-off выпущены.
11. `prepareToPlay()` сбрасывает очередь: первый блок после re-prepare не
    содержит старых note-off.
12. Стресс: 16 шагов, Chords + second voice, 200 блоков → число note-on равно
    числу note-off, зависших нет.
13. `producesMidi() == true` (статическая проверка контракта).

Также прогнать существующие тесты: `VesperaPluginTest` не должен сломаться от
очистки входного буфера. Сейчас тесты после `processBlock()` часто делают
`midi.clear()` — это станет не нужно, но не сломается. Тесты, которые не
очищают буфер между блоками, надо проверить: с `midiOut = false` выход пустой,
поэтому поведение не изменится.

## 10. Этап 7 — документация

После реализации обновить:

- `doc/ARCHITECTURE.md` — схема (MIDI Out), раздел процессора, шаги
  `processBlock`.
- `doc/PARAMETERS.md` — новый раздел OUTPUT/MIDI, исключения RANDOM, описание
  каналов.
- `doc/DEVELOPMENT.md` — карта кода (`MidiOutScheduler`), список тестов.
- `doc/README.md` — список возможностей и ограничения по форматам.
- `doc/ROADMAP.md` — отметить пункт, убрать «Нет MIDI-выхода» из ограничений.
- `doc/MIDI_OUTPUT.md` (этот файл) — перевести статус в «реализовано» или
  оставить как дизайн-заметки.

## 11. Крайние случаи и подводные камни

### Хосты и форматы

- **VST3:** JUCE создаёт event output bus при `producesMidi() == true`. Хост
  должен уметь маршрутизировать MIDI с инструмента: Reaper, Bitwig,
  Cubase/Nuendo, Studio One — да; Ableton Live — как правило нет;
  Logic/GarageBand (AU) — нет.
- **AU:** MIDI-выход инструмента не поддерживается; не тратить время на обход.
- **Standalone:** только через `MidiOutput` + IAC/loopMIDI.
- **AAX:** не собирается, вне задачи.
- Некоторые хосты показывают MIDI-выход только после перезагрузки плагина —
  `producesMidi()` должен быть `true` с самого начала.

### Ноты и каналы

- **Эхо входа.** `midi` — общий буфер; без `midi.clear()` входные ноты уйдут
  наружу. По умолчанию — не должны.
- **Зависшие ноты.** Без flush на стоп / выключение / закрытие внешний
  синтезатор оставит ноты. `allNotesOff` понимают не все; для panic —
  `allSoundOff`.
- **Drone** перекрывает следующий триггер; обязателен retrigger flush.
- **Одинаковые ноты lead и second voice на одном канале** могут «съесть» друг
  друга note-off'ами. Рекомендация: отдельный канал для second voice; или
  дедупликация в пределах шага (если совпали нота и канал — не отправлять
  второй note-on).
- **Chords velocity:** внутри плагина и в SMF используется `velocity / sqrt(N)`.
  Для live MIDI повторить, иначе аккорд на внешнем инструменте будет громче.
  Опционально — параметр `midiVelocityScale`.
- **Gate:** clamp к `stepSamples * 0.98` (как в SMF), а не «чистый» gate, иначе
  note-off окажется после следующего note-on.
- **Swing:** clamp должен использовать фактическую длину шага (с factor), иначе
  на коротком шаге note-off вылезет за границу.
- **Transpose / Key / Scale:** к моменту `scheduleMidiNoteOn()` питч уже посчитан
  (`root`, `second.note`), дополнительно транспонировать нельзя.
- **Pool пуст:** `triggerStep()` не генерирует нот; MIDI-выход молчит. Нормально.
- **Play Sync остановлен:** note-off всё равно должны выпускаться (хвосты),
  иначе зависнут.

### Реалтайм

- Никаких `std::vector`, `String`, `MidiMessageSequence` в `processBlock()`.
- `MidiBuffer::ensureSize()` — только в `prepareToPlay()`.
- `swapWith()` — O(1), без аллокаций.
- `MidiMessage::noteOn()` / `noteOff()` — stack-only.
- Очередь note-off — фиксированный `std::array`.
- Не блокировать аудио-поток на `MidiOutput`; в standalone использовать
  `sendBlockOfMessages()` (как плеер) или try-lock.

### Параметры

- `midiOut` не должен входить в `randomizeParameters()`.
- При загрузке пресета с `midiOut = off` нужно погасить все ноты (как при
  выключении).
- При смене `midiChannel` в момент удержания нот: старые note-off уйдут на
  старый канал (в `PendingNoteOff` хранится канал), новые — на новый. Это
  правильно; не «переносить» note-off на новый канал.
- При смене `secondRole` / `midiSecondChannel` — то же.

## 12. Открытые вопросы

1. Дефолт `midiOut`: off (рекомендуется) или on?
2. Дефолт `midiSecondChannel`: 0 (Same) или 2?
3. Отдавать ли MONITOR-ноты наружу? Рекомендация: нет; для входа есть
   `midiThru`.
4. Нужен ли режим «MIDI only» без аудио вообще (экономия CPU)? Сейчас
   `localSynth = off` всё равно рендерит тишину. Можно добавить early-out, но
   это отдельная оптимизация.
5. Нужен ли MIDI clock / start / stop / continue для standalone? Отдельная
   задача.
6. Нужна ли кнопка PANIC в UI? Полезно, но не обязательно для MVP.
7. Как поступать с CC / aftertouch на входе при `midiThru`? Пропускать всё или
   только ноты? Рекомендация: только ноты и pitch bend; CC не пропускать,
   чтобы не управлять чужим инструментом случайно.
8. Нужен ли `midiVelocityScale` / `midiTranspose` для внешнего инструмента?
   Пока нет.

## 13. Чеклист готовности

- [ ] `NEEDS_MIDI_OUTPUT TRUE`, `producesMidi() == true`.
- [ ] `midiOut` / `midiChannel` / `midiSecondChannel` / `midiThru` /
      `localSynth` в APVTS.
- [ ] `triggerStep()` пишет lead, chords и second voice в `midiOutBuffer`.
- [ ] Note-off через очередь, без аллокаций, с retrigger flush.
- [ ] Стоп транспорта / выключение MIDI Out / смена пресета гасят ноты.
- [ ] Вход не эхо-ится (кроме `midiThru`).
- [ ] UI: MIDI-ряд в OUTPUT, кнопка экспорта переименована.
- [ ] Standalone: выбор `MidiOutput`, IAC/loopMIDI, Play Sync не блокирует
      генератор.
- [ ] `Tests/MidiOutTest.cpp` зелёный; существующие тесты зелёные.
- [ ] Документация обновлена.

## 14. Порядок работ (MVP → полный)

1. **MVP VST3:** CMake + `producesMidi()` + `triggerStep()` + очередь note-off
   + параметр `midiOut`. Без UI (включать через хост или пресет). Тесты.
2. **UI и параметры:** каналы, `midiThru`, `localSynth`, ряд в OUTPUT,
   переименование экспортной кнопки.
3. **Standalone:** `MidiOutput`, выбор устройства, исправление Play Sync.
4. **Полировка:** DebugLogger, panic, документация, крайние случаи.
