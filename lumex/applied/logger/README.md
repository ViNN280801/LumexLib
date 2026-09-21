\page Logger Универсальная Система Логирования

## Обзор

**LumexLogger** - это универсальная система логирования для C++ приложений, реализованная как потокобезопасный синглтон. Логгер автоматически создает файлы журналов рядом с исполняемым файлом и предоставляет гибкие возможности настройки через конфигурационные файлы.

Класс живет в пространстве имен `lumex::applied::logger::logger` (файл `LumexLogger.hpp`, полное имя `lumex::applied::logger::logger::LumexLogger`), а для удобства использования в глобальном пространстве имен объявлен алиас `LumexLogger` (по аналогии с `LumexEnvironment` и `LumexLogging`). В коде достаточно писать `LumexLogger::getInstance()`, не указывая полный путь пространства имен.

Этот модуль (`lumex/applied/logger/`) - это отдельный, самостоятельный механизм логирования, синхронизированный по функциональности с `Logger` из проекта DChannel. Он **не связан** с `lumex/applied/logging/` (`LumexLogging`) - другим, независимо используемым внутри самой LumexLib логирующим фасадом (см. `LumexDebug.hpp`, `LumexHardwareCapabilities.cpp` и др.). Не путайте эти два модуля.

## Ключевые особенности

### Архитектурные преимущества

- **Синглтон-паттерн**: Единственный экземпляр логгера на все приложение
- **Потокобезопасность**: Полная защита от race conditions через мьютексы
- **Кроссплатформенность**: Поддержка Windows, Linux и macOS
- **Автоматическое определение пути**: Лог создается рядом с исполняемым файлом
- **Пространство имен**: Все публичные символы (`LogLevel`, `FunctionNameMode`, `logger_config_t`, внутренние
  хелперы форматирования вроде `logger_stringify`) находятся в `lumex::applied::logger::logger`, а не в глобальном
  пространстве имен - это специально сделано, чтобы избежать коллизий имен в общей библиотеке, используемой
  многими проектами.

### Уровни логирования

Система поддерживает 7 уровней логирования с возможностью фильтрации:

| Уровень         | Значение | Описание                                                                                                                  |
| --------------- | -------- | ------------------------------------------------------------------------------------------------------------------------- |
| `LEVEL_TRACE`   | 0        | Максимально детализированная отладочная информация                                                                        |
| `LEVEL_DEBUG`   | 1        | Отладочная информация                                                                                                     |
| `LEVEL_INFO`    | 2        | Общая информация (по умолчанию)                                                                                           |
| `LEVEL_SUCCESS` | 3        | Успешное выполнение операции                                                                                              |
| `LEVEL_WARNING` | 4        | Предупреждения (приложение может продолжать работу)                                                                       |
| `LEVEL_ERROR`   | 5        | Ошибки (в некоторых случаях приложение может продолжать работу, в некоторых это может привести к неожиданным результатам) |
| `LEVEL_FATAL`   | 6        | Критические ошибки при которых приложение должно завершить работу                                                         |

> **Важно для эксплуатации:** уровни `LEVEL_DEBUG` и особенно `LEVEL_TRACE` предназначены для коротких диагностических сессий и точечной отладки.
> Их длительное использование в рабочем режиме может быстро увеличивать объем файлов логов, усложнять анализ и создавать дополнительную нагрузку на систему.
> Для длительной работы и продакшена используйте `LEVEL_INFO`/`LEVEL_WARNING`/`LEVEL_ERROR` в зависимости от задачи.

### Производительность

- **Условная компиляция**: Макросы проверяют уровень на этапе компиляции
- **Буферизация логов**: Опциональная буферизация с автоматической записью при критических событиях
- **Принудительный flush**: Только для критических уровней (ERROR, FATAL)
- **Минимальные накладные расходы**: Оптимизированные операции записи
- **Практическая рекомендация по уровню**: `DEBUG/TRACE` не использовать для длинных непрерывных сессий; эти уровни нужны для ограниченной по времени диагностики

### Критические уровни всегда проходят, независимо от пресета

Это ключевое инвариантное поведение `log()`: **сначала** проверяется, является ли уровень сообщения критическим
(`WARNING`/`ERROR`/`FATAL`, либо уровень `BUFFERING_TRIGGER` и выше, если он задан), и только если уровень
**не** критический, применяется фильтрация по пресету (`PRESET=...`). Критическое сообщение от любого компонента
(даже не входящего в `PRESET`) всегда:

1. Флашит накопленный буфер (если буферизация включена и в буфере есть записи);
2. Записывается в файл само.

Это сделано намеренно: если бы фильтрация по пресету применялась раньше проверки критичности уровня, то
WARNING/ERROR/FATAL от компонента вне пресета терялся бы целиком и никогда не приводил бы к сбросу буфера -
что лишает буферизацию всякого смысла именно в момент, когда контекст важнее всего.

### Эксплуатационные ограничения и технические риски

Ниже перечислено, что разработчик должен учитывать при включении детального логирования:

1. **Стоимость каждого сообщения не нулевая**
   Логгер сериализует запись через мьютекс и выполняет файловый I/O. При высокой частоте событий (`DEBUG/TRACE`) это увеличивает конкуренцию за мьютекс, нагрузку на диск и задержки рабочих потоков.

2. **`DEBUG/TRACE` резко повышают объем данных**
   Уровни ниже `WARNING` обычно генерируют на порядок больше записей, поэтому при длинной сессии лог-файл может вырасти до очень больших размеров.

3. **Stacktrace существенно дороже обычной строки лога**
   При `STACKTRACE=true` трассировка формируется для всех уровней, что заметно увеличивает CPU cost и размер файлов.

4. **Буферизация меняет профиль нагрузки на I/O**
   При `BUFFERING=true` сброс буфера происходит пачкой при достижении уровня-триггера. Это полезно для контекста, но может давать кратковременные пики записи и задержки в момент dump.

5. **Практический стандарт для продакшена**
   Де-факто рабочий режим для длинных сессий - `WARNING` и выше (`ERROR`/`FATAL` для максимально "тихого" режима).
   `INFO` допускается для расширенного мониторинга на ограниченный период.
   `DEBUG/TRACE` - только целевая диагностика на ограниченном интервале времени.

### Расширенные возможности

- **Буферизация логов**: Настраиваемая буферизация с автоматической записью истории при критическом уровне
- **Пресеты компонентов**: Фильтрация логов по компонентам для фокусированного анализа, с гибким сопоставлением
  (точное имя, короткое имя метода, `Class::method`, полная сигнатура)
- **Древовидный разбор фреймов** (`DESCRIBE_FRAME`): опциональный дополнительный слой детального логирования
- **Идентификатор потока в каждой записи** (`[TID=...]`): упрощает анализ многопоточных логов

## Управление логированием

### Включение/отключение логирования

Логирование управляется через специальный файл-триггер `enable_logs` (по умолчанию), который должен находиться в той же директории, что и исполняемый файл (.exe, .dll, .so, .dylib). Имя файла-триггера можно изменить программно через метод `set_trigger_file_name()`.

#### Включение логирования

```bash
# Создать файл рядом с исполняемым файлом
touch enable_logs
```

#### Отключение логирования

```bash
# Удалить файл-триггер
rm enable_logs
```

#### Изменение имени файла-триггера

```cpp
LumexLogger& logger = LumexLogger::getInstance();
logger.set_trigger_file_name("my_custom_log_trigger"); // Теперь используется my_custom_log_trigger вместо enable_logs
std::string currentName = logger.get_trigger_file_name(); // Получить текущее имя
```

### Настройка уровня логирования

Уровень логирования настраивается через содержимое файла-триггера (по умолчанию `enable_logs`):

#### Формат файла конфигурации

В сборку входит ровно один reader. Формат задается CMake cache-переменной `LUMEX_LOGGER_CONFIG_FORMAT` и компилируется в `lumex::logger` как `LUMEX_LOGGER_CONFIG_FORMAT_*`. Расширение пути не влияет на разбор: `configured_logger_config_format()` возвращает выбранный enum без чтения имени файла. Смена формата требует reconfigure и пересборки logger.

| Значение `LUMEX_LOGGER_CONFIG_FORMAT` | Enum | Reader и зависимость |
| ------------------------------------- | ---- | -------------------- |
| `PLAIN_TEXT` (default) | `plain_text` | встроенный `KEY=value` reader; дополнительных модулей нет |
| `INI` | `ini` | `LumexSettingsINI`; PRIVATE `lumex::settings`; требует effective `LUMEX_BUILD_SETTINGS` |
| `JSON` | `json` | vendored nlohmann/json 3.12.0 |
| `YAML` | `yaml` | зарезервирован; любой вызов `read_config_from_path()` бросает `std::runtime_error`, пока нет `LumexSettingsYAML` |
| `XML` | `xml` | существующий `LumexXml`; PRIVATE `lumex::xml`; требует effective `LUMEX_BUILD_XML` |

`read_config_from_path()` возвращает `logger_config_t{}` при отсутствующем файле, неподдерживаемой структуре или большинстве ошибок разбора. Единственное намеренное исключение - сборка с `LUMEX_LOGGER_CONFIG_FORMAT=YAML`.

##### CMake

```cmake
set(LUMEX_LOGGER_CONFIG_FORMAT "PLAIN_TEXT") # или INI, JSON, YAML, XML
```

Эквивалент для командной строки:

```console
cmake -S . -B build -DLUMEX_LOGGER_CONFIG_FORMAT=XML
```

`INI` без effective settings и `XML` без effective XML завершают configure через `FATAL_ERROR`. Неизвестное значение `LUMEX_LOGGER_CONFIG_FORMAT` тоже является `FATAL_ERROR`. YAML включает только документированное исключение `unimplemented`.

##### Plain text

Новый формат с префиксами (рекомендуемый для стандартного `enable_logs`):

```txt
LEVEL=warning
FUNCNAME=normal
TIMESTAMPED=true
STACKTRACE=false
STACKTRACE_FRAMES=16
HINT=true
PRESET=NetworkManager,DatabaseConnection
BUFFERING=true
DESCRIBE_FRAME=false
BUFFER_SIZE=200
BUFFERING_TRIGGER=error
```

Порядок строк не важен. Пустые строки и строки с первым непробельным символом `#` пропускаются.

Старый трехстрочный формат сохранен для обратной совместимости:

```txt
<уровень_логирования>
<настройка_имени_функции>
<настройка_timestamped_логов>
```

##### INI

Reader обрабатывает только секцию `[logger]`. Значение `PRESET` с запятыми следует заключать в кавычки:

```ini
[logger]
LEVEL=warning
FUNCNAME=normal
TIMESTAMPED=true
STACKTRACE=false
STACKTRACE_FRAMES=16
HINT=true
PRESET="NetworkManager,DatabaseConnection"
BUFFERING=true
DESCRIBE_FRAME=false
BUFFER_SIZE=200
BUFFERING_TRIGGER=error
```

##### JSON

Предпочтительная форма содержит объект `logger`. Строки, числа и `bool` преобразуются к общей строковой модели ключей:

```json
{
  "logger": {
    "LEVEL": "warning",
    "FUNCNAME": "normal",
    "TIMESTAMPED": true,
    "STACKTRACE": false,
    "STACKTRACE_FRAMES": 16,
    "HINT": true,
    "PRESET": "NetworkManager,DatabaseConnection",
    "BUFFERING": true,
    "DESCRIBE_FRAME": false,
    "BUFFER_SIZE": 200,
    "BUFFERING_TRIGGER": "error"
  }
}
```

Если ключа `logger` нет, reader принимает те же ключи непосредственно в корневом JSON object:

```json
{
  "LEVEL": "info",
  "FUNCNAME": "short"
}
```

Массивы, objects в значениях и `null` пропускаются.

##### YAML

YAML пока не реализован. При `LUMEX_LOGGER_CONFIG_FORMAT=YAML` любой вызов `read_config_from_path()` приводит к точному исключению:

```text
logger config YAML is selected (LUMEX_LOGGER_CONFIG_FORMAT=YAML) but LumexSettingsYAML is not implemented yet
```

Целевая форма будущего reader-а:

```yaml
logger:
  LEVEL: warning
  FUNCNAME: normal
  BUFFERING: true
```

Пример обязан перехватить это исключение. Молчаливое чтение YAML как реализованного формата является ошибкой.

##### XML

XML реализован через существующий компонент `LumexXml`. Корневой элемент должен называться `logger`; каждый ключ представлен дочерним элементом:

```xml
<?xml version="1.0" encoding="utf-8"?>
<logger>
  <LEVEL>warning</LEVEL>
  <FUNCNAME>normal</FUNCNAME>
  <TIMESTAMPED>true</TIMESTAMPED>
  <STACKTRACE>false</STACKTRACE>
  <STACKTRACE_FRAMES>16</STACKTRACE_FRAMES>
  <HINT>true</HINT>
  <PRESET>NetworkManager,DatabaseConnection</PRESET>
  <BUFFERING>true</BUFFERING>
  <DESCRIBE_FRAME>false</DESCRIBE_FRAME>
  <BUFFER_SIZE>200</BUFFER_SIZE>
  <BUFFERING_TRIGGER>error</BUFFERING_TRIGGER>
</logger>
```

Имена XML elements регистрозависимы и записываются в указанной выше верхней форме. Отсутствующие и пустые elements сохраняют соответствующее значение по умолчанию. Неизвестные elements игнорируются. Невалидный XML или другой root возвращает default config.

##### Программный разбор

`read_config_from_path()` позволяет проверить config независимо от жизненного цикла singleton:

```cpp
#include "lumex/applied/logger/LumexLogger"
#include "lumex/applied/logger/config/LumexLoggerConfigFormat.hpp"

using lumex::applied::logger::configured_logger_config_format;
using lumex::applied::logger::logger_config_format_t;
using namespace lumex::applied::logger::logger;

logger_config_format_t const format = configured_logger_config_format();
logger_config_t const config =
    LumexLogger::read_config_from_path("C:/app/enable_logs");
```

Singleton читает trigger-файл при первом `LumexLogger::getInstance()` тем reader-ом, который скомпилирован в эту сборку. Вызов `set_trigger_file_name()` после создания singleton меняет имя для последующих проверок, но не перечитывает уже примененную конфигурацию. Для явной загрузки используйте `read_config_from_path()`.

#### Поддерживаемые ключи конфигурации

**Префиксы (case-insensitive, т.е. можно писать хоть `funcname=none`, хоть `FUnCnaMe=NOne`):**

- `LEVEL=` - уровень логирования
- `FUNCNAME=` - настройка имени функции (`none`/`short`/`normal`/`full`; `signature` - алиас для `full`)
- `TIMESTAMPED=` - настройка timestamped логов
- `STACKTRACE=` - настройка показа stacktrace (true/false)
- `STACKTRACE_FRAMES=` - максимальное количество фреймов в stacktrace (1-64)
- `HINT=` - создавать ли файл-хелпер с подсказкой для разработчиков (true/false, по умолчанию true)
- `PRESET=` - фильтрация по компонентам (через запятую). Совпадение: короткое имя (`WriteData`), нормальное
  (`Class::method`), полная сигнатура или точное имя
- `BUFFERING=` - включить/отключить буферизацию логов (true/false, по умолчанию false)
- `BUFFERING_TRIGGER=` - минимальный уровень, при котором сбрасывать буфер в файл (TRACE, DEBUG, INFO, SUCCESS,
  WARNING, ERROR, FATAL; case-insensitive). При задании сброс выполняется только при появлении сообщения этого
  уровня и выше; при выходе программы буфер **не** сбрасывается
- `DESCRIBE_FRAME=` - включить древовидный разбор фреймов пакетов в логах (true/1/yes или false/0/no,
  case-insensitive, по умолчанию false)
- `BUFFER_SIZE=` - размер буфера логов в количестве записей (положительное число, по умолчанию 100)

**Уровни логирования** (case-insensitive): `trace`, `debug`, `info`, `success`, `warning`, `error`, `fatal`.

**Настройка имен функций** (case-insensitive):

- `none` -> не показывать имя функции
- `short` -> короткое имя (`__func__`)
- `normal` -> вид `ClassName::methodName()` без типа возврата и аргументов
- `full` -> полная сигнатура (`__FUNCSIG__`/`__PRETTY_FUNCTION__`); `signature` - алиас для `full`
- По умолчанию: `full`

> **Примечание:** Старое значение `NO_FUNCNAME` эквивалентно `none`. Если указано значение, не входящее в
> приведенный список (например, опечатка), логгер по умолчанию использует `normal`.

**Настройка timestamped логов** (case-insensitive): `true`/`yes`/`1`/`TIMESTAMPED` -> включить; иначе выключено.

**Настройка stacktrace** (case-insensitive):

- `true`/`yes`/`1` -> показывать stacktrace **под абсолютно каждую функцию** (для всех уровней логирования)
- `false`/`no`/`0` -> не показывать stacktrace
- По умолчанию: `false`, но автоматически `true` для уровней WARNING и ERROR (даже если `STACKTRACE=false`)

**Настройка количества фреймов в stacktrace**: число от 1 до 64 (по умолчанию 16).

**Настройка файла-хелпера** (case-insensitive): `true`/`yes`/`1` -> создавать `LOGGER_README.txt`;
`false`/`no`/`0` -> не создавать (существующий удаляется). По умолчанию: `true`.

**Настройка пресетов компонентов** (case-insensitive):

- Список через запятую. Совпадение с именем в логе по одному из вариантов:
  - **Короткое имя:** `WriteData` - логи из любых классов с методом `WriteData`
  - **Нормальное:** `SerialPortChannel::WriteData` или `SerialPortChannel::WriteData()` - только этот метод
  - **Полная сигнатура:** точная строка из лога (при `FUNCNAME=full`)
  - Точное совпадение извлеченного компонента
- Формат в логе: `[ComponentName] message` или `[ComponentName]: message` (поддерживаются оба варианта)
- Если `PRESET` задан, логируются только совпадающие сообщения; сообщения без компонента не логируются
- По умолчанию: пресет отключен

**Настройка буферизации логов** (case-insensitive):

- `BUFFERING=true`/`yes`/`1` -> включить; `false`/`no`/`0` -> отключить (по умолчанию)
- При включенной буферизации логи некритических уровней сохраняются в буфере и не записываются сразу
- **Без `BUFFERING_TRIGGER`:** сброс буфера при WARNING/ERROR/FATAL и при завершении программы
- **С `BUFFERING_TRIGGER=`** (например `BUFFERING_TRIGGER=WARNING`): сброс буфера только при появлении сообщения
  указанного уровня или выше; при выходе программы буфер не сбрасывается
- Буфер имеет ограниченный размер, старые записи удаляются при переполнении

**Настройка триггера сброса буфера (`BUFFERING_TRIGGER`)** (case-insensitive):

- `BUFFERING_TRIGGER=TRACE|DEBUG|INFO|SUCCESS|WARNING|ERROR|FATAL` -> минимальный уровень, при котором накопленный
  буфер сбрасывается в файл (например `BUFFERING_TRIGGER=waRnINg` - сброс при WARNING, ERROR, FATAL)
- Используется только при `BUFFERING=true`
- Если не задан: сброс при WARNING/ERROR/FATAL и при завершении программы (поведение по умолчанию)
- **Пресет не влияет на буферизацию:** решение о сбросе буфера принимается только по уровню. Сообщение уровня
  триггера из любого компонента вызывает сброс накопленного буфера в файл, после чего в лог всегда записывается
  и само это сообщение-триггер (порядок: сначала сброс буфера, затем строка WARNING/ERROR/FATAL), даже если
  компонент не входит в пресет.
- После сброса буфера в файл записывается маркер вида `=======>>> Dumped <N> logs`, где `<N>` - количество
  сброшенных записей (для наглядного отделения блока накопленных логов от последующего сообщения-триггера).

**Настройка `DESCRIBE_FRAME`** (case-insensitive):

- `true`/`1`/`yes` -> включить древовидный разбор фреймов пакетов в логах
- `false`/`0`/`no` -> отключить (по умолчанию)
- Используется макросами `LUMEX_LOG_*_DESCRIBE_FRAME` / `LOGGER_LOG_*_DESCRIBE_FRAME`: запись через эти макросы
  происходит только когда уровень включен **и** `DESCRIBE_FRAME` включен одновременно
- Проверяется через `LumexLogger::is_describe_frame_enabled()`

**Настройка размера буфера логов**: положительное число, по умолчанию 100, минимум 1.

> Все параметры (LEVEL, FUNCNAME, TIMESTAMPED, STACKTRACE, PRESET, BUFFERING, BUFFERING_TRIGGER, DESCRIBE_FRAME,
> HINT, BUFFER_SIZE) - case-insensitive, пробелы и табуляции в значении стираются. Порядок строк в файле не важен.

## Использование в коде

### Два слоя макросов

- `LOGGER_LOG_*` - обобщенный (generic) слой, максимально переносимый между проектами.
- `LUMEX_LOG_*` - слой, адаптированный под LumexLib (аналог `DCHANNEL_LOG_*` в проекте DChannel). Используйте
  именно этот слой в коде LumexLib и зависимых проектов.

Ранее существовавший ультра-короткий слой `LOG_TRACE`/`LOG_INFO`/`LOG_ADDRESS_*`/`LOG_OBJECT_*` был убран из этой
версии - такие обобщенные имена слишком легко коллизируют с другими логирующими библиотеками в общем проекте.
Используйте `LUMEX_LOG_*`.

### Базовые макросы логирования

```cpp
#include "lumex/applied/logger/LumexLogger"

void exampleFunction() {
    LUMEX_LOG_TRACE("Детальная отладочная информация");
    LUMEX_LOG_DEBUG("Отладочное сообщение");
    LUMEX_LOG_INFO("Информационное сообщение");
    LUMEX_LOG_SUCCESS("Операция выполнена успешно");
    LUMEX_LOG_WARNING("Предупреждение");
    LUMEX_LOG_ERROR("Ошибка");
    LUMEX_LOG_FATAL("Критическая ошибка");

    // Логирование с переменными
    int value = 42;
    std::string name = "test";
    LUMEX_LOG_INFO("Обработка значения:", value, "для объекта:", name);

    // Логирование с компонентами (для использования с пресетами)
    LUMEX_LOG_INFO("[NetworkManager]: Инициализация сетевого менеджера");
    LUMEX_LOG_DEBUG("[DatabaseConnection]: Выполнение запроса, параметры:", value);
}
```

Каждая запись, сделанная через макросы, автоматически дополняется префиксом `[TID=<id потока>]` - идентификатором
текущего `std::thread`, что упрощает анализ логов в многопоточных приложениях:

```log
[2025-10-02 16:05:28.643] [TRACE] [ClassName::MethodName()] [TID=140234123456] Created new object
```

В режиме `FUNCNAME=none` строка выглядит как `[TID=<id>] сообщение` (без имени функции).

### Специализированные макросы

#### Логирование адресов объектов

```cpp
MyClass* obj = new MyClass();
LUMEX_LOG_ADDRESS_INFO("Создан объект", obj);
```

#### Логирование объектов с контекстом

```cpp
MyClass* obj = new MyClass();
LUMEX_LOG_OBJECT_DEBUG(obj, "Инициализация завершена");
```

#### Логирование с древовидным разбором фреймов

```cpp
// Пишет только если LEVEL включен И DESCRIBE_FRAME=true|1|yes в конфигурации
LUMEX_LOG_INFO_DESCRIBE_FRAME("Разбор входящего пакета:", packetDump);
```

### Программное управление

```cpp
LumexLogger& logger = LumexLogger::getInstance();

if (logger.is_logging_enabled()) { /* ... */ }
if (logger.is_level_enabled(lumex::applied::logger::logger::LogLevel::LEVEL_DEBUG)) { /* ... */ }
if (logger.is_describe_frame_enabled()) { /* ... */ }

logger.set_log_level(lumex::applied::logger::logger::LogLevel::LEVEL_WARNING);
logger.flush();

// Снимок расширенных флагов конфигурации, прочитанных из trigger-файла
auto configView = logger.get_applied_config_view();
```

### Буферизация логов

Буферизация позволяет не записывать все логи сразу, а сохранять их в буфере. При возникновении критического
события (WARNING, ERROR, FATAL, либо уровня `BUFFERING_TRIGGER` и выше) все записи из буфера автоматически
записываются в файл вместе с маркером `=======>>> Dumped <N> logs`, что позволяет видеть контекст перед
критическим событием.

```cpp
LumexLogger& logger = LumexLogger::getInstance();

logger.enable_buffering();     // Буфер на 100 записей (по умолчанию)
logger.enable_buffering(200);  // Буфер на 200 записей
logger.set_buffer_size(150);    // Изменить размер буфера
logger.disable_buffering();    // Отключить (буфер будет сброшен перед отключением)

if (logger.is_buffering_enabled()) {
    std::size_t bufferSize = logger.get_buffer_size();
}
```

Через конфигурационный файл:

```txt
LEVEL=debug
BUFFERING=true
BUFFER_SIZE=200
BUFFERING_TRIGGER=warning
```

#### Как работает буферизация (с учетом порядка проверок в `log()`)

1. Проверяется, является ли уровень сообщения критическим (WARNING/ERROR/FATAL, либо `BUFFERING_TRIGGER` и выше).
2. **Если критический** - буфер (если непуст) сбрасывается в файл с маркером `Dumped <N> logs`, затем **само**
   сообщение записывается в файл. Это происходит **безусловно**, от любого компонента, независимо от пресета.
3. **Если не критический** - применяется фильтрация по пресету; прошедшее фильтр сообщение либо добавляется в
   буфер (если буферизация включена), либо записывается сразу.

### Пресеты компонентов

Пресеты фильтруют **некритические** уровни логирования по компонентам. Формат компонента в сообщении:
`[ComponentName]: message` или `[ComponentName] message` (оба варианта поддерживаются).

```cpp
LumexLogger& logger = LumexLogger::getInstance();

logger.enable_preset({"NetworkManager", "RequestHandler"});
logger.set_preset_components({"AuthenticationService", "DatabaseConnection"}); // Автоматически включает пресет
logger.disable_preset();

if (logger.is_preset_enabled()) {
    auto components = logger.get_preset_components();
}
```

Сопоставление имени компонента с записью пресета гибкое - поддерживаются:

- точное совпадение (`NetworkManager`);
- короткое имя метода (`WriteData` совпадет с `SerialPortChannel::WriteData()`);
- нормальная форма `Class::method` (с необязательными `()`);
- полная сигнатура (когда `FUNCNAME=full`).

**Важно:** критические уровни (WARNING/ERROR/FATAL, либо `BUFFERING_TRIGGER` и выше) **не фильтруются пресетом** -
они всегда логируются и всегда флашат буфер, независимо от того, входит ли компонент в пресет.

## Формат вывода

### Структура сообщения

```log
[время] [уровень] [имя_функции] [TID=id_потока] сообщение
```

### Примеры вывода

```log
[2025-10-02 15:57:07.427] [TRACE] [TID=140234123456] Created new statistics object: <0x2031799451904>
[2025-10-02 16:05:28.643] [TRACE] [ClassName::MethodName()] [TID=140234123456] Processing data values:
```

### Маркер сброса буфера

```log
[2025-10-02 16:05:28.500] [DEBUG] [TID=140234123456] Шаг обработки #1
[2025-10-02 16:05:28.501] [DEBUG] [TID=140234123456] Шаг обработки #2
=======>>> Dumped 2 logs
[2025-10-02 16:05:28.502] [ERROR] [TID=140234123456] Критическая ошибка!
```

### Заголовок и завершение лога

```log
=== Log start of the program ===
Time start: 2025-10-02 16:05:28.643
Path to executable file: /path/to/executable
Logging enabled by file: enable_logs
Log level from trigger-file: LEVEL_DEBUG
Function name mode: NORMAL
Show stacktrace: NO
Stacktrace max frames: 16
Buffering enabled: YES
Buffer size: 200 entries
Buffering trigger: WARNING (dump only on this level or higher, no flush on exit)
Describe frame (tree breakdown): NO
Preset enabled: YES
Preset components: NetworkManager, DatabaseConnection, AuthenticationService
==================================
```

```log
=== End of the program ============
Time end: 2025-10-02 16:05:34.811
Active time: 6.168 seconds
===================================
```

## Файловая структура

```console
<директория_исполняемого_файла>/
├── myapp.exe                    # Исполняемый файл
├── enable_logs                  # Файл-триггер (создается пользователем, имя можно изменить)
├── LOGGER_README.txt            # Файл-хелпер с подсказкой (создается автоматически, если HINT=true)
└── logger.log                   # Файл лога (создается автоматически)
```

При `TIMESTAMPED=true` вместо `logger.log` создается директория `Logger_logs/` с файлами вида
`logger_DD.MM.YYYY-hh-mm-ss.log` (каждый запуск - отдельный файл, без перезаписи).

## Практические рекомендации

- Для продакшена: `LEVEL=warning`/`error`, `BUFFERING=true` с разумным `BUFFER_SIZE` для контекста при ошибках.
- Для точечной отладки конкретного компонента: `LEVEL=trace` + `PRESET=<Component>` + `BUFFERING=true`.
- `BUFFERING_TRIGGER` полезен, когда нужно захватить контекст только вокруг одного конкретного уровня события
  (например, только вокруг ERROR, игнорируя WARNING), и не хочется, чтобы буфер сбрасывался при выходе программы.
- `DESCRIBE_FRAME=true` включайте только на время анализа протокола - это дополнительный объем логов поверх
  обычного уровня детализации.
- `FUNCNAME=none` для минимального размера логов в продакшене; `FUNCNAME=normal` - хороший баланс читаемости
  и краткости для разработки (короче полной сигнатуры, но однозначно указывает класс и метод).

## Устранение неполадок

1. Логирование не работает - проверьте наличие и расположение файла-триггера (`get_trigger_file_name()`).
2. Неожиданный уровень - проверьте синтаксис файла-триггера (case-insensitive, но опечатки в значении FUNCNAME
   уводят к `normal`, а не к значению по умолчанию `full`).
3. Компонент не логируется при включенном пресете - убедитесь, что сообщение содержит `[ComponentName]` в начале
   (через двоеточие или пробел), а PRESET содержит совпадающую запись (точную, короткую или нормальную форму).
4. Критическое сообщение теряется - начиная с этой версии это не должно происходить: WARNING/ERROR/FATAL (и любой
   уровень на или выше `BUFFERING_TRIGGER`) всегда проходит и всегда флашит буфер, независимо от пресета.
