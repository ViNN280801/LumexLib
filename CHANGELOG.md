# Changelog LumexLib

<!-- markdownlint-disable MD022 MD024 MD032 -->

Все значимые изменения в библиотеке LumexLib документируются в этом файле.

Формат основан на [Keep a Changelog](https://keepachangelog.com/ru/1.1.0/).

Секция с пометкой **"в разработке"** накапливает изменения до выпуска релиза (git-тег, публикация артефактов). Учитываемая история начинается с `[v1.0.0.0]`. Номера `1.1.0`, `1.1.0.0` и `1.1.0.1` были экспериментальными и не считаются.

С 2026-10-03 номер версии назначается по правилам [VERSIONING.md](VERSIONING.md): MAJOR - несовместимый API, MINOR - несовместимый ABI при том же API, PATCH - функциональное изменение (новый модуль или функция, исправление, поведение, производительность), TWEAK - изменение без функционального эффекта (тесты, документация, сборка, примеры). Опубликованные выпуски не меняются; три выпуска, сделанные до правил с другим номером, отмечены в своих секциях.

---

## [v1.0.2.0] - в разработке

> Изменения поверх `v1.0.1.1` (ветка `feat/atomic-shared-ptr`). Версия в `CMakeLists.txt` поднята до `1.0.2.0`; дату секции ставит релизный коммит.

### [v1.0.2.0]

#### Добавлено

##### Примеры `count_leading_zeros` и `byte_swap`

**Файлы:** `lumex/examples/utility/example_utility.cpp`, `lumex/examples/utility/CMakeLists.txt`

**Коммит:** `bda89747`

**Суть:** обход utility печатает `count_leading_zeros` для `uint8_t` `0x0F` (4), `uint32_t` 0 (32) и `uint64_t` с установленным битом 32 (31), и `byte_swap` для `uint32_t` `0x12345678` (`0x78563412`). Пример собран как C++20, потому что `byte_swap` объявлен только с этого стандарта; вызов стоит за теми же `LUMEX_HAS_*`, что и объявление. Рабочий пример с длиной пакета не менялся.

##### `count_leading_zeros` в `lumex::core::utility::bit`

**Файлы:** `lumex/core/utility/bit/LumexBit.hpp`, `lumex/core/fmt/LumexFormat.hpp`, `lumex/tests/core/utility/LumexBit.cxx11.tests.cpp` (новый)

**Коммит:** `8ccf8e5b`

**Суть:** функция лежала в `lumex::core::fmt::Detail` и нигде не вызывалась. Теперь это `lumex::core::utility::bit::count_leading_zeros` (C++11, аналог C++20 `std::countl_zero`): беззнаковое целое в 1, 2, 4 или 8 байт, не `bool`. Ноль возвращает ширину типа. `unsigned __int128` в набор перегрузок не входит: внутренние функции считают нули в 32 или 64 битах и более широкий аргумент обрезали бы. `byte_swap` в том же заголовке по-прежнему требует C++20.

##### Ряд MSVC STL в бенчмарке `benchmarks/atomic`

**Файлы:** `benchmarks/atomic/README.md`, `lumex/core/atomic/README.md`, `benchmarks/atomic/results/msvc/`

**Коммит:** `1b522c5f`

**Суть:** пять наборов atomic на MSVC 19.51 x64 Release прошли (2232 теста, 0 падений; два теста `constinit` в C++11 и C++17 пропускаются, им нужен C++20). Полный прогон (Intel Core i9-12900H, 20 логических CPU, схема питания Balanced, 93.8 минуты) записан в `benchmarks/atomic/results/msvc` и не заменяет ряд libstdc++. При 20 потоках MSVC STL, выбор LumexLib по умолчанию и lock-based сборка C++11 стоят примерно одинаково (`load ()` 6.9-7.0); lock-based сборка C++20, которая ждет через `std::atomic::wait`, дороже (`load ()` 11.8, `compare_exchange_strong ()` 25.9; нижний квартиль `load ()` 6.7-12.8). Базовая линия блоков в пределах 3 % от среднего (0.975-1.021).

##### Уведомления сторонних лицензий: `THIRD-PARTY-NOTICES.md`

**Файлы:** `THIRD-PARTY-NOTICES.md` (новый), `CMakeLists.txt`, `conanfile.py`, `lumex/xml/LumexXml`, `lumex/tests/cmake/cases/wiring_license_install.cmake` (новый)

**Коммит:** `d1388a08`

**Суть:** модуль `lumex/xml` сделан на основе pugixml (MIT, Copyright (c) 2006-2026 Arseny Kapoulkine), а лицензия MIT требует сохранять уведомление во всех копиях. Теперь оно лежит в `THIRD-PARTY-NOTICES.md` (там же ссылки на лицензии nlohmann/json и GoogleTest в `3rdparty/`), `cmake --install` ставит его вместе с `LICENSE` в `share/LumexLib`, пакет Conan кладет оба файла в `licenses/`, зонтик `LumexXml` называет происхождение модуля. Кейс `cmake.wiring_license_install`.

##### `lumex_filesystem::replace_file_content`

**Файлы:** `lumex/core/filesystem/fs/LumexFilesystem.hpp`, `lumex/core/filesystem/fs/LumexFilesystem.cpp`, `lumex/tests/core/filesystem/LumexFilesystemReplaceFileContent.tests.cpp` (новый)

**Коммиты:** `a3f2dd76`, `444cbf1a`

**Суть:** `replace_file_content (path, content, write_mode)` записывает содержимое в `<путь>.tmp` (`KTEMPORARY_FILE_SUFFIX`) в том же каталоге и переименовывает его поверх файла; при любой ошибке временный файл удаляется, а прежний файл остается как был. Возвращает `filesystem_result<void>` с `errno` шага, который не удался, не бросает. `write_mode` (`text`, `binary`) доступен и как `lumex::write_mode`.

##### Модуль `core/atomic`: `atomic_shared_ptr` и `atomic_weak_ptr` начиная с C++11

**Файлы:**

- `lumex/core/atomic/LumexAtomic`, `lumex/core/atomic/smart_ptr/` (`LumexAtomicSharedPtr.hpp`, `LumexAtomicWeakPtr.hpp`, `LumexAtomicSmartPtrConfig.hpp`, `LumexAtomicSmartPtrCell.hpp`), `lumex/core/atomic/sync/` (`LumexBitLock.hpp`, `LumexAtomicWait.hpp`), `lumex/core/atomic/CMakeLists.txt`, `lumex/core/atomic/README.md`
- `lumex/core/CMakeLists.txt`, корневой `CMakeLists.txt`, `cmake/LumexOptions.cmake` (`LUMEX_BUILD_ATOMIC`), `cmake/LumexModules.cmake`, `cmake/LumexLibConfig.cmake.in`, `conanfile.py` (`core_atomic`), `Doxyfile`, `Doxyfile.in`
- `lumex/examples/atomic/` (`example_atomic_smart_ptr.cpp`, `example_atomic_config_reload.cpp`), `lumex/examples/CMakeLists.txt`
- `lumex/tests/core/atomic/`, `lumex/tests/core/CMakeLists.txt`, `lumex/tests/cmake/consumer/atomic_compile_checks/`, `lumex/tests/cmake/consumer/atomic_cross_module/`, `lumex/tests/cmake/cases/wiring_atomic.cmake` и затронутые кейсы `LumexCMake` (`options_declared`, `wiring_subdirs`, `wiring_distr`, `require_ok_*`, `setup_all_on`)

**Коммиты:** `e72f58c5`, `157ebd66`, `192fce55`, `ea0f880b`, `b72a5b5e`

**Суть:** интерфейс C++20 `std::atomic<std::shared_ptr<T>>` и `std::atomic<std::weak_ptr<T>>` (P0718R2, LWG 3661, LWG 3893) начиная с C++11 поверх обычных `std::shared_ptr` и `std::weak_ptr` любой стандартной библиотеки: `lumex::core::atomic::smart_ptr::atomic_shared_ptr<T>` и `atomic_weak_ptr<T>` (только в своем пространстве имен: глобальных псевдонимов нет, их отсутствие закрепляет `LumexAtomicGlobalNamesTest`), цель `lumex::atomic` (header-only, `Threads::Threads`). Это порт собственной реализации автора для LLVM libc++ (llvm-project pull request 194215), в которой два метода, lock-based и lock-free; это не сторонняя библиотека и не копия libstdc++, MSVC STL или Folly. Перенесен lock-based метод: замок из двух битов (занят, есть спящие) в отдельном 32-битном слове, а не в младших битах указателя на control block, как в libc++, потому что раскладку `std::shared_ptr` чужой библиотеки трогать нельзя; поток, проигравший гонку после пробуждения, снова ставит бит спящего (исправление потерянного пробуждения из libc++), старое значение и его deleter освобождаются после снятия замка. Lock-free метод не перенесен: он работает с control block libc++ (счетчик в слове control block, `__add_shared`, `__release_shared`), что для `std::shared_ptr` другой библиотеки - неопределенное поведение. Где библиотека предоставляет `__cpp_lib_atomic_shared_ptr` (libstdc++ 12+, MSVC STL в C++20), классы оборачивают стандартный тип; `wait` в обоих вариантах свой и соответствует стандарту (`wait` из libstdc++ 13 просыпается, когда другой поток лишь берет внутренний замок, и не замечает смены одного хранимого указателя). Потоки спят через `std::atomic::wait` начиная с C++20 и через таблицу из 64 полос `std::mutex` + `std::condition_variable` раньше; таблица одна на процесс на ELF и Mach-O. Выбор виден в `LUMEX_ATOMIC_SMART_PTR_USES_STD` и `LUMEX_ATOMIC_WAIT_USES_STD`, каждая комбинация живет в своем inline namespace, для тестов и бенчмарков есть `LUMEX_ATOMIC_SMART_PTR_FORCE_LOCK_BASED` и `LUMEX_ATOMIC_WAIT_FORCE_TABLE`. Clang сообщает о запрещенных стандартом константных memory order через `diagnose_if`. Пять наборов тестов (C++11, C++17, C++20, C++20 с принудительным lock-based, C++20 с принудительной таблицей; имена CTest с префиксом `atomic.`), проверки компиляции, проверка пробуждения через разделяемые библиотеки со скрытой видимостью. `README.md` модуля описывает оба метода libc++, расследование livelock в CAS и известную утечку ссылки в `load ()` lock-free метода.

##### Бенчмарк `benchmarks/atomic`

**Файлы:** `benchmarks/atomic/` (`bench_atomic_smart_ptr.cpp`, `bench_atomic_smart_ptr.hpp`, `bench_lumex_cxx11.cpp`, `bench_lumex_lock_based.cpp`, `bench_lumex_default.cpp`, `CMakeLists.txt`, `run_benchmark.py`, `plot_results.py`, `README.md`, `results/`), корневой `CMakeLists.txt`, `Doxyfile`, `Doxyfile.in` (`IMAGE_PATH`), `lumex/core/atomic/README.md`

**Коммит:** `86a01a27`

**Суть:** `load`, `store`, `exchange` и `compare_exchange_strong` при 1-20 потоках на одном объекте и без конкуренции, каждое число - отношение к `std::atomic<std::uint64_t>::compare_exchange_strong` того же процесса и того же числа потоков, медиана 100 запусков с паузами 30 с, реализации чередуются (методика бенчмарка автора для libc++). Сравниваются lock-based реализация LumexLib в C++11 и в C++20, выбор LumexLib по умолчанию в C++20 и `std::atomic<std::shared_ptr<T>>` из libstdc++ 13; ряд MSVC STL ждет запуска на Windows (todo 56). Итог полного прогона (Intel Core i7-12700K, 20 логических CPU, GCC 13.2, libstdc++ 13, 91 минута): базовая линия всех блоков в пределах 3 % от среднего; при 20 потоках lock-based реализация в 2.8-3.5 раза дешевле libstdc++ для `load ()`, в 2.4-2.9 раза для `compare_exchange_strong ()`, в 2.0-2.3 раза для `exchange ()` и в 1.6-1.8 раза для `store ()`; libstdc++ выигрывает `store ()` при 2-6 потоках; без конкуренции `load ()`, `store ()` и `exchange ()` стоят 17-22 нс у всех реализаций, а lock-based `compare_exchange_strong ()` - 44-47 нс против 57 нс. Выбор LumexLib по умолчанию в C++20 (обертка над типом libstdc++) не дороже самой libstdc++.

##### Правила версий: `VERSIONING.md`

**Файлы:** `VERSIONING.md`, `CHANGELOG.md`, `Doxyfile`, `Doxyfile.in`

**Суть:** записано, какое изменение поднимает какой компонент версии: MAJOR - несовместимый API, MINOR - несовместимый ABI при том же API (вместе с ним меняется SONAME `major.minor`), PATCH - функциональное изменение, TWEAK - изменение без функционального эффекта. Номер принадлежит выпуску, опубликованные выпуски не меняются. Три выпуска, сделанные до правил с номером TWEAK вместо PATCH (`v1.0.0.1`, `v1.0.0.2`, `v1.0.1.1`), отмечены в своих секциях; файл входит в документацию Doxygen.

#### Изменено

##### `ByteSwap` переименован в `byte_swap`

**Файлы:** `lumex/core/utility/bit/LumexBit.hpp`, `lumex/tests/core/utility/LumexBit.cxx11.tests.cpp`, `lumex/tests/core/utility/LumexBit.cxx20.tests.cpp`, `lumex/tests/core/utility/LumexBit.cxx23.tests.cpp`, `lumex/tests/core/utility/CMakeLists.txt`

**Коммит:** `bda89747`

**Суть:** публичное имя `lumex::core::utility::bit::ByteSwap` теперь `byte_swap`. Сигнатура, `constexpr` и порог C++20 те же. Имена тестов GoogleTest с `WhenByteSwap` оставлены: это названия проверок, не API.

##### Один набор тестов на каждый стандарт C++

**Файлы:** `lumex/tests/LumexTestStandards.cmake` (новый), `cmake/LumexGoogleTest.cmake` (`lumex_add_standard_suites`), `lumex/tests/cmake/cases/wiring_standard_suites.cmake` (новый), все каталоги `lumex/tests/`, кейсы `cmake.*` с именами тестов, README `atomic`

**Коммиты:** `a9a9c5c8`, `918b26ca`, `b3bc4e0c`, `9bf997aa`, `f08b0921`, `6f689261`, `430d1c9d`, `d7b89bbb`, `91196e58`, `b4855c6e`, `0975b1a9`, `de0082b6`, `b9e6ef45`, `bc24fa2c`, `24b31ec3`, `f8b1937a`, `75cf0a14`, `f4d99fbd`, `8933c659`, `8ba4da60`, `62dcdfbc`, `1b902cb4`

**Суть:** каждый модуль собирает тесты для своего нижнего стандарта и для каждого порога, на котором ветвится его код (`__cplusplus`, `__cpp_*`, `LUMEX_HAS_*`), плюс прежние наборы: utility 11/14/17/20/23/26, fmt, reflection, string, logger 11/14/17/20, crc 14/17/20, atomic, base64, expected, json, xml, exceptions, math 11/17/20, circular_buffer, filesystem 11/20, resource_monitor 17, остальные 11; C++23 и C++26 только там, где их знают CMake и компилятор. Таблица в одном месте, `lumex/tests/LumexTestStandards.cmake`; кейс `cmake.wiring_standard_suites` проверяет каждый каталог. Файлы тестов называются `<Компонент>.cxx<std>.tests.cpp`, набор стандарта компилирует файлы своего и всех младших стандартов; исполняемые файлы `Lumex<Компонент>Cxx<std>Tests`, у каждого имени CTest суффикс `.cxx<std>` и для нижнего стандарта (`base64.LumexBase64Test.Encode.cxx11`). Один исполняемый файл на модуль и стандарт убрал одинаковые имена CTest (в `utility` и `reflection` одни и те же исходники собирались в несколько файлов). Старые имена тестов сохранились с суффиксом. Тесты, которые раньше молча выпадали из компиляции на GCC 8 (он сообщает `__cplusplus` 201709L при `-std=c++2a`), теперь видны как пропущенные. **Потребителю и скриптам:** фильтры `ctest -R` по старым именам исполняемых файлов и тестов без суффикса нужно переписать.

##### Лицензия MIT во всех публичных файлах, обычным комментарием

**Файлы:** все `.hpp`, `.h` и `.cpp` в `lumex/` (кроме `tests/` и `examples/`), 26 зонтичных заголовков, `Scripts/CodeTools/check_license_headers.py` (новый), `lumex/tests/CMakeLists.txt`

**Коммиты:** `28239bdd`, `fa99a0c6`

**Суть:** комментарий с лицензией начинался с `/**`, и Doxygen переносил весь текст MIT на страницу каждого файла; теперь это обычный комментарий `/*`. 64 файла `.cpp` и 26 зонтиков, в которых лицензии не было, получили тот же текст. Тест `lint.license_headers` требует его в каждом публичном файле.

##### Настройки сохраняются через временный файл

**Файлы:** `lumex/applied/settings/json/LumexSettingsJSON.cpp`, `lumex/applied/settings/ini/LumexSettingsINI.cpp`, `lumex/applied/settings/xml/LumexSettingsXML.cpp`, заголовки этих классов и `ILumexSettings`, `lumex/tests/applied/settings/LumexSettingsSave.tests.cpp` (новый)

**Коммиты:** `e52f76c9`, `a3f2dd76`, `444cbf1a`

**Суть:** `LumexSettingsJSON::save` открывал файл на запись до сериализации и бросал nlohmann `type_error` 316 на значении не в UTF-8, оставляя обрезанный файл, хотя `ILumexSettings` обещает `false` без исключений. Теперь все три формата сначала строят текст целиком, при ошибке возвращают `false`, не трогая файл, и записывают через `lumex_filesystem::replace_file_content`. Что изменилось для потребителя: `save` нужна запись в каталог файла (только запись в сам файл уже не хватает), символическая ссылка на месте файла заменяется обычным файлом, у нового файла права по умолчанию.

##### `Expected::transform` и `transform_error` на C++20 принимают любой тип результата

**Файлы:** `lumex/core/expected/result/Expected.hpp`, `lumex/tests/core/expected/ExpectedMonadic.tests.cpp` (новый), тесты `Expected`

**Коммит:** `cdc95a21`

**Суть:** ветка C++20 требовала, чтобы результат функции приводился к прежнему `T` или `E`, поэтому `Expected<int, E>::transform` с функцией, возвращающей `std::string`, не компилировался; ветка до C++20 и `std::expected` это разрешают. Теперь все стандарты дают `Expected<U, E>` и `Expected<T, G>`; код, который компилировался, компилируется с тем же смыслом.

##### Имена тестов CTest повторяют каталоги

**Файлы:** `cmake/LumexTestNames.cmake` (новый), `cmake/LumexGoogleTest.cmake`, `lumex/tests/CMakeLists.txt`, `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/cases/wiring_test_names.cmake`, `lumex/tests/cmake/cases/wiring_test_names_outside_root.cmake`, `lumex/tests/cmake/cases/wiring_include_order_ctest.cmake`, `lumex/tests/cmake/cases/wiring_atomic.cmake`, `CMakeLists.txt` тестов `core/crc`, `core/utility`, `core/atomic`, `core/generators/number_generator`, `applied/serial`, `lumex/examples/cmake/LumexExampleHelpers.cmake`, `lumex/examples/fmt/CMakeLists.txt`, комментарии и README с именами тестов

**Коммиты:** `c99be74b`, `0cbcecb2`, `19c1179c`, `d25c08da`, `599178ac`, `f0853d95`, `d25c5374`

**Суть:** каждый тест получает префикс каталога, из которого зарегистрирован: путь под `lumex/tests/` без группы `core/` или `applied/`, с точками вместо `/` и точкой в конце (`crc.`, `xml.`, `json.`, `generators.number_generator.`). Поэтому `ctest -R '^math\.'` выбирает модуль вместе с его подкаталогами. Явный `TEST_PREFIX` переопределяет префикс, суффиксы `.cxx17` и `.cxx20` остались. Та же схема у всех остальных тестов: `LumexCMake.<кейс>` теперь `cmake.<кейс>`, `LumexIncludeOrder` теперь `lint.include_order`, примеры называются `examples.<модуль>.<имя>`, одиночные `add_test` модулей (number_generator, запасной путь для CMake < 3.18) тоже с префиксом; метки `cmake`, `lint`, `example` прежние. Правило закрепляют кейсы `cmake.wiring_test_names` и `cmake.wiring_test_names_outside_root`. Фильтры `ctest -R` по старым именам нужно переписать.

##### Документация: описания всех файлов и зонтичных заголовков, один шаблон `Doxyfile.in`

**Файлы:** 85 заголовков `lumex/` без блока `@file`, 26 зонтичных заголовков модулей, `Doxyfile.in`, `Doxyfile` (удален), корневой `CMakeLists.txt`, `Scripts/CodeTools/check_file_blocks.py` (новый), `lumex/tests/CMakeLists.txt`, `compile.py`, `.gitignore`, `Scripts/Doxygen/*.py`

**Коммиты:** `c3425a26`, `ae692944`, `d5b7d91d`, `3117ebc8`, `002d1c27`, `798042bb`

**Суть:** у 85 из 136 публичных заголовков не было блока `@file`, и их страницы в документации были без описания; теперь у каждого есть описание, написанное по коду. Doxygen не читал зонтичные заголовки без расширения, которые и включает потребитель: CMake передает их список в `INPUT`, `EXTENSION_MAPPING = no_extension=C++`, у каждого зонтика блок `@file` с назначением модуля, целью `lumex::<модуль>` и нужным стандартом. Тест `lint.file_blocks` (метка `lint`) падает, если у публичного заголовка нет блока `@file`. CMake читает шаблон `Doxyfile.in` с `FULL_PATH_NAMES = NO`; одинаковый с ним `Doxyfile` удален.

#### Исправлено

##### Тесты исключений не удаляют общий каталог отчетов о сбоях параллельно

**Файлы:** `lumex/tests/core/exceptions/CMakeLists.txt`, `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/cases/wiring_exception_crash_lock.cmake`

**Суть:** `TearDown` фикстуры удаляет каталог `crashes` рядом с исполняемым файлом, а наборы C++11, C++17 и C++20 лежат в одном `bin/` и `ctest -j` запускает их вместе. `to_crash_report` пишет только в этот каталог. Все найденные тесты трех наборов получают CTest `RESOURCE_LOCK` `lumex_exception_crash_dir`. Кейс `cmake.wiring_exception_crash_lock` требует эту строку в коде `CMakeLists.txt`, а не в комментарии.

##### Тесты `replace_file_content` и кавычек INI укладываются в предел пути Windows

**Файлы:** `lumex/tests/core/filesystem/LumexFilesystemReplaceFileContent.cxx11.tests.cpp`, `lumex/tests/applied/settings/LumexSettingsINI.cxx11.tests.cpp`

**Суть:** ctest запускает набор из каталога модуля. Имя каталога собиралось из полного имени теста, и вместе с `target.cfg.tmp` или повтором этого имени в имени файла путь превышал 259 символов, которые принимает ANSI-интерфейс Windows. `create_directories` и `save` возвращали неуспех до самой проверки. Каталог для `replace_file_content` теперь состоит из имени теста и pid, файл с кавычками называется `quotes.ini`.

##### `XmlNode`, `XmlAttribute` и `XmlText` на clang-cl больше не импортируют `string_view`

**Файлы:** `lumex/xml/node/XmlNode.hpp`, `lumex/xml/attribute/XmlAttribute.hpp`, `lumex/xml/text/XmlText.hpp`

**Коммит:** `5286d4dc`

**Суть:** три класса были `class LUMEX_API`. clang-cl даже в Release не встраивает inline-член такого класса и ставит на него `dllimport`. Перегрузки от `string_view_t` есть только в заголовке (C++17), поэтому `LumexXmlCxx17Tests` и `LumexXmlCxx20Tests` падали с LNK2019: 22 символа из одного теста. Теперь `LUMEX_API` стоит на членах, собранных в библиотеку (`char const *`, указатель и размер и остальные определения в `.cpp`). Перегрузки от `string_view_t` остаются inline и больше не импортируются.

##### `LumexBaseException` на clang-cl больше не импортирует `string_view`

**Файлы:** `lumex/core/exceptions/exception/LumexException.hpp`

**Коммит:** `28d7d596`

**Суть:** класс был `class LUMEX_API`. clang-cl даже в Release не встраивает inline-член такого класса и ставит на него `dllimport`. Конструктор от `std::string_view` есть только в заголовке (C++17), поэтому `LumexExceptionsCxx17Tests` и `LumexExceptionsCxx20Tests` падали с LNK2019. Теперь `LUMEX_API` стоит на конструкторах от `char const *` и `std::string` и на `to_stderr` и `to_crash_report`. Конструктор от `std::string_view` остается inline и больше не импортируется.

##### Base64 на clang-cl больше не импортирует `string_view` и `span`

**Файлы:** `lumex/core/base64/encode/Encoder.hpp`, `lumex/core/base64/decode/Decoder.hpp`, `lumex/core/base64/validate/Validator.hpp`

**Коммит:** `fdf84600`

**Суть:** `Encoder`, `Decoder` и `Validator` были `class LUMEX_API`. clang-cl даже в Release не встраивает inline-член такого класса и ставит на него `dllimport`. DLL собрана без перегрузок `std::string_view` и `std::span` (их нет в каждом стандарте), поэтому `LumexBase64Cxx17Tests` и `LumexBase64Cxx20Tests` падали с LNK2019. Теперь `LUMEX_API` стоит только на функциях с указателем и размером и на `encode` от `std::vector`. Обертки остаются inline в заголовке и больше не импортируются.

##### `__int128` форматируется на clang-cl без `__udivti3`

**Файлы:** `lumex/core/fmt/LumexFormat.hpp`, `lumex/tests/core/fmt/LumexFormatTypes.cxx11.tests.cpp`, `lumex/examples/fmt/example_format.cpp`, `lumex/examples/fmt/example_format_api.cpp`

**Коммит:** `a28c5c17`

**Суть:** `LUMEX_FORMAT_HAS_INT128` включается по `__SIZEOF_INT128__`, поэтому clang-cl собирал `write_integer` и `to_base` для `unsigned __int128` с операторами `/` и `%`. Эти операторы тянут `__udivti3` и `__umodti3` из compiler-rt, а `link.exe` у clang-cl их не предоставляет: `LumexFormatExample` и `LumexFormatExampleApi` не линковались (LNK2019). Десятичная запись теперь делит значение на `10^19` по двум `uint64_t`, основания 2, 8 и 16 берутся сдвигом и маской. Тип по-прежнему печатается везде, где компилятор его дает. Примеры `LumexFormatExample` и `LumexFormatExampleApi` выводят `2^64`.

##### `compile_commands.json` включает библиотеку с первой конфигурации

**Файлы:** корневой `CMakeLists.txt`, `lumex/tests/cmake/cases/wiring_compile_commands.cmake`

**Коммит:** `3445464a`

**Суть:** `configure_compile_commands` стоял после `add_subdirectory(lumex)`. `CMAKE_EXPORT_COMPILE_COMMANDS` копируется на цель в момент ее создания, поэтому при первой конфигурации единицы трансляции библиотеки не попадали в `compile_commands.json` до повторной конфигурации. Вызов перенесен выше `add_subdirectory(lumex)` и по-прежнему выполняется только при `LUMEX_IS_TOP_LEVEL`. Кейс `cmake.wiring_compile_commands` закрепляет этот порядок.

##### Найдено новыми наборами: `expected` на C++11, `fmt` на C++14, reflection `get<I>` на C++14, константы `CoreDumpGenerator`

**Файлы:** `lumex/core/expected/result/Expected.hpp`, `lumex/core/expected/result/ExpectedVoid.hpp`, `lumex/core/fmt/LumexFormat.hpp`, `lumex/core/reflection/field_reflection/LumexAggregateFields.hpp`, `lumex/core/utility/dump/LumexCoreDumpGenerator.{hpp,cpp}`, тесты этих модулей

**Коммиты:** `56c51f83`, `29a387f7`, `eaf75958`, `d40f234e`

**Суть:** `core/expected` обещал C++11, но не компилировался: в C++11 `constexpr`-функция-член неявно `const`, и `error () &` конфликтовал с `error () const &`; теперь неконстантные члены и функции из нескольких операторов помечены `LUMEX_CONSTEXPR_CXX14`, на C++14 и новее код не изменился. libstdc++ 13 объявляет `__cpp_lib_to_chars` и на C++14, поэтому `LumexFormat.hpp` включал вывод чисел через `std::to_chars` без `<charconv>`, и потребитель `fmt` на GCC 13 с `-std=c++14` не собирался; теперь только с C++17. В field reflection на C++14 `get<I>` компилировался, только если в той же единице трансляции раньше использовали `tuple_size<T>`; теперь `field_type` сам вызывает внедрение всех полей. `CoreDumpGenerator::KB_*` и `MB_1` были `static constexpr` без определения вне класса, и потребитель на C++11/14, привязавший их к ссылке, не линковался; теперь это `static const` с определением в библиотеке при любом стандарте.

##### Имена `xml` и `base64` больше не попадают в глобальное пространство имен

**Файлы:** 28 заголовков `lumex/xml/` и `lumex/core/base64/`, `XmlDocument.cpp`, `XPathQuery.cpp`, `Scripts/CodeTools/check_header_using.py` (новый), `lumex/tests/xml/LumexXmlGlobalNames.tests.cpp` (новый), `lumex/tests/CMakeLists.txt`

**Коммиты:** `72a65c28`, `7c1cde6e`

**Суть:** 79 директив `using namespace` на уровне файла (и `using ...::char_t`) переносили имена библиотеки в глобальное пространство каждого, кто включал `LumexXml` или `LumexBase64`, вопреки правилам библиотеки. Теперь директивы стоят внутри пространств имен `lumex`; экспортируемые символы не изменились. **Потребителю:** без квалификации больше не видны `lumex::xml` и его подпространства (`document`, `node`, `xpath`, `types`, `text`, `utility`, `memory`, `writer`, `constants`, `tree`, `attribute`, `range`), `types::Types` (`char_t`, `string_t`, `string_view_t`, перечисления и перечислители вроде `node_element`, `status_ok`, `encoding_auto`), `constants::Constants` (`kparse_*`, `kformat_*`), `xpath::memory`, `xpath::node`, `xpath::parser`, `xpath::constants::Constants`, `lumex::core::base64::codec::Types` (`byte_type`, `string_type_t`); такой код нужно квалифицировать или добавить свой `using`. Тест `lint.header_using_directives` запрещает новые директивы в заголовках.

##### Пять дефектов: `seh_translator`, `LumexException.hpp`, `LIKELY`, `LUMEX_FUNCTION_NAME`

**Файлы:** `lumex/core/exceptions/crash/WindowsSEHTranslator.{hpp,cpp}`, `lumex/core/exceptions/exception/LumexException.hpp`, `lumex/core/utility/attr/LumexAttributes.hpp`, `lumex/core/utility/macros/LumexMacros.hpp`, `lumex/applied/logging/log/LumexLoggingMacro.hpp`, новые тесты и кейсы `cmake.source_*`

**Коммиты:** `ddcd93f9`, `6ac19cb5`, `478064a2`, `f23a9905`, `46fdd1c5`

**Суть:** `seh_translator` был объявлен `inline` и определен только в `.cpp`, поэтому потребитель `SET_SEH_TRANSLATOR` на Windows мог не слинковаться: теперь это обычная экспортируемая функция. `LumexException.hpp` использовал `SET_SEH_TRANSLATOR`, не включая его заголовок. `LUMEX_ATTRIBUTE_LIKELY_COND` / `UNLIKELY_COND` не было до C++20 (теперь `__builtin_expect` на GCC и Clang, просто условие иначе), а `LUMEX_ATTRIBUTE_LIKELY` / `UNLIKELY` до C++20 раскрывались в `__attribute__ ((likely))`, который GCC 13 не принимает в этом месте: теперь они там пустые, как и обещает заголовок. `LUMEX_FUNCTION_NAME` брал `__FUNCSIG__` на MinGW, где его нет: выбор теперь по компилятору, и `LumexLoggingMacro.hpp` берет его из `LumexMacros.hpp`. Проверка на MSVC (todo 56) подтвердит Windows-часть.

##### `Expected`: `transform_error` до C++20, `or_else` с другим типом ошибки, сообщения assert

**Файлы:** `lumex/core/expected/result/Expected.hpp`, `lumex/core/expected/result/ExpectedVoid.hpp`, тесты `Expected`

**Коммиты:** `85853a80`, `70d2f31b`

**Суть:** `Expected<void, E>::transform_error()` для rvalue не компилировался на C++14 и C++17 (ветка до C++20 читала неактивный член и строила не тот тип); тесты с этим именем саму функцию не вызывали. `Expected<T, E>::or_else`, функция которого возвращает другой тип ошибки, не компилировался ни на одном стандарте. Сообщения `LUMEX_ASSERT` говорили «undefined behavior», хотя проверка работает во всех сборках и останавливает программу; теперь они называют, что вызвано и на чем, и это проверяют death-тесты.

##### Settings: исключения в `LumexSettingsGuard`, печать `is_ini_valid`

**Файлы:** `lumex/applied/settings/guard/LumexSettingsGuard.{hpp,cpp}`, `lumex/applied/settings/ini/LumexSettingsINI.{hpp,cpp}`, тесты settings

**Коммиты:** `a5c9432d`, `58637bd7`

**Суть:** `ensureKeysWithDefaults`, `ensureExistsWithDefaults` и `repairIfCorrupted` объявлены `noexcept`, но исключение объекта настроек (например, `save` JSON) приводило к `std::terminate`; теперь они его ловят, пишут в лог и возвращают `false`, сигнатуры прежние. `LumexSettingsINI::is_ini_valid` печатал неверные строки в `std::cout`.

##### Комментарии, которые не совпадали с кодом

**Файлы:** `lumex/applied/settings/**`, `lumex/core/expected/**`, `lumex/core/utility/util/LumexUtilities.hpp`, `lumex/xml/attribute/XmlAttribute.hpp`, `lumex/xml/text/XmlText.hpp`, `lumex/xml/xpath/variable/XPathVariable.hpp`, `lumex/xml/xpath/utility/XPathUtils.{hpp,cpp}`, `lumex/core/generators/number_generator/LumexNumberGenerator.hpp`

**Коммиты:** `57813aeb`, `0941b75b`, `9618a6bb`, `b2bc510e`

**Суть:** около тридцати мест, среди них: `LumexSettingsFactory::create` возвращает `nullptr`, а не бросает; `LUMEX_ASSERT` не отключается при `NDEBUG`; `as_int` и другие геттеры возвращают значение по умолчанию только для пустого значения; описания `Expected`, `Unexpected`, `BadExpectedAccess`, `in_place_tag` и `DistributionType` попадали на страницу пространства имен; `fd_to_ptr` / `ptr_to_fd` объявлены в глобальном пространстве. Удалены два `friend` несуществующих классов; `copy_xpath_variable` теперь одна реализация (экспортируемая версия вызывает встроенную). Комментарии `LumexSettingsGuard` больше не называют класс продукта.

##### Прочее

**Файлы:** `lumex/applied/logger/logger/LumexLogger.cpp`, `compile.py`, `benchmarks/atomic/README.md`

**Коммиты:** `0c1be589`, `9b98f87a`

**Суть:** Clang 23.1.0 предупреждал о неиспользуемой перегрузке `format_hex` для указателей (удалена). `compile.py --documentation` принимает только `html`: `pdf` не мог работать при `GENERATE_LATEX = NO`. Быстрый прогон бенчмарка в README пишет в каталог сборки, а не поверх `results/`.

##### Установка: header-only модули без `LUMEX_BUILD_UTILITY`, зонтик `LumexFilesystem`, `LumexNumberGenerator.hpp` один раз

**Файлы:** `lumex/CMakeLists.txt`, `lumex/core/filesystem/CMakeLists.txt`, `lumex/core/generators/CMakeLists.txt`, `lumex/tests/cmake/consumer/install_header_only_without_utility/` (новый), `lumex/tests/cmake/cases/wiring_install_umbrellas.cmake` (новый), `lumex/tests/cmake/CMakeLists.txt`

**Коммиты:** `a189ceea`, `212d2e44`

**Суть:** `string`, `math`, `generators`, `optional` и `atomic` включают семь header-only заголовков `core/utility` (`assert/LumexAssert.hpp`, `attr/LumexAttributes.hpp`, `compiler/LumexCheckFeatures.hpp`, `macros/LumexKeywords.hpp`, `macros/LumexConstantMacros.hpp`, `macros/LumexExceptionMacros.hpp`, `traits/LumexTypeTraits.hpp`), а при установке с `LUMEX_BUILD_UTILITY=OFF` их не было, и эти модули из префикса не компилировались. Теперь они ставятся при любом наборе модулей; кейс `cmake.install_header_only_without_utility` ставит библиотеку без utility и собирает потребителя с пятью зонтиками. Правило установки `filesystem` искало файл `lumex_filesystem`, а зонтик называется `LumexFilesystem`, поэтому установленная библиотека была без него; `generators` ставил все свое поддерево по `*.hpp`, и `LumexNumberGenerator.hpp` устанавливался второй раз рядом с правилом `number_generator`. Кейс `cmake.wiring_install_umbrellas` проверяет оба условия для каждого зонтика.

##### Даты выпусков `v1.0.1.0` и `v1.0.1.1`

**Файлы:** `CHANGELOG.md`

**Суть:** обе секции после выпуска оставались "в разработке". Теперь у них даты и релизные коммиты: `v1.0.1.0` - 2026-09-30, `c213b98b`; `v1.0.1.1` - 2026-10-03, `7187ef0f`.

##### Документация собирается без предупреждений Doxygen

**Файлы:** `Doxyfile`, `Doxyfile.in`, `CMakeLists.txt`, `Scripts/Doxygen/cpp_raw_string_filter.py`, комментарии и несколько определений в `lumex/`

**Коммиты:** `d7b4c0ef`, `f27888dc`, `06f94326`

**Суть:** сборка документации (`LUMEX_BUILD_DOCUMENTATION=ON`, Doxygen 1.8.20 на Astra) давала 587 предупреждений и 40 сообщений о самом `Doxyfile`; теперь ни одного, и с графами тоже. Настройки: Doxygen раскрывает макросы (без этого определения с `LUMEX_NOEXCEPT` и `LUMEX_PUBLIC_API` не сходились с объявлениями), нестандартные команды (`@thread_safety`, `@complexity`, `@unit` и другие) объявлены в `ALIASES`, 40 параметров Doxygen 1.13 со значениями по умолчанию, которых не знает 1.8.20, удалены (для 1.13 ничего не меняется), лимит узлов графа поднят до 128. Новый фильтр переписывает raw-строки в обычные строки с тем же значением: Doxygen 1.8 их не понимает и терял все после них, например класс `LumexSettingsINI`. Определения, которые Doxygen не мог сопоставить, записаны через настоящие имена классов (`LumexFilesystem.cpp` вместо псевдонимов `lumex::path` и других, `XPathQuery.cpp` внутри своего пространства имен), параметрам в объявлениях даны имена, которые документация уже называла. В комментариях исправлены неверные имена в `@param`, `@link` вместо ссылки на страницу, `@copydoc` на самого себя (`stringify`), описание конструктора перемещения у конструктора из `Unexpected` (`ExpectedVoid.hpp`), ссылка на `resolve_serial_port_path` в другом пространстве имен, текст, который Doxygen читал как HTML. Два служебных шаблона, на которых Doxygen ошибается (`crc_dispatch_t`, `make_index_sequence_impl`), скрыты от него `@cond`. В собранной документации не осталось ссылок на несуществующие файлы (в прежней `docs/` их было 2227, почти все на графы, которые не строились без Graphviz). Последние две вели со ссылок "More..." у членов анонимных `union` (`XmlBufferedWriter::scratch`, `XPathMemoryBlock`) на страницу `union`, которую Doxygen 1.8.20 не создает; описания этих членов теперь записаны через `@details` и видны на странице класса.

##### Документация сверена с кодом

**Файлы:** `lumex/core/fmt/README.md`, `benchmarks/fmt/README.md`, `lumex/applied/logger/README.md`, `lumex/applied/logger/logger/LumexLogger.hpp`, `docker/README.md`, `Scripts/Debugging/Linux/hang_diag.example.md`, `Scripts/Debugging/Windows/CDB/crash_diag.example.md`, `CHANGELOG.md`, `Doxyfile`, `Doxyfile.in`, `lumex/core/optional/opt/LumexOptional.hpp`, `lumex/core/utility/numeric/LumexSafeNumericComparator.hpp`, `lumex/core/filesystem/fs/LumexFilesystem.hpp`, `lumex/applied/settings/guard/LumexSettingsGuard.hpp`, `lumex/core/utility/dump/LumexCoreDumpGenerator.hpp`

**Коммиты:** `8c21ba17`, `c123949d`

**Суть:** утверждения документации проверены по коду и сборке. README `fmt` и бенчмарка `fmt`: `snprintf` не самый быстрый для всех чисел (для `int` быстрее `std::to_string`, для `{:.3f}` он в 1.4 раза медленнее LumexFormat). README логгера: `LumexLogging` используют `LumexHardwareCapabilities.cpp`, `LumexResourceMonitor.cpp` и `LumexSettingsGuard.cpp`, а не `LumexDebug.hpp`; страница логгера (`\page Logger`) теперь входит в Doxygen (`INPUT`). Комментарий класса `LumexLogger`: константы `kLoggingEnableFileName` нет (файл-триггер `kDefaultTriggerFileName`), в списке уровней не было `success`, имя файла с меткой времени `logger_DD.MM.YYYY-hh-mm-ss.log`, без двоеточий. README docker: каталогов `complete-*` и `analyzed-files.txt` скрипт не создает. Примеры отладочных скриптов ссылались на `Scripts/hang_diag.sh` и `Scripts\CDB\`, а скрипты лежат в `Scripts/Debugging/Linux/` и `Scripts\Debugging\Windows\CDB\`; пример вывода `hang_diag.sh` дополнен строками, которые скрипт печатает. `@file` в `LumexOptional.hpp` и `LumexSafeNumericComparator.hpp` называли несуществующие файлы. `LumexFilesystem.hpp` называл модуль header-only и обещал отсутствие исключений, а он собирается из `LumexFilesystem.cpp`, и `checkName` бросает `std::invalid_argument`. `LumexSettingsGuard.hpp`: реализаций `ILumexSettings` уже три (INI, JSON, XML). В этом файле в секции `[v1.0.1.0]`: флаги `-fkeep-inline-functions` и `-fkeep-static-functions` удалены, а не оставлены для GCC, и кейс называется `wiring_xml_no_keep_flags`. Комментарии пяти значений `KERNEL_*_DUMP` в `LumexCoreDumpGenerator.hpp` начинались с русских названий типов дампа из интерфейса Windows; теперь в них английские названия из документации Microsoft, как требует правило об английских комментариях.

##### Примеры из комментариев заголовков больше не попадают на страницу Examples

**Файлы:** `lumex/applied/hardware/caps/LumexCPUVectorizationCapabilities.hpp`, `lumex/applied/hardware/caps/LumexHardwareCapabilities.hpp`, `lumex/applied/logger/logger/LumexLogger.hpp`, `lumex/applied/resource_monitor/monitor/LumexResourceMonitor.hpp`, `lumex/core/environment/env/LumexEnvironment.hpp`, `lumex/core/exceptions/LumexExceptionWrapper.hpp`, `lumex/core/exceptions/exception/LumexException.hpp`, `lumex/core/exceptions/stacktrace/LumexStacktraceEntry.hpp`, `lumex/core/time/clock/LumexTime.hpp`, `lumex/core/time/timer/LumexTimer.hpp`, `lumex/core/utility/debug/LumexDebug.hpp`, `lumex/core/utility/demangle/LumexDemangle.hpp`, `lumex/core/utility/dump/LumexCoreDumpGenerator.hpp`, `lumex/core/utility/macros/LumexConstantMacros.hpp`, `lumex/core/utility/numeric/LumexSafeNumericComparator.hpp`, `lumex/core/utility/util/LumexUtilities.hpp`

**Коммит:** `a3c1a60f`

**Суть:** шестнадцать заголовков оформляли пример использования командой `@example`, а в Doxygen она документирует отдельный файл с примером. Поэтому страница Examples в `docs/` содержала 25 ложных примеров: 14 заголовков целиком, каждый под абсолютным путем к копии репозитория, на которой собиралась документация, семь значений-образцов, слова `Typical`, `Usage` и `Examples` как имена файлов и несуществующий `SafeNumericComparator_Examples.cpp`; Doxygen выдавал 30 предупреждений. Такой блок комментария целиком уходил на страницу примера, и его функция или класс оставались без описания. Теперь пример оформлен через `@par Example` (код без `@code` обернут в `@code` / `@endcode`, значения-образцы стали списком `@par Examples`), и описание снова стоит у своей функции или класса. Это открыло три описания, не совпадавших с кодом: у `fd_to_ptr` и `ptr_to_fd` в `@param` стояли не те имена параметров, а описание `lumDemangle` говорило о функции, принимающей искаженное имя, хотя макрос принимает тип или выражение и сам вызывает `typeid`. Блок, ссылавшийся на несуществующий файл примеров, стал обычным комментарием.

##### Пути в секции `[v1.0.0.0]`

**Файлы:** `CHANGELOG.md`

**Суть:** четыре пути в «Файлы:» секции `[v1.0.0.0]` не встречаются ни в одном коммите. Они заменены файлами из тега `v1.0.0.0`: `lumex/applied/serial/LumexSerialPort` вместо `lumex/applied/serial/LumexSerial`; `cmake/LibraryVersioning.cmake`, который пишет `<цель>_version.rc` в каталог сборки, вместо `lumex/core/utility/version/version.rc.in`; `lumex/applied/resource_monitor/monitor/LumexResourceMonitor.hpp` вместо `lumex/applied/utility/resourcemonitor/LumexResourceMonitor.hpp`. Строка `lumex/examples/**/*.hpp` в записи о лицензии MIT удалена: примеры состоят из `.cpp` без заголовка MIT.
---

## [v1.0.1.1] - 2026-10-03

> Тег `v1.0.1.1`, релизный коммит `7187ef0f`.
>
> **Номер версии.** По [VERSIONING.md](VERSIONING.md) изменения этого выпуска - уровня PATCH (изменились вывод стека вызовов и производительность), а поднят был TWEAK: выпуск сделан до правил. Номер `1.0.1.1` не меняется; следующий выпуск, `1.0.2.0`, следует правилам.

### [v1.0.1.1]

#### Исправлено

##### Стек вызовов на Linux больше не запускает `addr2line` на каждый кадр

**Файлы:**

- `lumex/core/utility/debug/LumexDebug.hpp`
- `lumex/core/exceptions/stacktrace/LumexStacktrace.hpp`, `lumex/core/exceptions/stacktrace/LumexStacktrace.cpp`
- `lumex/tests/core/utility/LumexDebug.tests.cpp`, `lumex/tests/core/exceptions/LumexException.tests.cpp`

**Коммит:** `dd948b4e`

**Суть:** `captureStackTrace` для кадра без экспортированного символа и `LumexStacktraceEntry::description ()` для любого кадра запускали `addr2line` отдельным процессом (`popen`). Когда рядом с модулем лежит отдельный файл отладочной информации (`.gnu_debuglink`, например после разбиения Release-сборки CMakeRoutines), каждый такой процесс заново читает весь этот файл: на исполняемом файле с 81 МБ DWARF это 1.2-1.3 с на кадр, несколько секунд на одну строку лога со стеком, и все это время поток, пишущий в лог, стоял. Без такого файла `addr2line` отвечал по таблице динамических символов ближайшим экспортированным именем, то есть неверным для неэкспортированной функции. Кроме того, `LumexStacktrace` передавал `addr2line` смещение от начала отображения, которое неверно для исполняемого файла, слинкованного по фиксированному адресу (не PIE). Теперь процессы не запускаются и отладочная информация не читается: кадр с экспортированным символом пишется как `имя +сдвиг (модуль+0xСМЕЩЕНИЕ) [0xАДРЕС]`, кадр без него как `модуль+0xСМЕЩЕНИЕ [0xАДРЕС]`, где `СМЕЩЕНИЕ` - адрес внутри образа модуля (через `dl_iterate_phdr`, верно для PIE, не PIE и разделяемых библиотек), а `АДРЕС` - адрес в процессе. По этому адресу `addr2line -e <модуль>` или gdb с модулем и его отладочным файлом символизируют стек офлайн, со строками исходников. Кадр стоит микросекунды. Файл и строка в `LumexStacktraceEntry` на POSIX всегда пустые. Внутренние `detail::get_source_info_addr2line`, `kDefaultBufferSize` и `kDefaultCmdSize` удалены, `<sys/wait.h>` и `<unistd.h>` больше не включаются в `LumexStacktrace.hpp`. Windows не изменился (DbgHelp символизирует в процессе).

##### CMakeRoutines: отладочная информация Release на ELF уходит в `.debug`

**Файлы:** подмодуль `CMakeRoutines` (`e03c7f3`)

**Суть:** при `DEBUG_SYMBOLS ON` Release на GCC и Clang под ELF больше не теряет символы (`-Wl,--strip-all` вместе с `-g`): DWARF переносится в `<файл>.debug`, бинарный файл получает Build ID и `.gnu_debuglink`. Подмодуль поднят в `2ebbbe9b`, после тега `v1.0.1.0`.

---

## [v1.0.1.0] - 2026-09-30

> Тег `v1.0.1.0`, релизный коммит `c213b98b`.

### [v1.0.1.0]

#### Исправлено

##### Сборка на Linux: GCC 13.2 и Clang 19 на Astra Linux SE 1.7 (glibc 2.28)

**Файлы:**

- `lumex/LumexExport.hpp`
- `lumex/core/environment/env/LumexEnvironment.cpp`
- `lumex/core/filesystem/fs/LumexFilesystem.cpp`
- `lumex/applied/hardware/caps/LumexHardwareCapabilities.cpp`
- `lumex/core/utility/dump/LumexCoreDumpGenerator.cpp`, `lumex/core/utility/dump/LumexCoreDumpGenerator.hpp`
- `lumex/xml/**/*.cpp`, `lumex/xml/CMakeLists.txt`
- `lumex/core/utility/CMakeLists.txt`, `lumex/core/exceptions/CMakeLists.txt`, `lumex/applied/logger/CMakeLists.txt`
- `cmake/LumexLibConfig.cmake.in`, `conanfile.py`

**Суть:** библиотеку собирали и проверяли только MSVC и clang-cl, поэтому сборка по умолчанию на Linux не проходила. Под GCC 13.2: `LUMEX_UTILITY_API` раскрывался в `__declspec (dllexport)` на любой платформе (теперь `__declspec` остался только для Windows и Cygwin, на ELF это `visibility ("default")`); в `LumexEnvironment.cpp` открытие пространств имен стояло внутри ветки `#if LUMEX_OS_WINDOWS`, и POSIX-код попадал в глобальную область; в `LumexFilesystem.cpp` переменная `perms` затеняла одноименный тип в своем инициализаторе, а `directory_iterator::Impl` определялся через псевдоним из пространства имен `lumex`; в `LumexHardwareCapabilities.cpp` две функции определялись с полной квалификацией внутри своего же пространства имен. Статические члены `CoreDumpGenerator` и `DumpFactory` были определены в `.cpp` через `LUMEX_INLINE_VARIABLE`, то есть как `inline`-переменные начиная с C++17, при объявлении без `inline` в заголовке: GCC не эмитировал константно инициализируемые члены (`s_mutex`, `s_activeOperations`, `s_initialized` и другие), и `libLumexXml.so` не линковалась с `-Wl,--no-undefined`. Теперь это обычные определения. Под Clang 19 `libLumexXml.so` не линковалась по той же причине для функций: 388 API-определений XML и `isFileExists` в `filesystem` были `inline` в `.cpp`, а Clang эмитирует такие функции только там, где они используются (GCC это скрывал флагом `-fkeep-inline-functions`, MSVC - `dllexport`). Теперь это обычные определения, а флаги `-fkeep-inline-functions` и `-fkeep-static-functions` больше не передаются (второй Clang отвергал как неизвестный; кейс `wiring_xml_no_keep_flags` проверяет, что их нет). До glibc 2.34 `pthread_*` и `dladdr` находятся в `libpthread` и `libdl`: `lumex::utility` получил `Threads::Threads` и `${CMAKE_DL_LIBS}` как PUBLIC-зависимости (их вызывает inline-код заголовков дампа и отладки), `exceptions` и `logger` - `${CMAKE_DL_LIBS}`; без этого у `libLumexCore_exceptions.so` и `libLumexApplied_logger.so` оставался неразрешенный `dladdr`. Конфигурация пакета вызывает `find_dependency(Threads)`, рецепт Conan добавляет `pthread` вне Windows и `dl` на Linux. Ни одна общая библиотека больше не содержит неразрешенных символов (`ldd -r`).

##### Предупреждения GCC и Clang при `WARNINGS HIGH`

**Файлы:** production-код под `lumex/`, блок прагм проекта в `3rdparty/nlohmann/json.hpp`

**Суть:** сборка по умолчанию дает 0 предупреждений под GCC 13.2 и под Clang 19. Условия `#if LUMEX_OS_X` заменены на `#if defined(LUMEX_OS_X)` (`-Wundef`; поведение то же, эти макросы определяются только как `1`). Неявные знаковые и сужающие преобразования сделаны явными `static_cast` к тому же типу, к которому они и так выполнялись (`SA_RESETHAND` в `sa_flags`, индексы после `readlink`, UTF-8 и числовой код XML-парсера, индексы и байты Base64); `(bool)` заменен на `static_cast<bool>`, `decltype (string_type::npos)` на `string_type::size_type`; переименованы затеняющие параметры конструкторов `xml_parse_result_t` и `XmlWriterFile`; `starts_with` в перечислении последовательных портов перенесен в Windows-ветку, где он используется; неиспользуемые на x86 константы CPUID помечены `LUMEX_ATTRIBUTE_MAYBE_UNUSED`. `LumexException_GetStackTraceTrampoline` объявлялся в глобальном пространстве имен, а определялся в `lumex::core::exceptions::exception`, то есть объявленная функция не имела определения; объявление перенесено к определению. `-Wswitch-enum` в двух `switch` класса `DumpFactory`, намеренно разбирающих часть `DumpType`, и `-Wformat-nonliteral` у `strftime` с форматом от вызывающего в `LumexTime` подавлены точечными `#pragma GCC diagnostic`. Прагмы с группами, которых нет в Clang 19 (`-Wnrvo`, `-Wunsafe-buffer-usage-in-libc-call`, `-Wvariadic-macro-arguments-omitted`), обернуты в `#if __has_warning(...)`. Сборка Debug под GCC показала еще два: `unspecified_bool_xml_node` в `XmlNode.cpp` была объявлена экспортируемой `inline`-функцией без объявления в заголовке и после снятия `inline` давала `-Wmissing-declarations`; теперь она внутренняя, как у соседних классов. Буфер `set_value_integer` в `XmlUtils.hpp` передавался как указатель на `const` неинициализированным (`-Wmaybe-uninitialized`), теперь он обнулен.

##### `LUMEX_FUNC_NAME` всегда раскрывался в `__func__`

**Файлы:** `lumex/core/utility/os/LumexCheckOS.hpp`

**Суть:** условие `#if defined(LUMEX_OS_IS_WINDOWS) && LUMEX_OS_IS_WINDOWS` проверяло функциональный макрос без скобок, который в `#if` дает 0, поэтому ветки `__FUNCSIG__` и `__PRETTY_FUNCTION__` не выбирались никогда. Выбор теперь идет по компилятору: MSVC и clang-cl - `__FUNCSIG__`, GCC и Clang (включая MinGW) - `__PRETTY_FUNCTION__`, иначе `__func__`. Меняется только текст диагностик `std::cerr` в `LumexFilesystem.cpp`.

##### CPUID: данные функции 7 читались без проверки ее поддержки

**Файлы:** `lumex/applied/hardware/caps/LumexCPUVectorizationCapabilities.cpp`

**Суть:** проверка поддержки расширенных функций была записана как `eax >= 0` для `uint32_t` и всегда была истинной. Теперь AVX2 и AVX-512 определяются по функции 7 только если CPUID(0) сообщает максимальную стандартную функцию не ниже 7. На процессорах с функцией 7 результат не меняется, на старых больше не читаются данные чужой функции.

##### Distr получает рантайм компилятора, а не системный

**Файлы:** `cmake/PublishDistr.cmake`, `cmake/LumexModules.cmake`

**Суть:** `CopyRuntimeDependencies` определяет имена рантайма по зависимостям библиотек, но находит сами файлы через кеш ldconfig и игнорирует `LD_LIBRARY_PATH`. Для сборки GCC 13.2 из `/opt` в `x64/DistrRelease` попадал системный `libstdc++.so.6.0.25` без `GLIBCXX_3.4.30`, которого требуют библиотеки, и Distr не загружался на чистой системе. `publish_distr` передает путь к компилятору, а `PublishDistr.cmake` заменяет каждый скопированный рантайм файлом, который сообщает сам компилятор (`-print-file-name`).

##### Тесты и примеры на Linux: ветки C++11, C++14 и C++17

**Файлы:**

- `lumex/core/utility/macros/LumexKeywords.hpp`, `lumex/core/utility/traits/LumexTypeTraits.hpp`, `lumex/core/utility/numeric/LumexSafeNumericComparator.hpp`
- `lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp`, `lumex/core/reflection/field_reflection/LumexAggregateFields.hpp`
- `lumex/core/crc/catalog/LumexCrcCatalog.cpp`, `lumex/core/exceptions/stacktrace/LumexStacktraceEntry.hpp`
- `lumex/applied/logger/logger/LumexLogger.hpp`, `lumex/applied/logger/logger/LumexLogger.cpp`
- `lumex/examples/logger/CMakeLists.txt`, `lumex/examples/cmake/LumexExampleHelpers.cmake`
- `lumex/tests/core/utility/`, `lumex/tests/core/reflection/`, `lumex/tests/core/crc/`, `lumex/tests/core/fmt/`, `lumex/tests/core/expected/`
- `lumex/tests/cmake/cases/source_crc_spec_storage_complete.cmake`, `lumex/tests/cmake/cases/wiring_posix_system_libraries.cmake`, `lumex/tests/cmake/CMakeLists.txt`

**Суть:** MSVC не опускается ниже C++14, а GCC 13 и Clang 19 по умолчанию собирают C++17, поэтому наборы тестов, закрепленные на C++11 и C++14, и примеры в стандарте по умолчанию на Linux не собирались. `LUMEX_CONSTEXPR_DTOR` в ветке C++17 раскрывался в `constexpr`, хотя `constexpr`-деструкторы появились только в C++20, и `Expected` не компилировался в C++17; теперь ветка пустая, а новый набор `LumexExpectedCxx17Tests` (имена CTest с `.cxx17`) проверяет C++17. Новый макрос `LUMEX_CONSTEXPR_CXX14` дает `constexpr` начиная с C++14 для тел, которым нужны правила C++14 (несколько операторов, `switch`, возврат `void`): им помечены `default_return<void>::value` и `toString` отражаемых перечислений, которые не компилировались в C++11. `LumexSafeNumericComparator` вызывал `std::exchange` (C++14) в ветке C++11. `First` и `Last` отражаемых перечислений вычислялись через `std::array::front ()` и `back ()`, которые `constexpr` только с C++14; теперь их дают вспомогательные функции C++11, и они остаются константными выражениями в C++11. До C++17 статический `constexpr`-член не является `inline`, и его odr-использование (привязка к ссылке, как в `EXPECT_EQ`) требует определения вне класса. Для перечислений, объявленных внутри класса, добавлен `LUMEX_DEFINE_REFLECTED_ENUM_STORAGE (Owner, EnumName)`: он пишется один раз в `.cpp` и с C++17 пуст. `LumexCrcCatalog.cpp` определяет константы всех 112 спецификаций CRC, без них `LumexCrcTests` (C++14) не линковался; кейс `LumexCMake.source_crc_spec_storage_complete` не дает пропустить новую спецификацию. `field_reflection::tuple_size<T>::value` и `traits::numeric::is_safe_comparable<T, U>::value` были обычными членами `static const` шаблонов, а такой член не бывает неявно `inline` ни в одном стандарте, поэтому `EXPECT_EQ` и `EXPECT_TRUE` с ними без оптимизации не линковались (Debug); теперь оба наследуют `std::integral_constant`, как стандартные трейты. `logger_config_t::kBufferSize` и `kMaxStackTraceFrames` были объявлены через `LUMEX_CONST_NUM`, то есть с C++17 это `inline`-переменные, которые Clang не эмитирует из библиотеки, собранной в C++17 (GCC - только при избыточном внешнем определении), и `LumexLoggerTests` (C++11) не линковался; теперь это обычные члены `static const` с определением в `LumexLogger.cpp`, и библиотека экспортирует их в любом стандарте. Пример `LumexLoggerExampleWorkflow` сам запускает `std::thread`, а до glibc 2.34 `pthread_create` лежит в `libpthread`: пример линкует `Threads::Threads`, а кейс `LumexCMake.wiring_posix_system_libraries` проверяет каждый пример, который включает `<thread>`. В самих тестах `LumexFormatMinMaxMacros.tests.cpp` включает стандартные заголовки до макросов `min` и `max` (с ними не компилируется сама libstdc++), шестнадцатеричные вещественные литералы (C++17) в наборе C++11 заменены на `std::ldexp`, `field_count_tag_t` наследует `std::integral_constant`. Каждое исправление закреплено тестом в своем стандарте. Сборка Clang 19 с libstdc++ из GCC 13.2 нашла еще одну ошибку: признак `traits::string::is_string_like` проверял `data ()` раньше `npos`, а у `std::vector<bool>` в libstdc++ `data ()` объявлен защищенным и удаленным, и Clang вместо отказа подстановки выдавал ошибку доступа (форматирование `std::vector<bool>` в `lumex::fmt` не компилировалось); теперь `npos` проверяется первым. `native_handle ()` и `operator bool` у `LumexStacktraceEntry` помечены `LUMEX_CONSTEXPR_CXX14`: класс не литеральный, а C++11 требует этого от `constexpr`-методов.

##### Экспортируемые функции base64, XML и исключений не зависят от стандарта C++

**Файлы:**

- `lumex/core/base64/decode/Decoder.hpp`, `lumex/core/base64/decode/Decoder.cpp`, `lumex/core/base64/encode/Encoder.hpp`, `lumex/core/base64/encode/Encoder.cpp`, `lumex/core/base64/validate/Validator.hpp`, `lumex/core/base64/validate/Validator.cpp`
- `lumex/xml/node/XmlNode.hpp`, `lumex/xml/node/XmlNode.cpp`, `lumex/xml/attribute/XmlAttribute.hpp`, `lumex/xml/attribute/XmlAttribute.cpp`, `lumex/xml/text/XmlText.hpp`, `lumex/xml/text/XmlText.cpp`, `lumex/xml/utility/XmlUtils.hpp`
- `lumex/core/exceptions/exception/LumexException.hpp`, `lumex/core/exceptions/exception/LumexException.cpp`
- `lumex/examples/base64/example_base64.cpp`
- `lumex/tests/core/base64/`, `lumex/tests/xml/LumexXml.tests.cpp`, `lumex/tests/core/exceptions/`
- `lumex/tests/cmake/consumer/standard_mismatch/`, `lumex/tests/cmake/CMakeLists.txt`

**Суть:** перегрузки, которые существуют только в части стандартов (`std::string_view` и `std::span` начиная с C++17 и C++20, `std::string const &` ниже C++17), экспортировались из библиотеки в том виде, в каком ее собрали. Поэтому библиотека в одном стандарте и потребитель в другом не линковались: например, `LumexBase64Tests` (C++11) против base64, собранного в C++17 по умолчанию у GCC 13 и Clang 19. Теперь экспортируются только функции с одинаковой сигнатурой во всех стандартах (указатель и размер, `std::string`, `std::vector`), а зависящие от стандарта перегрузки стали inline-обертками в заголовках, как в `core/crc`. В base64 это `Decoder::decode (char const *, std::size_t, ...)` и `Validator::is_valid_base64 (char const *, std::size_t)`; в XML - новые размерные `(char_t const *, std::size_t)` у всех именных функций `XmlNode` (`child`, `attribute` с подсказкой и без нее, `next_sibling`, `previous_sibling`, `append_*`, `prepend_*`, `insert_*`, `remove_*`), над которыми работают `string_view_t`-перегрузки `XmlNode`, `XmlAttribute` и `XmlText`; `LumexBaseException (std::string_view)` делегирует конструктору `std::string &&`. Меняется ABI модулей base64, XML и исключений, потребители пересобираются. Ядра с указателем считают `nullptr` недопустимым входом даже при нулевом размере, а обертки передают для пустой строки `""`, так что поведение строковых перегрузок не меняется. Попутно исправлено: декодер Base64 читал за концом входа без паддинга, который валидатор принимает (`"SGk"`: в C++11 лишний байт, в C++17 неопределенное поведение); теперь последняя группа из двух или трех символов декодируется явно. Пример base64 называл вход без паддинга `"SGVsbG8"` ошибкой длины, хотя он допустим; теперь пример показывает оба случая. Тесты: новые наборы `LumexBase64Cxx20Tests` и `LumexExceptionsCxx20Tests` (ветки C++17 и C++20 этих модулей раньше не исполнялись), тесты ядер (вход в середине большего буфера, NUL внутри, пустой вход, `nullptr` с нулевым и ненулевым размером, найдено и не найдено) и оберток; кейсы `LumexCMake.consumer_standard_mismatch_lib11_consumer20` и `LumexCMake.consumer_standard_mismatch_lib20_consumer11` собирают библиотеку в одном стандарте, а потребителя в другом. Ограничение Windows: обертки остаются членами классов с `LUMEX_API`, и MSVC без инлайнинга (Debug) вызывает их копию из DLL, поэтому на Windows DLL и потребитель по-прежнему собираются в одном стандарте.

##### Заголовки больше не включаются внутри пространств имен

**Файлы:**

- `lumex/core/temporary/tmp/LumexTemporary.cpp`, `lumex/applied/settings/ini/LumexSettingsINI.cpp`, `lumex/core/utility/dump/LumexCoreDumpGenerator.hpp`
- `Scripts/CodeTools/check_include_order.py`, `lumex/tests/CMakeLists.txt`, `lumex/tests/cmake/cases/wiring_include_order_ctest.cmake`, `lumex/tests/cmake/CMakeLists.txt`

**Суть:** `<Windows.h>`, `<process.h>`, `<sys/types.h>` и `<unistd.h>` в `LumexTemporary.cpp`, `<iostream>` в `LumexSettingsINI.cpp` и `<concepts>` в `LumexCoreDumpGenerator.hpp` включались внутри пространства имен `lumex`. Это неопределенное поведение ([using.headers]): код работал только потому, что те же заголовки уже были включены раньше, иначе их объявления оказались бы в `lumex::...` и не нашлись бы при линковке. Теперь они стоят в блоке включений. Проверка `Scripts/CodeTools/check_include_order.py` отвергает такие включения, знает стандартные заголовки C++17-C++26 и запускается как тест CTest `LumexIncludeOrder` (метка `lint`), регистрацию которого закрепляет кейс `LumexCMake.wiring_include_order_ctest`.

##### `LUMEX_OS_IS_*()` работают в `#if` и в коде, Apple получает `LUMEX_OS_UNIX`

**Файлы:**

- `lumex/core/utility/os/LumexCheckOS.hpp`, `lumex/applied/logger/logger/LumexLogger.hpp`
- `lumex/tests/core/utility/LumexCheckOS.tests.cpp`, `lumex/tests/core/utility/CMakeLists.txt`

**Суть:** `LUMEX_OS_IS_WINDOWS()`, `LUMEX_OS_IS_LINUX()` и остальные раскрывались в `defined (...)`: внутри `#if` это неопределенное поведение (GCC, Clang и MSVC его терпят), а в обычном коде такой макрос не компилировался, поэтому не собирался и `LUMEX_OS_DEBUG_INFO()`. Теперь каждый макрос раскрывается в константу `1` или `0`, вычисленную в заголовке; так же исправлены `LOGGER_OS_IS_*()` логгера. На Apple не определялся `LUMEX_OS_UNIX`, и `LumexTime`, `LumexTemporary`, обработчик сбоев и определение оборудования уходили в ветку неподдерживаемой ОС; теперь Apple определяет `LUMEX_OS_UNIX` вместе с `LUMEX_OS_APPLE`. Тест `LumexCheckOS.tests.cpp` проверяет макросы в `#if` и в коде в наборах C++11 (`LumexTypeTraitsTests`) и C++20 (`LumexUtilityTests`).

##### Стандартную библиотеку C++ под Clang выбирает проект верхнего уровня

**Файлы:**

- `cmake/LumexOptions.cmake`, `cmake/LumexBuild.cmake`, `conanfile.py`
- `lumex/tests/cmake/cases/wiring_clang_stdlib.cmake`, `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/consumer/run_consumer.cmake`
- сабмодуль `CMakeRoutines`: `optimizations/OptimizationLevelConfig.cmake`, `deployment/LinkCompilerRuntime.cmake`, самотесты

**Суть:** CMakeRoutines сам решал, брать ли libc++ (`-stdlib=libc++`, если с ним собирается и линкуется проверочная программа), и ставил флаг только самой цели. Встроенная библиотека навязывала libc++ проекту, который собирается с libstdc++, а потребитель библиотеки, собранной с libc++, флага не получал и не линковался из-за разного `std::string`. Теперь выбор задает опция `LUMEX_CLANG_STDLIB`: `AUTO` (проверка, как раньше) по умолчанию, когда LumexLib - проект верхнего уровня, `DEFAULT` (без `-stdlib`, библиотека родителя) при встраивании, `LIBCXX` требует libc++ и останавливает конфигурацию, если он непригоден. Рецепт Conan берет значение из `compiler.libcxx` профиля. В CMakeRoutines у `configure_optimization_level` появился аргумент `CXX_STDLIB`; у библиотек флаг `-stdlib=libc++` теперь PUBLIC (компиляция и линковка) и переходит к потребителям, у исполняемых файлов остается PRIVATE; `link_compiler_runtime` ищет `-stdlib` сначала у самой цели и у библиотек, которые она линкует, и только потом в глобальных флагах. Кейс `LumexCMake.wiring_clang_stdlib` и самотесты CMakeRoutines закрепляют поведение. Consumer-кейсы `LumexCMake.consumer_*` собирают встроенную библиотеку с `CMAKE_CXX_FLAGS` и `LUMEX_CLANG_STDLIB` вызывающего дерева, поэтому проходят и в дереве Clang с libc++, и в дереве Clang с libstdc++ из GCC 13.2.

##### Тесты: GoogleTest и libc++, кейсы `LumexCMake.*` на Linux

**Файлы:** `cmake/LumexBuild.cmake`, `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/cases/*.cmake`, сабмодуль `CMakeRoutines`

**Суть:** CMakeRoutines собирает цели Clang с `-stdlib=libc++`, если libc++ пригоден, но vendored GoogleTest обходит `lumex_configure_target` и оставался бы на libstdc++, и тесты не линковались бы из-за разного `std::string`. Выбор стандартной библиотеки теперь переносится на `lumex_gtest_*`. Новые кейсы: `wiring_posix_system_libraries`, `source_no_inline_variable_in_cpp`, `source_no_public_api_inline_in_cpp`, `wiring_xml_no_keep_flags`, `wiring_gtest_cxx_stdlib`; `wiring_distr` проверяет замену рантайма. Кейсы `publish_distr_filters` и `wiring_recursive_source_collection` искали временный каталог только в `TEMP`, который есть лишь в Windows (`publish_distr_filters` к тому же создавал только `.dll`), а `wiring_msvc_vcvars_after_project` проверяет поведение, существующее только на Windows: теперь первые два проходят на Linux, а третий регистрируется только на Windows. В CMakeRoutines непригодный libc++ больше не отбрасывается молча: конфигурация один раз сообщает, какая библиотека остается, а его самотесты больше не пишут в `/` на Linux.

##### `set_environment_variable` на POSIX не перезаписывал существующую переменную

**Файлы:** `lumex/core/environment/env/LumexEnvironment.cpp`, `lumex/tests/applied/settings/LumexSettingsINI.tests.cpp`

**Суть:** POSIX-стратегия вызывала `setenv (name, value, 0)`, поэтому повторная установка уже существующей переменной молча сохраняла старое значение, хотя `overwrite` по умолчанию `true` (на Windows `SetEnvironmentVariableA` перезаписывает всегда). Решение о перезаписи принимает `set_environment_variable`, стратегия теперь всегда вызывает `setenv (..., 1)`. Тест `GivenNoReadPermission_WhenIsIniValid_ThenReturnsFalse` на Linux ожидал `true` вопреки своему имени и раньше не выполнялся (заглушка вне Unix); теперь ожидает `false`, а под root пропускается, потому что права доступа root не ограничивают.


## [v1.0.0.2] - 2026-09-28

> Тег `v1.0.0.2`, релизный коммит `32a547d6`.
>
> **Номер версии.** По [VERSIONING.md](VERSIONING.md) изменения этого выпуска - уровня PATCH (новый API `success()` и `failure()`), а поднят был TWEAK: выпуск сделан до правил. Номер `1.0.0.2` не меняется.

### [v1.0.0.2]

#### Добавлено

##### core/expected: фабрики `success()` и `failure()`

**Файлы:**

- `lumex/core/expected/result/SuccessFailure.hpp`
- `lumex/core/expected/Expected`
- `lumex/tests/core/expected/SuccessFailure.tests.cpp`
- `lumex/tests/core/expected/CMakeLists.txt`
- `lumex/examples/expected/example_expected.cpp`

**Суть:** собрать `Expected` можно было только через `unexpect`-тег, что многословно в `return` и расходится с привычной идиомой boost.outcome (`return success();`, `return failure(error);`). Добавлены маркеры `success_t<ValueType>` и `failure_t<ErrorType>` с неявным преобразованием в `Expected<T, E>` и `Expected<void, E>`, ограниченным SFINAE: const-маркер копирует значение, rvalue-маркер перемещает, несовместимая цель ловится на `return`, а не молча меняет значение. Фабрики: `success()`, `success(value)`, `failure(error)`; `success_t<void>` покрывает и `Expected<void, E>`, и `Expected<T, E>` с конструируемым по умолчанию `T`. Пример расширен разделом с фабриками. Тесты: `SuccessFailure.tests.cpp` - компиляционная матрица типов, типовая сюита 8x8 (адресация пары в отчете плюс типы, не стоящие строки матрицы) и полная матрица 25x25 = 625 пар через pack-expansion (типовая сюита столько параметров не тянет: механике gtest не хватает глубины инстанцирования), плюс пограничные случаи (пустая ошибка, NUL внутри строки, 10k-сообщение, move-only значение и ошибка, бросающий конструктор по умолчанию, возврат из хелпера и лямбды). Размер списка типов - бюджет: стоимость матрицы квадратична, а матрица из 99 типов на MSVC собиралась больше 20 минут при повторных пиках памяти компилятора в 5-8 ГБ, поэтому список сознательно короткий, а расширять его надо по одному-два типа с замером.

## [v1.0.0.1] - 2026-09-28

> Тег `v1.0.0.1`, релизный коммит `8c6f8911`.
>
> **Номер версии.** По [VERSIONING.md](VERSIONING.md) изменения этого выпуска - уровня PATCH (исправление ошибки), а поднят был TWEAK: выпуск сделан до правил. Номер `1.0.0.1` не меняется.

### [v1.0.0.1]

#### Исправлено

##### field_reflection: `AggregateFields` безопасен для агрегатов только из `std::optional` на MSVC

**Файлы:**

- `lumex/core/reflection/field_reflection/LumexAggregateFields.hpp`
- `lumex/tests/core/reflection/LumexFieldReflection.tests.cpp`
- `cmake/LibraryVersioning.cmake`, `cmake/LumexLibraryVersioning.cmake`
- `CMakeLists.txt`

**Суть:** MSVC в `/std:c++20` может не объявлять `__cpp_structured_bindings`, хотя структурированные привязки компилирует, поэтому лазейка CWG 2118 для C++14 оставалась активной рядом с SB-веткой: тип внутри `std::optional` оказывался зафиксирован (ошибка layout `sizeof`) либо `loophole_fn` определялся повторно (C2084). Теперь структурированные привязки предпочитаются, когда доступны (`LUMEX_AGGREGATE_FIELDS_USE_SB`), а на loophole-пути тело friend определяется один раз и преобразуется через `operator U&() const&&`. Тесты покрывают агрегаты целиком из `std::optional` (форма `ChannelAmqpError::error_message_t`), включая вложенный host-тип. Отдельным коммитом исправлены автор и почта в метаданных версии, `CMakeLists.txt` поднят до `1.0.0.1`.

## [v1.0.0.0] - 2026-09-25

> Первая учитываемая версия. Тег `v1.0.0.0`, релизный коммит `df93fc30`.

### [v1.0.0.0]

#### Исправлено

##### MSVC-потребитель получает `/Zc:__cplusplus` от целей модулей

**Файлы:**

- `cmake/LumexBuild.cmake`
- `conanfile.py`
- `lumex/tests/cmake/consumer/cplusplus_macro/`, `lumex/tests/cmake/consumer/run_consumer.cmake`, `lumex/tests/cmake/CMakeLists.txt`

**Суть:** заголовки ветвятся по `__cplusplus`, а MSVC без `/Zc:__cplusplus` сообщает `199711L`. Свой сборщик библиотеки ставил флаг только на ее объектные файлы. Потребитель через `find_package` или Conan его не видел: `LUMEX_CONSTEXPR` раскрывался в пустоту, `LumexDebug.hpp` не компилировался. Теперь каждая цель `LumexCore_*`, `LumexApplied_*` и `LumexXml` отдает флаг как INTERFACE (в том числе header-only модули). В рецепте Conan тот же флаг стоит на каждом компоненте при компиляторе `msvc`. Проверка: `LumexCMake.consumer_cplusplus_macro` и `conan create` тестового потребителя без собственного флага.

##### LumexMath: усечение в `rms`/`rmse`, точность `checked_narrow_cast`, `avg` по `filter_view`

**Файлы:**

- `lumex/core/math/ops/LumexMath.hpp`
- `lumex/tests/core/math/LumexMath.tests.cpp`

**Суть:** `rms (a, b)`, `rmse (range, scalar)` и `rmse (a, b)` для целочисленных диапазонов делили сумму на N в целых до `sqrt`: `rmse ({1, 2}, 0)` давал `sqrt (2)` вместо `sqrt (2.5)`. Теперь деление в типе результата. `checked_narrow_cast` сравнивал границы в `long double`, который на MSVC равен `double`: `<uint64_t, int64_t> (2^63)` и `<double, int64_t> (9.3e18)` проходили проверку и давали `INT64_MIN`. Теперь целое в целое сравнивается точно, а для вещественного источника дополнительно проверяется точная граница `2^digits`. `avg` не компилировался для `std::views::filter` (view не итерируется через const). Каждый случай закреплен тестом.

##### Linux: корневой `project()` больше не требует компилятор ресурсов

**Файлы:**

- `CMakeLists.txt`
- `lumex/tests/cmake/cases/wiring_rc_windows_only.cmake`, `lumex/tests/cmake/CMakeLists.txt`

**Суть:** `project(LumexLib ... LANGUAGES CXX RC)` обрывал конфигурацию вне Windows (`No CMAKE_RC_COMPILER could be found`). Теперь `LANGUAGES CXX`, а `enable_language(RC)` только под `if(WIN32)` (ресурсы версий DLL). Закреплено `LumexCMake.wiring_rc_windows_only`.

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

**Суть:** `configure_compiler_flags` из CMakeRoutines задает `NOMINMAX` / `WIN32_LEAN_AND_MEAN` только на пути MSVC (clang-cl). При встраивании в PeakExpertNoGUI с GNU-like `clang++` макросы `min`/`max` из Windows SDK ломали `std::min` / `std::max` в `LumexLogger`. `lumex_configure_target` теперь всегда добавляет оба определения на WIN32 для каждого скомпилированного таргета.

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

##### LumexMath работает с C++11; `LumexMathSizeMismatchException`

**Файлы:**

- `lumex/core/math/ops/LumexMath.hpp`
- `lumex/tests/core/math/LumexMath.tests.cpp`, `lumex/tests/core/math/CMakeLists.txt`, `lumex/tests/cmake/cases/wiring_subdirs.cmake`
- `lumex/examples/math/CMakeLists.txt`, корневой `CMakeLists.txt` (комментарий)

**Суть:** заголовок больше не требует C++20: вместо concepts и ranges - SFINAE-трейты (`traits::is_numeric`; `traits::NumericConcept` остается в C++20) и собственный однопроходный обход. Диапазон - все, что принимают `begin`/`end`: контейнеры, C-массивы, временные объекты, `std::list`, а в C++20 views, включая не константно итерируемые (`filter`) и с sentinel-концом (`take_while`). У каждой функции одна перегрузка с forwarding-ссылкой, сохраняющая константность аргумента. `distance` и `squared_difference` для целых - `constexpr`, результат продвигается как `a - b`. Имя поля в `checked_narrow_cast` - любой выводимый в `std::ostream` тип (строковый литерал, `std::string`, `std::string_view`). Двухдиапазонные `rms` и `rmse` при разной длине, в том числе когда пуст только один диапазон (раньше возвращали 0), бросают `LumexMathSizeMismatchException` (объявлен через `LUMEX_DEFINE_EXCEPTION`, наследует `std::invalid_argument`); текст содержит обе длины. Наборы `LumexMathTests` (C++11), `LumexMathCxx17Tests`, `LumexMathCxx20Tests`; примеры собираются в C++11.

##### Conan-рецепт покрывает все модули

**Файлы:**

- `conanfile.py`

**Суть:** компоненты повторяют CMake-цели `lumex::<module>` (header-only без библиотек, системные библиотеки Windows/FreeBSD, define-ы settings и logger) и зонтичную `lumex::Lumex`; лицензия MIT, версия читается из `project(LumexLib VERSION ...)`, `exports_sources` включает `LICENSE`, `CMakeRoutines` и `3rdparty`, по умолчанию `shared=True` (как `LUMEX_BUILD_SHARED_LIBS`). Проверено `conan create` с тестовым пакетом.

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
- `lumex/applied/serial/LumexSerialPort`
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
- Локальные обертки `LOGGER_NOEXCEPT_*` / `LOGGER_CONSTEXPR_*` / `LOGGER_CONST*` / `LOGGER_ATTRIBUTE_*` в Logger сведены к `LUMEX_*`.
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
- `cmake/LibraryVersioning.cmake`

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
- `lumex/applied/resource_monitor/monitor/LumexResourceMonitor.hpp`

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
