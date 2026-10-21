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
| Stackful-корутина | `stream.async_read_some (buffer, yield[error])` внутри `asio::spawn` | `LUMEX_TRANSPORT_WITH_SPAWN` |
| C++20-корутина | `co_await stream.async_read_some (buffer, asio::use_awaitable)` | C++20 |

## Ошибки

`error::transport_errc`: `timeout`, `cancelled`, `closed`, `not_open`, `disconnected`, `would_deadlock`, `invalid_endpoint`, `report_too_large`, `bad_report_descriptor`, `permission_denied`. Ошибки операционной системы приходят без изменений в `std::system_category`. Собственные категории Asio никогда не выходят наружу: `operation_aborted` превращается в `cancelled`, `eof` и сброс TCP-соединения в `disconnected`, ошибка разрешения имени в `invalid_endpoint`.

Категория `transport_errc` своя в каждом бинарнике (библиотека статическая), поэтому код ошибки, полученный из другой DLL, сравнивайте через `error::is_transport_error (code, transport_errc::disconnected)`: эта функция сравнивает и по имени категории `"lumex.transport"`.

## Лог

Записи (`log::log_record_t`: уровень, компонент `transport.serial` / `transport.tcp` / `transport.udp`, экземпляр, текст, байты) уходят в слот этого бинарника. По умолчанию записи от `warning` и выше печатаются в `std::cerr`. Потребитель ставит свой приемник через `log::set_log_sink`, меняет порог `log::set_log_level` во время работы и ограничивает компоненты `log::set_component_filter`. Байты показываются через `log::format_dump` (hex и текст с именами управляющих байтов, по умолчанию не больше 256 байт). На уровне `trace` пишется каждый принятый и отправленный кусок.

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
| `GetChannelStatistic` | `statistics ()` (фаза 2b) |
| Файл-триггер логгера | приемник `log::set_log_sink` и перечитывание настроек `LumexLogger` (фаза 2b) |

Сознательно не входят в библиотеку протоколы конкретных устройств: 7-битная кодировка последовательного порта, транспортный протокол с номерами пакетов, подтверждениями и повторами, заголовок `[ReportID][Size]` HID-отчетов прибора и заголовок старого UDP-протокола приборов (длина, атрибут, номер пакета). Это форматы приборов, а не транспорта; при переходе их реализует код прибора поверх `byte_stream` или `message_transport` и, где удобно, стадий `core/framing`.

## Примеры

- `lumex/examples/transport/example_transport_sync.cpp`: синхронный обмен с эхо-сервером по TCP (или с последовательным портом, имя которого передано аргументом) и приемник лога.
- `lumex/examples/transport/example_transport_async.cpp`: цепочка колбэков и закрытие потока из другого потока во время чтения.
