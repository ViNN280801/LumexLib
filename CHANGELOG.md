# Changelog LumexLib

<!-- markdownlint-disable MD022 MD024 MD032 -->

Все значимые изменения в библиотеке LumexLib документируются в этом файле.

Формат основан на [Keep a Changelog](https://keepachangelog.com/ru/1.1.0/).

Секция с пометкой **"в разработке"** накапливает изменения до выпуска релиза (git-тег, публикация артефактов). Учитываемая история начинается с `[v1.0.0.0]`. Номера `1.1.0`, `1.1.0.0` и `1.1.0.1` были экспериментальными и не считаются.

---

## [v1.0.0.0] - в разработке

> Первая учитываемая версия. Не закоммичено на момент записи - дата и хеш коммита будут добавлены после коммита.

### [v1.0.0.0]

#### Исправлено

##### MSVC-потребитель получает `/Zc:__cplusplus` от целей модулей

**Файлы:**

- `cmake/LumexBuild.cmake`
- `conanfile.py`
- `lumex/tests/cmake/consumer/cplusplus_macro/`, `lumex/tests/cmake/consumer/run_consumer.cmake`, `lumex/tests/cmake/CMakeLists.txt`

**Суть:** заголовки ветвятся по `__cplusplus`, а MSVC без `/Zc:__cplusplus` сообщает `199711L`. Свой сборщик библиотеки ставил флаг только на ее объектные файлы. Потребитель через `find_package` или Conan его не видел: `LUMEX_CONSTEXPR` раскрывался в пустоту, `LumexDebug.hpp` не компилировался. Теперь каждая цель `LumexCore_*`, `LumexApplied_*` и `LumexXml` отдает флаг как INTERFACE (в том числе header-only модули). В рецепте Conan тот же флаг стоит на каждом компоненте при компиляторе `msvc`. Проверка: `LumexCMake.consumer_cplusplus_macro` и `conan create` тестового потребителя без собственного флага.

##### `LUMEX_WITH_FIELD_REFLECTION` больше не протекает к потребителям `lumex::reflection`

**Файлы:**

- `lumex/core/reflection/CMakeLists.txt`
- `lumex/tests/cmake/cases/wiring_field_reflection_no_leak.cmake`, `lumex/tests/cmake/consumer/`

**Суть:** при `LUMEX_WITH_FIELD_REFLECTION=ON` цель выставляла одноименное INTERFACE-определение, и umbrella `LumexReflection` подключал `LumexFieldReflection.hpp` с `<nlohmann/json.hpp>` у каждого потребителя, хотя путь к nlohmann намеренно не экспортируется. `LumexReflectionTests` и примеры reflection не собирались. Определение убрано: опция только собирает field-reflection тесты, потребитель, которому нужен `to_json`, задает макрос и nlohmann сам. Новые тесты: `LumexCMake.wiring_field_reflection_no_leak` и `LumexCMake.consumer_reflection_umbrella_field_reflection_{off,on}` (сборка мини-проекта с встроенной LumexLib).

##### `LUMEX_DEFINE_REFLECTED_ENUM` не компилировался в C++20

**Файлы:**

- `lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp`
- `lumex/tests/core/reflection/CMakeLists.txt`

**Суть:** сопутствующие константы (`Values`, `First`, `Size`, `Last`) объявлялись через `LUMEX_CONST_NUM`, который в C++20 раскрывается в `constinit`; `First = Values.front()` читал не-constexpr массив, и любой reflected enum падал с C2127. Теперь они `static LUMEX_INLINE_VARIABLE constexpr` во всех стандартах. Добавлен набор `LumexReflectionCxx20Tests`.

##### nlohmann/json 3.12.0 не попадает в include path потребителя

**Файлы:**

- `lumex/core/reflection/CMakeLists.txt`
- `cmake/LumexNlohmannJson.cmake`

**Суть:** `lumex::reflection` больше не добавляет `3rdparty` в INTERFACE include и не устанавливает заголовки nlohmann в публичный `include/`. Иначе `#include <nlohmann/json.hpp>` в каждом исходнике, который линкует reflection, резолвился в vendored 3.12.0 и расходился с копией потребителя. Скомпилированные модули (logger, settings) по-прежнему линкуют `nlohmann_json::nlohmann_json` как PRIVATE. Тесты field reflection линкуют этот таргет сами.

##### Windows: NOMINMAX для GNU-like clang++ / g++ при встраивании

**Файлы:**

- `cmake/LumexBuild.cmake`

**Суть:** `configure_compiler_flags` из CMakeRoutines задаёт `NOMINMAX` / `WIN32_LEAN_AND_MEAN` только на пути MSVC (clang-cl). При встраивании в PeakExpertNoGUI с GNU-like `clang++` макросы `min`/`max` из Windows SDK ломали `std::min` / `std::max` в `LumexLogger`. `lumex_configure_target` теперь всегда добавляет оба определения на WIN32 для каждого скомпилированного таргета.

#### Добавлено

##### Компонент `core/fmt` (`lumex::fmt`) - форматирование в стиле `std::format` с C++11

**Файлы:**

- `lumex/core/fmt/LumexFormat.hpp`, `LumexFormatRanges.hpp`, `LumexFormatChrono.hpp`, umbrella `lumex/core/fmt/LumexFormat`, `lumex/core/fmt/README.md` (страница Doxygen), `lumex/core/fmt/CMakeLists.txt`
- `cmake/LumexOptions.cmake` (`LUMEX_BUILD_FMT`, `LUMEX_BUILD_BENCHMARKS`), `cmake/LumexModules.cmake` (ребро `FMT -> UTILITY`), `cmake/LumexLibConfig.cmake.in`, `conanfile.py` (`core_fmt`), корневой `CMakeLists.txt`
- `lumex/core/utility/macros/LumexExceptionMacros.hpp` (`LUMEX_DEFINE_EXCEPTION` перенесен сюда, добавлен `LUMEX_DEFINE_EXCEPTION_WITH_BODY`)
- `lumex/tests/core/fmt/`, `lumex/tests/cmake/consumer/format_compile_checks/`, `lumex/examples/fmt/` (6 примеров), `benchmarks/fmt/`, `Scripts/CodeTools/check_fmt_examples_coverage.py`, `Scripts/Doxygen/markdown_image_filter.py`, `Doxyfile`, `Doxyfile.in`

**Суть:** собственная реализация (fmt 12.2.0 использовался только как справочник, код не копировался) в пространстве `lumex::core::fmt`: `format`, `format_to`, `format_to_n`, `formatted_size`, `vformat`, `vformat_to`, `runtime`, `try_format` (noexcept), `print`/`println`, `FormatError` с позицией поля. Полная мини-грамматика спецификаций std, именованные аргументы `arg ("name", v)`, `wchar_t`, `L` через `std::locale`, reflected enums, `Formatter<T>` для пользовательских типов, opt-in `OstreamFormatter`/`streamed`. Диапазоны, словари, множества, `pair`/`tuple` по правилам C++23; `std::chrono` длительности и `system_clock` time point со спецификациями `%H:%M:%S`, `%F`, ... . В C++20 литеральная строка формата проверяется `consteval` при компиляции. Вывод совпадает с `std::format` (MSVC): ширина по графемным кластерам, нулевое дополнение указателей (P2510), диапазон `{:c}` по типу символа, `L` для всех целочисленных представлений; сверено дифференциальным фаззингом (~120 000 случаев). Корректны экстремумы chrono (`duration::min ()`, далекие даты), где MSVC ошибается. Наборы `LumexFormatTests` (C++11), `LumexFormatCxx17Tests`, `LumexFormatCxx20Tests`, compile-fail `LumexCMake.format_compile_checks`, `require_fail_fmt_without_utility`, проверка полноты примеров `LumexFormatExamplesCoverage`. Бенчмарки против `std::format`, `ostringstream`, `snprintf`, `to_string` (CSV + SVG) и страница в Doxygen с графиками. `LumexString` форматтер не подключает.

##### Модуль `applied/json` (`lumex::json`, header-only)

**Файлы:**

- `lumex/applied/json/` (`LumexJson`, `diagnostics/`, `helper/`, `schema/`, `validation/`, `normalization/`)
- `lumex/tests/applied/json/`, `lumex/examples/json/`
- `cmake/LumexOptions.cmake` (`LUMEX_BUILD_JSON`), `cmake/LumexModules.cmake`, `cmake/LumexLibConfig.cmake.in`, корневой `CMakeLists.txt`

**Суть:** перенесено из PeakExpertWeb `Utility/Json/`: `LumexJsonHelper` (бывший `JsonHelper`, API в snake_case, без исключений, общий recursive mutex для файловых операций), движок схем draft-7 подмножества `LumexJsonSchemaTraverser`, `LumexJsonSchemaValidator`, абстрактный `LumexJsonSchemaNormalizer`, `LumexJsonSchemaException`. Каталог схем (`JsonApiSchemaCatalog`/`JsonApiSchemaId`) не перенесен. Модуль компилируется с nlohmann потребителя (3.x), диагностика идет через `diagnostics::set_diagnostic_reporter` (по умолчанию `warning`/`error` в `std::cerr`). Пол C++11; наборы `LumexJsonTests` (C++11) и `LumexJsonCxx20Tests`.

##### `LumexCallbackSlot` - слот для внешнего указателя на функцию

**Файлы:**

- `lumex/core/utility/callback/LumexCallbackSlot.hpp`
- `lumex/core/exceptions/LumexExceptionWrapper.cpp`

**Суть:** шаблон `LumexCallbackSlot<Tag, R (Args...)>` (atomic-указатель, `set`/`get`/`exchange`/`reset`/`invoke_or`, RAII `Scoped`). На нем построены `set_safe_call_reporter` и json-диагностика. Набор `LumexCallbackSlotTests` (C++11).

##### Именованный движок для каждого CRC каталога RevEng ширины 3..64

**Файлы:**

- `lumex/core/crc/parametric/LumexCrcParametric.hpp`
- `lumex/tests/core/crc/LumexCrc.tests.cpp`

**Суть:** У каждой из 112 спецификаций есть тип `Crc*` (`using` на `CrcParametric<spec>`), в том же порядке, что `all_crc_specs_t`. Раньше публичный класс был только у 17 алгоритмов. Прежние 17 имен сохранены; `calculate` у них тот же. CRC-82/DARC по-прежнему нет: полином ширины 82 не помещается в `std::uint64_t`.

#### Изменено

##### `core/string` разложен по компонентам, глобальный `stringify` убран

**Файлы:**

- `lumex/core/string/utility/LumexStringify.hpp` (путь прежний, неймспейс `lumex::core::string::utility`), `lumex/core/string/text/{LumexJoin,LumexQuote,LumexTextCase}.hpp`
- `lumex/core/utility/traits/LumexTypeTraits.hpp`
- вызовы в `LumexLogging.hpp`, `LumexException.hpp`, `LumexDebug.hpp`, тесты и примеры

**Суть:** глобальный `using lumex::core::string::utility::stringify;` конфликтовал с глобальным `stringify` DChannel у потребителя (PeakExpertWeb, около 100 единиц трансляции). Теперь `lumex::core::string::utility::stringify` / `stringify_v2`, `lumex::core::string::text::join`, `quote`, `quote_double`, `quote_single`, `to_case_insensitive` (snake_case вместо `Join`/`Quote*`/`ToCaseInsensitive`), ничего не выносится в глобальный неймспейс. `join`/`quote*` получили реализацию для C++11..17 (ranges-версия для C++20 сохранена), ограничения через SFINAE / `requires`. Все трейты (`is_streamable`, `all_streamable`, `*_v`, концепты `Streamable`/`AllStreamable`, `range_reference`, `is_iterable`, `has_streamable_elements`, `has_elements_convertible_to`) перенесены в `lumex::core::utility::traits` (см. следующую запись); потоковые SFINAE-трейты теперь есть во всех стандартах. Новые наборы: `LumexStringifyCxx20Tests`; тесты join, quote, ограничений, потоковых и range-трейтов.

##### Все обобщенные трейты в `LumexTypeTraits.hpp`, по пространствам имен тем

**Файлы:**

- `lumex/core/utility/traits/LumexTypeTraits.hpp`
- `lumex/core/base64/codec/Base64.hpp`, `lumex/core/expected/result/{Expected,ExpectedVoid,ExpectedTypes}.hpp`, `lumex/core/utility/{cast/LumexCast,mem/LumexMemRead,dump/LumexCoreDumpGenerator,numeric/LumexSafeNumericComparator}.hpp`, `lumex/core/reflection/{var_info/LumexVarInfo,field_reflection/LumexFieldReflection}.hpp`, `lumex/applied/logger/logger/LumexLogger.hpp`, все вызовы
- `lumex/tests/core/utility/LumexTypeTraitsTopics.tests.cpp`

**Суть:** `lumex::core::utility::traits::{meta, invoke, stream, range, string, tuple, value, enums, numeric}` вместо одного плоского пространства (без алиасов старых имен). Туда же перенесены обобщенные трейты из модулей (`has_convertible_size`, `has_convertible_indexed_access<T, To>`, `is_expected*`, `indirection_of`, концепты `PointerToClass`, `LvalueRefToClass`, `CompleteType`, `PreserveCV`, `Extractible`, `ByteLike`, `StringLike`, `is_optional_like`, `is_safe_comparable`, `ArithmeticType`, `SafeComparable`, `has_key_type`, `has_mapped_type`, `is_pair_like`, `is_any_string`); дубликаты в VarInfo и логгере удалены (новые `stream::is_ostreamable` / `all_ostreamable`). В модулях остались трейты, завязанные на свои типы; `NumericConcept` остается в `math` (иначе цикл `utility` <-> `math`). Потребители: `lumex::core::utility::traits::X` -> `traits::<тема>::X` (PeakExpertWeb: `SpectrophotometricDetector.cpp`).

##### `LumexStringView` / `LumexWStringView` неявно конструируются из C-строки и `std::basic_string`

**Файлы:**

- `lumex/core/string_view/view/LumexStringView.hpp`, `LumexWStringView.hpp`
- `lumex/tests/core/string_view/LumexStringViewImplicit.tests.cpp`

**Суть:** как у `std::string_view`: литерал или строку можно передать туда, где ожидается view; `nullptr` по-прежнему дает пустой view; обратное преобразование в строку остается `explicit`.

##### CMake: LumexLib встраивается через `add_subdirectory`

**Файлы:**

- `CMakeLists.txt`, `cmake/LumexOptions.cmake`, `cmake/LumexNlohmannJson.cmake`
- `CMakeLists.txt` модулей под `lumex/core/`, `lumex/applied/`, `lumex/xml/`; `lumex/CMakeLists.txt`, `lumex/examples/CMakeLists.txt`, `lumex/examples/xml/CMakeLists.txt`, `lumex/tests/cmake/CMakeLists.txt`

**Суть:** родительский проект может подключить LumexLib через `add_subdirectory` (раньше configure падал). `LUMEX_IS_TOP_LEVEL` вычисляется до `project()`; `configure_msvc_vcvars()` вызывается только для верхнего проекта; свой `CMakeRoutines` ставится первым в `CMAKE_MODULE_PATH`; пути внутри библиотеки идут от `LumexLib_SOURCE_DIR` (или от самого файла), а не от `CMAKE_SOURCE_DIR` родителя. При встраивании LumexLib не принуждает `CMAKE_BUILD_TYPE`, не трогает `CMAKE_EXPORT_COMPILE_COMMANDS`, не копирует `compile_commands.json` и не публикует `Distr` в свое дерево исходников. Правила `install()` закрыты опцией `LUMEX_INSTALL`.

##### CMake: пространство имен таргетов `Lumex::` -> `lumex::`

**Файлы:**

- `cmake/LumexModules.cmake`, `cmake/LumexLibConfig.cmake.in`, `cmake/LumexGoogleTest.cmake`, `cmake/LumexOptions.cmake`, `cmake/LumexNlohmannJson.cmake`
- все `CMakeLists.txt` под `lumex/` (модули, тесты, примеры) и `lumex/examples/cmake/LumexExampleHelpers.cmake`
- `conanfile.py`, `lumex/applied/logger/README.md`, `lumex/applied/logger/config/LumexLoggerConfigFormat.hpp`

**Суть:** CMake-таргеты переведены в пространство имен нижнего регистра: `install(EXPORT ... NAMESPACE lumex::)`, алиасы `lumex::<component>` (например `lumex::utility`, `lumex::logger`, `lumex::xml`), зонтичный `lumex::Lumex`, тестовые алиасы `lumex::gtest_main_cxx11` / `lumex::gtest_main_cxx17`. Имена таргетов, пакета и файлов экспорта не менялись: `LumexCore_*`, `LumexApplied_*`, `LumexXml`, `LumexLib`, `LumexLibConfig.cmake`. В README логгера заодно исправлено устаревшее написание пространства имен: `lumex::applied::logger::logger` (было `Lumex::Applied::Logger`). Написание пиннит новый CMake-тест `LumexCMake.wiring_export_namespace`.

##### Identifier naming (DTO structs)

**Файлы:**

- `lumex/applied/hardware/caps/LumexHardwareCapabilities.hpp`
- `lumex/applied/hardware/caps/LumexCPUVectorizationCapabilities.hpp`
- `lumex/applied/settings/guard/LumexSettingsGuard.hpp`
- `lumex/applied/logger/logger/LumexLogger.hpp`

**Суть:** public DTO structs now follow snake_case + `_t`: `hardware_info_t` (was `HardwareInfo`), `lumex_settings_key_spec_t` (was `LumexSettingsKeySpec`, member `default_value`), `logger_applied_config_view_t` (was `LoggerAppliedConfigView`). `cpu_vectorization_info_t` members dropped the `m_` prefix (`supports_avx2`, `max_simd_width_bits`, ...).

##### Identifier naming (CRC Specs, XML parse results, leftover methods)

**Файлы:**

- `lumex/core/crc/` (catalog / parametric `*Spec` types)
- `lumex/xml/text/XmlParseResult.hpp`
- `lumex/xml/xpath/` (`XPathParseResult`)
- `lumex/applied/logger/logger/LumexLogger.hpp`
- `lumex/applied/logger/logger/LumexLogger.cpp`
- `lumex/applied/hardware/caps/LumexHardwareCapabilities.hpp`
- `lumex/core/time/timer/LumexTimer.cpp`

**Суть:** CRC policy structs are `crc*_spec_t` / `all_crc_specs_t` (was `Crc*Spec`). XML result DTOs are `xml_parse_result_t` / `xpath_parse_result_t`. Logger / `logger_config_t` members are snake_case without `m_`. Leftover camelCase methods are snake_case: `elapsed_time_ms`, `get_applied_config_view`, `detect_hardware`, `get_instance`, `CrcParametric::calculate`, `calculate_crc*`. Filenames unchanged.

##### Found/unfound test pairs

**Файлы:**

- `lumex/tests/core/crc/LumexCrc.tests.cpp`
- `lumex/tests/xml/LumexXml.tests.cpp`
- `lumex/tests/applied/settings/LumexSettingsINI.tests.cpp`
- `lumex/tests/applied/settings/LumexSettingsJSON.tests.cpp`
- `lumex/tests/applied/settings/LumexSettingsXML.tests.cpp`
- `lumex/tests/core/environment/LumexEnvironment.tests.cpp`
- `lumex/tests/core/optional/LumexOptional.tests.cpp`
- `lumex/tests/core/filesystem/LumexFilesystem.tests.cpp`
- `lumex/tests/core/string_view/LumexStringView.tests.cpp`
- `lumex/tests/core/string_view/LumexWStringView.tests.cpp`
- `lumex/tests/core/base64/LumexBase64Validator.tests.cpp`
- `lumex/tests/applied/serial/LumexSerialPort.tests.cpp`
- `lumex/tests/applied/serial/LumexSerialPortEnumeration.tests.cpp`
- `lumex/tests/core/temporary/LumexTemporary.tests.cpp`
- `lumex/tests/core/reflection/LumexReflectedEnum.tests.cpp`
- `lumex/tests/core/reflection/LumexFieldReflection.tests.cpp`
- `lumex/tests/applied/hardware/LumexHardwareCapabilities.tests.cpp`
- `lumex/tests/applied/logger/LumexLogger.tests.cpp`
- `lumex/tests/core/expected/Expected.tests.cpp`
- `lumex/tests/core/time/LumexTime.tests.cpp`

**Суть:** named `WhenFound` / `WhenUnfound` cases for lookup APIs (catalog width, XML child/attr, Settings get, Environment get, Optional `value_or`, Filesystem exists, string_view find, Base64 alphabet, Serial name/state, Temporary missing dir, enum `to_string`, FieldReflection names, Hardware unknown CPU, Logger unknown key, Expected `has_value`, Time timestamp format).

#### Исправлено

##### LumexXml не собирался в C++17 и новее

**Файлы:**

- `lumex/xml/utility/XmlUtils.hpp`
- `lumex/core/filesystem/fs/LumexFilesystem.cpp` (ветка Apple)

**Суть:** внутри функций стоял `LUMEX_CONST_NUM` (`static inline const constinit`), а `inline` в блочной области запрещен начиная с C++17 (MSVC C7524, clang "inline declaration ... not allowed in block scope"). Заменено на `LUMEX_CONSTEXPR` в `XmlUtils.hpp` (значение служит размером массива) и на обычный `std::size_t const` в Apple-ветке `LumexFilesystem.cpp`. Найдено при сборке LumexLib внутри проекта на C++20.

##### PLAIN_TEXT logger unknown KEY=value overwrote LEVEL

**Файлы:**

- `lumex/applied/logger/logger/LumexLogger.cpp`
- `lumex/tests/applied/logger/LumexLogger.tests.cpp`

**Суть:** unknown `KEY=value` (example `NOT_A_REAL_KEY=zzz`) missed every `_hasPrefix` and fell into the legacy positional parser as line 1 (`LEVEL`). `_string_to_log_level` then defaulted to INFO and clobbered a prior `LEVEL=ERROR`. Else-branch now skips lines that contain `=`; positional format is lines without `=`. INI/JSON/XML readers already ignore unknown keys. Regression: `LegacyPositional_WhenNoEquals_ThenKnownKeysApply`.


##### Wall-clock Perf_* skip in Debug and sanitizers

**Файлы:**

- `lumex/tests/support/LumexPerfSkip.hpp`
- `lumex/tests/core/base64/LumexBase64Validator.tests.cpp`
- `lumex/tests/core/base64/LumexBase64Encoder.tests.cpp`
- `lumex/tests/core/base64/LumexBase64Decoder.tests.cpp`
- `lumex/tests/core/environment/LumexEnvironment.tests.cpp`
- `lumex/tests/core/temporary/LumexTemporary.tests.cpp`
- `lumex/tests/applied/logging/LumexLogging.tests.cpp`
- `lumex/tests/xml/LumexXml.tests.cpp`
- `lumex/tests/core/crc/LumexCrc.tests.cpp`
- `lumex/tests/applied/settings/LumexSettingsINI.tests.cpp`
- `lumex/tests/core/circular_buffer/CircularBuffer.tests.cpp`
- `lumex/tests/core/exceptions/LumexException.tests.cpp`
- `lumex/tests/applied/hardware/LumexHardwareCapabilities.tests.cpp`
- `lumex/tests/core/expected/Unexpected.tests.cpp`
- `lumex/tests/core/expected/BadExpectedAccess.tests.cpp`
- `lumex/tests/core/expected/ExpectedVoid.tests.cpp`

- Общий `LUMEX_PERF_WALL_CLOCK_ENABLED`: 0 без `NDEBUG` или при ASan/UBSan/TSan/MSan. Тело `Perf_*` под `#if`, иначе `GTEST_SKIP` (не C4702).
- Expected `Perf_*` переведены с `#ifdef NDEBUG` на тот же макрос (Release+санитайзер тоже пропускается).
- `LUMEX_MAXIMUM_STANDARD_COMPLIANCE` остается default ON.

##### XML print SCANWHILE, CMake require cases, logging ERROR file

**Файлы:**

- `lumex/xml/node/XmlNode.cpp`
- `lumex/tests/xml/LumexXml.tests.cpp`
- `cmake/LumexOptions.cmake`
- `lumex/tests/cmake/cases/require_ok_math_off_utility_off.cmake`
- `lumex/tests/cmake/cases/require_fail_*_without_utility.cmake`
- `lumex/applied/logging/log/LumexLogging.cpp`
- `lumex/tests/applied/logging/LumexLogging.tests.cpp`

- `text_output_escaped` сканировал `*str` вместо локального `ss` в `LUMEX_XML_SCANWHILE_UNROLL`: печать коротких атрибутов читала за конец буфера (ASan `LumexXmlExample3`) и портила `save_file` (`LumexSettingsExampleXml` load после save).
- `LUMEX_MAXIMUM_STANDARD_COMPLIANCE` остается default ON (решение продукта; `wiring_max_standard_compliance` проверяет ` ON)`).
- CMake-кейсы с `UTILITY=OFF` выключают `CIRCULAR_BUFFER` и `EXPECTED`, иначе первый FATAL всегда про circular_buffer.
- Windows `getLogsDirectory` кладет логи в `logs/<appName>/` при непустом `setAppName`; `toFile` делает flush перед close. Фикстура логирования изолирует каждый ctest-процесс своим app name.

##### MSVC ASan Debug: optional hash, field names, examples, Expected Perf

**Файлы:**

- `lumex/core/optional/opt/LumexOptional.hpp`
- `lumex/core/reflection/field_reflection/LumexAggregateFields.hpp`
- `lumex/examples/math/example_math.cpp`
- `lumex/examples/time/example_time.cpp`
- `lumex/examples/time/example_time_workflow.cpp`
- `lumex/tests/core/expected/BadExpectedAccess.tests.cpp`
- `lumex/tests/core/expected/Unexpected.tests.cpp`
- `lumex/tests/core/expected/ExpectedVoid.tests.cpp`

- `std::hash` для Lumex `optional` больше не специализирует `std::optional` внутри `namespace std` (C2953 при `#include <optional>`).
- `names_as_array` на MSVC: phantom `fake_object_storage`, NTTP через `addressof`, разбор `__FUNCSIG__` по последнему `->` (раньше возвращалось имя типа).
- Пример math: `(void)` на `[[nodiscard]]` `checked_narrow_cast` при ожидаемом throw (C4858).
- Примеры time: `measure_execution_time` хранится как `long long` (C4244).
- Expected `Perf_*`: тело под `#ifdef NDEBUG`, иначе `GTEST_SKIP` (C4702 unreachable в Debug).

#### Добавлено

##### `core/utility/process`: `get_current_pid`

**Файлы:**

- `lumex/core/utility/process/LumexProcess.hpp`
- `lumex/core/utility/LumexUtility`
- `lumex/tests/core/utility/LumexProcess.tests.cpp`, `CMakeLists.txt`
- `lumex/examples/utility/example_utility.cpp`, `example_utility_workflow.cpp`

**Суть:** header-only `lumex::core::utility::process::get_current_pid ()` возвращает PID текущего процесса (`GetCurrentProcessId` на Windows, `getpid` на POSIX) как `unsigned long`. Входит в зонтик `LumexUtility`. Сьют `LumexProcessTests` собран в C++11. Прямые вызовы `getpid` / `GetCurrentProcessId` внутри самой библиотеки пока не переведены.

##### Опция `LUMEX_INSTALL`, CMake-кейс встраивания, Xml-тесты в C++20

**Файлы:**

- `cmake/LumexOptions.cmake`
- `lumex/tests/cmake/cases/wiring_embedding.cmake`, `lumex/tests/cmake/CMakeLists.txt`
- `lumex/tests/xml/CMakeLists.txt`

**Суть:** `LUMEX_INSTALL` (по умолчанию ON только для верхнего проекта) управляет всеми `install()` библиотеки. `LumexCMake.wiring_embedding` падает, если вернется `${CMAKE_SOURCE_DIR}`, незащищенный `install()` или вызов vcvars при встраивании. `LumexXmlCxx20Tests` собирает те же Xml-тесты в C++20 (имена CTest с суффиксом `.cxx20`).

##### JSON Settings backend (`LumexSettingsJSON`)

**Файлы:**

- `lumex/applied/settings/json/LumexSettingsJSON.hpp`
- `lumex/applied/settings/json/LumexSettingsJSON.cpp`
- `lumex/applied/settings/LumexSettings`
- `lumex/applied/settings/CMakeLists.txt`
- `lumex/applied/settings/factory/LumexSettingsFactory.hpp`
- `lumex/applied/settings/factory/LumexSettingsFactory.cpp`
- `lumex/applied/settings/ini/SupportedConfigExtensions.hpp`
- `lumex/tests/applied/settings/LumexSettingsJSON.tests.cpp`
- `lumex/tests/applied/settings/CMakeLists.txt`
- `lumex/examples/settings/example_settings.cpp`
- `lumex/examples/settings/example_settings_json.cpp`
- `lumex/examples/settings/CMakeLists.txt`
- `cmake/LumexNlohmannJson.cmake`

- `ILumexSettings` реализация параллельно `LumexSettingsINI` / `LumexSettingsXML`: load/save/get/add/remove, только через vendored nlohmann 3.12.0 (вторая JSON-библиотека не подключается).
- Разбор: документ должен быть объектом. Вложенные объекты - секции; скаляры у корня идут в секцию `settings`. Плоский `{ "LEVEL": "DEBUG" }` загружается как секция `settings`. Вложенный `{ "logger": { "LEVEL": "DEBUG" } }` - секция `logger`. Сохранение всегда пишет объект объектов со строковыми значениями.
- `LUMEX_SETTINGS_WITH_JSON` PUBLIC, если есть `3rdparty/nlohmann/json.hpp`. Settings без nlohmann по-прежнему собирается; factory тогда возвращает `nullptr` для `JSON`.
- Logger JSON остается отдельным reader; не идет через Settings.

##### CRC catalog C++11 vector overload

**Файлы:**

- `lumex/core/crc/catalog/LumexCrcCatalog.hpp`
- `lumex/tests/core/crc/LumexCrc.tests.cpp`

- `ComputeCrcCatalog(index, std::vector<uint8_t> const &)` доступен с C++11, чтобы C++14 example (`example_crc.cpp`) не требовал pointer+size. Пустой vector равен нулевой длине.

##### XML Settings backend (`LumexSettingsXML`)

**Файлы:**

- `lumex/applied/settings/xml/LumexSettingsXML.hpp`
- `lumex/applied/settings/xml/LumexSettingsXML.cpp`
- `lumex/applied/settings/LumexSettings`
- `lumex/applied/settings/CMakeLists.txt`
- `lumex/applied/settings/factory/LumexSettingsFactory.hpp`
- `lumex/applied/settings/factory/LumexSettingsFactory.cpp`
- `lumex/applied/settings/ini/SupportedConfigExtensions.hpp`
- `lumex/tests/applied/settings/LumexSettingsXML.tests.cpp`
- `lumex/tests/applied/settings/CMakeLists.txt`
- `lumex/examples/settings/example_settings.cpp`
- `lumex/examples/settings/example_settings_xml.cpp`
- `lumex/examples/settings/CMakeLists.txt`

- `ILumexSettings` реализация параллельно `LumexSettingsINI`: load/save/get/add/remove, только через `LumexXml` (вторая XML-библиотека не подключается).
- Разбор: любой корневой элемент; дети с element-детьми - секции; иначе корень - одна секция (как `<logger>`). Сохранение всегда пишет `<settings><section><key>value</key></section></settings>`.
- `LUMEX_SETTINGS_WITH_XML` PUBLIC, если есть `Lumex::xml`. Settings без XML по-прежнему собирается; factory тогда возвращает `nullptr` для `XML`.
- `lumex_require_module(LUMEX_BUILD_SETTINGS LUMEX_BUILD_XML)` нет: INI не тянет XML.

##### Logger config compile-time format (`LUMEX_LOGGER_CONFIG_FORMAT`)

**Файлы:**

- `cmake/LumexOptions.cmake`
- `cmake/LumexModules.cmake`
- `lumex/applied/logger/CMakeLists.txt`
- `lumex/applied/logger/config/LumexLoggerConfigFormat.hpp`
- `lumex/applied/logger/logger/LumexLogger.hpp`
- `lumex/applied/logger/logger/LumexLogger.cpp`
- `lumex/applied/logger/README.md`
- `lumex/tests/applied/logger/LumexLogger.tests.cpp`
- `lumex/examples/logger/example_logger_config_formats.cpp`
- `lumex/tests/cmake/setup_all_on.cmake`
- `lumex/tests/cmake/cases/options_declared.cmake`
- `lumex/tests/cmake/cases/require_fail_logger_ini_without_settings.cmake`
- `lumex/tests/cmake/cases/require_fail_logger_xml_without_xml.cmake`
- `lumex/tests/cmake/cases/require_fail_logger_invalid_format.cmake`
- `lumex/tests/cmake/CMakeLists.txt`
- `lumex/tests/cmake/cases/require_ok_logger_ini.cmake`
- `lumex/tests/cmake/cases/require_ok_logger_xml.cmake`

- Одна cache-переменная `LUMEX_LOGGER_CONFIG_FORMAT` (default `PLAIN_TEXT`): `PLAIN_TEXT`, `INI`, `JSON`, `YAML`, `XML`.
- `configured_logger_config_format()` возвращает скомпилированный enum; расширение пути не читается.
- `INI` требует effective `LUMEX_BUILD_SETTINGS`; `XML` требует effective `LUMEX_BUILD_XML` и разбирает `<logger>` child elements через `LumexXml`.
- `YAML` бросает `std::runtime_error`, пока нет `LumexSettingsYAML`.
- Проверено в `build-tests` Debug Ninja+MSVC: GTest + `LumexLoggerExampleConfigFormats` для всех пяти форматов; `LumexCMake.require_ok_logger_*` / `require_fail_logger_*` / `options_declared` / `wiring_subdirs`.

##### Примеры как часть `ctest`

**Файлы:**

- `CMakeLists.txt`
- `cmake/LumexOptions.cmake`
- `lumex/examples/cmake/LumexExampleHelpers.cmake`

- `LUMEX_BUILD_TESTS=ON` собирает `lumex/examples` и регистрирует каждый example-исполняемый файл как `add_test` (код возврата 0, метка `example`, `WORKING_DIRECTORY` рядом с DLL).
- Бинарники примеров пишутся в `${CMAKE_BINARY_DIR}/bin`.

##### CMakeRoutines utils (BuildTiming / GenerateBuildInfo / WarningSuppression / RecursiveSourceCollection / CompileCommands)

**Файлы:**

- `cmake/LumexOptions.cmake`
- `cmake/LumexBuild.cmake`
- `cmake/LumexGoogleTest.cmake`
- `CMakeLists.txt`
- `lumex/tests/cmake/CMakeLists.txt`
- `lumex/tests/cmake/cases/wiring_build_timing.cmake`
- `lumex/tests/cmake/cases/wiring_generate_build_info.cmake`
- `lumex/tests/cmake/cases/wiring_warning_suppression.cmake`
- `lumex/tests/cmake/cases/wiring_recursive_source_collection.cmake`
- `lumex/tests/cmake/cases/wiring_compile_commands.cmake`
- `lumex/tests/cmake/cases/options_declared.cmake`

- Опции `LUMEX_BUILD_TIMING` / `LUMEX_GENERATE_BUILD_INFO` (default OFF) и `LUMEX_BUILD_INFO_FORMAT` (default `json`).
- `configure_build_timing` после `include(cmake/LumexBuild.cmake)`; `lumex_apply_build_info` на библиотеках `Lumex*` (configure + POST_BUILD через `GenerateBuildInfoScript.cmake` / `.py`).
- Include `WarningSuppression` / `RecursiveSourceCollection`; vendored gtest вызывает `suppress_warnings` при наличии команды.
- CompileCommands: закреплен ENABLE ON / CREATE_SYMLINK OFF + ALL-копия; `LumexCMake.wiring_compile_commands`.
- Локальный `LibraryVersioning.cmake` не заменен копией из CMakeRoutines.

##### LUMEX_MEASURE_TIME / measure_time (core/time)

**Файлы:**

- `lumex/core/time/timer/LumexTimer.hpp`
- `lumex/core/time/timer/LumexTimer.cpp`
- `lumex/core/time/CMakeLists.txt`
- `lumex/core/CMakeLists.txt`
- `cmake/LumexModules.cmake`
- `lumex/tests/core/time/LumexTime.tests.cpp`
- `lumex/tests/cmake/CMakeLists.txt`
- `lumex/tests/cmake/cases/require_fail_time_without_environment.cmake`
- `lumex/tests/cmake/cases/require_graph_all_edges.cmake`
- смежные `require_ok_*` / `require_fail_*` cases при `ENVIRONMENT=OFF`

- C++ API: `measure_execution_time`, `write_measure_time_report`, `measure_time` (ostream, по умолчанию `std::clog`; флаг `need_to_gate_via_env` + имя env, по умолчанию `LUMEX_ENABLE_MEASURE_TIME_LOG`).
- Тонкий макрос `LUMEX_MEASURE_TIME(expr)` / `LUMEX_MEASURE_TIME(expr, msg)` поверх этого API (без hard-link на logger).
- `LumexCore_time` PUBLIC зависит от `Lumex::environment`; `LUMEX_BUILD_TIME` требует `LUMEX_BUILD_ENVIRONMENT`.

##### WARNING санитайзера на Release / MinSizeRel и рутины CMakeRoutines

**Файлы:**

- `cmake/LumexSanitizerBuildType.cmake`
- `cmake/LumexOptions.cmake`
- `cmake/LumexBuild.cmake`
- `CMakeLists.txt`
- `lumex/tests/cmake/CMakeLists.txt`
- `lumex/tests/cmake/cases/sanitizer_*.cmake`
- `lumex/tests/cmake/cases/wiring_build_info.cmake`
- `lumex/tests/cmake/cases/wiring_msvc_vcvars.cmake`
- `lumex/tests/cmake/cases/wiring_msvc_vcvars_after_project.cmake`
- `lumex/tests/cmake/cases/wiring_version_config.cmake`
- `lumex/tests/cmake/cases/wiring_static_analysis.cmake`
- `lumex/tests/cmake/cases/wiring_max_standard_compliance.cmake`

- Любой `LUMEX_USE_*SAN=ON` вместе с `Release` / `MinSizeRel` (или multi-config списком, где они есть) дает configure `WARNING`: без debug info диагностика санитайзера неадекватна; минимум `RelWithDebInfo`, лучше `Debug`.
- Корневой `CMakeLists.txt` вызывает `configure_msvc_vcvars()` до `project()`, если есть vswhere / vcvarsall; LLVM-only clang-cl дерево без Visual Studio пропускает вызов (иначе FATAL).
- `LumexBuild` подключает `VersionConfig` (`configure_version`), `StaticAnalysisConfig`, `MaximumStandardCompliance`, `BuildInfoPrinter`. `print_build_info()` после `lumex_configure_all_compiled_targets()`.
- `LUMEX_USE_CLANG_TIDY`, `LUMEX_USE_CPPCHECK`, `LUMEX_MAXIMUM_STANDARD_COMPLIANCE` default OFF. Max compliance только GNU/Clang/AppleClang и только на целях с уже заданным `CXX_STANDARD`.

##### Санитайзеры ASan / UBSan / TSan через CMakeRoutines

**Файлы:**

- `cmake/LumexOptions.cmake`
- `cmake/LumexBuild.cmake`
- `CMakeLists.txt`
- `lumex/tests/cmake/cases/wiring_sanitizers.cmake`

- Опции `LUMEX_USE_ASAN`, `LUMEX_USE_UBSAN`, `LUMEX_USE_TSAN` (все default OFF). `configure_sanitizers` из `CMakeRoutines/testing/SanitizersConfig.cmake` применяется к библиотекам, example-бинарникам, тестовым executable и вендорному gtest. На clang-cl вызывается `_configure_clang_sanitizers`, иначе UBSan молча не ставит флаги. ASan+TSan и TSan на Windows - `FATAL_ERROR` на configure.

##### Distr-пакет shared-библиотек

**Файлы:**

- `cmake/PublishDistr.cmake`
- `cmake/LumexModules.cmake`
- `CMakeLists.txt`
- `lumex/tests/cmake/cases/wiring_distr.cmake`
- `lumex/tests/cmake/cases/publish_distr_filters.cmake`

- После сборки `publish_distr` копирует только Lumex shared libraries (и CRT / PDB / soname / build-info) в `<platform>/Distr<Config>` (как PeakExpertCE). Тестовые и example-бинарники в Distr не попадают. Работает и при `LUMEX_BUILD_TESTS=ON`, и без тестов.

#### Изменено

##### Примеры перенесены в `lumex/examples`

**Файлы:**

- `lumex/examples/**`
- `CMakeLists.txt`

- Каталог `examples/` перенесен рядом с тестами: `lumex/examples/<module>/`. Dual-mode `CMakeLists.txt` и `LumexExampleHelpers.cmake` сохранены.

##### Сборка больше не запускает тесты

**Файлы:**

- `CMakeLists.txt`
- `cmake/LumexModules.cmake`

- Удален `run_all_tests ALL`. `cmake --build` только компилирует. Программист запускает `ctest` отдельно.

#### Добавлено

##### Лицензия MIT

**Файлы:**

- `LICENSE`
- `lumex/**/*.hpp`
- `lumex/examples/**/*.hpp`

- Корневой `LICENSE` (MIT).
- Полный MIT-блок со `SPDX-License-Identifier: MIT` на каждом `.hpp` под `lumex/` и `examples/`.

##### Dual-vendor GoogleTest

**Файлы:**

- `3rdparty/googletest-1.12.1/`
- `3rdparty/googletest-1.18.0/`
- `cmake/LumexGoogleTest.cmake`
- `lumex/tests/CMakeLists.txt`

- Вендоринг `googletest-1.12.1` (C++11/14) и `googletest-1.18.0` (C++17+).
- Хелпер `lumex_test_use_gtest`: сьюиты C++11/14 линкуют 1.12.1, сьюиты C++17+ - 1.18.0.

##### Перечисление последовательных портов и резолвер держателя

**Файлы:**

- `lumex/applied/serial/enumeration/LumexSerialPortEnumeration.hpp`
- `lumex/applied/serial/enumeration/LumexSerialPortEnumeration.cpp`
- `lumex/applied/serial/resolver/LumexPortProcessResolver.hpp`
- `lumex/applied/serial/resolver/LumexPortProcessResolver.cpp`
- `lumex/applied/serial/LumexSerial`
- `lumex/tests/applied/serial/LumexSerialPort.tests.cpp`

- `enumerateSerialPorts(need_to_filter_bluetooth)`: Windows SetupAPI / Linux `/dev` + `sysfs`.
- `port_process_resolver`: PID и имя процесса, который держит порт (Windows: `CreateFile` + Restart Manager; Linux: `/proc/*/fd`).
- Флаг `need_to_filter_bluetooth` (по умолчанию `false`): при `true` отсекает Bluetooth-виртуальные COM-порты, чтобы не открывать их вслепую (на Windows открытие занятого BT-порта роняет Bluetooth-стек адаптера).

##### Макросы ключевых слов

**Файлы:**

- `lumex/core/utility/macros/LumexKeywords.hpp`
- `lumex/core/utility/macros/LumexConstantMacros.hpp`

- `LUMEX_CONSTEVAL` / `LUMEX_CONSTEVAL_FUNCTION`.
- `LUMEX_CONSTEXPR_IF`.
- `LUMEX_CONSTEXPR` как канонический алиас `LUMEX_CONSTEXPR_FUNCTION`.

##### Параметрический CRC (RevEng)

**Файлы:**

- `lumex/core/crc/parametric/LumexCrcParametric.hpp`
- `lumex/core/crc/catalog/LumexCrcCatalog.hpp`
- `lumex/core/crc/algo/LumexCrc.hpp`
- `lumex/core/crc/LumexCrc`
- `lumex/core/crc/CMakeLists.txt`
- `lumex/tests/core/crc/LumexCrc.tests.cpp`
- `lumex/tests/core/crc/LumexCrcTestHelpers.hpp`
- `lumex/tests/core/crc/CMakeLists.txt`

- Параметрический движок ширины 1..64 и каталог `all_crc_specs_t` (порядка 112 спецификаций).
- `ComputeCrcCatalog`, `ComputeCrcWithRevEngParams`, `TransportCrcMode`.
- Табличные `Crc4` / `Crc8` сохранены.
- `LumexCore_crc` и `LumexCrcTests` собираются как C++14.
- Тесты: прежние `Crc8`, плюс `CatalogCheck` / `RandomVectors` / `ManualCases` по всем спецификациям каталога (check ASCII `"123456789"`, независимый bitwise-референс, 250 ручных векторов на спецификацию).

#### Изменено

##### Макросы LUMEX_* в production-коде

**Файлы:**

- `lumex/applied/**`
- `lumex/core/**`
- `lumex/xml/**`
- `examples/**`

- Спецификаторы `noexcept`, `constexpr`, `if constexpr` и атрибуты `[[nodiscard]]` / `[[maybe_unused]]` / `[[noreturn]]` / `[[deprecated]]` заменены на `LUMEX_*` аналоги во всей production-кодовой базе. Каталог `lumex/tests/` не тронут.
- Локальные обёртки `LOGGER_NOEXCEPT_*` / `LOGGER_CONSTEXPR_*` / `LOGGER_CONST*` / `LOGGER_ATTRIBUTE_*` в Logger сведены к `LUMEX_*`.
- `LUMEX_XML_CONSTANT` стал алиасом `LUMEX_CONSTINIT_CONSTANT`.
- Оператор `noexcept(expr)` в выражениях оставлен сырым.

##### CircularBuffer: условный noexcept через макрос

**Файлы:**

- `lumex/core/circular_buffer/CircularBuffer.hpp`

- `is_swap_noexcept` считает `std::swap` аллокаторов через `LUMEX_NOEXCEPT_IF`, а не сырой оператор `noexcept(...)`.

##### CMake `option()` вынесены в отдельный модуль

**Файлы:**

- `cmake/LumexOptions.cmake`
- `CMakeLists.txt`
- `lumex/core/reflection/CMakeLists.txt`

- Все `option()` проекта собраны в `cmake/LumexOptions.cmake` (linkage / optional modules / documentation-examples-tests). Значения по умолчанию не менялись. `LUMEX_XML_WCHAR_MODE` объявлен всегда, а не только внутри `if(LUMEX_BUILD_XML)`. `LUMEX_WITH_FIELD_REFLECTION` перенесен из `lumex/core/reflection/CMakeLists.txt`.

##### CMake-опции на каждый крупный модуль

**Файлы:**

- `cmake/LumexOptions.cmake`
- `cmake/LumexModules.cmake`
- `cmake/LumexLibConfig.cmake.in`
- `CMakeLists.txt`
- `lumex/CMakeLists.txt`
- `lumex/core/CMakeLists.txt`
- `lumex/applied/CMakeLists.txt`
- `lumex/tests/core/CMakeLists.txt`
- `lumex/tests/applied/CMakeLists.txt`
- `examples/CMakeLists.txt`

- На каждый каталог `add_subdirectory` в `core/` и `applied/` добавлен `option(LUMEX_BUILD_<MODULE> ... ON)`. Тесты и примеры того же модуля выключаются той же опцией. `LUMEX_BUILD_XML` без изменений.
- Если модуль включен, а его CMake-зависимость выключена, configure дает `FATAL_ERROR` (не авто-включает зависимость).
- `install(EXPORT)` и `run_all_tests` DEPENDS пропускают отсутствующие таргеты. `Lumex::Lumex` в package config линкует только установленные компоненты.
- `lumex/` теперь подключает `core/` раньше `applied/`, чтобы ALIAS (`Lumex::filesystem` и т.д.) существовали к моменту `target_link_libraries` в applied.

##### Группы CORE / APPLIED и тесты CMake

**Файлы:**

- `cmake/LumexOptions.cmake`
- `cmake/LumexModules.cmake`
- `lumex/CMakeLists.txt`
- `lumex/tests/CMakeLists.txt`
- `examples/CMakeLists.txt`
- `lumex/tests/cmake/`

- `LUMEX_BUILD_CORE` и `LUMEX_BUILD_APPLIED` (default ON) выключают всю группу, как `LUMEX_BUILD_XML`. Модуль собирается только если группа AND модульный `option()` оба ON. Cache модульных опций при выключении группы не переписывается.
- `lumex_check_module_dependencies()` смотрит effective-флаги: `LUMEX_BUILD_CORE=OFF` при включенном applied/xml, которому нужен core, дает `FATAL_ERROR`. `logger` без core проходит (нет CMake-линка на модуль core).
- При `LUMEX_BUILD_TESTS=ON` регистрируются CTest `LumexCMake.*` (`lumex/tests/cmake/`): объявление опций, матрица effective ON/OFF, полный граф `lumex_require_module`, легальные срезы (logger без core, только math, XML без applied), краевые FATAL по каждому ребру, сверка `add_subdirectory` / export / `run_all_tests`, проводка field reflection без Boost.PFR.

##### Field reflection без Boost.PFR

**Файлы:**

- `lumex/core/reflection/field_reflection/LumexAggregateFields.hpp`
- `lumex/core/reflection/field_reflection/LumexFieldReflection.hpp`
- `lumex/core/reflection/CMakeLists.txt`
- `cmake/LumexNlohmannJson.cmake`
- `cmake/LumexOptions.cmake`
- `3rdparty/nlohmann/json.hpp`
- `lumex/tests/core/reflection/LumexFieldReflection.tests.cpp`

- `to_json` больше не тянет Boost.PFR. Подсчет полей, `get` и имена - оригинальный MIT-хелпер `LumexAggregateFields` (не дамп BSL-заголовков), рядом с единственным потребителем.
- Стандарт: C++11 - `tuple_size` (ubiq + binary search, до 32 полей). C++14 - indexed `get` (CWG 2118 friend-injection, `friend auto` на теге, смещения по sequential layout). C++20 - `names_as_array` / `to_json` (`LUMEX_FUNCTION_NAME` от pointer NTTP + structured binding к static instance). C++11 NTTP не умеет взять адрес I-го подобъекта без уже известного `&T::name`.
- Сьюты: `LumexFieldReflectionTests` CXX 11, `LumexFieldReflectionGetTests` CXX 14, `LumexFieldReflectionNamesTests` CXX 20.
- `nlohmann/json` 3.12.0 лежит single-include в `3rdparty/nlohmann/`. При `LUMEX_WITH_FIELD_REFLECTION=ON` include path вешается на `Lumex::reflection`; отдельный `nlohmann_json` target не экспортируется.
- Опция по умолчанию OFF. Нет `3rdparty/nlohmann/json.hpp` при ON - `FATAL_ERROR`.
- Typed GTest `LumexFieldArityTest` на каждый размер 1..32 (цикл типов int/char/double/bool/unsigned/float/short/long long, имена `f0`..`f31`). C++11 - `tuple_size`. C++14 - `get<I>` и типы полей. C++20 - `names_as_array` и `to_json` на каждом индексе.

##### TypeTraits: merge DChannel + PeakExpert

**Файлы:**

- `lumex/core/utility/traits/LumexTypeTraits.hpp`
- `lumex/tests/core/utility/LumexTypeTraits.tests.cpp`
- `lumex/tests/core/utility/CMakeLists.txt`

- `CleanType` снимает ref, затем cv (`std::remove_cvref_t` с C++20; иначе `remove_cv<remove_reference<T>>`). Старый порядок оставлял `int const&` как `int const`.
- `is_optional` доступен с C++11: cv-peel, `LumexOptional<T>`, `std::optional<T>` с C++17, `is_optional_v`.
- `is_invocable` / `is_invocable_v` рядом с `is_callable` / `is_callable_v` (та же INVOKE/SFINAE машина).
- Сьют `LumexTypeTraitsTests` CXX 11 с тех же исходников. `LumexUtilityTests` остается CXX 20 из-за Bit/Cast/Ranges и `LUMEX_DEFINE_ENUM_TRAITS`.

##### CMakeRoutines и Portable Release по умолчанию

**Файлы:**

- `CMakeRoutines/`
- `.gitmodules`
- `CMakeLists.txt`
- `cmake/LumexBuild.cmake`
- `cmake/LumexOptions.cmake`

- Сабмодуль `https://github.com/ViNN280801/cmake` подключен как `CMakeRoutines`.
- `cmake_minimum_required` поднят до 3.16.
- Глобальные `add_compile_options` / AVX2 / `-march=native` / IPO заменены на `configure_compiler_flags` + `configure_optimization_level` по каждому скомпилированному таргету.
- `LUMEX_OPTIMIZATION_LEVEL` по умолчанию `Portable` (O2, x86-64 generic, без LTO и без fast-math). `Maximum` отклоняется (`FATAL_ERROR`): IEEE-API (`IsNanInf`, stringify inf/nan).
- Локальный `cmake/LibraryVersioning.cmake` сохранен (обход `cmake_llvm_rc` на clang-cl + Ninja).
- В `run_all_tests` DEPENDS добавлен `LumexCrcTests`.

##### Имена noexcept-макросов и `std::size_t`

**Файлы:**

- `lumex/core/utility/macros/LumexKeywords.hpp`
- `lumex/**`
- `examples/**`

- `LUMEX_NOEXCEPT_FUNCTION` переименован в `LUMEX_NOEXCEPT`.
- `LUMEX_NOEXCEPT_FUNCTION_CONDITIONAL` переименован в `LUMEX_NOEXCEPT_IF`.
- `LUMEX_NOEXCEPT_IF` вариадический (`...` / `__VA_ARGS__`), чтобы запятая в условии не считалась вторым аргументом макроса.
- Неквалифицированный `size_t` заменен на `std::size_t`.
- `LUMEX_DEFINE_ENUM_TRAITS`: поля `values` / `first` / `size` / `last` объявлены как `static LUMEX_CONSTEXPR`, а не `LUMEX_CONST_NUM` (`constinit` не делает переменную `constexpr`, `values.front()` нельзя использовать в следующем `constinit`).

##### Версия проекта

**Файлы:**

- `CMakeLists.txt`
- `lumex/core/utility/version/version.rc.in`

- `project(... VERSION 1.0.0.0)`.
- Контакт вендора: `vladislav.semykin@gmail.com`.
- Ресурс версии на Windows собирается через `rc.exe`, а не `cmake_llvm_rc` (clang-cl 21 зависал на `.rc` без `windows.h`).

##### Пространства имен по каталогам

**Файлы:**

- `lumex/applied/**`
- `lumex/core/**`
- `lumex/xml/**`
- `examples/**`
- `lumex/tests/**`

- Публичные типы переведены с `Lumex::PascalCase` на вложенные `namespace lumex { namespace <dir> { ... } }` в том же порядке, что и include guard (пример dump: `lumex::core::utility::dump`). C++11, без `namespace a::b::c`.
- Каталог `logger/logger/` дает `lumex::applied::logger::logger`. Каталог `settings/interface/` остается в `lumex::applied::settings`: Windows SDK определяет `interface` как макрос.
- Глобальные `using Type = lumex::<dirs>::Type` сохранены. Тесты делают `using namespace` production-пространства.
- Примеры не оборачивают `main()` в `lumex::examples::...`.

##### Раскладка модулей

**Файлы:**

- `lumex/applied/**`
- `lumex/core/**`

- Модули `applied/` и `core/` переведены на один extensionless umbrella на модуль и реализацию во внутренних подкаталогах (образец: `lumex/core/exceptions/`).

##### Include-пути

**Файлы:**

- `lumex/**/*.hpp`
- `lumex/**/*.cpp`

- Родительские `#include "../..."` заменены на пути от корня репозитория (`#include "lumex/..."`).
- `LumexConstantMacros.hpp` подключает `LumexKeywords.hpp` как `lumex/core/utility/macros/LumexKeywords.hpp`.

##### Include guard

**Файлы:**

- `lumex/**/*.hpp`
- `lumex/xml/LumexXml`
- `lumex/core/reflection/LumexReflection`

- Guard переведены на шаблон `LUMEX_<DIR1>_..._<DIRN>_HPP` по каталогам под `lumex/` (пример: `lumex/core/crc/algo/LumexCrc.hpp` → `LUMEX_CORE_CRC_ALGO_HPP`).
- Если в одном каталоге несколько заголовков, к guard добавлен SNAKE_CASE stem файла (`LumexKeywords.hpp` → `LUMEX_CORE_UTILITY_MACROS_KEYWORDS_HPP`).

##### Переписанные serial- и monitor-файлы

**Файлы:**

- `lumex/applied/serial/enumeration/LumexSerialPortEnumeration.hpp`
- `lumex/applied/serial/enumeration/LumexSerialPortEnumeration.cpp`
- `lumex/applied/serial/resolver/LumexPortProcessResolver.hpp`
- `lumex/applied/serial/resolver/LumexPortProcessResolver.cpp`
- `lumex/applied/serial/port/LumexSerialPort.hpp`
- `lumex/applied/serial/port/LumexSerialPort.cpp`
- `lumex/applied/utility/resourcemonitor/LumexResourceMonitor.hpp`

- Структуры `serial_port_info_t`, `port_holder_info_t`.
- На переписанных файлах: `LUMEX_NOEXCEPT` вместо сырого `noexcept`; `LUMEX_CONST_NUM` / `LUMEX_CONST_STR` для именованных констант.

##### CRC umbrella

**Файлы:**

- `lumex/core/crc/algo/LumexCrc.hpp`
- `lumex/core/crc/parametric/LumexCrcParametric.hpp`

- `algo/LumexCrc.hpp` в конце подключает parametric-заголовок.
- Include guard алгоритма: `LUMEX_CORE_CRC_ALGO_HPP`.
- `ValidateSpec` объявлен как `LUMEX_CONSTEXPR_FUNCTION void`; диспетчер ширины вынесен в `crc_dispatch_t`.

##### CRC: только parametric + catalog

**Файлы:**

- `lumex/core/crc/LumexCrc`
- `lumex/core/crc/catalog/LumexCrcCatalog.hpp`
- `lumex/core/crc/catalog/LumexCrcCatalog.cpp`
- `lumex/core/crc/parametric/LumexCrcParametric.hpp`
- `lumex/tests/core/crc/LumexCrc.tests.cpp`

- Umbrella подключает `parametric/LumexCrcParametric.hpp` и `catalog/LumexCrcCatalog.hpp`.
- `TransportCrcMode::Default` считает CRC-8/MAXIM-DOW через `Crc8MaximDow` (тот же алгоритм, что была таблица `Crc8`).

#### Удалено

##### Табличные Crc4 / Crc8

**Файлы:**

- `lumex/core/crc/algo/LumexCrc.hpp`
- `lumex/core/crc/algo/LumexCrc.cpp`

- Табличные классы `Crc4` и `Crc8` удалены. CRC-8/MAXIM-DOW доступен как `Crc8MaximDow` / `CrcParametric<crc8_maxim_dow_spec_t>`. CRC-4 - как `crc4_g704_spec_t` / `crc4_interlaken_spec_t` в каталоге.

#### Исправлено

##### Optional tests CMakeLists: имя исходника

**Файлы:**

- `lumex/tests/core/optional/CMakeLists.txt`

- `add_executable` ссылался на отсутствующий `optional.tests.cpp`. Фактический файл - `LumexOptional.tests.cpp`. Полный reconfigure `build-tests` падал на generate.

##### CircularBuffer / LumexAssert: runtime и compile-time проверки через `LUMEX_*`

**Файлы:**

- `lumex/core/utility/assert/LumexAssert.hpp`
- `lumex/core/circular_buffer/CircularBuffer.hpp`
- `lumex/core/circular_buffer/CMakeLists.txt`
- `lumex/core/expected/CMakeLists.txt`
- `cmake/LumexModules.cmake`
- `lumex/tests/cmake/cases/require_graph_all_edges.cmake`
- `lumex/tests/core/circular_buffer/CircularBuffer.header.tests.cpp`
- заголовки, которые вызывали `static_assert` напрямую (`Expected`, `ExpectedVoid`, CRC, Base64, Logger, NumberGenerator, TypeTraits, Timer, Stringify, SafeNumericComparator, AggregateFields, ExceptionWrapper)

- `front()` / `back()` и остальные runtime-проверки идут через `LUMEX_ASSERT` (`lumex_assert_handler` в `LumexCore_utility`). Header-only модули `circular_buffer` и `expected` линкуют `Lumex::utility`; `LUMEX_BUILD_CIRCULAR_BUFFER` / `LUMEX_BUILD_EXPECTED` требуют `LUMEX_BUILD_UTILITY`.
- `static_assert` в production-коде заменен на `LUMEX_STATIC_ASSERT` / `LUMEX_STATIC_ASSERT_MSG`.
- Регрессия IWYU: `CircularBuffer.header.tests.cpp` включает umbrella до gtest и покрывает `front`/`back`, overwrite, iterators, assignment, swap, empty/full.

##### NumberGenerator: MSVC Debug assert на `exponential_distribution`

**Файлы:**

- `lumex/core/generators/number_generator/LumexNumberGenerator.hpp`
- `lumex/tests/core/generators/number_generator/LumexNumberGenerator.tests.cpp`

- Конструктор больше не меняет местами `from`/`to` для не-`UNIFORM` распределений (для exponential `from` - это lambda).
- Lambda <= 0 заменяется на 1, чтобы `std::exponential_distribution` не срабатывал на MSVC `_DEBUG` assert `invalid lambda argument`.

##### CoreDump: C++11 static members и namespace по каталогам

**Файлы:**

- `lumex/core/utility/dump/LumexCoreDumpGenerator.hpp`
- `lumex/core/utility/dump/LumexCoreDumpGenerator.cpp`
- `lumex/core/utility/LumexUtility`
- `lumex/tests/core/utility/LumexCoreDumpGenerator.tests.cpp`

- `inline` static members (C++17, MSVC C7525 при `/std:c++14`) вынесены в `.cpp` через `LUMEX_INLINE_VARIABLE` (на C++11/14 макрос пустой; одно определение в TU библиотеки). Зонтик снова включает дамп без порога C++17.
- Типы дампа перенесены в `lumex::core::utility::dump` (как в include guard `LUMEX_CORE_UTILITY_DUMP_HPP`).
- `string_view` / concepts / optional по-прежнему за `HAS_*`.

##### Visual Studio: ALL-цель `lumex_copy_compile_commands` валила сборку

**Файлы:**

- `CMakeLists.txt`
- `cmake/CopyCompileCommandsIfPresent.cmake`

- `CMAKE_EXPORT_COMPILE_COMMANDS` не создает `compile_commands.json` у генератора Visual Studio. Цель `ALL` вызывала `copy_if_different` несуществующего файла (`MSB8066`). Копирование пропускается, если файла нет.

##### XmlUtils: пропущенная точка с запятой после `LUMEX_CONST_NUM`

**Файлы:**

- `lumex/xml/utility/XmlUtils.hpp`

- В `set_value_convert` для `float` и `double` константа `kBufSize = 128U` оказалась на одной строке с `char_t buf[kBufSize]` без `;`. clang-cl останавливал `LumexXml` (`expected ';' at end of declaration`).

##### Settings-тесты оставляли каталоги в корне репозитория

**Файлы:**

- `lumex/tests/applied/settings/LumexSettingsGuard.tests.cpp`
- `lumex/tests/applied/settings/LumexSettingsINI.tests.cpp`
- `.gitignore`

- `LumexSettingsGuardFileTest` не имел `TearDown`; в `LumexSettingsINITest` удаление каталога было закомментировано. После прогона в корне оставались `test_settings_guard/` и `test_ini_settings/`.
- Оба фикстура удаляют свой каталог в `TearDown`. Имя каталога уникально на тест (как у `LumexFilesystemTest`), чтобы `gtest_discover_tests` не травил соседние процессы общим путем.
- Регрессия: `LumexSettingsGuardCleanup.RemoveDirectoryIfExistsDeletesCreatedDirectory` создает каталог с `test.ini` и проверяет, что helper его удаляет.

##### LumexKeywords.hpp

**Файлы:**

- `lumex/core/utility/macros/LumexKeywords.hpp`

- В ветке до C++11 убран дубликат `LUMEX_CONSTEXPR_DEFAULTED_VIRTUAL_DTOR`.
