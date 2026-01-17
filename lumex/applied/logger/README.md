\page Logger Универсальная Система Логирования

## Обзор

**Logger** - это универсальная система логирования для C++ приложений, реализованная как потокобезопасный синглтон. Логгер автоматически создает файлы журналов рядом с исполняемым файлом и предоставляет гибкие возможности настройки через конфигурационные файлы.

## Ключевые особенности

### Архитектурные преимущества

- **Синглтон-паттерн**: Единственный экземпляр логгера на все приложение
- **Потокобезопасность**: Полная защита от race conditions через мьютексы
- **Кроссплатформенность**: Поддержка Windows, Linux и macOS
- **Автоматическое определение пути**: Лог создается рядом с исполняемым файлом

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

### Производительность

- **Условная компиляция**: Макросы проверяют уровень на этапе компиляции
- **Буферизация логов**: Опциональная буферизация с автоматической записью при критических событиях
- **Принудительный flush**: Только для критических уровней (ERROR, FATAL)
- **Минимальные накладные расходы**: Оптимизированные операции записи

### Расширенные возможности

- **Буферизация логов**: Настраиваемая буферизация с автоматической записью истории при WARNING/ERROR/FATAL
- **Пресеты компонентов**: Фильтрация логов по компонентам для фокусированного анализа

## Управление логированием

### Включение/отключение логирования

Логирование управляется через специальный файл-триггер `enable_logs` (по умолчанию), который должен находиться в той же директории, что и исполняемый файл (.exe, .dll, .so, .dylib). Имя файла-триггера можно изменить программно через метод `setTriggerFileName()`.

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
Logger& logger = Logger::getInstance();
logger.setTriggerFileName("my_custom_log_trigger"); // Теперь используется my_custom_log_trigger вместо enable_logs
std::string currentName = logger.getTriggerFileName(); // Получить текущее имя
```

### Настройка уровня логирования

Уровень логирования настраивается через содержимое файла-триггера (по умолчанию `enable_logs`):

#### Формат файла конфигурации

Поддерживаются два формата конфигурации:

**Новый формат с префиксами (рекомендуемый):**

```txt
LEVEL=info
FUNCNAME=none
TIMESTAMPED=true
```

**Старый формат (для обратной совместимости):**

```txt
<уровень_логирования>
<настройка_имени_функции>
<настройка_timestamped_логов>
```

> **Важно:** В новом формате с префиксами порядок строк не важен! Можно писать строки в любом порядке.

#### Примеры конфигурации

**1. Минимальная конфигурация (только уровень):**

```txt
LEVEL=trace
```

- Уровень: TRACE
- Имена функций: полная сигнатура (по умолчанию)
- Timestamped логи: отключены (по умолчанию)
- Stacktrace: отключен (по умолчанию, но включен для WARNING/ERROR)
- Файл-хелпер: создается (по умолчанию)

**2. Отключение файла-хелпера:**

```txt
LEVEL=info
HINT=false
```

- Уровень: INFO
- Файл-хелпер: не создается (существующий файл удаляется)

**3. Конфигурация с пресетом компонентов (базовая):**

```txt
LEVEL=debug
PRESET=NetworkManager,DatabaseConnection,FileProcessor
```

- Уровень: DEBUG
- Пресет: включен, логируются только компоненты NetworkManager, DatabaseConnection и FileProcessor
- Формат компонентов в логах: `[NetworkManager]: message`, `[DatabaseConnection]: message`, `[FileProcessor]: message`

**4. Отладка только сетевого компонента:**

```txt
LEVEL=trace
PRESET=NetworkManager
STACKTRACE=true
```

- Уровень: TRACE (максимальная детализация)
- Пресет: включен, логируется только NetworkManager
- Stacktrace: включен для детального анализа

**5. Мониторинг критических компонентов в продакшене:**

```txt
LEVEL=warning
PRESET=DatabaseConnection,AuthenticationService
TIMESTAMPED=true
```

- Уровень: WARNING (только важные события)
- Пресет: включен для DatabaseConnection и AuthenticationService
- Timestamped логи: включены для сохранения истории

**6. Полная отладка с фокусом на нескольких компонентах:**

```txt
LEVEL=trace
FUNCNAME=short
TIMESTAMPED=true
PRESET=CacheManager,SessionManager,NotificationService
STACKTRACE=false
STACKTRACE_FRAMES=32
```

- Уровень: TRACE (максимальная детализация)
- Имена функций: короткие
- Timestamped логи: включены
- Пресет: включен для CacheManager, SessionManager и NotificationService
- Stacktrace: отключен (но будет автоматически включен для WARNING/ERROR)
- Максимум фреймов: 32 (для случаев, когда stacktrace включится)

**7. Анализ производительности определенных компонентов:**

```txt
LEVEL=info
PRESET=RequestHandler,DataValidator
FUNCNAME=none
HINT=false
```

- Уровень: INFO
- Пресет: включен для RequestHandler и DataValidator
- Имена функций: отключены (для меньшего размера логов)
- Файл-хелпер: не создается

**8. Отладка интеграции между компонентами:**

```txt
LEVEL=debug
PRESET=NetworkManager,DatabaseConnection,ConfigLoader
STACKTRACE=true
STACKTRACE_FRAMES=20
```

- Уровень: DEBUG
- Пресет: включен для компонентов, которые взаимодействуют друг с другом
- Stacktrace: включен для отслеживания вызовов между компонентами

**Полная конфигурация с префиксами:**

```txt
LEVEL=debug
FUNCNAME=short
TIMESTAMPED=true
STACKTRACE=true
STACKTRACE_FRAMES=10
```

- Уровень: DEBUG
- Имена функций: короткие (short)
- Timestamped логи: включены
- Stacktrace: включен
- Максимум фреймов: 10
- Файл-хелпер: создается (по умолчанию)

**Пример с произвольным порядком строк:**

```txt
TIMESTAMPED=true
FUNCNAME=signature
LEVEL=warning
STACKTRACE=false
```

- Порядок строк не важен!
- Уровень: WARNING
- Имена функций: полная сигнатура (signature)
- Timestamped логи: включены
- Stacktrace: отключен (но все равно будет показан для WARNING по умолчанию)

**9. Старый формат (все еще поддерживается):**

```txt
debug
none
TIMESTAMPED
```

- Уровень: DEBUG
- Имена функций: отключены (none)
- Timestamped логи: включены

> **Примечание:** Старый формат `NO_FUNCNAME` все еще поддерживается для обратной совместимости, но рекомендуется использовать `none`.

**10. Комбинированный пример: пресет + буферизация через конфигурацию**

```txt
LEVEL=info
PRESET=DatabaseConnection,AuthenticationService
TIMESTAMPED=true
FUNCNAME=short
```

Затем в коде:

```cpp
Logger& logger = Logger::getInstance();
logger.enableBuffering(200); // Включаем буферизацию программно
// Пресет уже активен из конфигурационного файла
```

**11. Отладка конкретного компонента с максимальной детализацией**

```txt
LEVEL=trace
PRESET=NetworkManager
STACKTRACE=true
STACKTRACE_FRAMES=30
TIMESTAMPED=true
FUNCNAME=signature
```

Идеально для глубокой отладки одного проблемного компонента.

**12. Минимальное логирование в продакшене с пресетом**

```txt
LEVEL=error
PRESET=AuthenticationService,DatabaseConnection
FUNCNAME=none
TIMESTAMPED=true
HINT=false
```

Только критические ошибки от самых важных компонентов, минимальное влияние на производительность.

**13. Включение буферизации через конфигурационный файл**

```txt
LEVEL=debug
BUFFERING=true
BUFFER_SIZE=200
```

- Буферизация включена
- Размер буфера: 200 записей
- При WARNING/ERROR/FATAL предыдущие 200 логов будут автоматически записаны

**14. Комбинирование буферизации и пресета через конфигурацию**

```txt
LEVEL=info
PRESET=NetworkManager,DatabaseConnection,AuthenticationService
BUFFERING=true
BUFFER_SIZE=150
TIMESTAMPED=true
```

- Пресет включен для трех компонентов
- Буферизация включена с размером 150 записей
- Timestamped логи включены для сохранения истории
- Идеально для мониторинга критических компонентов с контекстом

**15. Отладка с буферизацией и максимальным размером буфера**

```txt
LEVEL=trace
BUFFERING=true
BUFFER_SIZE=500
STACKTRACE=false
FUNCNAME=short
```

- Максимальная детализация с большим буфером
- При ошибке будет записано до 500 предыдущих логов для полного контекста (**если столько вообще было накоплено!**)

#### Поддерживаемые значения

**Префиксы (case-insensitive, т.е. можно писать хоть `funcname=none`, хоть `FUnCnaMe=NOne`):**

- `LEVEL=` - уровень логирования
- `FUNCNAME=` - настройка имени функции (none/short/signature)
- `TIMESTAMPED=` - настройка timestamped логов
- `STACKTRACE=` - настройка показа stacktrace (true/false)
- `STACKTRACE_FRAMES=` - максимальное количество фреймов в stacktrace (1-64)
- `HINT=` - создавать ли файл-хелпер с подсказкой для разработчиков (true/false, по умолчанию true)
- `PRESET=` - фильтрация логов по компонентам (список компонентов через запятую, например: NetworkManager,DatabaseConnection,FileProcessor)
- `BUFFERING=` - включить/отключить буферизацию логов (true/false, по умолчанию false)
- `BUFFER_SIZE=` - размер буфера логов в количестве записей (положительное число, по умолчанию 100)

**Уровни логирования** (case-insensitive):

- `trace` -> LEVEL_TRACE
- `debug` -> LEVEL_DEBUG
- `info` -> LEVEL_INFO
- `success` -> LEVEL_SUCCESS
- `warning` -> LEVEL_WARNING
- `error` -> LEVEL_ERROR
- `fatal` -> LEVEL_FATAL

**Настройка имен функций** (case-insensitive):

- `none` -> отключить имена функций
- `short` -> показывать короткое имя функции (`__func__`)
- `signature` -> показывать полную сигнатуру функции (`__FUNCSIG__`/`__PRETTY_FUNCTION__`)
- По умолчанию: `signature` (полная сигнатура)

> **Примечание:** Старое значение `NO_FUNCNAME` все еще поддерживается для обратной совместимости и эквивалентно `none`.

**Настройка timestamped логов** (case-insensitive):

- `true`, `yes`, `1`, `TIMESTAMPED` -> использовать timestamped логи
- Любое другое значение или пустая строка -> использовать обычный файл лога

**Настройка stacktrace** (case-insensitive):

- `true`, `yes`, `1` -> показывать stacktrace в логах **под абсолютно каждую функцию** (для всех уровней логирования)
- `false`, `no`, `0` -> не показывать stacktrace
- По умолчанию: `false`, но автоматически `true` для уровней WARNING и ERROR (даже если `STACKTRACE=false`)
- **Важно:** Если `STACKTRACE=true` указан явно, то stacktrace будет записываться для **всех** уровней логирования (TRACE, DEBUG, INFO, SUCCESS, WARNING, ERROR, FATAL), а не только для WARNING/ERROR

**Настройка количества фреймов в stacktrace**:

- Число от 1 до 64 -> максимальное количество фреймов для захвата
- По умолчанию: `16`

**Настройка файла-хелпера** (case-insensitive):

- `true`, `yes`, `1` -> создавать файл `LOGGER_README.txt` с подсказкой для разработчиков
- `false`, `no`, `0` -> не создавать файл-хелпер (если файл существует - удаляется)
- По умолчанию: `true` (файл создается автоматически при старте Logger)

**Настройка пресетов компонентов** (case-insensitive):

- Список компонентов через запятую (например: `NetworkManager,DatabaseConnection,FileProcessor`) -> логируются только указанные компоненты
- Формат компонента в логах: `[ComponentName]: message`
- Если `PRESET` задан, логируются только сообщения от указанных компонентов
- Сообщения без компонента не логируются при включенном пресете
- По умолчанию: пресет отключен (логируются все сообщения)

**Настройка буферизации логов** (case-insensitive):

- `BUFFERING=true` или `BUFFERING=yes` или `BUFFERING=1` -> включить буферизацию
- `BUFFERING=false` или `BUFFERING=no` или `BUFFERING=0` -> отключить буферизацию (по умолчанию)
- При включенной буферизации логи TRACE, DEBUG, INFO, SUCCESS сохраняются в буфере и не записываются сразу
- При возникновении WARNING/ERROR/FATAL все записи из буфера автоматически записываются в файл перед критическим сообщением
- Буфер имеет ограниченный размер, старые записи удаляются при переполнении

**Настройка размера буфера логов**:

- Положительное число -> максимальное количество записей в буфере (по умолчанию 100)
- Используется только при `BUFFERING=true`
- Минимальное значение: 1
- Рекомендуемые значения: 50-500 в зависимости от частоты логирования и требований к контексту

> Причем, что уровни логирования можно писать хоть `TrAcE`, хоть `TRACE`, система все равно поймет, она case-insensitive. То же самое касается и всех других параметров (FUNCNAME, TIMESTAMPED, STACKTRACE). Также в строке могут быть пробелы и символы табуляции, они будут стерты, например из " waRNIng " -> "WARNING". Файл-триггер останется прежним.
>
> **Важно о stacktrace**:
>
> - По умолчанию stacktrace автоматически включается для уровней WARNING и ERROR, даже если `STACKTRACE=false` не указан явно. Это потенциально может помочь при отладке ошибок и предупреждений.
> - Если `STACKTRACE=true` указан явно, то stacktrace будет записываться **под абсолютно каждую функцию** для **всех** уровней логирования (TRACE, DEBUG, DEBUG, INFO, SUCCESS, WARNING, ERROR, FATAL), а не только для WARNING/ERROR. Это может значительно увеличить размер логов и снизить производительность.

#### Практические сценарии использования пресетов

**Сценарий 1: Отладка проблем с сетевыми запросами**

Когда приложение не может подключиться к API или запросы падают, включите логирование только сетевых компонентов:

```txt
LEVEL=debug
PRESET=NetworkManager,RequestHandler
STACKTRACE=true
TIMESTAMPED=true
```

В логах будут только сообщения от `NetworkManager` и `RequestHandler`, что упрощает поиск проблемы.

**Сценарий 2: Анализ производительности базы данных**

При медленных запросах к БД сфокусируйтесь на компонентах, работающих с данными:

```txt
LEVEL=info
PRESET=DatabaseConnection,CacheManager,DataValidator
FUNCNAME=none
TIMESTAMPED=true
```

Логи будут содержать только операции с БД, кэшем и валидацией данных, без лишней информации.

**Сценарий 3: Расследование проблем безопасности**

При подозрении на проблемы с аутентификацией или авторизацией:

```txt
LEVEL=warning
PRESET=AuthenticationService,SessionManager,ConfigLoader
TIMESTAMPED=true
STACKTRACE=true
```

Все предупреждения и ошибки от компонентов безопасности будут логироваться с полным контекстом.

**Сценарий 4: Отладка файловых операций**

При проблемах с чтением/записью файлов:

```txt
LEVEL=trace
PRESET=FileProcessor,ConfigLoader
STACKTRACE=false
FUNCNAME=short
```

Максимальная детализация только для файловых операций.

**Сценарий 5: Мониторинг критической функциональности в продакшене**

Для продакшена, когда нужно отслеживать только критически важные компоненты:

```txt
LEVEL=error
PRESET=DatabaseConnection,AuthenticationService,NetworkManager
TIMESTAMPED=true
HINT=false
```

Только критические ошибки от важнейших компонентов системы.

**Сценарий 6: Полная отладка нового функционала**

При разработке нового функционала, включающего несколько компонентов:

```txt
LEVEL=trace
PRESET=NewFeatureProcessor,DataValidator,CacheManager,NotificationService
STACKTRACE=true
STACKTRACE_FRAMES=20
TIMESTAMPED=true
```

Все логи от компонентов, связанных с новой функциональностью, с максимальной детализацией.

**Сценарий 7: Анализ интеграции между модулями**

Когда нужно понять, как взаимодействуют компоненты:

```txt
LEVEL=debug
PRESET=NetworkManager,DatabaseConnection,SessionManager,CacheManager
STACKTRACE=true
FUNCNAME=short
```

Логи всех компонентов, которые передают данные друг другу, с информацией о вызовах функций.

**Сценарий 8: Минимальное логирование для продакшена**

Для продакшена с минимальным влиянием на производительность:

```txt
LEVEL=warning
PRESET=AuthenticationService,DatabaseConnection
FUNCNAME=none
TIMESTAMPED=true
HINT=false
```

Только предупреждения от самых критичных компонентов, без лишних деталей.

**Сценарий 9: Отладка с буферизацией для анализа контекста ошибок**

При необходимости видеть полный контекст перед ошибками:

```txt
LEVEL=debug
BUFFERING=true
BUFFER_SIZE=300
PRESET=NetworkManager,DatabaseConnection
TIMESTAMPED=true
```

Буферизация включена с большим размером буфера, при ошибках будет записано до 300 предыдущих логов с временными метками.

**Сценарий 10: Продакшен мониторинг с буферизацией**

Для продакшена с сохранением контекста при критических ошибках:

```txt
LEVEL=error
BUFFERING=true
BUFFER_SIZE=100
PRESET=AuthenticationService,DatabaseConnection,NetworkManager
TIMESTAMPED=true
FUNCNAME=none
```

Только критические ошибки, но с контекстом из последних 100 логов перед ошибкой.

## Использование в коде

### Базовые макросы логирования

```cpp
#include "Logger.hpp"

void exampleFunction() {
    // Различные уровни логирования
    LOGGER_LOG_TRACE("Детальная отладочная информация");
    LOGGER_LOG_DEBUG("Отладочное сообщение");
    LOGGER_LOG_INFO("Информационное сообщение");
    LOGGER_LOG_SUCCESS("Операция выполнена успешно");
    LOGGER_LOG_WARNING("Предупреждение");
    LOGGER_LOG_ERROR("Ошибка");
    LOGGER_LOG_FATAL("Критическая ошибка");

    // Логирование с переменными
    int value = 42;
    std::string name = "test";
    LOGGER_LOG_INFO("Обработка значения:", value, "для объекта:", name);

    // Логирование с компонентами (для использования с пресетами)
    LOGGER_LOG_INFO("[NetworkManager]: Инициализация сетевого менеджера");
    LOGGER_LOG_DEBUG("[DatabaseConnection]: Выполнение запроса, параметры:", value);
}
```

### Специализированные макросы

#### Логирование адресов объектов

```cpp
MyClass* obj = new MyClass();
LOGGER_LOG_ADDRESS_INFO("Создан объект", obj);
// Вывод: [2025-10-02 15:57:07.427] [LEVEL_INFO] Создан объект <0x2031799451904>
```

#### Логирование объектов с контекстом

```cpp
MyClass* obj = new MyClass();
LOGGER_LOG_OBJECT_DEBUG(obj, "Инициализация завершена");
// Вывод: [2025-10-02 15:57:07.427] [LEVEL_DEBUG] Object <0x2031799451904>: Инициализация завершена
```

### Программное управление

```cpp
// Получение экземпляра логгера
Logger& logger = Logger::getInstance();

// Проверка состояния
if (logger.isLoggingEnabled()) {
    // Логирование включено
}

// Проверка уровня
if (logger.isLevelEnabled(LogLevel::LEVEL_DEBUG)) {
    // DEBUG уровень активен
}

// Установка уровня программно
logger.setLogLevel(LogLevel::LEVEL_WARNING);

// Принудительная запись в файл
logger.flush();
```

### Буферизация логов

Буферизация позволяет не записывать все логи сразу, а сохранять их в буфере. При возникновении критических событий (WARNING, ERROR, FATAL) все записи из буфера автоматически записываются в файл, что позволяет видеть контекст перед критическим событием.

Буферизацию можно настроить двумя способами:

1. **Через конфигурационный файл** (`enable_logs`) - настройки применяются при запуске программы
2. **Программно** - настройки можно менять во время выполнения

#### Включение и настройка буферизации программно

```cpp
Logger& logger = Logger::getInstance();

// Включить буферизацию с размером буфера 100 записей (по умолчанию)
logger.enableBuffering();

// Включить буферизацию с пользовательским размером буфера
logger.enableBuffering(200); // Буфер на 200 записей

// Изменить размер буфера
logger.setBufferSize(150);

// Отключить буферизацию (все записи из буфера будут записаны)
logger.disableBuffering();

// Проверка состояния
if (logger.isBufferingEnabled()) {
    size_t bufferSize = logger.getBufferSize();
    // Буферизация активна
}
```

#### Как работает буферизация

- **Обычные уровни (TRACE, DEBUG, INFO, SUCCESS)**: При включенной буферизации логи сохраняются в буфере и не записываются сразу
- **Критические уровни (WARNING, ERROR, FATAL)**: При возникновении критического события:
  1. Все записи из буфера записываются в файл с сохранением временных меток
  2. Затем записывается само критическое сообщение
  3. Это позволяет видеть контекст перед ошибкой

#### Пример использования программно

```cpp
Logger& logger = Logger::getInstance();
logger.enableBuffering(100); // Буфер на 100 записей

// Эти логи будут сохранены в буфере
LOGGER_LOG_TRACE("Начало обработки");
LOGGER_LOG_DEBUG("Шаг 1 выполнен");
LOGGER_LOG_INFO("Шаг 2 выполнен");
// ... еще 97 логов ...

// При возникновении ошибки все 100 предыдущих логов будут записаны
LOGGER_LOG_ERROR("Критическая ошибка!"); // Буфер записывается автоматически
```

#### Пример использования через конфигурационный файл

**Пример 1: Включение буферизации через конфигурацию**

```txt
# В файле enable_logs
LEVEL=debug
BUFFERING=true
BUFFER_SIZE=200
```

В этом случае при запуске программы буферизация автоматически включится с размером буфера 200 записей. Никаких изменений в коде не требуется!

**Пример 2: Комбинирование буферизации и пресета через конфигурацию**

```txt
# В файле enable_logs
LEVEL=info
PRESET=NetworkManager,DatabaseConnection
BUFFERING=true
BUFFER_SIZE=150
TIMESTAMPED=true
```

При запуске программы:

- Буферизация будет включена с размером 150 записей
- Пресет будет активен для NetworkManager и DatabaseConnection
- Логи будут сохраняться с временными метками

**Пример 3: Отладка проблем с полным контекстом**

```cpp
// В конфигурационном файле enable_logs:
// LEVEL=trace
// BUFFERING=true
// BUFFER_SIZE=300

void processComplexOperation() {
    // Эти логи будут сохраняться в буфере (300 записей)
    LOGGER_LOG_TRACE("[NetworkManager]: Инициализация соединения");
    LOGGER_LOG_DEBUG("[NetworkManager]: Отправка запроса");
    LOGGER_LOG_INFO("[DatabaseConnection]: Выполнение запроса");
    // ... множество других логов ...

    // При ошибке все 300 предыдущих логов автоматически запишутся в файл
    if (errorOccurred) {
        LOGGER_LOG_ERROR("[NetworkManager]: Ошибка при обработке запроса");
        // В файл будут записаны все предыдущие 300 логов + текущая ошибка
        // Это даст полный контекст перед ошибкой для анализа
    }
}
```

### Пресеты компонентов

Пресеты позволяют фильтровать логи по компонентам, что полезно для фокусированного анализа работы конкретных частей системы.

#### Формат компонентов в логах

Для использования пресетов компоненты должны быть указаны в формате `[ComponentName]: message`:

```cpp
// Компонент NetworkManager
LOGGER_LOG_INFO("[NetworkManager]: Инициализация сетевого менеджера");
LOGGER_LOG_DEBUG("[NetworkManager]: Отправка запроса на сервер:", url);

// Компонент DatabaseConnection
LOGGER_LOG_INFO("[DatabaseConnection]: Подключение к базе данных");
LOGGER_LOG_WARNING("[DatabaseConnection]: Медленный запрос, время выполнения:", duration, "ms");

// Компонент AuthenticationService
LOGGER_LOG_INFO("[AuthenticationService]: Проверка токена пользователя");
LOGGER_LOG_ERROR("[AuthenticationService]: Неверный токен, отказ в доступе");
```

#### Включение и настройка пресетов

```cpp
Logger& logger = Logger::getInstance();

// Включить пресет с набором компонентов (мониторинг сетевых операций)
std::unordered_set<std::string> networkComponents = {"NetworkManager", "RequestHandler"};
logger.enablePreset(networkComponents);

// Или установить компоненты (автоматически включает пресет)
// Фокус на компонентах безопасности и данных
logger.setPresetComponents({"AuthenticationService", "DatabaseConnection", "DataValidator"});

// Отключить пресет (логируются все компоненты)
logger.disablePreset();

// Проверка состояния
if (logger.isPresetEnabled()) {
    auto presetComponents = logger.getPresetComponents();
    // Пресет активен, логируются только указанные компоненты
    for (const auto& component : presetComponents) {
        LOGGER_LOG_DEBUG("[ConfigLoader]: Пресет включает компонент:", component);
    }
}
```

#### Настройка буферизации через конфигурационный файл

Буферизацию можно настроить через файл-триггер без изменения кода:

**Пример 1: Включение буферизации с размером по умолчанию (100)**

```txt
LEVEL=debug
BUFFERING=true
```

**Пример 2: Включение буферизации с пользовательским размером**

```txt
LEVEL=info
BUFFERING=true
BUFFER_SIZE=250
```

**Пример 3: Комбинирование буферизации с пресетом**

```txt
LEVEL=debug
PRESET=NetworkManager,DatabaseConnection
BUFFERING=true
BUFFER_SIZE=200
```

- Буферизация автоматически включится при запуске программы
- Можно изменить настройки в любой момент, просто отредактировав файл
- Изменения применяются при следующем запуске программы

#### Настройка пресетов через конфигурационный файл

Пресеты можно настроить через файл-триггер без изменения кода. Это позволяет менять набор логируемых компонентов без перекомпиляции:

**Пример 1: Отладка сетевого стека с буферизацией**

```txt
LEVEL=trace
PRESET=NetworkManager,RequestHandler
BUFFERING=true
BUFFER_SIZE=250
STACKTRACE=true
```

- При запуске программы пресет и буферизация автоматически включится
- Логируются только NetworkManager и RequestHandler
- Буферизация сохраняет до 250 предыдущих логов для контекста при ошибках
- Stacktrace включен для детального анализа вызовов

**Пример 2: Мониторинг компонентов безопасности с буферизацией**

```txt
LEVEL=info
PRESET=AuthenticationService,SessionManager
BUFFERING=true
BUFFER_SIZE=100
TIMESTAMPED=true
```

- Логируются только AuthenticationService и SessionManager
- Буферизация включена для сохранения контекста при ошибках безопасности
- Уровень INFO подходит для продакшена
- Timestamped логи сохраняют историю для анализа безопасности

**Пример 3: Отладка проблем с данными с большим буфером**

```txt
LEVEL=debug
PRESET=DatabaseConnection,DataValidator,CacheManager
BUFFERING=true
BUFFER_SIZE=300
FUNCNAME=short
```

- Фокус на компонентах, работающих с данными
- Большой буфер (300 записей) для сложных операций с данными
- Короткие имена функций для уменьшения размера логов
- DEBUG уровень обеспечивает достаточную детализацию

**Пример 4: Анализ производительности**

```txt
LEVEL=info
PRESET=FileProcessor,ConfigLoader,NotificationService
BUFFERING=false
FUNCNAME=none
HINT=false
```

- Логируются только компоненты, которые могут влиять на производительность
- Буферизация отключена для минимальной задержки записи
- Имена функций отключены для минимального размера логов
- Файл-хелпер отключен

**Пример 5: Отладка интеграции между компонентами с полным контекстом**

```txt
LEVEL=trace
PRESET=NetworkManager,DatabaseConnection,CacheManager,SessionManager
BUFFERING=true
BUFFER_SIZE=400
STACKTRACE=true
STACKTRACE_FRAMES=25
TIMESTAMPED=true
```

- Логируются компоненты, которые активно взаимодействуют друг с другом
- Очень большой буфер (400 записей) для полного контекста взаимодействия
- Максимальная детализация с stacktrace
- Timestamped логи для отслеживания временных зависимостей

**Пример 6: Продакшен мониторинг с минимальной буферизацией**

```txt
LEVEL=error
PRESET=AuthenticationService,DatabaseConnection,NetworkManager
BUFFERING=true
BUFFER_SIZE=50
TIMESTAMPED=true
FUNCNAME=none
HINT=false
```

- Только критические ошибки от важнейших компонентов
- Минимальный буфер (50 записей) для экономии памяти
- Timestamped логи для анализа временных паттернов ошибок

**Важные замечания:**

- Можно изменить список компонентов, настройки буферизации и другие параметры в любой момент, просто отредактировав файл `enable_logs`
- Изменения применяются при следующем запуске программы (при инициализации Logger)
- Компоненты в списке разделяются запятыми, пробелы игнорируются
- Имена компонентов чувствительны к регистру (case-sensitive): `NetworkManager` ≠ `networkmanager`
- Буферизация может быть включена/отключена независимо от пресета, они работают совместно

#### Поведение пресетов

- **Пресет отключен (по умолчанию)**: Логируются все сообщения независимо от компонента
- **Пресет включен**: Логируются только сообщения от компонентов, указанных в пресете
- **Сообщения без компонента**: Если сообщение не содержит компонент в формате `[ComponentName]:`, оно не логируется при включенном пресете
- **Совместимость**: Существующие макросы работают как обычно, но для использования пресетов нужно явно указывать компонент в сообщении

#### Пример использования

**Пример 1: Базовое использование пресетов**

```cpp
Logger& logger = Logger::getInstance();

// Включить пресет для сетевых компонентов
logger.enablePreset({"NetworkManager", "RequestHandler"});

// Эти логи будут записаны (компоненты в пресете)
LOGGER_LOG_INFO("[NetworkManager]: Инициализация сетевого стека");
LOGGER_LOG_DEBUG("[RequestHandler]: Обработка HTTP запроса:", method, url);

// Эти логи НЕ будут записаны (компоненты не в пресете)
LOGGER_LOG_INFO("[DatabaseConnection]: Подключение к БД");
LOGGER_LOG_INFO("[FileProcessor]: Обработка файла:", filename);
LOGGER_LOG_INFO("[AuthenticationService]: Проверка пользователя");

// Это сообщение НЕ будет записано (нет компонента)
LOGGER_LOG_INFO("Общее сообщение без указания компонента");
```

**Пример 2: Реальный сценарий - отладка проблем с аутентификацией**

```cpp
// В конфигурационном файле enable_logs:
// LEVEL=debug
// PRESET=AuthenticationService,SessionManager,DatabaseConnection

void handleUserLogin(const std::string& username, const std::string& password) {
    // Этот лог будет записан (AuthenticationService в пресете)
    LOGGER_LOG_DEBUG("[AuthenticationService]: Попытка входа пользователя:", username);

    // Проверка учетных данных
    bool isValid = validateCredentials(username, password);

    if (isValid) {
        // Этот лог будет записан (SessionManager в пресете)
        LOGGER_LOG_INFO("[SessionManager]: Создана новая сессия для пользователя:", username);

        // Этот лог НЕ будет записан (CacheManager не в пресете)
        LOGGER_LOG_DEBUG("[CacheManager]: Обновление кэша пользователя");
    } else {
        // Этот лог будет записан (AuthenticationService в пресете)
        LOGGER_LOG_WARNING("[AuthenticationService]: Неудачная попытка входа:", username);
    }

    // Этот лог НЕ будет записан (FileProcessor не в пресете)
    LOGGER_LOG_DEBUG("[FileProcessor]: Сохранение лога доступа в файл");
}
```

**Пример 3: Мониторинг производительности конкретных компонентов**

```cpp
// В конфигурационном файле enable_logs:
// LEVEL=info
// PRESET=DatabaseConnection,CacheManager,FileProcessor

void processUserRequest(const Request& req) {
    auto startTime = std::chrono::steady_clock::now();

    // Этот лог будет записан
    LOGGER_LOG_INFO("[DatabaseConnection]: Выполнение запроса к БД");
    auto data = database.query(req.getQuery());

    // Этот лог будет записан
    LOGGER_LOG_INFO("[CacheManager]: Обновление кэша, ключ:", req.getCacheKey());
    cache.update(req.getCacheKey(), data);

    // Этот лог будет записан
    LOGGER_LOG_INFO("[FileProcessor]: Сохранение результата в файл");
    fileProcessor.save(data, req.getOutputPath());

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();

    // Этот лог НЕ будет записан (NetworkManager не в пресете)
    LOGGER_LOG_INFO("[NetworkManager]: Отправка ответа клиенту, время:", duration, "ms");
}
```

#### Комбинирование буферизации и пресетов

Буферизация и пресеты могут работать вместе для создания мощной системы отладки:

**Пример 1: Отладка сетевых проблем с контекстом**

```cpp
// В конфигурационном файле enable_logs:
// LEVEL=debug
// PRESET=NetworkManager,RequestHandler

Logger& logger = Logger::getInstance();
logger.enableBuffering(150); // Буфер на 150 записей
// Пресет уже включен через конфигурационный файл

void processNetworkRequest() {
    // Эти логи будут буферизироваться (компоненты в пресете)
    LOGGER_LOG_TRACE("[NetworkManager]: Создание нового соединения");
    LOGGER_LOG_DEBUG("[RequestHandler]: Парсинг HTTP заголовков");
    LOGGER_LOG_DEBUG("[NetworkManager]: Отправка запроса на сервер");
    LOGGER_LOG_INFO("[RequestHandler]: Получен ответ, статус:", statusCode);

    // ... еще множество логов NetworkManager и RequestHandler ...

    // При ошибке все предыдущие 150 логов будут записаны в файл
    if (statusCode >= 500) {
        LOGGER_LOG_ERROR("[NetworkManager]: Ошибка сервера, код:", statusCode);
        // В файл будут записаны все предыдущие логи NetworkManager и RequestHandler
        // Это позволяет увидеть полный контекст перед ошибкой
    }
}
```

**Пример 2: Анализ проблем с базой данных (буферизация через конфигурацию)**

```cpp
// В конфигурационном файле enable_logs:
// LEVEL=trace
// PRESET=DatabaseConnection,DataValidator,CacheManager
// BUFFERING=true
// BUFFER_SIZE=200

// Никаких изменений в коде не требуется!
// Буферизация и пресет автоматически включатся при запуске

void processDatabaseTransaction() {
    // Буферизируются только логи компонентов из пресета
    LOGGER_LOG_DEBUG("[DatabaseConnection]: Начало транзакции");
    LOGGER_LOG_TRACE("[DataValidator]: Валидация входных данных");
    LOGGER_LOG_DEBUG("[DatabaseConnection]: Выполнение SQL запроса");
    LOGGER_LOG_INFO("[CacheManager]: Инвалидация кэша после изменения данных");

    // Логи других компонентов не попадут в буфер (не в пресете)
    LOGGER_LOG_DEBUG("[FileProcessor]: Архивирование старых данных"); // НЕ будет записано

    // При ошибке буфер запишется
    if (dbError) {
        LOGGER_LOG_ERROR("[DatabaseConnection]: Ошибка транзакции:", errorMessage);
        // В файл будут записаны все предыдущие логи DatabaseConnection, DataValidator и CacheManager
    }
}
```

**Пример 3: Продакшен мониторинг критических компонентов (все через конфигурацию)**

```cpp
// В конфигурационном файле enable_logs:
// LEVEL=info
// PRESET=AuthenticationService,SessionManager,DatabaseConnection
// BUFFERING=true
// BUFFER_SIZE=100
// TIMESTAMPED=true

// Все настройки применяются автоматически при запуске, без изменений в коде!

void handleSecureRequest() {
    // Информационные логи буферизируются
    LOGGER_LOG_INFO("[AuthenticationService]: Проверка токена");
    LOGGER_LOG_INFO("[SessionManager]: Получение сессии пользователя");
    LOGGER_LOG_INFO("[DatabaseConnection]: Загрузка данных пользователя");

    // При критической ошибке буфер запишется
    if (authenticationFailed) {
        LOGGER_LOG_ERROR("[AuthenticationService]: Критическая ошибка безопасности");
        // Все предыдущие логи будут записаны с сохранением временных меток
        // Это критично для анализа инцидентов безопасности
    }
}
```

**Пример 4: Комбинирование программной и конфигурационной настройки**

```cpp
// В конфигурационном файле enable_logs:
// LEVEL=debug
// PRESET=NetworkManager,RequestHandler

Logger& logger = Logger::getInstance();
// Пресет уже включен из конфигурации
// Но мы можем включить буферизацию программно с другим размером
logger.enableBuffering(250); // Переопределяем размер буфера программно

void processRequest() {
    // Логи NetworkManager и RequestHandler буферизируются
    LOGGER_LOG_DEBUG("[NetworkManager]: Создание соединения");
    LOGGER_LOG_INFO("[RequestHandler]: Обработка запроса");

    // При ошибке буфер запишется
    if (error) {
        LOGGER_LOG_ERROR("[NetworkManager]: Ошибка сети");
    }
}
```

## Формат вывода

### Структура сообщения

```log
[время] [уровень] [имя_функции] сообщение
```

### Примеры вывода

#### Без именования функций

```log
[2025-10-02 15:57:07.427] [LEVEL_TRACE] Created new statistics object: <0x2031799451904>
[2025-10-02 15:57:07.427] [LEVEL_DEBUG] Object <0x2031799451904>: Copied all atomic values from source object
```

#### С именованием функций

```log
[2025-10-02 16:05:28.643] [LEVEL_TRACE] [__cdecl MyNamespace::MyClass::MyClass(void)] Created new object: <0x1543201391656>
[2025-10-02 16:05:28.644] [LEVEL_TRACE] [__cdecl MyNamespace::MyClass::ProcessData(void)] Processing data values:
```

### Заголовок и завершение лога

#### Начало программы

**Пример 1: Без буферизации и пресета**

```log
=== Log start of the program ===
Time start: 2025-10-02 16:05:28.643
Path to executable file: /path/to/executable
Logging enabled by file: enable_logs
Log level from trigger-file: LEVEL_TRACE
Function name mode: SIGNATURE
Show stacktrace: NO
Stacktrace max frames: 16
Buffering enabled: NO
Preset enabled: NO
==================================
```

**Пример 2: С буферизацией и пресетом**

```log
=== Log start of the program ===
Time start: 2025-10-02 16:05:28.643
Path to executable file: /path/to/executable
Logging enabled by file: enable_logs
Log level from trigger-file: LEVEL_DEBUG
Function name mode: SHORT
Show stacktrace: NO
Stacktrace max frames: 16
Buffering enabled: YES
Buffer size: 200 entries
Preset enabled: YES
Preset components: NetworkManager, DatabaseConnection, AuthenticationService
==================================
```

#### Завершение программы

```log
=== End of the program ============
Time end: 2025-10-02 16:05:34.811
Active time: 6.168 seconds
===================================
```

## Файловая структура

### Расположение файлов

**Обычный режим логирования:**

```console
<директория_исполняемого_файла>/
├── myapp.exe                    # Исполняемый файл
├── enable_logs                  # Файл-триггер (создается пользователем, имя можно изменить)
├── LOGGER_README.txt            # Файл-хелпер с подсказкой (создается автоматически, если HINT=true)
└── logger.log                   # Файл лога (создается автоматически)
```

**Режим timestamped логов:**

```console
<директория_исполняемого_файла>/
├── myapp.exe                    # Исполняемый файл
├── enable_logs                  # Файл-триггер (создается пользователем, имя можно изменить)
├── LOGGER_README.txt            # Файл-хелпер с подсказкой (создается автоматически, если HINT=true)
└── Logger_logs/                 # Директория timestamped логов (создается автоматически)
    ├── logger_01.01.2025-15:30:45.log
    ├── logger_01.01.2025-16:45:12.log
    └── logger_02.01.2025-09:15:30.log
```

### Особенности файлов

**enable_logs (или другое имя, заданное через setTriggerFileName()):**

- Создается пользователем для включения логирования
- Содержит конфигурацию (уровень и настройки)
- Может быть пустым файлом (используются значения по умолчанию)

**logger.log (обычный режим):**

- Создается автоматически при запуске программы
- Перезаписывается при каждом запуске
- Содержит полный журнал работы программы

**Logger_logs/ (timestamped режим):**

- Директория создается автоматически при первом запуске с TIMESTAMPED
- Каждый запуск создает новый файл с уникальным timestamp
- Файлы не перезаписываются, сохраняется история запусков
- Формат имени файла: `logger_DD.MM.YYYY-hh-mm-ss.log`

**LOGGER_README.txt (файл-хелпер):**

- Создается автоматически при старте Logger, если `HINT=true` (по умолчанию)
- Содержит подробную инструкцию по настройке логгера на русском и английском языках
- Обновляется при каждом запуске программы (перезаписывается)
- Если в конфигурации указано `HINT=false`, файл не создается, а существующий файл удаляется
- Полезен для разработчиков, которые не знают, как настроить логирование

## Практические рекомендации

### Выбор режима логирования

**Обычный режим (перезаписываемый файл):**

```txt
LEVEL=info
BUFFERING=true
BUFFER_SIZE=100
```

- Подходит для большинства случаев
- Один файл лога, перезаписывается при каждом запуске
- Буферизация для сохранения контекста перед ошибками
- Простота управления и анализа

**Timestamped режим (история запусков):**

```txt
LEVEL=info
TIMESTAMPED=true
BUFFERING=true
BUFFER_SIZE=100
```

- Подходит для отладки и анализа истории запусков
- Каждый запуск создает отдельный файл
- Буферизация работает в каждом файле независимо
- Позволяет отслеживать изменения в поведении программы
- Полезно для долгосрочного мониторинга

### Выбор уровня логирования

**Для разработки:**

```txt
LEVEL=trace
BUFFERING=true
BUFFER_SIZE=300
```

- Максимальная детализация
- Буферизация для сохранения контекста перед ошибками
- Помогает в отладке сложных алгоритмов

**Для тестирования:**

```txt
LEVEL=debug
BUFFERING=true
BUFFER_SIZE=200
```

- Баланс между детализацией и производительностью
- Буферизация обеспечивает контекст при ошибках
- Подходит для поиска проблем

**Для продакшена:**

```txt
LEVEL=info
BUFFERING=true
BUFFER_SIZE=100
```

- Минимально необходимые сообщения
- Буферизация позволяет видеть контекст перед критическими ошибками
- Оптимальная производительность с сохранением полезного контекста

**Для критических систем:**

```txt
LEVEL=warning
BUFFERING=true
BUFFER_SIZE=50
```

- Только важные события и проблемы
- Небольшой буфер для минимального контекста при ошибках
- Максимальная производительность

### Использование пресетов компонентов

Пресеты компонентов - мощный инструмент для фокусированной отладки и мониторинга. Вот практические рекомендации:

**1. Для отладки конкретных проблем:**

```txt
# Отладка сетевых проблем с буферизацией для контекста
LEVEL=trace
PRESET=NetworkManager,RequestHandler
BUFFERING=true
BUFFER_SIZE=200
STACKTRACE=true

# Отладка проблем с БД
LEVEL=debug
PRESET=DatabaseConnection,DataValidator
BUFFERING=true
BUFFER_SIZE=150
TIMESTAMPED=true
```

**2. Для мониторинга в продакшене:**

```txt
# Мониторинг критических компонентов с буферизацией для контекста ошибок
LEVEL=warning
PRESET=AuthenticationService,DatabaseConnection,NetworkManager
BUFFERING=true
BUFFER_SIZE=100
TIMESTAMPED=true
FUNCNAME=none
HINT=false
```

**3. Для анализа производительности:**

```txt
# Анализ медленных операций
LEVEL=info
PRESET=DatabaseConnection,CacheManager,FileProcessor
FUNCNAME=short
TIMESTAMPED=true
```

**4. При разработке новой функциональности:**

```txt
# Полная отладка нового модуля
LEVEL=trace
PRESET=NewFeatureProcessor,RelatedComponent1,RelatedComponent2
STACKTRACE=true
TIMESTAMPED=true
```

**Рекомендации по именованию компонентов:**

- Используйте понятные имена: `NetworkManager`, `DatabaseConnection`, `FileProcessor`
- Избегайте слишком коротких имен: `NM`, `DB`, `FP` (плохо) → `NetworkManager`, `DatabaseConnection`, `FileProcessor` (хорошо)
- Будьте последовательны: используйте одинаковый стиль именования во всем проекте (PascalCase, camelCase и т.д.)
- Группируйте логически связанные компоненты: все компоненты работы с сетью могут иметь префикс `Network*`

**Типичные наборы компонентов для различных сценариев:**

- **Сетевая подсистема**: `NetworkManager`, `RequestHandler`, `ResponseBuilder`, `ConnectionPool`
- **Работа с данными**: `DatabaseConnection`, `DataValidator`, `CacheManager`, `QueryBuilder`
- **Безопасность**: `AuthenticationService`, `SessionManager`, `AuthorizationHandler`, `EncryptionService`
- **Файловая система**: `FileProcessor`, `ConfigLoader`, `LogRotator`, `ArchiveManager`
- **Обработка событий**: `EventDispatcher`, `NotificationService`, `MessageQueue`, `TaskScheduler`

### Оптимизация производительности

1. **Используйте условную компиляцию**: Макросы автоматически исключают неактивные уровни
2. **Избегайте частых flush()**: Критические уровни автоматически сбрасывают буферы
3. **Настройте уровень под задачу**: Не используйте TRACE в продакшене
4. **Отключайте имена функций**: Для максимальной производительности используйте `FUNCNAME=none`
5. **Используйте пресеты**: Логирование только нужных компонентов значительно снижает нагрузку на I/O
6. **Используйте буферизацию для контекста**: При необходимости видеть полный контекст перед ошибками включите буферизацию через `BUFFERING=true` в конфигурационном файле
7. **Настраивайте размер буфера под задачу**: Для частого логирования используйте больший буфер (200-500), для редкого - меньший (50-100)

### Безопасность и надежность

- **Потокобезопасность**: Все операции защищены мьютексами
- **Обработка ошибок**: Исключения не прерывают работу программы
- **Fallback механизмы**: При ошибках используются значения по умолчанию
- **Кроссплатформенность**: Единый интерфейс для всех ОС

## Устранение неполадок

### Логирование не работает

1. Проверьте наличие файла-триггера (по умолчанию `enable_logs`)
2. Убедитесь, что файл находится рядом с исполняемым файлом
3. Проверьте права доступа к директории
4. Если используется кастомное имя файла-триггера, проверьте его через `getTriggerFileName()`

### Неожиданный уровень логирования

1. Проверьте содержимое файла-триггера (по умолчанию `enable_logs`)
2. Убедитесь в корректности синтаксиса (case-insensitive)
3. Проверьте, что файл не поврежден

### Проблемы с производительностью

1. Используйте более высокий уровень логирования
2. Отключите имена функций (`FUNCNAME=none`)
3. Проверьте частоту вызовов логирования
