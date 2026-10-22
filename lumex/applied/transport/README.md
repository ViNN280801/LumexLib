\page Transport Транспорт: байтовые потоки и транспорты сообщений

## Обзор

Модуль `lumex/applied/transport` (цель `lumex::transport`, опция `LUMEX_BUILD_TRANSPORT`) дает два вида транспорта поверх Asio:

- `stream::byte_stream` - поток байтов: последовательный порт (`serial::make_serial_stream`), TCP (`net::make_tcp_stream`), заглушка для тестов (`testing::make_mock_stream`);
- `datagram::message_transport` - транспорт целых сообщений: UDP (`net::make_udp_socket`), заглушка (`testing::make_mock_datagram`); USB HID появится в фазе 3.

У каждого транспорта есть синхронный API с дедлайнами и асинхронный API для любого completion token Asio. Модуль работает начиная с C++11 и подключается одним заголовком:

```cpp
#include "lumex/applied/transport/LumexTransport"
```

Разбор кадров (SLIP, разделители, префикс длины, Modbus RTU и другие) находится в отдельном модуле `lumex/core/framing` и с транспортом не связан: кодек принимает байты, которые вернул транспорт.

## Сборка

| Опция | По умолчанию | Назначение |
| --- | --- | --- |
| `LUMEX_BUILD_TRANSPORT` | `ON` | Собирать модуль (требует `serial`, `expected`, `span`, `contracts`, `utility`) |
| `LUMEX_TRANSPORT_WITH_SPAWN` | `ON` | Токен `asio::yield_context` (собирает поставляемый Boost.Context) |
| `LUMEX_TRANSPORT_WITH_TESTING` | `ON` | Публичные заглушки `testing/` (`mock_stream`, `mock_datagram`) |
| `LUMEX_TRANSPORT_LOG_MIN_LEVEL` | `TRACE` | Нижний уровень лога, который вообще компилируется |

Библиотека всегда статическая, даже при `LUMEX_BUILD_SHARED_LIBS=ON`: каждый бинарник (EXE и каждая DLL) получает свою копию, свой `io_context` по выбору потребителя, свою категорию ошибок и свой слот лога. Asio (1.36, `ASIO_STANDALONE`, `ASIO_NO_DEPRECATED`) поставляется вместе с LumexLib и устанавливается вместе с заголовками; `LUMEX_ASIO_DIR` подставляет Asio потребителя той же версии.

## Потоки и время жизни

- Транспорт создается на executor, обычно на `stream::io_runner` (`io_context` со своими потоками).
- Все операции транспорта, а также `cancel ()` и `close ()`, выполняются на его strand; `cancel ()` и `close ()` можно вызывать из любого потока. Незавершенные операции заканчиваются с `transport_errc::cancelled`.
- Обработчик завершения никогда не вызывается внутри функции, которая начала операцию.
- Обработчики держат реализацию живой, поэтому `byte_stream` можно уничтожить в любой момент, в том числе внутри его обработчика: деструктор закрывает транспорт и сразу возвращается.

## Синхронный API

```cpp
namespace tr = lumex::applied::transport;

tr::stream::io_runner runner;                     // один поток для всех транспортов
tr::net::tcp_options_t options;
options.host = "192.0.2.10";
options.port = 4001;
tr::stream::byte_stream tcp = tr::net::make_tcp_stream (runner.get_executor (), options);

tr::status_result_t opened = tcp.open (tr::deadline_t (3000));
std::vector<std::uint8_t> reply (64);
tr::io_result_t got = tcp.read_some (tr::mutable_bytes_t (reply), tr::deadline_t (500));
if (!got.has_value () && got.error () == tr::error::transport_errc::timeout)
  ; // ответа нет, поток по-прежнему открыт
```

Синхронный вызов запускает асинхронную операцию и ждет ее. По дедлайну он отменяет операцию, дожидается ее завершения и возвращает `timeout`. Нужен поток, который выполняет executor; вызов из такого потока сразу возвращает `would_deadlock` (проверка понимает `io_context`, `thread_pool` и их strand).

## Асинхронный API

Каждая операция `async_open`, `async_read_some`, `async_write_some`, `async_write_all`, `async_receive`, `async_send` принимает любой completion token:

| Стиль | Пример | Условие |
| --- | --- | --- |
| Колбэк | `stream.async_read_some (buffer, [] (std::error_code e, std::size_t n) { ... })` | C++11 |
| `std::future` | `stream.async_open (asio::use_future).get ()` | C++11 |
| Отложенная операция | `auto op = stream.async_read_some (buffer, asio::deferred);` затем `std::move (op) (token)` | C++11 |
| Stackful-корутина | `stream.async_read_some (buffer, yield[error])` внутри `asio::spawn` | `LUMEX_TRANSPORT_WITH_SPAWN` |
| C++20-корутина | `co_await stream.async_read_some (buffer, asio::use_awaitable)` | C++20 |

Отмена отдельной операции идет через cancellation slot обработчика (`asio::bind_cancellation_slot`, `asio::cancel_after`, корутина). Сигнал `cancellation_type::terminal`, пока операция не завершена, отменяет транспорт так же, как `cancel ()`: все его незавершенные операции заканчиваются с `cancelled`, транспорт остается рабочим. Типы `partial` и `total` не поддерживаются и ничего не меняют (`cancellation_type::all` содержит `terminal`).

- **Отменяется весь транспорт.** Терминальная отмена записи завершает и цикл чтения, который параллельно идет на том же транспорте.
- Операция считается незавершенной, пока не выполнен ее обработчик. Сигнал, пришедший после того, как устройство завершило операцию, но до вызова обработчика, все равно отменяет остальные незавершенные операции транспорта (сама операция завершается со своим результатом). После вызова обработчика сигнал ни на что не влияет; слот остается таким, каким его оставляют операции самой Asio.
- Как и для объектов самой Asio, сигнал подается на executor обработчика, а не одновременно с завершением операции.
- Таймер `asio::cancel_after` выполняется на strand транспорта. Обработчик, привязанный к голому executor многопоточного `io_runner` (или корутина, запущенная на нем), выполнялся бы рядом с этим таймером, чего `cancel_after` в Asio не допускает: такой обработчик нужно привязать к strand (`asio::bind_executor (asio::make_strand (runner.get_executor ()), handler)`).
- Открытие последовательного порта не прерывается ни `cancel ()`, ни терминальной отменой: оно заканчивается вместе с ограниченным открытием, самое позднее через `open_deadline`.

Транспорт не держит работу (outstanding work) на executor обработчика: обработчик, который держит своего владельца и хранится в незавершенной операции, держал бы эту работу вечно, и деструктор его `io_context` не вернулся бы. Поэтому контекст, который выполняет только обработчики (`asio::bind_executor (ui, ...)`), может остаться без работы и выйти из `run ()` раньше обработчика. Если `run ()` не должен возвращаться раньше обработчика, держите на этом контексте `executor_work_guard`, пока операция не завершена. Executor обработчика должен жить, пока обработчик не выполнен или не уничтожен, поэтому его контекст объявляется раньше `io_runner` и уничтожается после него:

```cpp
asio::io_context ui;                 // контекст обработчиков: объявлен первым,
tr::stream::io_runner runner;        // поэтому runner и его незавершенные операции
                                     // уничтожаются раньше ui
tr::stream::byte_stream stream = make_stream (runner.get_executor ());

auto guard = asio::make_work_guard (ui);  // run () ждет обработчик
stream.async_read_some (buffer, asio::bind_executor (ui,
    [&guard] (std::error_code error, std::size_t bytes)
      {
        guard.reset ();                    // работы больше нет
        // ...
      }));
ui.run ();

// Неправильно: io_runner runner; asio::io_context ui; (ui уничтожается первым,
// а завершившаяся в runner операция отправила бы обработчик в уничтоженный ui).
```

## Ошибки

`error::transport_errc`: `timeout`, `cancelled`, `closed`, `not_open`, `disconnected`, `would_deadlock`, `invalid_endpoint`, `report_too_large`, `bad_report_descriptor`, `permission_denied`. Ошибки операционной системы приходят без изменений в `std::system_category`, с одним исключением: отказ в доступе при открытии последовательного порта в POSIX (`EACCES`, `EPERM`) становится `permission_denied` (см. раздел о правах доступа). В Windows `ERROR_ACCESS_DENIED` при открытии COM-порта обычно значит, что порт занят, поэтому остается системной ошибкой. Собственные категории Asio никогда не выходят наружу: `operation_aborted` превращается в `cancelled`, `eof` и сброс TCP-соединения в `disconnected`, ошибка разрешения имени в `invalid_endpoint`.

Значения с переносимым смыслом сравниваются и со стандартными условиями: `timeout` равен `std::errc::timed_out`, `cancelled` равен `std::errc::operation_canceled`, `permission_denied` равен `std::errc::permission_denied`. У остальных значений такого соответствия нет.

Категория `transport_errc` своя в каждом бинарнике (библиотека статическая), поэтому код ошибки, полученный из другой DLL, сравнивайте через `error::is_transport_error (code, transport_errc::disconnected)`: эта функция сравнивает и по имени категории `"lumex.transport"`.

## Лог

Записи (`log::log_record_t`: уровень, компонент `transport.serial` / `transport.tcp` / `transport.udp`, экземпляр, текст, байты) уходят в слот этого бинарника. По умолчанию записи от `warning` и выше печатаются в `std::cerr`. Потребитель ставит свой приемник через `log::set_log_sink`, меняет порог `log::set_log_level` во время работы и ограничивает компоненты `log::set_component_filter`. Байты показываются через `log::format_dump` (hex и текст с именами управляющих байтов, по умолчанию не больше 256 байт). На уровне `trace` пишется каждый принятый и отправленный кусок.

С модулем `logger` (`LUMEX_BUILD_LOGGER`, тогда определен `LUMEX_TRANSPORT_WITH_LOGGER`) лог транспорта можно направить в файл `LumexLogger`:

- `log::install_lumex_logger_sink ()` делает `LumexLogger` приемником этого бинарника и подключает порог транспорта к уровню логгера: при каждом перечитывании файла-триггера (`enable_logs`) порог меняется вместе с ним, а пока логгер выключен, порог `off`. Возвращает `false`, если все 8 мест для функций уведомления логгера заняты другими бинарниками: тогда порог выставляется один раз.
- `log::uninstall_lumex_logger_sink ()` возвращает приемник по умолчанию.
- Программа, которая пишет в лог только через транспорт, сама файл-триггер не проверяет (пока логгер выключен, вызовов логгера нет). Ей нужен `trigger_file_watcher` логгера или `log::lumex_logger_watch_timer` (таймер на `io_runner`, уничтожается раньше него).

## Статистика

`byte_stream::statistics ()` и `message_transport::statistics ()` возвращают снимок `statistics::statistics_t`: байты и операции в обе стороны (для транспорта сообщений: сообщения), удачные открытия, ошибки по значениям `transport_errc` и прочие ошибки, тайм-ауты, отмены, время последней активности. Счетчики атомарные и считаются в базовых классах, поэтому последовательный порт, TCP, UDP и заглушки считают одинаково. Дедлайн синхронного вызова считается один раз, как тайм-аут. `reset_statistics ()` обнуляет счетчики; сброс во время обмена приблизительный, снимок читает счетчики по одному.

## Права доступа (Linux)

Если ОС отказывает в открытии узла последовательного порта (`EACCES` / `EPERM`), открытие завершается `permission_denied`, а в лог (уровень `error`) пишется узел, его владелец, группа, режим и способ исправить. Справка строится на рабочем потоке открытия и только при включенном логе ошибок, поток ввода-вывода не обращается к базе пользователей. Шаблоны правил udev для доступа без root ставятся в `share/LumexLib/udev` (`99-lumex-serial.rules` по группе и VID `16e0`, `70-lumex-uaccess.rules` для пользователя за консолью); как их установить, описано в `README.md` рядом с ними.

## Сессии и переподключение

`session::stream_session` и `session::message_session` держат открытым один транспорт, который делает фабрика: открывают его, замечают потерю связи и открывают новый. Это транспортный уровень переподключения; повтор `identify ()` и повторную отправку данных после переподключения решает клиент протокола (фазы 4 и 5).

```cpp
namespace ss = tr::session;

ss::serial_selector_t selector;                   // порт по USB-адаптеру, а не по имени
selector.vid = "16E0";
selector.serial_number = "B71946KX";
selector.settings.baud_rate = 115200;

ss::session_options_t options;
options.name = "device";
options.policy.max_attempts = 0;                  // пытаться без ограничения
ss::stream_session link (runner.get_executor (), ss::make_serial_factory (selector), options);

ss::device_watcher watcher (runner.get_executor ());   // опрос портов раз в секунду
watcher.watch_presence (selector, link.presence_sink ());

link.open (tr::deadline_t (5000));
link.async_read_some (buffer, handler);           // потеря связи запускает переподключение
```

- **Состояния** `channel_state`: `closed` до `start ()`, `connecting` в попытках первого открытия, `open`, `reconnecting` после потери связи, `faulted`, когда цикл исчерпал `max_attempts` (`start ()` начинает новый цикл). `close ()` из любого состояния возвращает `closed`. `state_snapshot ()` читает состояние вместе с номером изменения, `async_wait_state_change` от снимка не пропускает изменений; `on_state_change` вызывается на strand сессии при каждом изменении.
- **Повторы** `reconnect_policy_t`: первая попытка цикла сразу, затем ожидание `initial_delay * multiplier^(n - 1)`, не больше `max_delay`, с разбросом `jitter`; между двумя попытками не меньше 1 мс. Связь, которая не продержалась `initial_delay`, не дает переподключаться подряд: без переданных данных это неудачная попытка того же цикла, с данными новый цикл, первая попытка которого ждет. Попытка открытия ограничена `open_timeout`.
- **Потеря связи** видна по вводу-выводу самой сессии: ошибка, которую принимает `is_link_lost` (все, кроме `cancelled`, `timeout`, `would_deadlock`, `report_too_large`), запускает переподключение до вызова обработчика. Тихую потерю у сессии, которая ничего не читает, сообщает только присутствие устройства: `set_device_present (false)` (его вызывает `device_watcher` через `presence_sink ()`) останавливает попытки, `set_device_present (true)` сразу делает следующую. Операция, начатая, пока сессия не открыта, завершается с `not_open`; ничего не отправляется повторно.
- **Селекторы** делают фабрики: `make_serial_factory` по пути или по VID, PID и серийному номеру адаптера (порт ищется при каждом открытии, поэтому `COM7`, вернувшийся как `COM9`, находится снова; путь `/dev/serial/by-id/...` сравнивается через `realpath`), `make_tcp_factory`, `make_udp_factory`. Перечисление портов селекторов и `device_watcher` не открывает порты (`enumerate_serial_ports_without_probe`), чтобы не дергать линии модема приборов и не мешать открытию сессии.
- **`close ()` и `cancel ()`** сразу заканчивают подключение и переподключение и завершают ожидающие `async_open` и `async_wait_state_change` с `cancelled`. `close ()` закрывает транспорт; `cancel ()` открытой сессии отменяет ее ввод-вывод и оставляет связь.
- **Отмена отдельной операции** работает как у транспортов. Терминальный сигнал на слоте `async_open` или `async_wait_state_change` делает то же, что `cancel ()`; на слоте операции ввода-вывода он отменяет незавершенные операции текущего транспорта, связь остается. `partial` и `total` ничего не меняют. Начальные функции сообщают strand сессии, поэтому таймер `asio::cancel_after` выполняется на нем. Работу на executor обработчика сессия не держит (см. асинхронный API).
- **Время жизни.** Сессию можно уничтожить в любой момент, в том числе в ее колбэке состояния или обработчике: деструктор закрывает асинхронно и возвращается, колбэк состояния после него не вызывается. Уничтожайте сессию раньше `io_context` ее executor.
- **Счетчики:** `statistics ()` возвращает счетчики транспорта последней попытки, `session_statistics ()` попытки, неудачные попытки и переподключения.

## Транспорты

- **Последовательный порт.** Открытие ограничено дедлайном `open_deadline` (рабочий поток с `CancelSynchronousIo` в Windows, `O_NONBLOCK` в POSIX), поэтому отсутствующий Bluetooth-порт не подвешивает вызов. Настройки `serial_settings_t` (скорость, биты данных, четность, стоповые биты, управление потоком); значение, которое платформа не умеет (четность mark/space, 1,5 стоповых бита вне Windows), дает `std::errc::not_supported`.
- **TCP.** Разрешение имени и подключение; `no_delay` (по умолчанию включен) и `keep_alive`. Отмена или закрытие во время открытия завершают его сразу, даже если имя еще разрешается.
- **UDP.** Сокет, подключенный к одному адресату: один прием возвращает одну датаграмму, одна отправка пишет одну. Датаграмма длиннее `max_datagram_size` обрезается до этого размера на всех платформах (с предупреждением в логе). Отправка сообщения с ненулевым `report_id` дает `std::errc::invalid_argument`.

## Соответствие DChannel

Модуль закрывает транспортную часть DChannel. Код продуктов на него пока не переводится; таблица показывает, чем заменить каждую часть при переходе.

| DChannel | LumexLib |
| --- | --- |
| `DCT_SERIAL_PORT`, `DCT_SERIAL_PORT_SYNC`, `Create`, `CreateSerialPort` | `serial::make_serial_stream` и `serial_stream_options_t` (оба режима DChannel дает один поток: синхронный и асинхронный API) |
| `DCT_NET_TCP` | `net::make_tcp_stream` |
| `DCT_NET_UDP` | `net::make_udp_socket` |
| `DCT_USB_HID`, `SelectDevice`, `EnumerateDevices`, `GetNextDeviceName`, `GetNextSerialNumber` | `hid_device` (фаза 3); последовательные порты перечисляет `applied/serial` |
| `DCT_NAMED_PIPE_SERVER`, `DCT_NAMED_PIPE_CLIENT` | не переносятся |
| `DCP_SIMPLE` | `byte_stream` без кодека |
| `DCP_ASCII`, `SetFrameDelimiters` | `core/framing`: `delimiter_codec` |
| `DCP_SLIP` | `core/framing`: `slip_codec` |
| `DCP_TCP` (длина в первом байте) | `core/framing`: `length_prefix_codec` с `width = 1` |
| `DCP_HID_1` | `hid_device` и кодек потребителя (фаза 3) |
| `DCP_COM7BIT`, `DCP_TRANSPORT`, `DCP_UDP` | не переносятся (см. ниже) |
| `IsOpened`, `IsConnected` | `is_open ()`; потеря связи приходит ошибкой `disconnected` |
| `Read`, `Write` | `read_some`, `write_some`, `write_all` с дедлайном |
| `StartDataExchange`, `GetReceivedData`, `SendData` | `framed_stream` (фаза 3) |
| `StopDataExchange`, `Close`, `Delete` | `close ()` из любого потока; блокирующее чтение завершается сразу с `cancelled` |
| `SetNotification` (окно, сообщение, событие) | completion token: колбэк, future, корутина |
| `GetChannelStatistic` | `statistics ()` |
| Файл-триггер логгера | `log::install_lumex_logger_sink ()`: лог транспорта в `LumexLogger`, порог следует за перечитыванием файла-триггера |

Сознательно не входят в библиотеку протоколы конкретных устройств: 7-битная кодировка последовательного порта, транспортный протокол с номерами пакетов, подтверждениями и повторами, заголовок `[ReportID][Size]` HID-отчетов прибора и заголовок старого UDP-протокола приборов (длина, атрибут, номер пакета). Это форматы приборов, а не транспорта; при переходе их реализует код прибора поверх `byte_stream` или `message_transport` и, где удобно, стадий `core/framing`.

## Примеры

- `lumex/examples/transport/example_transport_sync.cpp`: синхронный обмен с эхо-сервером по TCP (или с последовательным портом, имя которого передано аргументом) и приемник лога.
- `lumex/examples/transport/example_transport_async.cpp`: цепочка колбэков и закрытие потока из другого потока во время чтения.
