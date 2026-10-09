# Changelog LumexLib

<!-- markdownlint-disable MD022 MD024 MD032 -->

Все значимые изменения в библиотеке LumexLib документируются в этом файле.

Формат основан на [Keep a Changelog](https://keepachangelog.com/ru/1.1.0/).

Секция с пометкой **"в разработке"** накапливает изменения до выпуска релиза (git-тег, публикация артефактов). Учитываемая история начинается с `[v1.0.0.0]`. Номера `1.1.0`, `1.1.0.0` и `1.1.0.1` были экспериментальными и не считаются.

С 2026-10-03 номер версии назначается по правилам [VERSIONING.md](VERSIONING.md): MAJOR - несовместимый API, MINOR - несовместимый ABI при том же API, PATCH - функциональное изменение (новый модуль или функция, исправление, поведение, производительность), TWEAK - изменение без функционального эффекта (тесты, документация, сборка, примеры). Опубликованные выпуски не меняются; три выпуска, сделанные до правил с другим номером, отмечены в своих секциях.

---

## [v2.0.0.0] - в разработке

> Изменения поверх `v1.0.3.1`.

### [v2.0.0.0]

#### Добавлено

##### `configure_optimization_level` умеет сам добавить BUILD_RPATH рантайма компилятора (`TOOLCHAIN_RUNTIME_RPATH`)

**Файлы:** `cmake/LumexBuild.cmake`, `lumex/tests/cmake/cases/wiring_toolchain_rpath.cmake`, `lumex/tests/cmake/cases/wiring_clang_stdlib.cmake`; подмодуль `CMakeRoutines`: `optimizations/OptimizationLevelConfig.cmake`, `deployment/ToolchainRuntimeRpath.cmake`, `README.md`, `tests/target/cases/toolchain_runtime_rpath.cmake`, `tests/target/cases/toolchain_runtime_rpath_build.cmake`, `tests/target/fixtures/toolchain_runtime_rpath/CMakeLists.txt`

**Суть:** встраивающий проект (PeakExpertWeb, PeakExpertCE) получал BUILD_RPATH рантайма компилятора только на целях Lumex, а для своих исполняемых файлов должен был сам вызывать `configure_toolchain_runtime_rpath`. В `configure_optimization_level` (подмодуль `CMakeRoutines`) появился параметр `TOOLCHAIN_RUNTIME_RPATH <ON|OFF>`: при `ON` последним шагом вызова для переданной цели выполняется `configure_toolchain_runtime_rpath`, уже после выбора `-stdlib=` (`CXX_STDLIB`). По умолчанию параметр выключен, поэтому вызовы без него ведут себя как раньше. Значение - любая булева запись CMake, остальное это `FATAL_ERROR`. Явный вызов `configure_toolchain_runtime_rpath` на той же цели до или после ничего не дублирует. `lumex_configure_target` теперь передает `TOOLCHAIN_RUNTIME_RPATH ON` вместо отдельного вызова; `include(deployment/ToolchainRuntimeRpath)` в `LumexBuild.cmake` остался, чтобы функция была доступна встраивающему проекту. Каталог `/usr/lib/gcc-astra/lib64` Astra Linux считается системным (путь под `/usr/lib*`), поэтому компилятор gcc-astra RUNPATH не получает: это описано в README подмодуля, кода для него нет. Подмодуль поднят на ветку `feat/runpath-automatic-parameter` (еще не отправлена в origin).

**Проверено:** самотесты `CMakeRoutines/tests/target`: GCC 13.2 - 332 проверки, Clang 23.1.0 - 346, GCC 8.3 - 318, все проходят; слой `tests` - 23 теста. Фикстура собирается в Release четырьмя способами (явный вызов, параметр, оба, ничего): у первых трех `fixture_app` и `libfixture_lib.so` содержат каталоги `/opt/gcc-13.2.0/lib64` (и `/opt/llvm-23.1.0/lib/x86_64-unknown-linux-gnu` для libc++) в RUNPATH по одному разу и запускаются без `LD_LIBRARY_PATH`, у четвертого этих каталогов нет. Мутации копии подмодуля (проверены с GCC 8.3, которому нужны только имитирующие случаи): снять вызов, включить параметр по умолчанию, убрать проверку значения, убрать защиту от повтора, зафиксировать `STDLIB libstdc++`, сузить разбор значения - все падают; три мутации `LumexBuild.cmake` (`OFF`, параметр удален, второй явный вызов) падают в `cmake.wiring_toolchain_rpath`. Release-сборка `LumexBase64Example` и `LumexBase64DecodeCxx11Tests` GCC 13.2 и Clang 23.1.0: RUNPATH `/opt/gcc-13.2.0/lib64` и (Clang) `/opt/llvm-23.1.0/lib/x86_64-unknown-linux-gnu:/opt/gcc-13.2.0/lib64`, оба бинарника запускаются без `LD_LIBRARY_PATH`, 33 теста gtest проходят. `ctest -L cmake` без `consumer` - 151 тест проходит на обоих деревьях, пять lint-скриптов проходят.

##### Пример `examples.reflection.LumexFieldReflectionExample`: имена, `get<I>` и `to_json` агрегатов

**Файлы:** `lumex/examples/reflection/example_field_reflection.cpp` (новый), `lumex/examples/reflection/CMakeLists.txt`, `lumex/tests/cmake/cases/wiring_field_reflection_no_leak.cmake`

**Суть:** у field reflection не было примера. `example_field_reflection.cpp` показывает зарегистрированный агрегат (`LUMEX_DEFINE_FIELD_NAMES`), `tuple_size`, `names_as_array`, чтение и запись через `get<I>`, `to_json` вложенного агрегата с вектором агрегатов, пустым и непустым optional и типом со своим `to_json` (его перегрузка имеет приоритет), а с C++20 еще и агрегат без регистрации. Он собирается дважды: `LumexFieldReflectionExample` (C++11) и `LumexFieldReflectionExampleCxx20` (C++20), CTest-имена `examples.reflection.*`; сам проверяет документ и возвращает 1 при расхождении. Пути nlohmann в CMake примера: в дереве при `LUMEX_WITH_FIELD_REFLECTION=ON` через `lumex_setup_nlohmann_json ()` (встроенная копия `3rdparty/nlohmann`), отдельно после установки - через `find_package (nlohmann_json 3)`, без нее пример пропускается; `LUMEX_WITH_FIELD_REFLECTION` задается на самих целях. Два прежних примера по-прежнему собираются из `lumex::reflection` без nlohmann: `cmake.wiring_field_reflection_no_leak` теперь разрешает упоминание nlohmann в CMake примеров только за условием опции и после объявления прежних двух.

**Проверено:** `examples.reflection.LumexFieldReflectionExample` и `...Cxx20` завершаются с кодом 0 на GCC 13.2 (Release, Debug, ASan с UBSan) и Clang 23.1.0 (libstdc++ и libc++); GCC 8.3 собирает оба; синтаксис для MinGW GCC 8.3 проверен на C++11 и `-std=c++2a`.

##### `to_json` агрегата рекурсивно пишет вложенные агрегаты и контейнеры агрегатов

**Файлы:** `lumex/core/reflection/field_reflection/LumexFieldReflection.hpp`, `lumex/tests/core/reflection/field_reflection/` (`LumexFieldReflectionNestedFixtures.hpp`, `LumexFieldNamesJsonNested.cxx11.tests.cpp`, `LumexFieldNamesJsonNested.cxx17.tests.cpp`, `LumexFieldNamesJsonNested.cxx20.tests.cpp` - новые; `CMakeLists.txt`, `LumexFieldReflectionRegisteredFixtures.hpp`), `lumex/tests/cmake/consumer/field_names_compile_checks/` (`check.cpp`, `CMakeLists.txt`, случаи 13 и 14)

**Суть:** поле, тип которого агрегат, nlohmann записывал только через `to_json (nlohmann::json &, Type const &)`, найденный ADL, иначе - ошибка в nlohmann. Теперь значение поля пишется по первому подходящему правилу: пустое optional-подобное поле (`has_value` и унарная `*`) пропускается, непустое пишется значением по тому же правилу; тип, который nlohmann умеет преобразовать (`std::is_constructible<nlohmann::json, T const &>`: числа, строки, перечисления, стандартные контейнеры таких типов и любой тип со своим `to_json`, найденным ADL), преобразует nlohmann - поэтому собственный `to_json` агрегата имеет приоритет над рекурсией; map с ключами, из которых строится `std::string`, это объект, любой другой контейнер и `std::array` - массив, каждый элемент по тому же правилу (пустой optional-подобный элемент это `null`; контейнеры контейнеров агрегатов работают); любой другой класс - вложенный агрегат, его пишет сам `to_json` с его именами (регистрация или, с C++20, компилятор) на любой глубине. Ниже C++20 вложенный агрегат без регистрации дает прежний `static_assert` `to_json` с именем макроса; агрегат со своим `to_json` имен не требует. map с ключами не из строк и значениями-агрегатами дает `static_assert` (случай 14 проверки компиляции). Прежние тесты, где вложенный агрегат записывался через ADL-перегрузку (`RegPoint`, `RegShape`), не менялись: приоритет перегрузки сохранен. Объем: `std::optional` и контейнеры, которые nlohmann и так преобразует, ведут себя как раньше.

**Проверено:** `reflection.field_reflection` на C++11 / 14 / 17 / 20: 111 / 156 / 162 / 261 тест, все проходят на GCC 13.2 (Release, Debug, ASan с UBSan), Clang 23.1.0 (libstdc++ и libc++); GCC 8.3 без ошибок (тесты имен компилятора пропускаются). Компиляция новых тестов с `-S` на C++20: 5-13 МБ ассемблера, 3 с на единицу. Новых тестов: `LumexFieldNamesJsonNested` на каждом стандарте (вложенные уровни, контейнеры, map, контейнеры контейнеров, optional с агрегатами, пустой вложенный агрегат, приоритет `to_json`, агрегат со своим `to_json` без имен; C++17: `std::optional`, `std::unordered_map`; C++20: имена компилятора на каждом уровне). `cmake.field_names_compile_checks`: случаи 13 (вложенный агрегат без имен ниже C++20) и 14 (map агрегатов с ключами не из строк) отвергаются со своим сообщением. Мутации (все пойманы): вложенный агрегат дает пустой объект, правило агрегата стоит перед правилом ADL (приоритет `to_json` теряется), пустой optional-элемент не `null`, map пишется массивом, элементы контейнера не обходятся, значение optional-поля не обходится. Предупреждения: `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast` на новых тестах и примере (GCC 8 и Clang 23 на C++11, GCC 13 на C++14, 17, 20, MinGW): без диагностик; `create_release.sh` для GCC 8.3, GCC 13.2, Clang 23.1.0 и MinGW на C++11 и C++20: `warns/` пуст.
##### `lumex_add_standard_suites (... STANDARDS <std>...)`: свой список стандартов для каталога тестов

**Файлы:** `cmake/LumexGoogleTest.cmake`, `lumex/tests/LumexTestStandards.cmake` (функция `lumex_test_standards_resolve`, раздел "Narrowing the row of one directory"), `lumex/tests/cmake/cases/wiring_standard_suites.cmake`

**Суть:** каталог тестов строил весь ряд стандартов своего модуля (у `utility` - 11, 14, 17, 20, 23, 26), даже если ни его тесты, ни проверяемый код не зависят от стандарта. Ключевое слово `STANDARDS` (например, `STANDARDS 11 17 20`) сужает ряд для одного каталога. Функция `lumex_test_standards_resolve` требует: строго возрастающий список известных стандартов, каждый из них - стандарт модуля, список начинается с низшего стандарта модуля, содержит стандарт каждого тестового файла каталога (иначе файл тихо не собирался бы), не совпадает с рядом модуля (тогда ключ лишний) и не используется вместе с `VARIANT`. Набор стандарта по-прежнему собирает файлы своего и всех нижних стандартов. `cmake.wiring_standard_suites` проверяет функцию (6 случаев отказа с текстом ошибки и 5 успешных) и закрепляет вызов в хелпере; для каждого каталога с `STANDARDS` он ищет в тестах каталога и в проверяемых им исходниках упоминание отброшенного стандарта (порог `__cplusplus`, макрос возможности, имя файла `cxx14` и подобное) и отказывает, если находит.

**Проверено:** `cmake.wiring_standard_suites` проходит на GCC 13.2, GCC 8.3 и Clang 23; мутации (не проверять принадлежность ряду, пропуск файла отброшенного стандарта, список равен ряду, функция игнорирует список, хелпер не передает `STANDARDS`, добавленный в исходник порог `__cplusplus >= 201402L` или `> 202302L` в каталоге без 14 и 23) - все пойманы.

##### Тесты `LUMEX_MATH_CONSTANTS_*`: цифры, `float` / `double` / `long double`, тождества, макро-формы, `std::numbers`, проверки компиляции

**Файлы:** `lumex/tests/core/math/constants/` (новый: `LumexMathConstantsDigits.cxx11.tests.cpp`, `LumexMathConstantsValues.cxx11.tests.cpp`, `LumexMathConstantsRelations.cxx11.tests.cpp`, `LumexMathConstantsMacros.cxx11.tests.cpp`, `LumexMathConstantsSpecialFunctions.cxx17.tests.cpp`, `LumexMathConstantsStdNumbers.cxx20.tests.cpp`, `LumexMathConstantsReference.hpp`, `LumexMathConstantsSupport.hpp`, `LumexMathConstantsReference.py`, `CMakeLists.txt`), `lumex/tests/core/math/CMakeLists.txt`, `lumex/tests/cmake/consumer/math_constants_compile_checks/` (новый: `CMakeLists.txt`, `check.cpp`), `lumex/tests/cmake/cases/wiring_math_constants.cmake` (новый), `lumex/tests/cmake/CMakeLists.txt`

**Суть:** пункт 86а списка дел: у `lumex/core/math/constants` тестов не было. Константы - макросы `LUMEX_MATH_CONSTANTS_*`, раскрывающиеся в литералы `double`, поэтому переменных и ODR-использования нет (у значения нет адреса, оно привязывается к `const`-ссылке как временный объект); во втором блоке единиц трансляции нет смысла. Эталон считается вне библиотеки: `LumexMathConstantsReference.py` (Python 3.7, модуль `decimal`, 140 знаков, формулы без заголовка: Мэчин и Гаусс-Лежандр для pi, ряд для e, Эйлер-Маклорен для постоянной Эйлера-Маскерони, ряд Апери, быстрый ряд Каталана, AGM для константы лемнискаты, ряд Стирлинга для Gamma(1/3) и Gamma(1/4) с перекрестными проверками) печатает таблицу `LUMEX_TEST_MATH_CONSTANTS_REFERENCE` в `LumexMathConstantsReference.hpp`: 60 знаков истинного значения и правильно округленные `float`, `double` и 64-битный `long double` как мантисса * 2^порядок. Точные физические константы - определения СИ 2019 и CGPM 1901; постоянная Брауна - опубликованные 12 знаков 1.902160583104 (OEIS A065421), пересчитать ее здесь нельзя.

Digits: текст раскрытого макроса (`#x` через два уровня) - один десятичный литерал без суффикса, знака и скобок, знаков не меньше заявленных (50, у Брауна 12) и каждый знак равен усеченному или округленному в последнем знаке эталону; тест сам проверяет, что умеет падать. Values: `static_assert` для каждого макроса на каждом стандарте набора (тип `double`, не ссылка, ближайший `double`, ближайший `float` и из `double`, и из литерала `F`, ближайший x87 `long double` из литерала `L`), использование в `constexpr`-функции, границе массива, аргументе шаблона; во время выполнения те же сравнения, привязка к `const`-ссылке и то, что `long double` из самого макроса имеет точность `double` (знаки для `long double` дает только вклеенный суффикс `L`: `LUMEX_TEST_LONG_DOUBLE_LITERAL`). Relations: для `float`, `double` и `long double` (константы с суффиксом типа) корни, обратные величины, золотое и серебряное сечения, пластическое число, `exp` / `log`, ряд для e, тригонометрия и pi (арктангенсы, Мэчин, `cos (pi/5) = phi/2`), pi/180, лемниската двумя путями (AGM и Gamma), `std::tgamma`, ряды Апери, Каталана и Эйлера-Маскерони, нижняя граница постоянной Брауна по решету до 10^6, допуск в ulp. Macros: заголовок включается дважды и через `LumexMath`, все 25 имен определены, нескольких имен нет, имя охранного макроса. `.cxx17`: `std::riemann_zeta`, `std::comp_ellint_1`, `std::beta` (до 4 ulp у libstdc++, допуск 16; без специальных функций - `GTEST_SKIP`). `.cxx20`: дифференциал с `std::numbers` (в libc++ 23 константы `long double` заданы литералом `double`: такое значение принимается) и `consteval` / `constinit`. Проверки компиляции `cmake.math_constants_compile_checks`: 9 отвергаемых использований (адрес, ссылка без `const`, присваивание, `%`, сужение в фигурных скобках, значение в `#if`, суффикс `LL`, `float`- и `long double`-литерал против `double`-макроса) на C++11, 17, 20. `cmake.wiring_math_constants` сверяет макросы заголовка со строками таблицы: новая константа без эталона не пройдет.

Что показали тесты: четыре константы с неверным хвостом цифр (отдельная запись об исправлении); макрос назван `LUMEX_MATH_CONSTANTS_LEMMISCATE` (в слове lemniscate лишняя буква m; правило потребителя пишет `LEMNISCATE`) - не менялось, переименование решает пользователь; `LUMEX_MATH_CONSTANTS_PLANCK_CONSTANT` (6.62607015) и `LUMEX_MATH_CONSTANTS_AVOGADRO_CONSTANT` (6.02214076) несут только мантиссу без 1e-34 и 1e23, тест фиксирует это как есть.

**Проверено:** GCC 13.2 Release: `math.constants.` 58 тестов на C++11, 67 на C++17, 72 на C++20, 0 упало; GCC 8.3 (C++11, C++17) `math.` и три cmake-случая: 179 из 179; Clang 23.1 с libc++ (C++11, 17, 20): 239 из 239 (6 `Skipped`: libc++ без специальных функций), с libstdc++ 13: 197 из 197; ASan и UBSan (GCC 13.2 Debug, C++11, 17, 20): 197 из 197 без замечаний. Под `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast` без замечаний: GCC 8.3 (C++11), GCC 13.2 (C++11, 14, 17, 20), Clang 23 (C++11, 17, 20, обе библиотеки), MinGW (C++11, 17, 2a). Опровержение мутациями (30 мутантов, все пойманы): 16 в заголовке (цифра внутри и за пределами `double`, суффикс `F` и `L`, не константное выражение, переменная вместо литерала, порядок 1e-34, скобки, целое число, обмен двух констант, снятый макрос, старые неверные хвосты) и 14 в тестах (мантиссы эталона, `round_to`, `truncate_to`, допуск в ulp, тождества, `std::numbers`, `consteval`); живой мутант один и равнозначный - последний (60-й) знак эталона, который никто не сравнивает. Девять отвергаемых случаев проверки компиляции падают по своей причине на GCC 13 (C++11, 17, 20) и Clang 23; мутация заголовка (адрес берется) роняет `cmake.math_constants_compile_checks`. Не проверено: MSVC и clang-cl (склейка `3.14...F`, `long double` равен `double`), aarch64 (`long double` на 113 бит: тест значений пропускается, тождества идут с допуском).

##### `ranges::iterator_range::size ()` для итераторов с произвольным доступом

**Файлы:** `lumex/core/utility/ranges/LumexIteratorRange.hpp`, `lumex/tests/core/utility/ranges/LumexIteratorRangeSize.cxx11.tests.cpp` (новый)

**Суть:** у `iterator_range` были `begin ()`, `end ()` и `empty ()`; число элементов можно было получить только через `std::distance`. Теперь `size ()` возвращает `std::size_t` (`end () - begin ()`), но только если `std::iterator_traits<Iterator>::iterator_category` - `std::random_access_iterator_tag` или производный от него тег (указатели, итераторы `std::vector`, `std::deque`, `std::string`, `std::array`, обратные итераторы, свои итераторы с таким тегом). Для `std::list`, `std::set`, `std::map`, `std::forward_list`, одноразовых итераторов и итераторов без `iterator_traits` члена нет (подсчет обошел бы последовательность): ограничение - аргумент по умолчанию шаблона члена, то есть отказ подстановки члена, а не ошибка класса, поэтому диапазон по итератору без признаков остается пригодным. Нужен диапазон `memory_filters_range_t` генератора дампов ниже C++20, чтобы его `size ()` совпадал с `size ()` у `std::ranges::ref_view` (C++20). Заголовок теперь включает `<cstddef>`, `<iterator>` и `<type_traits>`.

**Проверено:** GCC 13.2 Release, `utility.ranges.`: 96 тестов на C++11 (было 86) и 107 на C++20 (было 97), 10 новых `IteratorRangeSizeTest`; GCC 8.3 и Clang 23 (C++11) - 96, ASan/UBSan Debug - 96. Новый файл чисто собирается под `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast -Werror` на GCC 8.3, GCC 13.2 и Clang 23 (C++11, 17, 20, Clang и с libc++). 2 мутации (тег `forward_iterator_tag` вместо произвольного доступа - у `std::list` появляется `size ()`; разность наоборот), обе пойманы.

##### Многопоточные тесты `atomic_shared_ptr` и `atomic_weak_ptr`: ABA, линеаризуемость, живость, счет ссылок; общие помощники `lumex/tests/support/`

**Файлы:** `lumex/tests/support/` (новые: `LumexTestConfig.hpp`, `LumexTestSchedule.hpp`, `LumexTestThreads.hpp`, `LumexTestReusePool.hpp`, `LumexTestLedger.hpp`, `LumexTestHistory.hpp`, `LumexTestSubprocess.hpp`, `LumexTestReplay.hpp`, `LumexTestSupport.cxx11.tests.cpp`, `LumexTestSupportNoFork.cxx11.tests.cpp`, `CMakeLists.txt`), `lumex/tests/core/atomic/` (`LumexAtomicTestSupport.hpp`; новые: `LumexAtomicTestEngines.hpp`, `LumexAtomicScenarios.hpp`, `LumexAtomicScenariosWeak.hpp`, `LumexAtomicScenariosLifecycle.hpp`, `LumexAtomicScenariosStructures.hpp`, `LumexAtomicScenariosGap.hpp`, `LumexAtomicScenarioBundle.hpp`; `CMakeLists.txt` трех каталогов), `lumex/tests/core/atomic/smart_ptr/` (новые: `LumexAtomicSmartPtrAba.cxx11/17/20.tests.cpp`, `...Linearizability.cxx11...`, `...Structures.cxx11...`, `...Lifecycle.cxx11...`, `...Progress.cxx11...`, `...WaitStress.cxx11...`, `...Falsification.cxx11...`, `...Soak.cxx11...`), `cmake/LumexGoogleTest.cmake` (ключ `SOAK_FILTER`), `cmake/LumexOptions.cmake` (`LUMEX_BUILD_SOAK_TESTS`), `lumex/tests/CMakeLists.txt`, `lumex/core/atomic/README.md`

**Суть:** пункт 110 todo: ABA для нового lock-free движка и для нынешнего - обязательный сценарий, а не случайный стресс. Библиотека не менялась. Тесты написаны против одного псевдонима движка: `LumexAtomicTestSupport.hpp` объявляет `atomic_shared_ptr` / `atomic_weak_ptr` как шаблоны-псевдонимы, цель которых задают макросы `LUMEX_ATOMIC_TEST_SHARED_ENGINE` / `LUMEX_ATOMIC_TEST_WEAK_ENGINE` (по умолчанию общие имена); движок, который уничтожает замененное значение позже вызова, объявляется `LUMEX_ATOMIC_TEST_ENGINE_DEFERS_DESTRUCTION=1` и дает `LUMEX_ATOMIC_TEST_ENGINE_QUIESCE ()`. Новый движок (`atomic_shared_ptr_lock_free`, `_std_backed`) подключается одним `VARIANT` в `CMakeLists.txt` и строкой в `LumexTestStandards.cmake`, тела тестов не меняются. Каждая проверка - шаблон над описателем движка, пишет в `lumex_test::Verdict`, поэтому один код служит и тесту "ноль нарушений", и самопроверке на сломанном движке.

Что проверяется (группы в `smart_ptr/`): `Aba` - адрес освобожденного узла (объект и управляющий блок) возвращается следующему узлу сразу (`lumex_test::ReusePool`, LIFO, с ASan-отравлением), "хэндл" только с этим адресом не равен новому узлу; `A -> B -> A` того же владельца совпадает, равный по содержимому новый объект нет; все тройки (текущее, ожидаемое, желаемое) шести значений с алиасингом (тот же указатель под другим владельцем, один владелец с другим указателем, пустое) для strong, weak и `exchange`; остановленный держатель `expected` при `A -> B -> A` с 0..5 лишними `store` того же адреса; `weak`-владелец по адресу объекта при закрепленном управляющем блоке; `weak.lock ()` против последнего сильного освобождения (два порядка публикации), монотонность `expired ()`; кольцо значений: успешные `exchange`/CAS образуют один путь (баланс входящих и выходящих ребер, не требует истории). `Linearizability` - поиск Вонга-Гоулда на коротких историях (регистр с exchange и CAS, стек, очередь, множество; 2 и 3 потока, 3-5 операций), счетчик из CAS-циклов, перестановка токенов `exchange`, монотонные загрузки, точность strong CAS (отказ только при неравных значениях, число повторов ограничено чужими успехами). `Structures` - стек Трейбера, очередь Майкла-Скотта и список Харриса-Майкла из `shared_ptr`-узлов (мультимножество, сумма, FIFO по производителю, чистые приращения по ключам, леджер конструкторов/деструкторов, баланс аллокатора) и принудительное чередование ABA: тот же стек над сырыми `std::atomic<Node*>` с немедленным повторным использованием адреса ДОЛЖЕН быть пойман (безопасно: память пула не возвращается системе, узлы тривиальны, порча читается по полю состояния), стек с тегом версии и стек из `shared_ptr` проходят то же расписание. `Lifecycle` - освобождение замененного значения, баланс `use_count` при self-assignment и удержанных копиях, одно значение в 7 атомиках, потоки, завершающиеся с ссылкой в `thread_local`, удалители, вызывающие любую операцию того же атомика (вложенно, из многих потоков), удалитель, припаркованный на защелке, пока остальные потоки работают, атомики статической длительности (проверка в `atexit`). `Progress` - загрузчики против CAS-свопперов за фиксированное время с минимумом успехов на каждого (регрессия ливлока libc++). `WaitStress` - кольцо токена с `notify_all`/`notify_one`, 100 ожидающих на 100 атомиках (больше 64 полос таблицы), потерянные пробуждения. `Falsification` - те же проверки на испорченных копиях (см. ниже). `Soak` - длительные прогоны.

Разнообразие расписаний: числа потоков `LUMEX_TEST_THREADS` (по умолчанию 1, 2, 4, 8 и один больше числа ядер), четыре вида расписания (плотный цикл, уступки, случайные сны, холодный кэш), общий барьер старта, зерно `LUMEX_TEST_SEED` (по умолчанию 20261008; сбой печатает строку повтора с зерном и числом потоков), масштаб `LUMEX_TEST_SCALE`; в сборках с санитайзерами объем делится (`slowdown_divisor ()`). Сторожевой таймер `LUMEX_TEST_WATCHDOG_SECONDS`.

Самопроверка (мутационное тестирование кода тестов, библиотека не мутируется): `LumexAtomicTestEngines.hpp` содержит исправный эталон на мьютексе, движок-коробку без освобождения (форма будущего lock-free движка с поздним уничтожением; все проверки должны его принимать) и испорченные копии: `broken_cas_split_lock`, `broken_cas_unlocked`, `broken_cas_pointer_only`, `broken_cas_owner_only`, `broken_cas_content_equal`, `broken_strong_fails_spuriously`, `broken_exchange_split`, `broken_store_keeps_old`, `broken_store_leaks_forever`, `broken_stale_load`, наивный lock-free "ящик", освобождающий сразу (жертва ABA; детерминированно ловится остановкой в зазоре между сравнением и CAS), и ящик с relaxed-публикацией (видно только ThreadSanitizer). Небезопасные по построению (CAS без блокировки, наивный ящик под нагрузкой, настоящая утечка) запускаются в дочернем процессе `fork`: падение, сигнал, отчет санитайзера или нарушение считаются поимкой; без `fork` (Windows) такие тесты пропускаются, а ветка без `fork` собирается и проверяется всюду (`LUMEX_TEST_FORCE_NO_FORK`).

Общие помощники для других модулей (`hazard_pointer`): `lumex/tests/support/`, пространство `lumex_test`, без зависимости от библиотеки: `LumexTestConfig.hpp` (переменные окружения, зерно, `thread_counts`, `scaled`, `slowdown_divisor`, определение санитайзеров), `LumexTestSchedule.hpp` (`SeededRandom`, `Schedule`, `ScheduleKind`), `LumexTestThreads.hpp` (`StartBarrier`, `Gate`, `StallPoint`, `Rendezvous`, `run_threads`, `TestWatchdog`, `Verdict`), `LumexTestReusePool.hpp` (`ReusePool`, `ReuseAllocator`, `make_pooled`), `LumexTestLedger.hpp` (`ObjectLedger`, `LedgerEntry`), `LumexTestHistory.hpp` (`HistoryRecorder`, `is_linearizable`, модели регистра, стека, очереди, множества), `LumexTestSubprocess.hpp` (`run_in_child`), `LumexTestReplay.hpp`. У них свои тесты (CTest `support.`, три стандарта и вариант без `fork`).

Метки и запуск: у всех тестов `atomic.` метка `tsan` (`ctest --test-dir <дерево с -DLUMEX_USE_TSAN=ON> -L tsan`; GCC - только под `setarch x86_64 -R`); тесты `*Soak*` не входят в обычную регистрацию, при `-DLUMEX_BUILD_SOAK_TESTS=ON` (CMake 3.22 и новее) регистрируются под меткой `soak` с `LUMEX_TEST_SOAK=1` (`ctest -L soak`; длина - `LUMEX_TEST_SOAK_SECONDS`, по умолчанию 20 с на набор), либо запускаются бинарником напрямую с `LUMEX_TEST_SOAK=1`. Старые переменные `LUMEX_ATOMIC_STRESS_*` работают; `LUMEX_TEST_SEED`, `LUMEX_TEST_THREADS`, `LUMEX_TEST_SCALE` имеют приоритет.

**Проверено:** GCC 13.2 Release, `atomic.` и `support.` на C++11, 17, 20 (с вариантами `lock_based`, `wait_table`): 2694 теста, 0 упало (13 с на `ctest -j4`); в каждом наборе `smart_ptr/` добавлено 57 тестов на C++11, 59 на C++17 и в трех наборах C++20 по 59 (`Soak` не в обычной регистрации), у `support.` 160 тестов. Добавленное время по `ctest -j1` (каждый тест - отдельный процесс): около 3,0 с на набор C++11 и C++17, 10,6 с на три набора C++20. GCC 8.3 (C++11, `-std=c++2a`), Clang 23.1 с libc++ и с libstdc++ (C++11, 20): по 2138 тестов, 0 упало; GCC 13 Debug с ASan и UBSan: 2138, 0 упало (35 с). ThreadSanitizer: GCC 13 под `setarch x86_64 -R` и Clang 23 с libstdc++ - по 2138, 0 упало (107 и 133 с); Clang 23 с libc++ дает ложные срабатывания внутри `__shared_weak_count::__release_shared` (в libc++ нет аннотаций TSan для освобождения управляющего блока): падают и прежние тесты `LumexAtomicSmartPtrConcurrencyTest.GivenSeededMixedOperations...`, `...GivenExpiringTargets...`, поэтому TSan на Clang запускать с `-DLUMEX_CLANG_STDLIB=DEFAULT`. Предупреждения `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast` на новых тестах: 0 (GCC 8 и Clang 23 на C++11 и 20, GCC 13 на C++14, 17, 20); MinGW `-fsyntax-only` (C++11, 17, 2a): 0. Дефектов библиотеки не найдено. Мутации: 15 мутаций помощников `support/` пойманы все (две непойманные при первом прогоне - эквивалентный `balanced ()` и сравнение времени холодного расписания - заменены точными проверками); 8 мутаций КОПИИ `lock_based_cell` (чтение, запись и CAS без блокировки, утечка в `store`, разрезанные `exchange` и CAS, сравнение только по указателю или только по владельцу, `expected` не обновляется) пойманы все, контроль без мутации проходит; 12 испорченных движков (включая относительную публикацию, видимую только TSan) ловятся (в сводной таблице проверка x движок каждая из 23 проверок поймана хотя бы одним движком поймана хотя бы одним движком, наивный "ящик" - детерминированно остановкой в зазоре). Принудительное чередование ABA: стек из сырых указателей порчен (`head` указывает на освобожденный узел, значение извлечено дважды), стеки с тегом и из `shared_ptr` проходят. `lint.*` (6, с `lint.headers_standalone`) и `cmake.*` (149, с `cmake.wiring_standard_suites`) проходят; `-DLUMEX_BUILD_SOAK_TESTS=ON` регистрирует 10 тестов `*Soak*` под меткой `soak` (`ctest -L soak` проходит). Пока нет: запуск на MSVC и lock-free движка (подключается одним `VARIANT`).

##### Модуль `core/hazard_pointer`: `hazard_pointer`, `hazard_pointer_obj_base` и пакетные функции с C++11

**Файлы:** `lumex/core/hazard_pointer/` (новый: `LumexHazardPointer`, `README.md`, `CMakeLists.txt`, `base/LumexHazardPointerObjBase.hpp`, `holder/LumexHazardPointerHolder.hpp`, `holder/LumexHazardPointerBatch.hpp`, `engine/LumexHazardPointerEngine.hpp`, `engine/LumexHazardPointerDomain.cpp`), `lumex/LumexExport.hpp` (`LUMEX_HAZARD_POINTER_API`), `lumex/core/CMakeLists.txt`, `CMakeLists.txt`, `cmake/LumexOptions.cmake` (`LUMEX_BUILD_HAZARD_POINTER`, `LUMEX_BUILD_SOAK_TESTS`), `cmake/LumexModules.cmake`, `cmake/LumexLibConfig.cmake.in`, `conanfile.py`, `Doxyfile.in`, `THIRD-PARTY-NOTICES.md`, `lumex/examples/hazard_pointer/` (новый), `lumex/tests/core/hazard_pointer/` (новый: `base/`, `holder/`, `engine/`, `LumexHazardPointerTestStructures.hpp`, `LumexHazardPointerTestStress.hpp`), `lumex/tests/cmake/consumer/hazard_pointer_{hooks,two_modules,static_destruction,compile_checks,soak}/` (новые), `lumex/tests/cmake/cases/{wiring_hazard_pointer,wiring_hazard_pointer_export,require_fail_hazard_pointer_without_utility}.cmake` (новые), `lumex/tests/cmake/consumer/run_consumer.cmake` (`RUN_PREFIX`), `lumex/tests/LumexTestStandards.cmake`, `benchmarks/hazard_pointer/` (новый)

**Суть:** новая скомпилированная библиотека `lumex::hazard_pointer` (`LumexCore_hazard_pointer`, опция `LUMEX_BUILD_HAZARD_POINTER`, по умолчанию ON, зависит от `utility` и `span`, собирается как C++11) с интерфейсом `<hazard_pointer>` из C++26 ([saferecl.hp], P2530R3, пакетные функции P3428R4) в `lumex::core::hazard_pointer`: `hazard_pointer`, `hazard_pointer_obj_base<T, D>`, `make_hazard_pointer`, `swap`, `make_hazard_pointer_batch`, `clear_hazard_pointer_batch`; расширения - `clean_up ()` (проход освобождения сейчас) и перегрузки `try_protect` / `protect` с вызываемым источником и фильтром (теги в указателе, перезагрузка `seq_cst`). Классы собственные на любом стандарте и компиляторе: алиасов на `std::hazard_pointer` нет (решение пользователя 2026-10-09), форма API стандартная. Код написан заново по идеям Folly, pull request libc++ 218218 и libcds, ничего не скопировано (абзац в `THIRD-PARTY-NOTICES.md`).

Движок: один процессный домен, инициализируемый константами и не разрушаемый, поэтому он работает из статических конструкторов и деструкторов. Слоты - записи по линии кэша в блоках, которые не освобождаются; свободный слот лежит в кэше потока (8 слотов, без атомарных операций) или в общем пуле (lock-free стек, бит 0 слова головы блокирует выборки: нет ABA самого пула). Кэш доступен через тривиально разрушаемый `thread_local`, при выходе потока его возвращает pthread key (FLS на Windows, модуль там закрепляется `GetModuleHandleEx`); поток после хука использует пул напрямую. Retired-узлы лежат в 8 lock-free списках; проход (один за раз) забирает все, ставит `seq_cst` барьер, читает все слоты в хеш-множество (при нехватке памяти - сопоставление по слотам, `retire` остается `noexcept`), освобождает незащищенное, защищенное кладет обратно; запускается потоком, чей `retire` достиг `max (1000, 2 * слотов)` ожидающих узлов или спустя 2 с, деструкторы могут вызывать `retire` и использовать hazard pointer. Читатель: release-запись слота, `engine::reader_fence ()` (единственное место выбора барьера), перезагрузка `acquire`; слот хранит адрес узла базы, поэтому множественное наследование работает. Экспортируются только шесть свободных функций (`LUMEX_HAZARD_POINTER_API`), подписи не зависят от стандарта; ELF-библиотека линкуется с `-z nodelete`. Граница числа неосвобожденных объектов и остальное - в `README.md` модуля.

Тесты: наборы на C++11, 14, 17 и 20 по каталогам исходников (`base/`, `holder/`, `engine/`): все члены и `noexcept`, признак `is_hazard_protectable`, делетеры, смещение базы, копии и присваивания retired-объектов, пул и кэш потока, выход потока, пороги прохода. ABA-тесты на аллокаторе, который тут же возвращает освобожденный адрес: стек Трайбера, очередь Майкла-Скотта и список Майкла, каждые с защитой и без нее; сценарий вызывается из хука внутри окна ABA на том же потоке, поэтому детерминирован: защищенная структура остается верной, а тот же сценарий портит незащищенную (доказательство, что тест видит ABA). Стресс-тесты с отравленной нагрузкой и воспроизводимым сидом (`LUMEX_HP_SEED`), 1, 2, 4, 8 и 2xядер потоков; заведомо неверная реализация без барьера (`Naive`). Отдельные проекты-фикстуры: движок с хуками для детерминированной гонки Декера (читатель объявил и ждет, проход прочитал слоты и ждет, `clean_up` ждет идущий проход, пакет без памяти не меняет ничего), две разделяемые библиотеки на одном домене, статические объекты и `thread_local`-деструкторы, 22 отвергаемых использования. Долгие прогоны вне обычного запуска: `-DLUMEX_BUILD_SOAK_TESTS=ON` добавляет `ctest -L soak` и `ctest -L tsan`. Бенчмарк `benchmarks/hazard_pointer/` (отношение к `std::atomic<uint64_t>`, сокращенный прогон в `results/`).

**Проверено:** GCC 13.2 Release: наборы модуля 83 теста на C++11 и C++14, 85 на C++17, 88 на C++20, вместе с `cmake.*`, `lint.*` и примерами 510 из 510; GCC 8.3 (C++11) 83; Clang 23.1.0 с libc++ и с libstdc++ 13 (C++11 и C++17) 83 и 85; ASan и UBSan (GCC 13.2, C++11 и C++20) без замечаний; TSan с Clang (C++11, C++17, плюс `cmake.hazard_pointer_soak` 60 с и `cmake.hazard_pointer_tsan` 20 с) и с GCC 13.2 через `setarch x86_64 -R` (C++11) без замечаний (тесты с незащищенными структурами под TSan пропускаются: они гоняют данные намеренно). Матрица `create_release.sh` (GCC 8.3, GCC 13.2, Clang 23.1, MinGW; C++11 и C++20): 8 сборок, 0 предупреждений, `warns/` пуст; компиляция библиотеки, тестов, примеров и фикстур строгими флагами (`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast`) на GCC 8 и Clang 23 (C++11) и GCC 13.2 (C++14, 17, 20) без замечаний, MinGW компилирует все (запуск на Windows не проверялся: FLS-хук и закрепление DLL остаются непроверенными). Doxygen 1.8.20: ни одного замечания по файлам модуля. Сценарии ABA на GCC 13.2: незащищенный стек, очередь и список портятся, защищенные остаются верными; заведомо неверная реализация нашла использование после освобождения за 20 мс. Мутации: 18 из 20 пойманы (пул пропускает последний слот блока, самый новый блок, первый слот в линейном сопоставлении; адреса сравниваются по младшим 32 битам с обеих сторон; `retire` освобождает сразу; `release_slot` не очищает слот; кэш потока не возвращается; проход теряет защищенный остаток; проход не уменьшает счетчик; `clean_up` без блокировки прохода; `acquire_slots` не возвращает слоты при ошибке; у `try_protect` нет барьера читателя; `swap` прекращает защиту; копия объекта копирует отметку retire; проверка всегда успешна; слот хранит адрес объекта, а не узла), выжили две, где убран сам `seq_cst` барьер в проходе и в `reader_fence ()`: гонка на x86 не воспроизводится детерминированно, порядок вызова барьера проверяет фикстура с хуками. Бенчмарк (сокращенный прогон, GCC 13.2, 1 поток): `protect` + сброс 5.9 нс против 1.6 нс у обычной загрузки, 14.8 нс у блокировки `shared_mutex`, 24.7 нс у `std::atomic_load` над `shared_ptr`; `retire` с проходами 45 нс против 9 нс у `new` + `delete`, хвост 11 мкс на 99.9-м процентиле.

##### Тесты `expected`: наборы C++14 и C++23, свойства специальных функций, `constexpr` по стандартам, порядок операций, сравнение с `std::expected`

**Файлы:** `lumex/tests/LumexTestStandards.cmake` (строка `expected` теперь `11 14 17 20 23`), `lumex/tests/core/expected/result/CMakeLists.txt`, `lumex/tests/core/expected/error/CMakeLists.txt` (комментарии и новые вспомогательные заголовки), новые: `lumex/tests/core/expected/ExpectedMembersSupport.hpp`, `lumex/tests/core/expected/ExpectedProbe.hpp`, в `lumex/tests/core/expected/result/`: `ExpectedMembers.cxx11.tests.cpp`, `ExpectedReinit.cxx11.tests.cpp`, `ExpectedStandardBehavior.cxx11.tests.cpp`, `ExpectedTags.cxx11.tests.cpp`, `ExpectedConstexpr.cxx11.tests.cpp`, `ExpectedConstexpr.cxx14.tests.cpp`, `ExpectedConstexpr.cxx17.tests.cpp`, `ExpectedConstexpr.cxx20.tests.cpp`, `ExpectedStdDifferential.cxx23.tests.cpp`

**Суть:** пункт 89 (в) списка дел и проверки к предыдущей записи.

1. Строка `expected` таблицы стандартов получила 14 (в C++14 `constexpr`-функция может менять объекты и иметь несколько операторов, и часть возможностей `expected` появляется именно там) и 23. Наборы `LumexExpectedResultCxx14Tests`, `LumexExpectedResultCxx23Tests` и такие же `LumexExpectedError*` собирают те же `*.cxx11.tests.cpp` на новых стандартах.
2. `ExpectedMembers` (28 тестов): условия стандарта для конструкторов, присваиваний, деструктора и `swap` записаны в `ExpectedMembersSupport.hpp` по тексту разделов, а не по реализации, и проверяются на 154 парах типов (14 типов значения на 11 типов ошибки: `int`, `const int`, тривиальная структура, `noexcept`-тип, бросающее перемещение, тип без объявленного перемещения, тип с удаленным перемещением, тип без копирования и перемещения, тип без присваивания, тип с тривиальным копированием и нетривиальным деструктором, `std::string`, `std::unique_ptr<int>`) и на 11 типах ошибки для `expected<void, E>`; несовпадение называет пару типов.
3. `ExpectedReinit` (34 теста): зонд `probe_t` пишет в журнал каждое конструирование, присваивание, `swap` и уничтожение и может бросить из заданной операции; тесты сверяют с текстом стандарта всю последовательность для трех веток `reinit-expected`, таблицы 72 и 73, `emplace`, и что объект сохраняет прежнее содержимое, если конструирование бросило.
4. `ExpectedConstexpr` на C++11, 14, 17, 20: `static_assert` на то, что работает в константном выражении на каждом стандарте (таблица в предыдущей записи); тесты `TEST` повторяют вызовы во время выполнения и сравнивают результаты; файл C++20 пропускается, если у компилятора нет `constexpr`-времени жизни члена union (GCC 8 с `-std=c++2a`).
5. `ExpectedStandardBehavior` (9 тестов): `has_error`, ограничение `emplace`, тип значения с перегруженным `operator&`, `expected<const T, E>`, явное `operator bool`; `ExpectedTags` (3 теста): теги.
6. `ExpectedStdDifferential` (22 теста, набор C++23, защита `__cpp_lib_expected`; без `std::expected` один пропущенный тест): один и тот же код выполняется над `std::expected` и над `expected` и ответы сравниваются: признаки типов специальных функций на 154 парах и 11 типах `void`, что принимают конструкторы, преобразующие конструкторы (15 случаев), `noexcept` наблюдателей и модификаторов, порядок операций на зонде (присваивания, `swap`, `emplace`, конструкторы, `value_or`, `error_or`, монадические операции в четырех категориях объекта) для 9 пар типов зонда и 3 типов ошибки `void` с отказом в каждой из 10 операций, типы результатов наблюдателей, сравнения (все пары состояний и допустимость `==`), тип и категория исключения `value ()`, `bad_expected_access`, move-only содержимое, теги. Допустимые расхождения (стандарт оставляет свободу) названы в тесте: `noexcept` может быть строже. Три отличия libstdc++ 13 записаны явно (см. предыдущую запись).

**Проверено:** число тестов и компиляторы - в записи выше; мутации - там же. Дифференциальный тест на GCC 13.2 (libstdc++ 13) нашел три отличия библиотеки (LWG 4026, LWG 3836, условия Constraints у `==`), они записаны в тесте и срабатывают только на библиотеке без исправления.

##### `stringify_v2` работает с C++11

**Файлы:** `lumex/core/string/utility/LumexStringify.hpp`, `lumex/tests/core/string/utility/` (`LumexStringifyV2.cxx11.tests.cpp` - новый, `LumexString.cxx20.tests.cpp`, `CMakeLists.txt`)

**Суть:** `stringify_v2` существовала только при `LUMEX_HAS_CONCEPTS` (ограничение `requires` над концептом `AllStringifiable`). Теперь она объявлена с C++11, возвращает тот же текст, что и `stringify` (тело вызывает `stringify` с полным именем, чтобы ADL не подхватила чужую функцию), а ограничение - одна форма `std::enable_if` над условием `detail::are_stringifiable<Args...>`: с C++20 это концепт `AllStringifiable`, тот же, что ограничивает `stringify`, ниже - признак `traits::stream::all_streamable`, то есть то же, что проверяет `static_assert` в `stringify`. Отличие от `stringify`: аргумент, который нельзя вывести в поток, не находит перегрузки во всех стандартах (у `stringify` ниже C++20 это `static_assert` в теле), так что вызов обнаруживается SFINAE. `stringify` не менялась. C++20-частей, которые пришлось бы оставить за защитой, не осталось (`std::format` не нужен).

Найдено при сравнении признака с концептом: `traits::stream::is_streamable<wchar_t>` истинно по явной специализации (тест `LumexStreamTraits` требует этого на всех стандартах), а с C++20 `operator<<` узкого потока для `wchar_t` в стандартной библиотеке удален (libstdc++ 13): концепт `AllStringifiable<wchar_t>` ложен, а `all_streamable<wchar_t>` истинно. Поэтому условие `stringify_v2` с C++20 берется у концепта, а не у признака, и вызов с `wchar_t` не находит перегрузки в одном ряду с `stringify`; признак не менялся (вопрос к пользователю: убрать ли специализацию `wchar_t`).

**Проверено:** GCC 13.2 Release, `string.utility.`: 58 тестов на C++11, 14 и 17, 60 на C++20 (14 новых тестов `LumexStringifyV2Test` на каждом стандарте и 2 на C++20: сравнение условия, концепта, признака и самого вызова на 30 списках аргументов, отдельная проверка `wchar_t`); все проходят. 5 мутаций реализации, все пойманы падением тестов: `stringify_v2` возвращает пустую строку или не использует аргументы, ограничение убрано, условие всегда истинно (оба варианта), условие C++20 взято у `all_streamable`. GCC 8.3: 58 тестов на C++11, 14, 17 и `-std=c++2a` проходят (без `<concepts>` условие - признак); Clang 23.1.0 с libc++ и с libstdc++ 13 и ASan с UBSan (GCC 13.2, Debug) на C++11, 14, 17, 20: все проходят; 12 отдельных компиляций тестов со строгими предупреждениями без диагностик.

##### `ranges::Algorithm::get_nearest_to` работает с C++11

**Файлы:** `lumex/core/utility/ranges/LumexRanges.hpp`, `lumex/core/utility/LumexUtility`, `lumex/tests/core/utility/ranges/` (`LumexRanges.cxx11.tests.cpp` - перенесен из `.cxx20`; `LumexRanges.cxx20.tests.cpp` - переписан под C++20; новые `LumexRangesProjection.cxx11.tests.cpp`, `LumexRangesConstraints.cxx11.tests.cpp`, `LumexRangesOracle.cxx11.tests.cpp`)

**Суть:** заголовок целиком стоял под `LUMEX_HAS_STD_RANGES` и ниже C++20 ничего не объявлял. Теперь обе перегрузки (итератор и страж, диапазон) объявлены с C++11. Алгоритм тот же: первый элемент, не стоящий перед значением (`std::ranges::lower_bound` с C++20, `std::lower_bound` ниже), сравнивается со своим предшественником, при равных расстояниях выигрывает предшественник. Умолчания - `functional::identity` и `functional::less` из записи об общих частях ниже: `less` это `std::less<void>`, а не прежний `std::ranges::less`; для чисел результат тот же, разница только в том, что `std::ranges::less` дополнительно требует `std::totally_ordered_with`, и типы, у которых есть `<`, но нет остальных сравнений, теперь принимаются.

Ограничения - одна форма `std::enable_if` во всех стандартах; проверка идет по ступеням (итератор и страж, затем проекция, затем сравнение). С C++20 (`LUMEX_HAS_STD_RANGES`) условия итератора, стража, диапазона и порядка - стандартные концепты (`std::bidirectional_iterator`, `std::sentinel_for`, `std::ranges::bidirectional_range`, `std::indirect_strict_weak_order`): итераторы представлений вроде `std::views::iota` с более слабой `iterator_category` по-прежнему принимаются; ниже C++20 это формы C++11 (`iterator_category`, операции `==` и `!=`, вызываемость). Новое во всех стандартах: проекция должна давать числовой тип, иначе перегрузки нет (раньше - ошибка внутри тела). Вызов проекции - `Detail::invoke_projection`: с C++17 это `std::invoke`, ниже - вызов через `std::mem_fn` для указателей на члены и обычный вызов для остального, так что указатель на данные-член или на функцию-член без аргументов (`&item::key`, `&item::get_key`, `&std::pair<int, std::string>::first`) работает как у алгоритмов `std::ranges`, а простое `proj (*it)` их бы потеряло. Своей функции `invoke` в библиотеке нет (есть признаки `traits::invoke`), и заводить ее ради одной проекции не стали. Элементом может быть объект, указатель, умный указатель или `std::reference_wrapper`.

Диапазон передается как lvalue (массив, `std::vector`, `std::list`, `std::set`, `iterator_range`) через `begin` и `end` с поиском по ADL; временный контейнер не находит перегрузки в любом стандарте (итератор повис бы), с C++20 принимаются заимствованные диапазоны (`std::span`, `std::views::iota`, `std::ranges::subrange`). Раньше временный контейнер на C++20 проходил проверку перегрузки и падал в теле. Страж другого типа, чем итератор (`std::counted_iterator` и `std::default_sentinel`, конец массива с признаком окончания, `iterator` и `const_iterator` одного контейнера), теперь поддержан: до поиска один раз проходим до него, как `std::ranges::lower_bound` для стража без размера; раньше возврат `last` требовал неявного преобразования в итератор. Заголовок всегда подключает `lumex/core/math/LumexMath` (раньше только с C++20), функция объявлена `LUMEX_CONSTEXPR_CXX14`: `constexpr` с C++14, как и прежде в константных выражениях с C++20. Прежний текст оставлен в тесте `LumexRanges.cxx20.tests.cpp` как эталон: на всех отсортированных векторах до пяти элементов (252 вектора, 17 запросов каждый) и с проекцией и `std::ranges::greater` результат совпадает с прежней реализацией.

**Проверено:** GCC 13.2 Release, `utility.ranges.`: C++11, 14, 17 по 86 тестов, C++20 - 97, C++23 - 98 (было 9 на C++11, 14 и 17, 32 на C++20 и 33 на C++23: `iterator_range` и прежние тесты), все проходят. 19 мутаций реализации, все пойманы (14 падением тестов, 5 ошибкой компиляции): `upper_bound` вместо `lower_bound`; ничья в пользу найденного; без охраны попадания в первый элемент и промаха за конец; страж не проходится до конца; предшественник не выбирается; значение не обязано быть числом; итератор не обязан быть двунаправленным; проекция не обязана давать число; сравнение не проверяется; временный диапазон принимается; перепутаны сравнение и проекция; конец взят от начала; указатели на члены вызываются как обычные вызываемые (ниже C++17 и с `std::invoke`); сравнение проверяется только в одном порядке; и три в ветке C++20: итератор произвольного доступа или только однонаправленный вместо двунаправленного, страж всегда подходит. GCC 8.3 (C++11, 14, 17, `-std=c++2a`): 86, 86, 86 и 87 тестов проходят (на `-std=c++2a` нет `<ranges>`, остается один пропуск); Clang 23.1.0 с libc++ и с libstdc++ 13 (C++11, 14, 17, 20) и ASan с UBSan (GCC 13.2, Debug, C++11, 14, 17, 20): все проходят; 36 отдельных компиляций тестов с `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast -fsyntax-only` на тех же сочетаниях компиляторов и стандартов без диагностик. Случай, найденный GCC 8.3: `std::iterator_traits<void *>` в libstdc++ 8 дает ошибку компиляции (ссылка на `void`), поэтому указатели на `void` и на функции отсеиваются до обращения к `iterator_traits`.

##### `downcast`, `downcast_noexcept` и `bad_down_cast` работают с C++11

**Файлы:** `lumex/core/utility/cast/LumexCast.hpp`, `lumex/core/utility/LumexUtility`, `lumex/tests/LumexTestStandards.cmake` (комментарий), `lumex/tests/core/utility/cast/` (`LumexCast.cxx11.tests.cpp` - перенесен из `.cxx20`; новые `LumexCastConstraints.cxx11.tests.cpp`, `LumexCastBehavior.cxx11.tests.cpp`, `LumexCastConcepts.cxx20.tests.cpp`; `CMakeLists.txt`)

**Суть:** весь заголовок стоял под `LUMEX_HAS_STD_CONCEPTS` и ниже C++20 ничего не объявлял. Теперь `downcast` (указатель и lvalue-ссылка), `downcast_noexcept` и `bad_down_cast` объявлены с C++11. Ограничения - одна форма `std::enable_if` во всех стандартах поверх `traits::meta::is_pointer_to_class`, `is_lvalue_ref_to_class`, `is_complete_type`, `is_derived_from` и `preserves_cv` из записи об общих частях ниже; допускается только настоящее нисходящее приведение: указатель к указателю или lvalue-ссылка к lvalue-ссылке, полные классы, полиморфный источник, цель выведена из источника через открытое однозначное основание (или тот же класс), `const` и `volatile` не теряются. Любое другое использование не находит перегрузки и обнаруживается SFINAE; закрытое, защищенное и неоднозначное наследование отвергаются (`std::is_base_of` один их пропускает). Проверка идет по ступеням (форма, полнота, остальное), чтобы `std::is_polymorphic` не спрашивался у неполного класса: раньше это обеспечивало короткое замыкание концепта. Прежние `Detail::DynCastForm` и `Detail::ValidDownCast` (концепты) удалены: они не были интерфейсом, а их место занял признак `Detail::is_valid_down_cast<Base, Derived>`; прежнее определение осталось эталоном в `LumexCastConcepts.cxx20.tests.cpp`. Поведение с C++20 то же: сравнение прежних концептов с новыми ограничениями на каждой паре из 108 исходных форм (указатель и lvalue-ссылка, с `const` и `volatile`, для 18 типов - классов, союза и перечисления) и 188 целевых типов - 20 304 пары, без расхождений и для признака, и для самих `downcast` и `downcast_noexcept`. Два отличия, оба в том, что раньше не компилировалось: (1) `downcast_noexcept<D &, B &> (b)` с явно названным `Base` теперь не находит перегрузки (раньше объявление проходило, а тело `return nullptr` не компилировалось); (2) `downcast_noexcept` вызывает `dynamic_cast` сразу, а не `downcast` с перехватом `bad_down_cast`: результат тот же (нулевой указатель на выходе для нулевого и для не подошедшего), но функция с `noexcept` больше не строит исключение с `std::string`, которое могло бросить `std::bad_alloc` и оборвать программу. Литерал `nullptr` по-прежнему не принимается (это не указатель на класс), переменная-нулевой-указатель принимается. Заголовок больше не включает `<stdexcept>`, `<concepts>` и `LumexCheckFeatures.hpp`. Тесты каталога начинаются с C++11 (раньше только с C++20): `LumexTestStandards.cmake` больше не называет `utility.cast` примером каталога без нижних наборов.

**Проверено:** GCC 13.2 Release, `utility.cast.`: C++11, 14, 17 по 72 теста, C++20 и C++23 по 74 (было 23 на C++20 и C++23), все проходят; 16 мутаций реализации, все пойманы (13 падением тестов, 3 ошибкой компиляции): проверка наследования заменена на `true` и на `std::is_base_of`, без проверки `const`/`volatile`, без проверки полиморфности, без проверки полноты, без совпадения формы (указатель/ссылка), ссылки не форма `dynamic_cast`, указатель без проверки нуля, указатель без исключения при неудаче, ссылка без обертки `std::bad_cast`, сообщение о статическом типе вместо динамического, сообщение без подробностей, `downcast_noexcept` без `noexcept`, с принятием ссылочной цели и с возвратом нуля всегда, другое начало сообщения. GCC 8.3 (C++11, 14, 17 и `-std=c++2a`): 72, 72, 72 и 74 теста проходят, два сравнения с концептами пропускаются (на `-std=c++2a` у GCC 8 нет `<concepts>`); Clang 23.1.0 с libc++ и с libstdc++ 13 (C++11, 14, 17, 20) и ASan с UBSan (GCC 13.2, Debug, C++11, 14, 17, 20): все тесты каталога проходят; 28 отдельных компиляций тестов с `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast -fsyntax-only` (GCC 8.3 и Clang 23 на C++11, GCC 13 на C++14, 17, 20, Clang 23 и GCC 8.3 на C++20, Clang с обеими библиотеками) без диагностик.

##### Генератор дампов `core/utility/dump` работает с C++11: строковые перегрузки, `get_memory_filters_range`, `get_optional_dump_directory`

**Файлы:** `lumex/core/utility/dump/LumexCoreDumpGenerator.hpp`, `lumex/tests/core/utility/dump/CMakeLists.txt`, `lumex/tests/core/utility/dump/` (новые: `LumexCoreDumpTestTypes.hpp`, `LumexCoreDumpConfigStrings.cxx11.tests.cpp`, `LumexCoreDumpConfigStrings.cxx17.tests.cpp`, `LumexCoreDumpConfigStrings.cxx20.tests.cpp`, `LumexCoreDumpInstance.cxx11.tests.cpp`, `LumexCoreDumpInstance.cxx17.tests.cpp`, `LumexCoreDumpInstance.cxx20.tests.cpp`)

**Суть:** заголовок генератора дампов уже компилировался с C++11, но три группы его членов существовали только с C++17 или C++20. Теперь все они есть в каждом стандарте.

1. Перегрузки `set_filename`, `set_directory`, `add_memory_filter` у `dump_configuration` и `generate_instance_dump (reason)`, `generate_instance_dump (reason, std::error_code &)` у `core_dump_generator` принимали `traits::string::StringLike T` и существовали только с C++20. Теперь это шаблоны с SFINAE во всех стандартах (`std::enable_if` с `traits::string::is_string_convertible<T>`, двойником концепта): они принимают все, что неявно приводится к `std::string` (литерал, `char const *`, класс с `operator std::string`), а с C++17 и `std::string_view` с его неявными преобразованиями. Набор принимаемых типов на C++20 прежний (тест сравнивает пять членов с концептом на 24 типах и формах аргумента); на C++11, C++14 и C++17 перегрузки новые, и на C++17 `std::string_view` теперь принимается без `std::string` у вызова. Сам концепт `StringLike` остается в `LumexTypeTraits.hpp`.
2. `get_memory_filters_range` возвращает `core_dump_generator::memory_filters_range_t`: `std::ranges::ref_view<std::vector<std::string> const>` там, где у стандартной библиотеки есть `<ranges>` (тот же тип, что давал `| std::views::all`, поведение на C++20 не изменилось), и `ranges::iterator_range` этой библиотеки над константными итераторами списка фильтров в остальных случаях. Общее у них: `begin ()`, `end ()`, `empty ()`; `size ()` и остальное от `view_interface` есть только у стандартного вида.
3. `get_optional_dump_directory` возвращает `core_dump_generator::optional_dump_directory_t`: `std::optional<std::string>` с C++17 и `lumex::core::optional::opt::optional<std::string>` раньше. Заголовок типов опционала подключается только до C++17 и ничего не объявляет в глобальной области (зонтик `LumexOptional` с глобальными именами не включается). `get_dump_directory_if_set (std::string &)`, который был только до C++17, теперь есть во всех стандартах. Новой CMake-зависимости нет: `utility` уже требует и подключает `optional`.

Тесты не вызывают `initialize ()` (он ставит обработчики сигналов, пишет `core_pattern` через `sudo` и запускает поток-монитор): экземпляр без инициализации создается через `instance ()` при временно поднятом закрытом флаге, а закрытая статическая конфигурация и каталог экземпляра задаются явной инстанциацией шаблона, к которой правила доступа не применяются.

**Проверено:** GCC 13.2 (Release), `ctest -R '^utility\.dump\.'`: 323 теста, все проходят (бинарники по стандартам: C++11 - 74, C++14 - 74, C++17 - 84, C++20 - 91; прежние 38 тестов проходят без изменений, новых 36, 36, 46 и 53). Те же наборы без ошибок: GCC 8.3 (C++11, C++14, C++17 и `-std=c++2a`, где 2 теста пропускаются: нет концептов и `<ranges>`), Clang 23.1.0 с libstdc++ (C++11, C++17, C++20) и с libc++ (C++11, C++17, C++20), ASan и UBSan на GCC 13.2 (C++11, C++14, C++17, C++20) и на Clang 23.1.0 с libc++ (C++11, C++20). Строгие предупреждения (`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast`, `-fsyntax-only`, заголовок, `.cpp` и все тесты каталога): GCC 8.3 (C++11, C++2a), Clang 23.1.0 (C++11 с libstdc++ и с libc++, C++20), GCC 13.2 (C++14, C++17, C++20) - 0 предупреждений. `create_release.sh` на четырех компиляторах (GCC 8.3, GCC 13.2, Clang 23.1.0, MinGW) в режимах C++11 и C++20: 0 предупреждений, 0 ошибок. `ctest -R '^(cmake|lint)\.'`: 148 из 148, включая `lint.headers_standalone`. Тесты проверены мутациями: 25 мутантов (отрицание и снятие ограничения SFINAE, подмена члена, потеря кода ошибки, потеря `noexcept`, пустой, сдвинутый и кэшированный диапазон, не тот вид на C++20, инвертированный и общий для вызовов каталог, неверный псевдоним на C++17, снятое подключение заголовка опционала) пойманы все: 11 ошибками компиляции (`static_assert`) и 14 падениями тестов.

##### Общие части для C++11 в `core/utility`: двойники концептов, `index_sequence`, `identity` и `less`

**Файлы:** `lumex/core/utility/traits/LumexTypeTraits.hpp`, `lumex/core/utility/sequence/LumexIndexSequence.hpp` (новый), `lumex/core/utility/functional/LumexFunctional.hpp` (новый), `lumex/core/utility/LumexUtility`, `lumex/core/reflection/field_reflection/LumexAggregateFields.hpp`, `lumex/core/crc/catalog/LumexCrcCatalog.cpp`, `lumex/tests/core/utility/CMakeLists.txt`, `lumex/tests/core/utility/LumexUtilityFoundations.cxx11.tests.cpp` (новый), `lumex/tests/core/utility/traits/` (новые: `LumexTypeTraitsTwins.cxx11.tests.cpp`, `LumexTypeTraitsTwins.cxx20.tests.cpp`), `lumex/tests/core/utility/sequence/` (новый каталог: `CMakeLists.txt`, `LumexIndexSequence.cxx11.tests.cpp`, `LumexIndexSequence.cxx14.tests.cpp`), `lumex/tests/core/utility/functional/` (новый каталог: `CMakeLists.txt`, `LumexFunctional.cxx11.tests.cpp`, `LumexFunctional.cxx14.tests.cpp`, `LumexFunctional.cxx20.tests.cpp`), `lumex/tests/core/reflection/field_reflection/LumexFieldReflectionIndexSequence.cxx11.tests.cpp` (новый)

**Суть:** общие заготовки, которые нужны портам `cast`, `ranges` и `dump` на C++11 и существовали только как концепты C++20 или как частные копии. `traits::meta` получает двойники концептов, которые остаются как есть: `is_pointer_to_class<T>` (`PointerToClass`), `is_lvalue_ref_to_class<T>` (`LvalueRefToClass`), `is_complete_type<T>` (`CompleteType`), `preserves_cv<From, To>` (`PreserveCV`) и `is_derived_from<Derived, Base>` (двойник `std::derived_from`, которого у `meta` не было: без него проверка нисходящего приведения пропускает закрытое, защищенное и неоднозначное наследование); `traits::string::is_string_convertible<T>` - двойник `StringLike` (неявно преобразуется в `std::string`, с C++17 и в `std::string_view`; `lumex_string_view` проходит через свой неявный оператор в `std::string`). Все наследуют `std::integral_constant`, как `is_extractible` и `is_byte_like`. Новый заголовок `sequence/LumexIndexSequence.hpp` (`lumex::core::utility::sequence`) - одна реализация `integer_sequence`, `index_sequence`, `make_integer_sequence`, `make_index_sequence` и `index_sequence_for`: с C++14 (`LUMEX_HAS_STD_INTEGER_SEQUENCE`) это псевдонимы стандартных, поэтому результат - стандартный тип; ниже - свой класс `integer_sequence` с `value_type` и `size ()` и псевдонимы над ним, так что функция с параметром `index_sequence<I...>` выводит индексы так же. Список строится делением пополам (глубина инстанцирования - логарифм счета); счет больше 16384, в том числе отрицательный, превратившийся в огромный `std::size_t`, не дает типа: ошибка вроде `make_index_sequence<N - 1>` при `N == 0` останавливается сразу, а не вычерпывает память компилятора. Предел 16384, потому что Clang 23 строит список из 32768 и из 65536 чисел пустым без диагностики (32767 строит верно). Новый заголовок `functional/LumexFunctional.hpp` (`lumex::core::utility::functional`) - `identity` и `less`: `identity` это `std::identity` там, где есть `<ranges>` (`LUMEX_HAS_STD_RANGES`), иначе свой класс; `less` это `std::less<void>` с C++14 (`LUMEX_HAS_STD_TRANSPARENT_OPERATORS`), иначе свой класс с тем же вызовом (пересылка операндов, `is_transparent`, `noexcept` по `<`, `constexpr`, результат - то, что вернет `<`, нет участия в разрешении перегрузки для несравнимых операндов, полный порядок указателей на объекты через `std::less<void const volatile *>`). `less` не `std::ranges::less` намеренно: тот добавляет требование `std::totally_ordered_with`, которое на C++11 не выразить; алгоритмы диапазонов C++20 принимают оба. `field_reflection::detail` берет `index_sequence` и `make_index_sequence<N>::type` из `utility` (включение, `using` и обертка из трех строк, чтобы вызовы не менялись), `LumexCrcCatalog.cpp` (библиотека остается на C++11) - `make_index_sequence`; двух частных копий больше нет. Зонтик `LumexUtility` подключает оба заголовка. Остальные концепты заголовка новых двойников не просят: для `Extractible`, `ByteLike`, `Streamable`, `is_expected_concept` и `SafeComparable` они уже есть, `ArithmeticType` и `AllStringifiable` нужны не портам этой серии, а `stringify_v2`.

**Проверено:** GCC 13.2 Release, тесты `utility.traits.`, `utility.sequence.`, `utility.functional.`, `utility.`, `reflection.field_reflection.` и `crc.` на всех стандартах (3717 тестов): новые тесты проходят - `sequence` по 11 на C++11, 14, 17, 20 и 23, `functional` 21 (C++11), 24 (C++14, 17), 27 (C++20, 23), `LumexTypeTraitsTwins` 17 (C++11-17), 23 (C++20, 23) с сравнением каждого двойника с концептом, зонтик по 3, проверка частной копии `field_reflection` по 1 на C++11-20; остаются 79 падений `reflection.field_reflection` на C++20 (имена полей `names_as_array` и `to_json`: GCC 13 пишет в `__PRETTY_FUNCTION__` квалифицированное имя члена, разбор берет пространство имен; тот же результат ("ns ns" для `ns::Plain`) на исходниках ветки без этой правки). GCC 8.3 (`-std=c++2a` как C++20): 3552 теста проходят, тесты с концептами C++20 пропускаются (нет `<concepts>`); Clang 23.1.0 с libc++: 3717 из 3717, с libstdc++ 13 (C++11 и C++20): 1807 из 1807; ASan и UBSan (GCC 13.2, Debug, C++11 и C++20): все новые тесты проходят (те же 79 тестов имен полей падают, два из них аварийно). `create_release.sh` с GCC 8.3, GCC 13.2, Clang 23.1.0 и MinGW 8.3 на C++11 и C++20: 8 сборок, 0 предупреждений, 0 ошибок; `cmake.*` и `lint.*`: 144 из 144; `format.py --check` и пять `check_*.py` без замечаний; 37 отдельных компиляций новых тестов с `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast` (GCC 8.3 и Clang 23 на C++11, GCC 13 на C++14, 17, 20, Clang 23 и GCC 8.3 на C++20) без диагностик. 27 мутаций реализации, все пойманы (7 падением теста, 20 ошибкой компиляции в `static_assert` или в вызове). Признаки (8): `is_object` вместо `is_class` у указателя, любая ссылка вместо lvalue-ссылки, полнота всегда истинна, `const` и `volatile` в `preserves_cv` не учитываются (по одной мутации), в `is_derived_from` убрано преобразование указателей и обращен `is_base_of`, у `is_string_convertible` явное преобразование считается неявным. Последовательности (10): сдвиг на единицу при склейке, неверные половины при нечетном счете, список из одного числа начинается с 1, предел поднят до 131072, `size ()` на единицу больше, `value_type` равен `int`, `index_sequence_for` и `make_index_sequence` другого счета и типа, отказ от псевдонимов на C++14; тот же сдвиг при склейке ломает `reflection.field_reflection` и `crc.catalog`. Функциональные объекты (9): отказ от `std::identity` на C++20 и от `std::less<void>` на C++14, `identity` возвращает копию, `less` с `>`, обратный порядок указателей, снятый `noexcept`, снятый `is_transparent` у `less` и у `identity`, операнды без пересылки. Не поймана по построению мутация "порядок указателей через `<` вместо `std::less`": на плоской памяти результат тот же.

##### Имена полей, `get` и `to_json` агрегатов с C++11: макрос `LUMEX_DEFINE_FIELD_NAMES`

**Файлы:** `lumex/core/reflection/field_reflection/LumexAggregateFields.hpp`, `lumex/core/reflection/field_reflection/LumexFieldReflection.hpp`, `lumex/core/reflection/LumexReflection`, `conanfile.py`, `lumex/tests/core/reflection/field_reflection/` (`CMakeLists.txt`, `LumexFieldReflectionRegisteredFixtures.hpp`, `LumexFieldNamesRegistration.cxx11.tests.cpp`, `LumexFieldNamesRegistration.cxx14.tests.cpp`, `LumexFieldNamesGet.cxx11.tests.cpp`, `LumexFieldNamesGet.cxx14.tests.cpp`, `LumexFieldNamesJson.cxx11.tests.cpp`, `LumexFieldNamesStdOptional.cxx17.tests.cpp`, `LumexFieldNamesAgreement.cxx20.tests.cpp` - все новые), `lumex/tests/cmake/` (`CMakeLists.txt`, `cases/wiring_field_reflection.cmake`, `consumer/field_names_compile_checks/` - новый)

**Суть:** имена полей агрегата брались из указателя на подобъект в `template <auto>` и разбора `LUMEX_FUNCTION_NAME`, поэтому `names_as_array` и `to_json` появлялись только с C++20, а `get<I>` - с C++14. Макрос `LUMEX_DEFINE_FIELD_NAMES (Type, a, b, c)` (от 1 до 32 имен) ставится на уровне пространства имен в пространстве имен типа, после его определения и до первого использования. Он определяет inline-перегрузку `lumex_field_registry (registry_tag<Type>)`, которую находит ADL (специализировать шаблон библиотеки из пространства имен пользователя нельзя, так же сделан Boost.Describe): имена берутся из лексем, а члены - как `member_constant<decltype (&Type::a), &Type::a>` на каждое поле. Указатели на члены оказываются константами типа, `get<I>` читает член без таблицы времени выполнения, а от времени выполнения остается указатель на статический массив имен. `static_assert` сравнивает число имен с числом полей по автоматическому подсчету, имя, которого нет среди членов, не компилируется. С регистрацией `names_as_array`, `get<I>` и `to_json` работают с C++11 и дают одно и то же на C++11-C++20, на любом компиляторе и при любой оптимизации; регистрация выигрывает у автоматических источников там, где они есть (имена на C++20, `get` с C++14), `tuple_size` остается автоматическим. I-е имя и i-й член - одно поле по построению: регистрация в порядке, отличном от порядка объявления, дает согласованные имена и значения (но `get<I>` тогда не I-й объявленный член). Незарегистрированные типы идут по прежнему коду C++14-C++20 без изменений (ветки `get` отличаются только диспетчеризацией по признаку регистрации). Без регистрации `names_as_array` и `to_json` ниже C++20 и `get<I>` на C++11 - один `static_assert` с названием макроса; агрегат без полей регистрации не требует. `to_json` объявлен на всех стандартах (nlohmann нужен только C++11); поле-агрегат nlohmann преобразует обычным путем, через `to_json (nlohmann::json &, Type const &)`, который находит ADL (он может вызвать `field_reflection::to_json`). Ограничения прежние: до 32 полей, без базовых классов, битовых полей, ссылок и массивов-членов (массив считается несколькими полями, и регистрация не проходит проверку числа имен; с C++17 так же для базового класса, а на C++11 и C++14 класс с базой не агрегат и подсчет ненадежен). Решения: имя по соседям (`LUMEX_DEFINE_REFLECTED_ENUM`, `LUMEX_DEFINE_EXCEPTION`); макрос не внутри структуры, чтобы регистрировать и вложенные в класс, и чужие типы; регистрация должна стоять до первого использования типа в единице трансляции (ADL разрешается при инстанцировании). Нижние границы в комментариях модуля и зонтика и в описании `conanfile.py` обновлены.

**Проверено:** GCC 13.2 Release, набор `reflection.field_reflection`: C++11 87 тестов, C++14 132, C++17 135, C++20 223, из них новых 48, 52, 55 и 61. Все проходят, кроме C++20: 83 падения, это прежние 79 (имена автоматического пути: GCC 13 пишет в `__PRETTY_FUNCTION__` квалифицированное имя члена, разбор берет пространство имен, "lumex_field_reflection_tests" вместо "id") и 4 новых сравнения зарегистрированных имен с именами компилятора, которые падают вместе с ним; все остальные тесты зарегистрированных типов на C++20 при -O2 проходят, то есть регистрация обходит сбой имен GCC 13. Clang 23.1.0 (libstdc++ 13 и libc++) и GCC 8.3 (C++11, 14, 17 и `-std=c++2a` как C++20): все тесты проходят, на Clang 23 в том числе сравнение зарегистрированных имен с именами компилятора (на GCC 8.3 в режиме `-std=c++2a` имен компилятора нет, 5 тестов пропускаются). ASan и UBSan (GCC 13.2, Debug): C++11, 14 и 17 проходят целиком; на C++20 проходят 57 новых тестов из 61, остальные 4 - те же сравнения с именами компилятора (они неверны и при -O0 с санитайзерами, а существующий тест имен падает в nlohmann на отсутствующем ключе и завершает набор аварийно, как и до правки). Предупреждения `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast` на новых тестах: ни одного на GCC 8.3 (C++11, 14, 17, `-std=c++2a`), GCC 13.2 (C++11, 14, 17, 20), Clang 23.1.0 (C++11, 14, 17, 20) и MinGW 8.3. `cmake.field_names_compile_checks`: 42 случая (макрос, `names_as_array`, `get` и `to_json` без регистрации, где стандарт не дает автоматического источника, меньше, больше и несуществующие имена, массив, индекс за пределом, больше 32 имен, вторая регистрация, закрытый и ссылочный член) не компилируются с сообщением своей проверки на GCC 8.3, GCC 13.2, Clang 23.1.0 (libstdc++ и libc++) и MinGW 8.3, корректные компилируются на C++11, 14, 17 и 20. Мутации: 14 правок, все пойманы - 7 тестами (имена в обратном порядке, не то имя из `#`, не тот член, `get` на C++17 мимо регистрации, `to_json` с одним именем, граница индекса, имя компилятора раньше регистрации), 4 ошибкой компиляции (смещение индекса `get`, поле пропущено в регистрации, два имени переставлены в регистрации, агрегат без полей считается незарегистрированным) и 3 проверками `cmake.field_names_compile_checks` (убрана проверка числа имен, изменены тексты двух `static_assert`). Первая версия тестов (типизированный тест по 32 агрегатам с рекурсивным обходом полей) давала 230 МБ ассемблера на файл на GCC 8.3; новые файлы - от 2 до 17 МБ и 1-3 с. `cmake.*` и `lint.*`: 149 тестов проходят; форматирование (clang-format 21.1.7) и Doxygen (0 предупреждений для модуля) без замечаний. `create_release.sh` (GCC 8.3, GCC 13.2, Clang 23.1.0, MinGW 8.3; C++11 и C++20): 0 предупреждений и 0 ошибок во всех восьми сборках.

##### `strong_ordering`, `weak_ordering` и `partial_ordering` с C++11; трехстороннее сравнение `safe_comparator` с C++11

**Файлы:** `lumex/core/utility/numeric/LumexOrdering.hpp` (новый), `lumex/core/utility/numeric/LumexSafeNumericComparator.hpp`, `lumex/core/utility/LumexUtility`, `lumex/examples/utility/example_utility_workflow.cpp`, `lumex/tests/core/utility/numeric/` (новые: `LumexOrdering.cxx11.tests.cpp`, `LumexOrderingLinkage.cxx11.tests.cpp`, `LumexOrdering.cxx20.tests.cpp`, `LumexSafeThreeWayCompare.cxx11.tests.cpp`, `LumexSafeThreeWayCompare.cxx20.tests.cpp`, `LumexNumericSamples.hpp`; правки `CMakeLists.txt` и комментария в `LumexSafeNumericComparator.cxx20.tests.cpp`), `lumex/tests/cmake/` (новые: `cases/wiring_ordering.cmake`, `consumer/ordering_compile_checks/`; правка `CMakeLists.txt`)

**Суть:** три класса `lumex::core::utility::numeric::strong_ordering`, `weak_ordering` и `partial_ordering` (новый заголовок `LumexOrdering.hpp`, входит в зонтик `LumexUtility`) повторяют категории сравнения `<compare>` для каждого стандарта от C++11. Значения: `less`, `equal` и `equivalent` (одно значение) у `strong_ordering`, `equivalent` у `weak_ordering`, `unordered` у `partial_ordering`, `greater`. Неявные преобразования `strong_ordering` в `weak_ordering` в `partial_ordering`; сравнения с литералом `0` в обоих порядках операндов (`==`, `!=`, `<`, `<=`, `>`, `>=`; параметр принимает только литерал `0` и `nullptr`, как неуказанный тип стандарта, переменная со значением 0 не подходит); `==` и `!=` между упорядочениями (категории сравниваются через преобразование к более слабой); `is_eq`, `is_neq`, `is_lt`, `is_lteq`, `is_gt`, `is_gteq`. Классы `constexpr`, тривиально копируемые, размером в один байт, без конструктора по умолчанию и из числа. `<` между двумя упорядочениями не компилируется, как у стандартных (MS STL его тоже не дает); `<=>` у классов нет (оператор C++20). Класс один для всех стандартов и не псевдоним `std::strong_ordering` (как `span` не псевдоним `std::span`): с C++20 два семейства существуют рядом, преобразования и сравнения между ними нет. Псевдонимы `strong_ordering_t`, `weak_ordering_t` и `partial_ordering_t` называют упорядочение библиотеки при используемом стандарте: `std::*_ordering`, когда у компилятора и стандартной библиотеки есть `<compare>` (`LUMEX_HAS_THREE_WAY_COMPARISON`, условие прежнее), иначе классы библиотеки; трейт `is_ordering<T>` истинен для классов обоих семейств. Именованные значения (`strong_ordering::less`, ...) - константы класса, пригодные для константных выражений в каждом стандарте. Шаблонный базовый класс с ними не работает: член шаблона класса пригоден для константного выражения только если объявлен `constexpr` в классе, а там тип неполон (GCC: `was not declared constexpr`, Clang: `initializer of 'less' is unknown`). Поэтому, как в стандартной библиотеке, они объявлены в классе и определены после него в заголовке: с C++17 как `inline`, раньше с признаком единственного определения компилятора (макрос `LUMEX_ORDERING_LINK_ONCE`): слабый символ на GCC, `selectany` на MSVC, clang-cl, MinGW и Cygwin, `inline`-переменная как расширение C++17 на Clang (ее предупреждение отключено в заголовке). Clang не считает слабую переменную константой, GCC 8 не позволяет отключить предупреждение об `inline`-переменной, поэтому ветви разные. Цена слабого символа на GCC до C++17: значение читается из памяти, а не подставляется.

Трехсторонняя часть `safe_comparator` больше не за `LUMEX_HAS_THREE_WAY_COMPARISON` и не помечена `@since C++20`: `safe_three_way_compare` (член и функция), `three_way_comparison_result` и `three_way_comparison_result_t`, `is_equal`, `is_not_equal`, `is_less`, `is_less_equal`, `is_greater`, `is_greater_equal` работают с C++11. Результат - `strong_ordering_t` для двух целых типов и `partial_ordering_t` для остальных (NaN дает `unordered`). С C++20 это те же `std::strong_ordering` и `std::partial_ordering`, что и раньше, и значения те же: код сравнений не менялся, заменены только имена типов. Функции `is_*` принимают упорядочения обоих семейств (раньше только стандартные); любой другой тип по-прежнему отвергает `static_assert`. Тип результата члена и функции записан явно (`three_way_comparison_result_t`), а не `auto` (выводимый возвращаемый тип - C++14). Унаследованное поведение не менялось, оно закреплено тестами: при бесконечности и другом типе (другой тип с плавающей точкой, целое) `safe_three_way_compare` отвечает `unordered`, если бесконечности не равны, тогда как `safe_less` и `safe_greater` для целого упорядочивают бесконечность по знаку (позже в этой версии порядок выровнен, см. запись "Бесконечность упорядочивается по знаку" в разделе "Изменено"); целое число приводится к общему типу, как в `safe_equal`. Пример `example_utility_workflow.cpp` показывает псевдонимы результата.

**Проверено:** 49 новых тестов в каждом наборе от C++11 и еще 20 от C++20 (в `utility.numeric.` 1427 тестов на C++11, 14 и 17 и 1455 на C++20 и 23). GCC 13.2 (Release): 7191 тест `utility.numeric.` (C++11, 14, 17, 20, 23) и 2 примера `utility` проходят (25 пропусков - прежние тесты отрицательных значений беззнаковых типов); Clang 23.1.0 с libc++ и с libstdc++ 13 (C++11 и C++20): по 2883 теста с примером проходят; GCC 8.3 (C++11, 14, 17, `-std=c++2a`): 5737 тестов проходят, на `-std=c++2a` 28 тестов C++20 пропускаются (у GCC 8 нет `<compare>`: там результат - классы библиотеки, их проверяет файл C++11). Тесты сравнивают классы со стандартными (libstdc++ 13, libc++ 23): значения, 12 сравнений с 0, сравнения между категориями, преобразования, `is_eq` - `is_gteq`, признаки типов, `is_*` компаратора, и сверяют результат `safe_three_way_compare` с точным эталоном (знак и модуль, без преобразований), `std::cmp_less` и `<=>`: все пары типов целых от `signed char` до `unsigned long long` на границах типов и около нуля, `float`, `double` и `long double` с NaN, бесконечностями, обоими нулями и денормализованным числом, целые против плавающих в обе стороны; результат согласован с шестью прежними булевыми функциями (`safe_less` и другие). ASan и UBSan: 49 тестов (69 на C++20) без замечаний на GCC 13.2 (C++11, 14, 17, 20), Clang 23 (libc++ и libstdc++, C++11 и C++20) и GCC 8.3 (C++11). Мутации: 55 правок обоих заголовков (обмен `less` и `greater`, NaN, знак, ширина, значение `unordered`, сравнения с 0, преобразования между категориями, `is_*`, тип результата, пустой признак единственного определения) - ловятся все 55, контрольная правка без изменения не ловится; 25 из них, которые ловятся ошибкой компиляции `static_assert` в тестах, ловятся и прогоном с выключенными `static_assert` (23 тестами, 2 ошибками компиляции в самом заголовке). `cmake.*` и `lint.*`: 137 тестов проходят (новые `cmake.wiring_ordering` и `cmake.ordering_compile_checks`: 13 случаев, которые стандартные упорядочения отвергают, не компилируются на GCC 13.2 в C++11, C++17 и C++20; из шести правок, проверяющих эти два случая, ловятся пять, шестая - снятый `static_assert` в `is_less` - нет, так как `is_less (1)` не компилируется и без него). Предупреждения: `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast` на тестах и примере - ни одного на GCC 8.3 (C++11, 17, `-std=c++2a`), Clang 23 (C++11, 20), GCC 13.2 (C++14, 17, 20, 23), MinGW 8.3 (C++11). `create_release.sh` (GCC 8.3, GCC 13.2, Clang 23.1.0, MinGW 8.3; C++11 и C++20; `-Werror`): 0 предупреждений и 0 ошибок во всех восьми сборках. Doxygen 1.8.20: без предупреждений на обоих заголовках. Форматирование (clang-format 21.1.7) и пять проверок `lint.*` без замечаний.

##### Модуль `core/span`: `span` и `as_bytes` с C++11

**Файлы:** `lumex/core/span/LumexSpan` (новый зонтик), `lumex/core/span/view/LumexSpan.hpp` (новый), `lumex/core/span/view/LumexSpanTraits.hpp` (новый), `lumex/core/span/CMakeLists.txt` (новый), `lumex/core/CMakeLists.txt`, `cmake/LumexOptions.cmake` (`LUMEX_BUILD_SPAN`), `cmake/LumexModules.cmake`, `cmake/LumexLibConfig.cmake.in`, `CMakeLists.txt`, `conanfile.py`, `lumex/CMakeLists.txt`, `lumex/tests/core/span/` (новый каталог: `LumexSpanGlobalNames.cxx11.tests.cpp`, `LumexSpanGlobalNames.cxx20.tests.cpp` и в `view/` `LumexSpanTraits`, `LumexSpanConstruction`, `LumexSpanObservers`, `LumexSpanSubviews`, `LumexSpanBytes`, `LumexSpanConstexpr` (`.cxx11.tests.cpp`), `LumexSpan.cxx17.tests.cpp`, `LumexSpanRanges.cxx20.tests.cpp`, `LumexSpanStdDifferential.cxx20.tests.cpp`, `LumexSpanTestSupport.hpp`), `lumex/tests/LumexTestStandards.cmake`, `lumex/tests/cmake/` (`cases/wiring_span.cmake`, `consumer/span_compile_checks/`, правки `setup_all_on.cmake`, `options_declared`, `require_ok_*`, `wiring_distr`, `wiring_subdirs`, `wiring_test_names`, `install_header_only_without_utility`), `lumex/examples/span/` (новый: `example_span.cpp`, `example_span_workflow.cpp`)

**Суть:** новый заголовочный модуль `lumex::span` (опция `LUMEX_BUILD_SPAN`, по умолчанию ON, без зависимости от других модулей: он включает только заголовки макросов `core/utility`, как `optional`) с классом `lumex::core::span::view::span<T, Extent>`, который компилируется с C++11 и повторяет интерфейс `std::span` из C++20 вместе с поздними добавлениями: `const_iterator`, `cbegin`/`cend`/`crbegin`/`crend` (C++23), `at` (C++26, бросает `std::out_of_range`) Реализация написана заново по описанию стандарта (P0122R7, P1024, P1976, LWG 3255) с оглядкой на `<span>` libc++ 23 и libstdc++ 13 и на `boost/core/span.hpp`; код этих библиотек не копировался, `THIRD-PARTY-NOTICES.md` не менялся. Решения: один класс для всех стандартов, а не псевдоним `std::span` (как `optional` и `string_view`: типы и наборы перегрузок не зависят от стандарта); с C++20 он взаимозаменяем со `std::span` через конструкторы-из-диапазона обоих классов (в обе стороны, в том числе для rvalue, с правилами `explicit` для статической протяженности) и входит в `std::ranges::enable_borrowed_range` и `enable_view`; итератор - сырой указатель; статическая протяженность не занимает памяти (`sizeof (span<T, N>)` равен размеру указателя); концепты C++20 заменены определением членов (`data ()` с указателем и `size ()`, не массив, не `std::array`, не `span`, lvalue или константные элементы, преобразование элементов только добавлением квалификатора); конструкторы от итераторов принимают указатели, а до C++20 другие итераторы только при специализации `is_contiguous_iterator` (C++11 не отличает итератор `std::vector` от итератора `std::deque`; с C++20 - `std::contiguous_iterator`); `byte` - это `std::byte` с C++17 и собственное перечисление с теми же операторами и `to_integer` до него (с `may_alias`, иначе запись через `span<byte>` не видна чтению объекта); имена не выносятся в глобальное пространство (пробный тест `LumexSpanGlobalNames`); предусловия не проверяются, как в `string_view`, проверяет только `at`; конструктор из `std::initializer_list` (C++26, P2447) не предоставлен: он делает вызов `f ({1, 2, 3})` неоднозначным между перегрузками для `std::vector` и для `span`, а такие перегрузки уже есть в `base64` и `crc` (объект `std::initializer_list` в `span` из константных элементов преобразуется как любой диапазон).

**Проверено:** тесты `span.` и `span.view.`: GCC 13.2 Release 508 тестов (C++11 - 151, C++17 - 161, C++20 - 196) и 2 примера проходят; Clang 23.1.0 с libc++ и с libstdc++ 13 - те же 508; GCC 8.3 (C++11, C++17, `-std=c++2a`) - 476, из них 3 пропускаются (нет `<span>` и концептов); ASan и UBSan (GCC 13.2, Debug) - 508 из 508. Дифференциальные тесты со `std::span` сравнивают конструируемость и неявность 684 сочетания типов элементов, протяженностей и источников (указатель и размер, пара указателей и итераторов, массив, `std::array`, контейнеры, `std::vector<bool>`, `std::deque`, `std::list`, `std::string`, `std::string_view`, `initializer_list`, спаны обоих видов), типы-члены, типы результатов подвидов и результаты на одних данных: совпадают с libstdc++ 13 и с libc++ 23 (расхождения по замыслу перечислены в тесте). `cmake.span_compile_checks`: 34 случая, которые `std::span` отвергает, не компилируются на C++11, C++17 и C++20 (102 попытки), корректные компилируются. Мутации: 75 правок кода модуля, 72 ловятся (60 тестами, 12 проверками компиляции `cmake.span_compile_checks`: ослабленный `nodiscard`, `static_assert` подвидов, потерянный `explicit`), 3 нет: исключение `span` из конструктора диапазона (дублируется проверкой `extent`) и `static_assert` на `void` и ссылку (они только улучшают сообщение). Предупреждения: `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast` на всех тестах и примерах - ни одного на GCC 8.3 (C++11, 17, `-std=c++2a`), MinGW 8.3 (C++11, 14, 17, `-std=c++2a`), Clang 23 (C++11, 14, 17, 20, 23), GCC 13.2 (C++14, 17, 20, 23). `cmake.*` и `lint.*` на вершине ветки (поверх `release/v2.0.0.0`): 135 тестов, все проходят. Форматирование (`format.py`, clang-format 21.1.7) - без замечаний, документация Doxygen - ни одного предупреждения для модуля. `create_release.sh` (GCC 8.3, GCC 13.2, Clang 23.1.0, MinGW 8.3; C++11 и C++20; `-Werror`): 0 предупреждений и 0 ошибок во всех восьми сборках.

##### Бенчмарк `span`: `lumex::span`, `std::span` и `boost::span`

**Файлы:** `benchmarks/span/` (новый: `bench_span.cpp`, `bench_span_impl.hpp`, `compile_time.cpp`, `CMakeLists.txt`, `run_benchmark.py`, `plot_results.py`, `README.md`, `results/`), `CMakeLists.txt`, `lumex/tests/cmake/cases/wiring_span.cmake`

**Суть:** сравнение `span` этой библиотеки (C++11 и C++20) со `std::span` (C++20) и `boost::span` Boost.Core (C++11 и C++20) на тех же сценариях и одном исходнике: создание из указателя и размера, массива, `std::array`, `std::vector`, копирование, суммы тремя способами на 8, 1 024 и 1 000 000 элементов, `first`, `last`, `subspan` с динамическими и статическими счетчиками, преобразования между протяженностями, передача по значению в `noinline` и встраиваемую функцию, рекурсивная сумма на половинах (много маленьких спанов), `as_bytes`, `front` и `back`. Один исполняемый файл на реализацию и стандарт (`BENCH_SPAN_IMPL`), флаги профиля Release одинаковые; перед замером каждый сценарий сверяется с эталоном на сырых указателях, а `run_benchmark.py` сравнивает контрольные суммы всех исполняемых файлов. Опция `LUMEX_BUILD_BENCHMARKS` (по умолчанию OFF) и `LUMEX_BUILD_SPAN` включают каталог в корневом `CMakeLists.txt`; Boost необязателен и только заголовочный (`LUMEX_BENCH_BOOST_INCLUDE_DIR` или `find_package(Boost 1.78 CONFIG)`), без него варианты `boost` пропускаются. Драйвер `run_benchmark.py` гоняет варианты нескольких наборов инструментов по очереди, оставляет лучший проход, помечает неустойчивые значения, умеет `--merge` и `--compile-time-only` (время `-fsyntax-only` одной единицы трансляции на вариант); `plot_results.py` (только стандартная библиотека Python) пишет таблицу Markdown и диаграммы SVG.

**Проверено:** Intel Core i7-12700K, GCC 13.2.0 с libstdc++ и Clang 23.1.0 с libc++, Boost 1.92.0 (только заголовки), 5 проходов по 15 повторений на одном ядре. При одном стандарте и одном наборе инструментов `span` этой библиотеки отличается от `std::span` и `boost::span` не более чем на 13 % в каждом сценарии (среднее геометрическое отношения: 0.999 и 0.992 против `std::span`, 1.002 и 1.000 против `boost::span` C++20, 1.002 и 0.999 против `boost::span` C++11); различия между столбцами C++11 и C++20 одинаковы у всех трех реализаций и относятся к размещению кода. Потерь не найдено, код `span` ради скорости не менялся. Цена - время компиляции: единица трансляции с полным API стоит поверх заглушки на GCC 13.2 +121 мс (C++11) и +373 мс (C++20) против +135 и +249 мс у `boost::span` и +97 мс у `std::span` (C++20). Заголовок `span` перестал включать `<memory>` до C++17 (`address_of` вместо `std::addressof`: около 50 мс на C++11). Проверка: подмена `last` в `span` останавливает сценарий (`last (size 1024): checksum 4184, the raw-pointer reference 5136`), правка регистрации в `CMakeLists.txt` роняет `cmake.wiring_span`; оба исполняемых файла собираются без предупреждений на обоих наборах инструментов.

##### CPU и память отдельных процессов: `LumexProcessMonitor`

**Файлы:** `lumex/applied/resource_monitor/process/LumexProcessMonitor.hpp` (новый), `lumex/applied/resource_monitor/process/LumexProcessMonitor.cpp` (новый), `lumex/applied/resource_monitor/process/detail/LumexProcessDetail.hpp` (новый), `lumex/applied/resource_monitor/process/detail/LumexProcessDetail.cpp` (новый), `lumex/applied/resource_monitor/monitor/detail/LumexProcFs.hpp`, `lumex/applied/resource_monitor/monitor/detail/LumexProcFs.cpp`, `lumex/applied/resource_monitor/LumexResourceMonitor`, `lumex/applied/resource_monitor/CMakeLists.txt`, `cmake/LumexModules.cmake`, `conanfile.py`, `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/cases/require_fail_resource_monitor_without_expected.cmake` (новый), `lumex/tests/applied/resource_monitor/LumexProcessDetail.cxx17.tests.cpp` (новый), `lumex/tests/applied/resource_monitor/LumexProcessMonitor.cxx17.tests.cpp` (новый)

**Суть:** `lumex::applied::resource_monitor::process::LumexProcessMonitor` замеряет процессы по запросу: `sample(pid)` возвращает `Expected<process_usage_t, process_query_error>` (`not_found`, `access_denied`, `read_failed`, `unsupported`), `sample_by_name(name)` - по замеру на каждый процесс с таким именем исполняемого файла, `find_by_name`, `current_process_id`, `forget_exited`; `total()` складывает замеры. В `process_usage_t`: доля CPU всей машины (0-100 %) с предыдущего замера этого процесса этим монитором (пусто при первом), суммарное время CPU, число логических процессоров, резидентная и частная память. Повторно выданный системой PID распознается по времени старта процесса. Имя на Linux - имя файла `/proc/<pid>/exe`, иначе первого аргумента, иначе `comm` (15 символов `comm` совпадают и с более длинным запрошенным именем). Модуль теперь зависит от `expected` (`lumex_require_module`, компонент Conan, кейс `cmake.require_fail_resource_monitor_without_expected`). Поиск по имени пропускает зомби Linux (завершились, родитель их еще не прибрал): у них нет памяти и читаемого `exe`, остается только обрезанный `comm`. Реализовано для Linux (`/proc`) и Windows (`OpenProcess`, `GetProcessTimes`, `GetProcessMemoryInfo`, Toolhelp32; имя - без учета регистра, с `.exe` или без); на других платформах запрос возвращает `unsupported`. Прогон набора на Windows еще не выполнен.

**Проверено:** 26 тестов (разбор `/proc/<pid>/stat` с `comm` со скобками и пробелами, `status` без `RssAnon` и без `VmRSS`, правила имен; живые: свой процесс, доля CPU после нагрузки, несуществующий и завершившийся процесс, поиск по имени, имя файла против `argv[0]`, `total`), GCC 13.2 Release; шесть мутаций реализации (нет доли CPU, имя только из `comm`, `ENOENT` не как `not_found`, частная память при неизвестной, правило `comm` без условия, `forget_exited` забывает все) ловятся. Распознавание повторного PID тестом не покрыто: повтор PID в тесте не воспроизвести надежно.

##### `LumexProcessMonitor`: процесс вместе с потомками

**Файлы:** `lumex/applied/resource_monitor/process/LumexProcessMonitor.hpp`, `LumexProcessMonitor.cpp`, `process/detail/LumexProcessDetail.hpp`, `LumexProcessDetail.cpp`, `lumex/tests/applied/resource_monitor/LumexProcessMonitor.cxx11.tests.cpp`, `LumexProcessDetail.cxx11.tests.cpp`

**Суть:** `sample(pid, include_children = true, breakdown = false)` и `sample_by_name(name, include_children = true, breakdown = false)` складывают потребление процесса и всех процессов под ним (дети, их дети и так далее): CPU-время, доля CPU и память суммируются, `process_count` считает процессы, `pid` и `name` остаются у верхнего. `breakdown = true` кладет в `members` по записи (`process_member_t`) на каждый процесс суммы, верхний первым; у одного процесса `members` пуст. `find_descendants(pid)` отдает ID потомков, ближайшие первыми. Дерево строится по родительскому PID (Linux: поле 4 `/proc/<pid>/stat`, Windows: Toolhelp32 `th32ParentProcessID`); процесс, начавшийся раньше своего родителя, и все под ним в дерево не входят: ID родителя был использован заново. Для имени каждый процесс считается один раз: процесс с тем же именем внутри дерева другого входит в сумму того, поэтому результат - одна запись на дерево, а при `include_children = false` по-прежнему одна запись на процесс. Процесс-потомок, начавшийся после прошлого замера, доли CPU пока не имеет, и сумма считает доли тех, у кого она есть. `watch_list_t` получил поля `include_children` (по умолчанию `true`) и `breakdown` (по умолчанию `false`). **Поведение изменилось до выпуска 2.0.0.0:** `sample(pid)` и `sample_by_name(name)` теперь по умолчанию учитывают потомков; один процесс дает `include_children = false`.

**Проверено:** GCC 13.2, Release: 48 тестов `LumexProcessMonitorTest` и `LumexProcessDetailTest` проходят (на Linux потомки, внуки, сумма, разбивка и одно дерево на имя проверены на настоящих процессах через `fork`; правила дерева - на списках связей: устаревшая связь, петля, процесс - сам себе родитель, неизвестное время запуска, два корня друг у друга), четыре мутации (проверка времени запуска, отсев корней под другим корнем, игнорирование потомков в `sample`, пустой `members`) их ломают; библиотека собирается без предупреждений GCC 8.3, GCC 13.2, Clang 23.1.0 и MinGW 8.3 на C++11 и C++20; `format.py` с clang-format 21.1.7: 435 файлов соответствуют. Ветка Windows (Toolhelp32 и время создания процесса) собрана MinGW, но не запускалась.

##### `LumexResourceMonitor::start_with_watch_list`: процессы в журнале монитора

**Файлы:** `lumex/applied/resource_monitor/monitor/LumexResourceMonitor.hpp`, `LumexResourceMonitor.cpp`, `monitor/detail/LumexWatchLog.hpp` (новый), `LumexWatchLog.cpp` (новый), `lumex/tests/applied/resource_monitor/LumexWatchLog.cxx11.tests.cpp` (новый), `LumexResourceMonitor.cxx11.tests.cpp`, `lumex/examples/resource_monitor/example_resource_monitor.cpp`

**Суть:** `LumexResourceMonitor::start_with_watch_list(logDirectory, watch_list_t, pollInterval)` запускает тот же фоновый сэмплер и дописывает в каждую строку журнала после системной части по записи ` | ` на каждый пункт списка: `name[pid] 3.1% 210.4Mb` (доля CPU всей машины и резидентная память), `name[pid +2] ...` (два процесса под ним сложены), `name[x6] ...` (сумма шести процессов с таким именем), `777 exited`, `777 access denied`, `777 unreadable` (процесс не прочитан), `name[x0] not running` (нет такого имени), `n/a` вместо доли, которой еще нет. Базовый замер процессов берется вместе с системным, поэтому в первой строке доли уже есть. `watch_list_t::include_children` (по умолчанию `true`) решает, складываются ли процессы под каждым пунктом, `watch_list_t::breakdown` (по умолчанию `false`) выводит под строкой замера по строке на каждый процесс суммы с отступом в четыре пробела: `4390 python3 4.0% 800.1Mb`. Пустой список дает те же строки, что `start_if_enabled`, а его формат не менялся. Форматирование записей - чистые функции `monitor/detail/LumexWatchLog.hpp`. Пример `example_resource_monitor` показывает потомков процесса и список наблюдения. Закрывает пункт 84 (a) списка задач.

**Проверено:** GCC 13.2, Release: 88 тестов набора `LumexResourceMonitorCxx11Tests` (15 новых на форматирование, 4 на настоящий журнал: записи, пустой список, разбивка с `fork`-потомком, пункт без потомков) и `ctest -R 'resource_monitor|ResourceMonitor|lint\.'`: 97 тестов проходят; три мутации (флаг `include_children` игнорируется, нет базового замера, строки разбивки без отступа) ломают тесты; библиотека без предупреждений на GCC 8.3, GCC 13.2, Clang 23.1.0 и MinGW 8.3 при C++11 и C++20; `format.py` с clang-format 21.1.7: 438 файлов соответствуют; пример запущен. Ветка Windows (Toolhelp32) собрана MinGW, не запускалась.

##### Параметр CMake `LUMEX_WERROR`: предупреждения как ошибки

**Файлы:** `cmake/LumexOptions.cmake`, `cmake/LumexBuild.cmake`, `CMakeLists.txt`, `lumex/tests/cmake/cases/wiring_werror.cmake` (новый), `lumex/tests/cmake/cases/options_declared.cmake`, `lumex/tests/cmake/CMakeLists.txt`

**Суть:** новая опция `LUMEX_WERROR` (по умолчанию `OFF`). При `ON` каждая библиотечная цель получает `-Werror` (`/WX` на MSVC и clang-cl) сразу после включения предупреждений уровня `HIGH`, поэтому новое предупреждение останавливает сборку. Тесты и примеры, у которых предупреждения выключены целиком, не затрагиваются. Опция не зависит от `CMAKE_CXX_STANDARD` (прежний `WERROR` из CMakeRoutines работал только для целей с закрепленным стандартом и был зашит в `OFF`).

**Проверено:** при `ON` `-Werror` стоит в 69 из 73 единиц трансляции библиотеки (остальные 4 - исходники вендорного GoogleTest) и ни в одной единице тестов и примеров, при `OFF` его нет нигде; внесенная в `Encoder.cpp` неиспользуемая переменная останавливает сборку при `ON` и дает только предупреждение при `OFF`; новый кейс `cmake.wiring_werror` падает, если переименовать условие `if(LUMEX_WERROR)` (GCC 13.2).


##### `create_release.sh`: MinGW, `-Werror`, `--no-package`, журналы предупреждений, стандарт в имени пакета, `--dry-run`

**Файлы:** `create_release.sh`, `create_release.ps1`, `Scripts/ReleaseTools/extract_diagnostics.py` (новый), `lumex/tests/cmake/cases/release_diagnostics_script.cmake` (новый), `lumex/tests/cmake/cases/release_script_options.cmake` (новый), `lumex/tests/cmake/cases/release_script_dry_run.cmake` (новый), `lumex/tests/cmake/fixtures/release_diagnostics/` (новые), `lumex/tests/cmake/CMakeLists.txt`

**Суть:** (1) Компилятор MinGW-w64 (`x86_64-w64-mingw32-g++-posix`, нужны потоки posix) собирает DLL под Windows; скрипт сам пишет файл toolchain, кладет рядом с DLL библиотеки рантайма MinGW (`libstdc++-6`, `libgcc_s_seh-1`, `libwinpthread-1`, зависимости находит через `objdump`) и упаковывает все в `LumexLib-<версия>_win_x64_mingw<версия>_cxx<стандарт>.zip`; `--formats` на него не действует. (2) Сборка идет с `-DLUMEX_WERROR=ON`, `--no-werror` отключает. (3) `--no-package` только собирает и проверяет предупреждения, без установки и упаковки и без инструментов упаковки; стандарт задается `--std` (и `-Std` в `create_release.ps1`): один или список через запятую, `--std 11,14,17,20`, на каждый стандарт своя сборка и свой пакет. (4) Предупреждения и ошибки каждой сборки лежат в `<output-dir>/warns/<компилятор>_c++<стандарт>_warn.log` и `_err.log`, файл создается только если в нем что-то есть; предупреждение, которое `-Werror` сделало ошибкой, лежит в `_warn.log`. (5) Сборка, которая упала, не останавливает остальные: ошибки в `_err.log`, итоговый код 1. (6) Для `--std 20` на GCC 8 и MinGW 8.3, где есть только `-std=c++2a`, скрипт собирает черновик и пишет об этом. (7) Цели `package` и `package_source` (CPack) больше не входят в сборку библиотек; языком сообщений компиляторов принудительно служит `LC_ALL=C`, чтобы разбор не зависел от локали. (8) Пример в справке называет все четыре компилятора. (9) Стандарт, с которым собрана библиотека, входит в имя пакета: `_cxx11`, `_cxx14`, `_cxx17`, `_cxx20`, а для черновика `_cxx2a` и `_cxx2b`; потребителю он нужен для совместимости ABI. Без `--std` в имя идет стандарт по умолчанию компилятора (у `create_release.ps1` - 20, как в `compile.py`). Во внутренних путях deb и rpm (`Package:`, каталог установки) стандарта нет. (10) `--dry-run` показывает все команды, которые скрипт запустил бы, и ничего не запускает и не пишет: нет ни выходного каталога, ни деревьев сборки, ни журналов, ни worktree для `--tag`; проверки, от которых зависит план (запускается ли компилятор, линкуется ли, знает ли он стандарт), выполняются по-настоящему. В конце список пакетов, которые были бы созданы. (11) Каждая команда показывается циановой строкой после `$` (длинная - по одному аргументу на строке, ее можно скопировать в терминал), ее вывод идет цветом терминала; `--quiet` убирает вывод команд, оставляя команды и итог каждой сборки (вывод в `<output-dir>/.work/<сборка>/*.log`). Итог сборки зеленый при `0 warning(s), 0 error(s)`, желтый с предупреждениями, красный с ошибками; `--color auto|always|never` (по умолчанию `auto`: цвет только в терминале, учитывается `NO_COLOR`). (12) `--use-ninja` добавляет `-G Ninja` и требует `ninja`; без него генератор выбирает CMake (`$CMAKE_GENERATOR`, иначе Unix Makefiles, нужен `make`), раньше Ninja был зашит. Пакеты от генератора не зависят; после сбоя Ninja собирает больше (`make` не собирает библиотеки, зависящие от упавшей), поэтому один неудачный прогон с `--use-ninja` показывает больше диагностик. Для `make` скрипт передает `-k -Otarget`: вывод задачи печатается целиком, строки диагностики параллельных задач не перемешиваются; `Scripts/ReleaseTools/extract_diagnostics.py` не включает в журналы строки `[ 20%] Linking`, `Built target` и `make[2]: *** ... Error 1`. (13) Число параллельных задач (по умолчанию все ядра) задается `-j N`, `-jN`, `--jobs N`, `--parallel N` и формами `--jobs=N`, `--parallel=N`; значение идет в `cmake --build --parallel N`, `-j0` и нечисло отвергаются; кейс `cmake.release_script_dry_run` проверяет все шесть форм, три мутации разбора его ломают. (14) `create_release.ps1` получил `-DryRun` (те же правила: команды показываются, ничего не запускается и не пишется; проверки набора инструментов, компиляторов, NSIS, tar и тега выполняются, для `-Tag` версия берется из `git show` тега), `-Quiet` и `-NoColor` (учитывается `NO_COLOR`): каждая команда - голубая строка после `$` (длинная - по одной опции со значением на строку, с продолжением обратной кавычкой), ее вывод выводится и в журнал (`Tee-Object`), цвет консоли не меняется; `Compress-Archive` и `Copy-Item` показываются так же, как внешние команды; заголовок сборки `== [n/N] ...`, пакеты помечаются `(would be created)`.

**Проверено:** GCC 8.3, GCC 13.2, Clang 23.1.0 и MinGW 8.3 на C++11, 14, 17 и 20 с `-Werror`: 16 сборок, 0 предупреждений и 0 ошибок, `warns/` пуст; внесенное в `Encoder.cpp` предупреждение: с `-Werror` скрипт завершается с кодом 1 и пишет `_warn.log` и `_err.log` (у GCC и Clang), с `--no-werror` код 0 и только `_warn.log`; полная упаковка (`tar.gz` для GCC 13.2 и zip для MinGW): в zip 346 файлов, 16 DLL библиотеки и 3 DLL рантайма; разбор журналов покрыт кейсом `cmake.release_diagnostics_script` (шесть фрагментов реальных журналов GCC, Clang и MinGW), две мутации разбора его ломают; кейс `cmake.release_script_options` проверяет строки параметров; кейс `cmake.release_script_dry_run` (только на хосте Linux: скрипт читает версию glibc, а `CMAKE_HOST_UNIX` истинна и в MSYS2 и Cygwin; на остальных хостах он пропускается) запускает скрипт с `--dry-run` на компиляторе хоста и проверяет число сборок и имена пакетов для `--std 11,17`, циановую команду при `--color always`, отсутствие цвета в пайпе, отсутствие выходного каталога и отказ при `--color purple`, шесть мутаций скрипта его ломают; реальный прогон на GCC 8.3 с `--std 11`, `--formats tar.gz`, `--color always` (14 с, 0 предупреждений, пакет `..._cxx11.tar.gz`) и сборка с `-Wpadded` в обертке компилятора (красная строка `build FAILED because of the warnings`, код 1); DLL на Windows не запускались. `create_release.ps1` не запускался (на этой машине нет PowerShell): разбор проверен только глазами, балансом скобок и лексером Pygments (0 ошибок, как у прежнего файла), строки параметров проверяет `cmake.release_script_options`; `-DryRun`, `-Quiet` и цвет нужно один раз прогнать на Windows. `--use-ninja` и сборка по умолчанию (Unix Makefiles): GCC 8.3 на C++11, `tar.gz` - оба пакета содержат одинаковые 374 файла и 0 предупреждений (13 с против 25 с при `--jobs 4`), MinGW 8.3 - zip с 346 файлами, Clang 23.1.0 и GCC 13.2 на C++17 - 0 предупреждений; сборка с `-Wpadded` и `--no-werror` дает 94 предупреждения при Ninja и при `make`, прежний разбор оставлял в `_warn.log` 6 строк прогресса `make`, кейс `cmake.release_diagnostics_script` с двумя новыми фрагментами журнала `make` падает на прежнем разборе.

##### `lumex_string_view` и `std::string_view` преобразуются друг в друга неявно с C++17 (то же для `lumex_wstring_view` и `std::wstring_view`)

**Файлы:** `lumex/core/string_view/view/LumexStringView.hpp`, `lumex/core/string_view/view/LumexWStringView.hpp`, `lumex/core/base64/LumexBase64`, `lumex/core/crc/LumexCrc`, `lumex/xml/LumexXml`, `lumex/tests/LumexTestStandards.cmake`, `lumex/tests/core/string_view/view/CMakeLists.txt`, `lumex/tests/core/string_view/view/LumexStringViewStd.cxx17.tests.cpp` (новый), `lumex/tests/core/base64/encode/LumexBase64Encoder.cxx17.tests.cpp`, `lumex/tests/core/base64/decode/LumexBase64Decoder.cxx17.tests.cpp`, `lumex/tests/core/base64/validate/LumexBase64Validator.cxx17.tests.cpp`, `lumex/tests/core/crc/catalog/LumexCrcCatalog.cxx17.tests.cpp`, `lumex/tests/core/exceptions/exception/LumexException.cxx17.tests.cpp`, `lumex/tests/xml/LumexXml.cxx17.tests.cpp`

**Суть:** как `span` и `std::span`. С C++17 представление библиотеки неявно преобразуется в стандартное и строится из него (конструктор и оператор преобразования шаблоны, которые принимают ровно `std::string_view` / `std::wstring_view`). Шаблонные члены не входят в экспортируемый интерфейс класса и не импортируются классом `dllimport`, поэтому библиотека `string_view`, собранная на C++11, обслуживает потребителя на C++17: новых экспортов и ребра линковки нет. Теперь аргумент `lumex_string_view` принимают перегрузки со строкой `base64`, `crc`, `exceptions` и `xml` и на C++17 и выше (там параметр `std::string_view`), а `std::string::append` и `+=` принимают представление библиотеки. Раз представления преобразуются друг в друга, сравнение `lumex_string_view` со `std::string_view` стало бы неоднозначным между операторами библиотеки и стандартными; для `==`, `!=`, `<`, `>`, `<=`, `>=` добавлены ограниченные шаблоны, которые принимают ровно стандартное представление с любой стороны и выигрывают разрешение перегрузок; сравнения со строкой C и `std::string` идут к прежним операторам. Не принимается по-прежнему: преобразование в `std::string` (явное), узкого представления в широкое и обратно, `lumex_string_view` в `std::wstring_view`.

**Проверено:** 12 новых тестов `string_view` (`LumexStringViewStd`, `LumexStringViewPortable`) и по одному тесту аргумента `lumex_string_view` в `base64` (кодирование, декодирование, проверка), `crc`, `exceptions` и `xml`; наборы `string_view` теперь идут на C++11, 17 и 20. Тесты `string_view`, `base64`, `crc`, `exceptions` и `xml` на всех их стандартах, GCC 13.2 Release: 412, 308, 2599, 174 и 624 теста, все проходят, кроме нестабильного `exceptions.exception...ToCrashReport_ThreadSafe` (он ждет сообщения всех пяти потоков в первом файле каталога отчетов, падает не в каждом запуске, в том числе поодиночку, и падал до изменений). GCC 8.3 (C++11, 17, `-std=c++2a`), Clang 23.1.0 с libc++ и с libstdc++ 13: `string_view` вместе с `filesystem` 437, 667 и 667 тестов, остальные четыре модуля 596, 1210 и 1210, все проходят. ASan и UBSan (GCC 13.2, Debug): 437 и 1210 тестов, чисто, кроме того же теста. Проверка правкой: 10 порч (размер и указатель в преобразовании и в конструкторе, перевернутый, отрицаемый и ослабленный операторы сравнения со стандартным представлением в обоих порядках, преобразование без ограничения на тип, широкое представление вместо узкого) роняют тесты на C++17 и на C++20. Предупреждения: `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast` на новых и измененных тестах, на `LumexException.cpp`, `LumexFilesystem.cpp` и потребителе `standard_mismatch` (76 компиляций: GCC 8.3 C++11 и 17, Clang 23.1.0 C++11, 17 и 20, GCC 13.2 C++11, 14, 17 и 20) - ни одного. Остальные пользователи представлений (`fmt`, `json`, `string`, `utility.traits`, `resource_monitor`, `settings`, `logger`, `logging`; 2097 тестов на C++11, 17 и 20) на GCC 13.2 и Clang с libc++ не получили новых падений: прежние падения `fmt` (сравнение со `std::format` стандартной библиотеки на C++20 и `GivenHexTypeAtLimits` на C++17 и выше) те же на родительском коммите. `cmake.*` и `lint.*`: 149 тестов проходят. Doxygen 1.8.20: в измененных файлах предупреждений нет.

##### `lumex::path` и `std::filesystem::path` преобразуются друг в друга неявно с C++17

**Файлы:** `lumex/core/filesystem/fs/LumexFilesystem.hpp`, `lumex/core/filesystem/LumexFilesystem`, `lumex/tests/LumexTestStandards.cmake`, `lumex/tests/core/filesystem/fs/CMakeLists.txt`, `lumex/tests/core/filesystem/fs/LumexFilesystemStdPath.cxx17.tests.cpp` (новый)

**Суть:** там, где есть `<filesystem>` (C++17 и выше), `lumex::core::filesystem::fs::path` неявно преобразуется в `std::filesystem::path` и строится из него; макрос `LUMEX_HAS_STD_PATH_CONVERSION` равен 1 (иначе 0). Конструктор и оператор преобразования это шаблоны членов, которые принимают ровно стандартный путь: они встроенные, не экспортируются и не импортируются классом `dllimport`, поэтому библиотека, собранная на C++11, обслуживает потребителя на C++17 (новых экспортов и ребра линковки нет). Узкая строка пути это UTF-8 и она же родная узкая строка стандартного пути на POSIX, поэтому байты сохраняются (в том числе не ASCII); на Windows преобразование идет через `wstring` и `from_wide_string`. Операторы `==`, `!=`, `<`, `<=`, `>`, `>=` и `/` между двумя классами добавлены ограниченными шаблонами (сравнение по правилам `path`, `/` возвращает тип левого операнда), а `/=` и `+=` получили шаблоны для стандартного пути, поэтому ни одно выражение не неоднозначно между двумя библиотеками; перегрузки с `std::string` и строкой C работают как раньше.

**Известные ограничения** (описаны в заголовке и закреплены тестом): `std_path = lumex_path;` на POSIX неоднозначно (раньше собиралось через преобразование `path` в `std::string`; нужно написать `std_path = std::filesystem::path (lumex_path);`, на Windows присваивание работает), а с libstdc++ 8 неоднозначна любая прямая инициализация стандартного пути из `path` (`std::filesystem::path p (lumex_path)`, `emplace_back`); копирующая инициализация и `push_back` работают везде.

**Проверено:** 13 новых тестов `LumexFilesystemStdPath` на C++17 и C++20 (преобразование и построение в обе стороны, круговые прогоны, пустой путь, завершающий разделитель, путь не ASCII на Linux с проверкой байтов, все шесть сравнений и `/` в обоих порядках, `/=` и `+=`, перегрузки рядом со строкой C и `std::string`, контейнеры, вызовы библиотеки со стандартным путем и обратно на диске, потоки); наборы `filesystem` теперь идут на C++11, 17 и 20 (для GCC ниже 9 к наборам C++17 и C++20 подключается библиотека `std::filesystem`). GCC 13.2 Release: 255 тестов `filesystem`, все проходят; `string_view` вместе с `filesystem`: GCC 8.3 437, Clang 23.1.0 с libc++ 667 и с libstdc++ 13 667, ASan и UBSan 437, все проходят. Проверка правкой: 11 порч (лишний символ в обоих преобразованиях, потерянный путь в конструкторе, пустой результат преобразования, отрицаемое и перевернутые сравнения, `/` в обратном порядке, `/=` без разделителя, `+=` с разделителем) роняют тесты на C++17 и C++20; порча «`std >= lumex` всегда истина» сначала выжила и закрыта проверками четырех соседних сравнений. Предупреждения: `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast` на новом тесте - ни одного на GCC 8.3 (C++17), Clang 23.1.0 и GCC 13.2 (C++17 и 20). Ветка Windows (преобразование через `wstring` и `from_wide_string`) проверена компиляцией MinGW 8.3 на C++17, но не запускалась. Цена включения `<filesystem>`: `LumexFilesystem.hpp` дороже на 113 мс на единицу трансляции на C++17 (628 -> 741 мс) и на 30 мс на C++20 (GCC 13.2, `-fsyntax-only`, среднее трех запусков); на C++11 заголовок не меняется. `nm -D` библиотеки `filesystem` не изменился (148 символов), таблица экспорта MinGW-DLL тоже. `cmake.*` и `lint.*`: 149 тестов проходят.

#### Изменено

##### Меньше наборов тестов: 27 каталогов собираются на 11, 17, 20 и только на нужных стандартах

**Файлы:** `CMakeLists.txt` каталогов `lumex/tests/core/utility` (корень, `attr`, `bit`, `callback`, `cast`, `debug`, `demangle`, `dump`, `functional`, `mem`, `numeric`, `os`, `process`, `ranges`, `sequence`, `traits`, `util`), `lumex/tests/core/hazard_pointer` (корень, `base`, `engine`, `holder`), `lumex/tests/applied/logger/config`, `lumex/tests/core/reflection/var_info`, `lumex/tests/core/string/text`, `lumex/tests/core/crc/catalog`, `lumex/tests/core/expected/error`, `lumex/tests/core/optional/opt`

**Суть:** пункт 86в списка дел, решение пользователя от 2026-10-08. Для каждого каталога тестов сравнивался текст после препроцессора собственных файлов каталога и исходников, которые он проверяет (`lumex/<тот же путь>/`, без подкаталогов), на отбрасываемом стандарте и на нижнем (та же команда компиляции, меняется только `-std=`; GCC 13.2 для 14 и 23, Clang 23 для 23 и 26); поиск `__cplusplus`, `LUMEX_HAS_*`, `LUMEX_CONSTEXPR_CXX14`, `cxx14` / `cxx23` / `cxx26` по тем же файлам. Стандарт отброшен, только если текст совпал и порогов нет. Результат: `utility` (корень, `callback`, `cast`, `debug`, `demangle`, `dump`, `mem`, `numeric`, `os`, `process`, `util`), `hazard_pointer` (4 каталога), `logger/config`, `reflection/var_info`, `string/text`, `crc/catalog` - 11 17 20; `utility/bit` и `optional/opt` - 11 17 20 23; `utility/attr` и `utility/ranges` - 11 14 17 20 23; `utility/functional`, `sequence`, `traits` и `expected/error` - 11 14 17 20. Каталог остается на 14, если там есть файл `.cxx14`, другой текст после препроцессора (переменные шаблоны `_v`, `constexpr`) или порог 201402; на 23, если есть `.cxx23`. Не сужались: `utility/macros` (в `LumexKeywords.hpp` пороги `>= 201402L` и `>= 202303L`; второй на практике срабатывает только на C++26 у Clang, у GCC 13 `-std=c++23` дает 202100L, у Clang 202302L - ни один тест каталога эти макросы не раскрывает), `fmt`, `logger/logger`, `reflection/field_reflection`, `reflection/reflected_enum`, `crc/parametric`, `string/utility`, `expected/result`, `base64`, `span` и остальные, у которых текст на 14 отличается или есть файлы этого стандарта. Пример пользователя - `utility/macros` на 11 17 20 - не подтвердился по этой причине.

**Проверено:** GCC 13.2 Release, `ninja -j4` с нуля, полный `ctest -j4`, до и после (после - с тремя новыми наборами `math.constants.`): исполняемых файлов тестов 262 -> 229 (убрано 36, добавлено 3), шагов сборки 1285 -> 1163, сборка 1486 с -> 1380 с (-7 %), тестов в CTest 25641 -> 21487 (убрано 4353: 2281 на `.cxx14` и 2072 на `.cxx23`; добавлено 199: 197 `math.constants.` и 2 `cmake.`), `ctest -j4` 190 с -> 196 с (добавились новые тесты, разница в пределах шума). Ни одно имя теста не потеряно: каждое убранное имя без суффикса стандарта есть в новом списке на оставленном стандарте. Падения до и после одни и те же (`reflection.field_reflection` 83 - пункт 103, `fmt` 9, `exceptions.exception` 2-3 нестабильный `ToCrashReport_ThreadSafe`, `serial.probe` 1; примеры не собирались, так как собирались только цели `*Tests`); новых падений нет. На Clang, где есть C++26, дополнительно уходят 17 наборов C++26 каталогов `utility` (`utility/macros` остается на всем ряду); дерево Clang 23 теперь 230 исполняемых файлов. `cmake.*` и `lint.*` проходят целиком.

##### `LumexTypeTraits.hpp` больше не включает `<array>`

**Файлы:** `lumex/core/utility/traits/LumexTypeTraits.hpp`, `lumex/tests/core/reflection/reflected_enum/LumexReflectedEnum.cxx11.tests.cpp`

**Суть:** решение пользователя 2026-10-08. Заголовок включал `<array>`, но сам `std::array` не использует; через него `<array>` получали заголовки, которые его не включали (заголовки `xml` уже исправлены веткой `refactor/xml-cleanup`, правило закреплено случаем `cmake.source_std_array_included`). Включение удалено. Остальные файлы дерева (`lumex/`, тесты, примеры, `benchmarks/`, `test_package/`) проверены поиском `std::array` без `#include <array>`: единственный случай - макрос `LUMEX_DEFINE_REFLECTED_ENUM` разворачивается в `std::array` в тестовом файле `LumexReflectedEnum.cxx11.tests.cpp`; в него добавлено `#include <array>` (заголовок `LumexReflectedEnum.hpp` свое включение имеет). Остальные четыре совпадения - упоминания в комментариях. Другие файлы править не пришлось. Вне библиотеки код, который получал `std::array` транзитивно через `LumexTypeTraits.hpp`, должен включить `<array>` сам (libc++ и STL MSVC этого не дают).

**Проверено:** полная сборка всех целей GCC 13.2 Release (329 целей: библиотеки, тесты, примеры, бенчмарки; без целей `*BenchmarkRun` и целей, пишущих в рабочее дерево), 0 ошибок; ctest `utility.traits.`, `string.`, `fmt.`, `reflection.`, `lint.`, `cmake.` (2740 тестов): все проходят, кроме 9 известных (`fmt`: сравнения с `std::format` на C++20, `GivenHexTypeAtLimits`) и 83 известных `reflection.field_reflection` на GCC 13 (имена полей, отдельная задача). Синтаксическая проверка (`-fsyntax-only`) каждой единицы трансляции базы компиляции (библиотека, тесты, примеры, бенчмарки, GoogleTest): Clang 23.1 с libc++ 23 - 954 из 954, Clang 23.1 с libstdc++ 13 - 954 из 954, GCC 8.3 - 818 из 821, MinGW GCC 8.3 (кросс-компиляция, без запуска) - 818 из 821 (20 единиц `tests/core/exceptions` сначала не прошли на флаге `/Zi`, который CMake дает им для MinGW, и прошли после его удаления; к `<array>` это не относится); не проходят только три файла `benchmarks/atomic` на GCC 8 и MinGW (`#error "this unit must be built at C++20"`, `std::atomic::value_type`), к `<array>` отношения не имеют.

##### `stringify` отбрасывается через SFINAE во всех стандартах, `stringify_v2` стала переадресацией

**Файлы:** `lumex/core/string/utility/LumexStringify.hpp`, `lumex/core/reflection/var_info/LumexVarInfo.hpp` (комментарий), `lumex/tests/core/string/utility/` (`LumexStringifySfinae.cxx11.tests.cpp` - новый, `LumexStringifyV2.cxx11.tests.cpp`, `LumexString.cxx11.tests.cpp`, `LumexString.cxx20.tests.cpp`)

**Суть:** решение пользователя 2026-10-08. Ниже C++20 `stringify` была четырьмя вариантами с `static_assert` в теле, и вызов с аргументом без `operator<<` останавливал сборку внутри функции. Теперь у `stringify` одна форма `std::enable_if` над условием `detail::are_stringifiable<Args...>` во всех стандартах (с C++20 это концепт `AllStringifiable`, ниже - признак `traits::stream::all_streamable`): такой аргумент не находит перегрузки, и вызов обнаруживается SFINAE. Тело одно: свертка при `LUMEX_HAS_FOLD_EXPRESSIONS`, иначе массив `int expanded[]` (порядок слева направо в обоих случаях); перегрузка без аргументов (`noexcept`, без потока) осталась, `stringify<> ()` берет шаблон с пустым пакетом. Текст принятых списков не изменился. `stringify_v2` оставлена под тем же именем как переадресация: возвращаемый тип `decltype (stringify (...))`, собственного условия у нее больше нет, она существует ровно тогда, когда существует `stringify`. Неиспользуемый `#include "lumex/core/utility/assert/LumexAssert.hpp"` убран (он нужен был для `LUMEX_STATIC_ASSERT_MSG`). Замечание в `LumexVarInfo.hpp` о жестком `static_assert` обновлено. Изменение поведения: вместо ошибки `static assertion failed ... streamable` теперь `no matching function for call to 'stringify(...)'`. Не тронута `logger_stringify` модуля `logger` (у нее прежний `static_assert` ниже C++20; вопрос пользователю). Тесты: детектор `can_stringify` проверяет 26 допустимых и 19 отвергнутых списков, порядок вычисления слева направо (тип с журналом), `stringify<> ()`, `noexcept` пустой перегрузки, пользовательские перегрузки, выбираемые по возможности `stringify`; символьные типы следуют потоку (до C++17 `"65"`, с C++20 на измеренных библиотеках нет перегрузки); тест C++20 сверяет условие, концепт, признак и вызовы `stringify` и `stringify_v2`.

**Проверено:** GCC 13.2 Release, `utility.traits.`, `string.utility.`, `string.text.`: 1087 тестов на C++11-C++23, все проходят; GCC 8.3: 966; Clang 23.1 с libstdc++: 489; с libc++: 725; все проходят. 124 компиляции со строгими предупреждениями без новых диагностик. 11 мутаций, все пойманы, кроме одной эквивалентной: условие всегда истинно (ошибка сборки тестов), условие инвертировано (ошибка сборки), условие C++20 всегда истинно (ошибка сборки), перестановка аргументов в свертке и в массиве (34 теста на C++17 и C++11), пустая перегрузка без `noexcept` (ошибка сборки), `stringify_v2` возвращает пустую строку (9 тестов), у `stringify_v2` нет условия (3 теста), `stringify<> ()` отвергнут (ошибка сборки), последний аргумент отброшен (падение 17 тестов). Эквивалентна и потому не поймана замена концепта признаком `all_streamable` в условии C++20 (на libstdc++ 13 они совпадают на всех проверяемых типах, это и проверяет тест согласия).

##### `traits::stream::is_streamable<wchar_t>` следует стандартной библиотеке

**Файлы:** `lumex/core/utility/traits/LumexTypeTraits.hpp`, `lumex/tests/support/LumexOstreamProbe.hpp` (новый), `lumex/tests/core/utility/traits/` (`LumexStreamTraits.cxx11.tests.cpp`, `LumexStreamTraits.cxx20.tests.cpp`), `lumex/tests/core/string/utility/LumexString.cxx20.tests.cpp`, `lumex/tests/core/string/text/LumexTextWideChars.cxx11.tests.cpp` (новый)

**Суть:** у признака `is_streamable` была явная специализация `is_streamable<wchar_t> : std::true_type`, а с C++20 стандартная библиотека удаляет (`= delete`) вставку `wchar_t`, `char8_t`, `char16_t`, `char32_t` и указателей на них в узкий поток (P1423R3, заголовочный синопсис `<ostream>`, [ostream.syn]). Концепт `Streamable<wchar_t>` был ложен, признак истинен: признак и концепт расходились, и тест `LumexStreamTraitsSfinaeTest.GivenFundamentalTypes` требовал этого расхождения. Решение пользователя 2026-10-08: специализация `wchar_t` удалена. Теперь `wchar_t`, `char16_t`, `char32_t`, `char8_t` и указатели на них проверяет само выражение `os << value`, как и любой тип без специализации. Измерено (GCC 8.3 и libstdc++ 8, GCC 13.2 и libstdc++ 13, Clang 23.1 с libstdc++ 13 и с libc++ 23): с C++11 по C++17 вставка допустима везде (пишется число, для указателей - адрес, `stringify (L'A')` дает `"65"`), поэтому признак истинен; с C++20 (libstdc++ 13, libc++ 23) вставка удалена, признак ложен, как и концепт; libstdc++ 8 с `-std=c++2a` удаленных вставок не имеет и остается истинным; `std::wstring`, `std::u16string`, `std::u32string` ложны во всех стандартах; `std::nullptr_t` истинен с C++17 только там, где библиотека имеет `operator<<(nullptr_t)` (libstdc++ 12, libc++; libstdc++ 8 не имеет). Таблица записана в описании `is_streamable`. Явные специализации остались только для типов, которые принимает любая библиотека в любом стандарте. Пользователи признака: `stringify` (через `all_streamable`), `join` и `quote*` ниже C++20 (элементы и разделитель) - проверены, по `rg` `fmt` и `logger` его не используют (`logger_stringify` берет `is_ostreamable` и `AllStreamable`, то есть выражение), так что менять их не пришлось. До C++17 поведение не изменилось; с C++20 на библиотеке без концептов или `<ranges>` (GCC 8 с `-std=c++2a` ее не касается) `stringify` и `join` с `wchar_t` теперь не находят перегрузки в том же месте, где ее не находит концепт. Тесты: независимый пробник `lumex_tests_support::ostream_accepts` (то же выражение без признаков библиотеки) сравнивается с признаком для 9 символьных типов; таблица по стандартам (до C++17 истина и текст `"65"`, с C++20 на измеренных библиотеках ложь); тест согласия признака с концептом на 54 типах и 11 списках (`LUMEX_STREAM_TRAITS_AGREE`, C++20); `join` и `quote` с широкими символами.

**Проверено:** GCC 13.2 Release, `utility.traits.`, `string.utility.`, `string.text.`: 1087 тестов на C++11, 14, 17, 20, 23, все проходят; GCC 8.3 (C++11, 14, 17, `-std=c++2a`): 966 тестов, все проходят (тесты концептов пропускаются); Clang 23.1 с libstdc++ (C++11, 20): 489, с libc++ (C++11, 17, 20): 725, все проходят. 124 компиляции новых и измененных тестов со строгими предупреждениями (`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast`) на GCC 8, GCC 13, Clang с libstdc++ и с libc++, C++11-C++23, без новых диагностик. 5 мутаций признака, все пойманы: специализация `wchar_t` возвращена (4 теста на C++20), `wchar_t` всегда ложен (2 теста на C++11 и ошибки сборки тестов `stringify` и `join`), истинная специализация `char16_t` (4 теста на C++20), истинная специализация `std::nullptr_t` (1 тест), ложная специализация `wchar_t const *` (3 теста).
##### Несовместимо: `expected` как `std::expected`: специальные функции-члены, тривиальность, `constexpr`, `emplace` и присваивания по `reinit-expected`

**Файлы:** `lumex/core/expected/result/ExpectedStorage.hpp` (новый), `lumex/core/expected/result/Expected.hpp`, `lumex/core/expected/result/ExpectedVoid.hpp`, `lumex/core/expected/result/ExpectedDetail.hpp`, `lumex/core/expected/result/ExpectedTypes.hpp`, `lumex/core/expected/Expected` (комментарий), `lumex/tests/core/expected/result/Expected.cxx11.tests.cpp`, `lumex/tests/core/expected/result/ExpectedConversion.cxx11.tests.cpp` (`emplace` только для того, что не бросает)

**Суть:** пункт 89 списка дел (а, б, г) и непокрытая часть пункта 69. Решение пользователя от 2026-10-08: делать так, как говорят стандарт и его предложения. Нормативный текст: рабочий проект `[expected]` (eel.is/c++draft/expected, C++26), P0323R12, P2505R5, P2549R1 и задачи LWG 3687, 3703, 3754, 3836, 3843, 3866, 3877, 3886, 3891, 3938, 3940, 3951, 3973, 4025, 4026, 4031, 4141, 4222, 4366. Номера разделов записаны в комментариях заголовков.

Что было: конструкторы копирования и перемещения, присваивания и деструктор были написаны в самом классе вместе с ручным `union` и `placement new`. Поэтому `std::is_copy_constructible` и `std::is_copy_assignable` давали `true` для `expected<std::unique_ptr<int>, int>` (ошибка возникала внутри конструктора), класс не был тривиально копируемым и не работал в константных выражениях, а присваивание строило временный `expected` и менялось с ним через `swap`.

Что стало: хранилище вынесено в `ExpectedStorage.hpp`. Это именованный `union` из `val` и `unex` (его конструкторы начинают нужную альтернативу в списке инициализации, поэтому конструкторы `expected` могут быть `constexpr` уже в C++11), флаг `has_val` и цепочка базовых классов `expected_storage`, `expected_destruct_base`, `expected_copy_base`, `expected_move_base`, `expected_copy_assign_base`, `expected_move_assign_base` (техника libstdc++ и MSVC STL, потому что в C++11 нет `requires` и условно определяемых членов). У каждой из пяти специальных функций три варианта: оставлена компилятору (тривиальна, если тривиальна по условиям `[expected.object.cons]/10` и `/16`, `[expected.object.dtor]/2`, `[expected.object.assign]/5` и `/10`), написана в слое, удалена. Копирующие конструктор и присваивание удалены, пока не выполнены условия `[expected.object.cons]/9` и `[expected.object.assign]/4`. Перемещающие конструктор и присваивание в стандарте не удалены, а исключены условием Constraints (`/11`, `/6`): у слоя они `= delete`, поэтому такая же функция слоев выше и самого `expected` определена как удаленная, а разрешение перегрузок игнорирует удаленную функцию по умолчанию, и rvalue копируется. Удаление записано в самом слое, а не в дополнительном базовом классе: нетривиальная функция в базе под удаленной в производном классе делает `std::is_trivially_copyable` ложным, а удаленная в слое оставляет его истинным. `expected<void, E>` построен на том же слое с пустым `Unit` вместо значения, поэтому условия `[expected.void]` следуют из условий `[expected.object]` (у копирующего присваивания void нет условия про бросающее перемещение, у перемещающего это Constraints, как в LWG 4025). Присваивания и `swap` написаны по тексту стандарта: `reinit-expected` (`[expected.object.assign]/1`) с тремя ветками по `noexcept` конструирования и перемещения, таблица 72 (`[expected.object.swap]`) и таблица 73 (`[expected.void.swap]`) с восстановлением при исключении; у `expected<void, E>` ошибка строится без временного объекта, как в `[expected.void.assign]/1.2` и `[expected.void.swap]`.

Таблица "было -> стало":

| Что | Было | Стало |
| --- | --- | --- |
| `std::is_copy_constructible` и `std::is_copy_assignable` для `expected<std::unique_ptr<int>, int>`, `expected<int, std::unique_ptr<int>>`, `expected<void, std::unique_ptr<int>>` | `true` | `false` |
| `std::is_trivially_copyable` для `expected<int, int>`, `expected<void, int>` и других с тривиальными `T` и `E` | `false` | `true` |
| `std::is_trivially_destructible`, `is_trivially_copy_constructible`, `is_trivially_move_constructible`, `is_trivially_copy_assignable`, `is_trivially_move_assignable` | `false` | по условиям стандарта (выше) |
| `std::is_nothrow_move_constructible`, `std::is_nothrow_move_assignable` | по условиям, но присваивание не `noexcept` при бросающем перемещении | как `[expected.object.cons]/15` и `[expected.object.assign]/9` |
| копирующий конструктор и присваивание | без `noexcept` | `noexcept`, когда копирование `T` и `E` не бросает (строже стандарта, `[res.on.exception.handling]/5`, так же в libstdc++) |
| `emplace (args...)`, `emplace (list, args...)` | любой конструктор; временный `expected` и `swap` | только если `std::is_nothrow_constructible<T, Args...>` (`[expected.object.assign]/18`); `noexcept`; чтобы заменить значение бросающим конструктором, пишут `uut = expected (in_place, args...)` |
| присваивание другого `expected` | копия во временный `expected` и `swap` (нужен перемещаемый `T` и `E`) | `val = *rhs`, `unex = rhs.error ()` или `reinit-expected`; нет временного `expected` |
| `uut = value`, `uut = unexpected<G>` | то же через временный `expected` | `reinit-expected` и условие `is_nothrow_constructible<..> \|\| is_nothrow_move_constructible<T> \|\| is_nothrow_move_constructible<E>` (`/11`, `/15`) |
| аргумент шаблона по умолчанию в `expected (U &&)`, `operator= (U &&)` | `T` | `std::remove_cv_t<T>` (LWG 3886) |
| хранение `val` для `expected<const T, E>` | `const T` | `std::remove_cv_t<T>` (LWG 3891) |
| `value_or (U &&)`, `error_or (G &&)` | `static_cast` принимал и явные преобразования | `static_assert` (Mandates): `U` неявно преобразуется в `T`, `G` в `E` |
| `value ()` | `value () &&` переносил ошибку в исключение, `E` могла быть только перемещаемой | `static_assert` (Mandates, LWG 3843, 3940): `E` копируема, для `&&` и `const &&` еще и перемещаема |
| `error ()` (все четыре формы), `operator*`, `operator->` у `expected<T, E>` | `error ()` без `noexcept` | `noexcept`, предусловия проверяет `LUMEX_ASSERT` |
| `swap` (член и свободная функция) | всегда | только если `T` и `E` swappable, перемещаемы и один из них перемещается без исключения (`[expected.object.swap]/1`); свободная `swap` - по члену |
| `expected<void, E>::value ()`, `operator*` | по четыре перегрузки | как в стандарте: `value () const &`, `value () &&`, `operator* () const` |
| `has_error ()` | нет | есть (рабочий проект C++26, `[expected.object.obs]/8`, `[expected.void.obs]/2`; в C++23 нет) |
| `emplace_error (args...)` | при исключении объект мог остаться в недопустимом состоянии | `reinit-expected`: при исключении прежнее содержимое остается |
| `unexpect_t`, `in_place_tag` | неявный конструктор по умолчанию | `explicit`, как у `std::unexpect_t` и `std::in_place_t`: `unexpect_t tag = {};` не компилируется |
| внутренний `invoke_call` | `std::invoke` с C++17 | собственная реализация INVOKE во всех стандартах (`std::invoke` `constexpr` только с C++20) |

Что можно в константном выражении (то, что объявлено `constexpr` в стандарте, `constexpr` настолько, насколько позволяет язык; макрос `LUMEX_EXPECTED_CONSTEXPR_CXX20` из `ExpectedDetail.hpp` включается при `__cplusplus >= 202002L`, `__cpp_constexpr >= 201907L` и `__cpp_lib_constexpr_dynamic_alloc`):

| Стандарт | Добавляется |
| --- | --- |
| C++11 | конструкторы из значения, `in_place_tag`, `unexpect_t`, `unexpected`, по умолчанию; копирование и перемещение (и деструктор) для тривиальных `T` и `E`; `has_value`, `has_error`, `explicit operator bool`, `operator*`, `operator->`, `value`, `error`, `value_or`, `error_or` у const-объекта; все четыре монадические операции у const-объекта с функциональным объектом; `==` и `!=`. Функция-член `constexpr` в C++11 неявно const, поэтому не const члены `constexpr` быть не могут; `void` в C++11 не литеральный тип, поэтому у `expected<void, E>` `value ()` и `operator*` - с C++14 |
| C++14 | наблюдатели, `value_or ()`, `error_or ()` и монадические операции у не const объекта и у rvalue (расслабленный `constexpr`, N3652); `value ()` и `operator*` у `expected<void, E>`; тривиальное копирующее и перемещающее присваивание (union копируется целиком) |
| C++17 | монадические операции с лямбдой (constexpr-лямбды, P0170) и указателем на функцию-член |
| C++20 | то, что меняет активную альтернативу union или заканчивает ее жизнь: копирование и перемещение нетривиальных типов, преобразующие конструкторы из `expected<U, G>`, нетривиальный деструктор, все присваивания, `emplace`, `emplace_error`, `swap` (P0784, P1330, P1002, `std::construct_at`); GCC 8 с `-std=c++2a` этих возможностей не имеет, макрос пуст, функции обычные |
| C++23 | то же, что C++20 (для `expected` нового нет) |

Остаются осознанные отличия от `std::expected`: `error ()`, `operator*` и `operator->` проверяют предусловие через `LUMEX_ASSERT` и в сборке `NDEBUG` ("hardened preconditions" всегда включены); свои теги `in_place_tag` и `unexpect_t`; `noexcept` у конструкторов, копирования и присваивания строже, чем в стандарте (`[res.on.exception.handling]/5`); `==` и `swap` - шаблоны функций в пространстве имен, а не скрытые друзья, как в черновике; `emplace_error`, `rebind` (есть и в стандарте), `Unit`, `make_expected` и `success ()` / `failure ()` - дополнения.

Сравнение с `std::expected` из libstdc++ 13 (тест `ExpectedStdDifferential`) нашло отличия самой библиотеки, а не `lumex`: libstdc++ 13 не делает тривиальными копирующее и перемещающее присваивание (LWG 4026, C++26), для `bool` строит значение из всего `expected<int, E>` (LWG 3836), не накладывает условия Constraints на `==` (`[expected.object.eq]`, `[expected.void.eq]`). Тест записывает эти три отличия явно и сравнивает все остальное.

Размер и время компиляции: измерено на загруженной машине (лучшее из трех запусков, разброс порядка 15%), `-O2`, два файла: только `#include` зонтика и файл с восемью парами типов (`int`, структура, `std::string`, `std::vector<int>`, move-only) и всеми операциями. Подключение зонтика: GCC 13.2 190 -> 191 мс (C++11), 290 -> 291 (C++17), 379 -> 379 (C++20); GCC 8.3 149 -> 154, 227 -> 227; Clang 23 231 -> 191, 351 -> 278, 373 -> 376 (было -> стало), то есть не меняется. Файл с использованием: GCC 13.2 484 -> 567 мс (C++11), 603 -> 657 (C++17), 595 -> 685 (C++20); GCC 8.3 386 -> 470, 477 -> 545; Clang 23 506 -> 786 (C++11, выброс), 706 -> 590 (C++17), 626 -> 720 (C++20): около 10-17% на GCC из-за цепочки базовых классов, на Clang в пределах разброса. Размер `text+data` объектного файла: GCC 13.2 12183 -> 10073 байт (C++11), 12490 -> 10359 (C++17), 1032 -> 1040 (C++20, все свернуто); GCC 8.3 8493 -> 7712, 9085 -> 8348; Clang 23 7328 -> 8772, 7660 -> 9230, 9022 -> 9717.

**Проверено:** GCC 13.2 Release, `expected.`: C++11, 14, 17, 20, 23 - `Result` 560, 561, 568, 569, 591 тестов и `Error` 105, 105, 106, 106, 106 (было на C++11 478 и 105), все проходят; GCC 8.3 (C++11, 14, 17 и `-std=c++2a`): 560, 561, 568, 569 (один пропуск: constexpr-время жизни union на `-std=c++2a`) и 105, 105, 106, 106; Clang 23.1.0 с libc++ и с libstdc++ 13 на всех пяти стандартах: 560, 561, 568, 569, 591 и 105, 105, 106, 106, 106 - дифференциальный тест выполняется против `std::expected` обеих библиотек и проходит (записанные отличия libstdc++ там не нужны или не срабатывают, тест от них не зависит); ASan с UBSan (GCC 13.2, Debug): C++20 и C++23 все проходят (2 пропуска), C++11: 553 из 555 без смертных тестов проходят, смертные тесты `ExpectedDeathTest` с gtest 1.12.1 и `death_test_style=threadsafe` обрывают прогон так же и на коде до этой записи (проверено на минимальном тесте со старыми и новыми заголовками), к `expected` это не относится. 35 мутаций реализации (условия существования, тривиальности и `noexcept` каждой специальной функции, удаленные функции слоя, ограничение `swap`, три ветки `reinit-expected`, особые случаи `Unit` в присваивании и `swap`, порядок `swap`, ограничение `emplace`, `has_error`, макрос `constexpr` C++20, `constexpr` у значения, `operator*`, конструктора, сравнения и `invoke_call`, `explicit` у тегов, нетривиальный деструктор на тривиальном пути, удаление перемещения `void`): 34 пойманы (21 падением тестов, 13 ошибкой компиляции теста), одна эквивалентна (условие `is_trivially_destructible<T>` в тривиальном копирующем присваивании избыточно: `is_trivially_copy_constructible<T>` на GCC и Clang уже ложно для типа с нетривиальным деструктором); три мутации сначала выжили (нет типа с копирующим присваиванием без копирующего конструктора, нет типа с бросающим конструктором и `noexcept` присваиванием, шаблон правки не совпал с отформатированным кодом) - в зоопарк добавлены `assign_only_t` и `throwing_ctor_t`, после чего все три пойманы. Строгие предупреждения (`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast`, 150 компиляций тестов и примеров: GCC 8.3 и Clang 23 на C++11, GCC 13.2 на C++14, 17, 20, 23, Clang 23 на C++20 и 23): ни одного предупреждения в заголовках `expected`, предупреждения прежних тестов не затронуты, в новых файлах исправлены. `lint.*` (в том числе `lint.headers_standalone`), `cmake.*` (в том числе `cmake.wiring_standard_suites` с `.cxx23`) и `examples.expected.*` проходят (164 из 164 после перебазирования на релиз c0aa14caf); `Scripts/CodeTools/format.py` с clang-format 21.1.7: 590 файлов соответствуют, дважды; матрица `create_release.sh --std 11,20` на GCC 8.3, GCC 13.2, Clang 23.1.0 и MinGW: 8 сборок, 0 предупреждений, `warns/` пуст. Не проверялось: MSVC и clang-cl.

##### Несовместимо: у `make_unexpected` остался один вид - `make_unexpected<E> (args...)`, возвращающий `unexpected<E>`

**Файлы:** `lumex/core/expected/result/Expected.hpp`, `lumex/tests/core/expected/result/MakeUnexpected.cxx11.tests.cpp` (новый)

**Суть:** пункт 100 списка дел. В `Expected.hpp` рядом с фабрикой `make_unexpected<E> (args...)` (возвращает `unexpected<E>`, как `unexpected<E> (E (args...))`) были две устаревшие (`deprecated`) перегрузки с параметром `ErrorType &&`: `make_unexpected<T> (error)` возвращала `expected<T, std::decay_t<Error>>`, а `make_unexpected (error)` - `expected<void, Error>`. Они делали вызов `make_unexpected<int> (1)` неоднозначным с вариадической фабрикой (компилятор не мог выбрать между `make_unexpected<T, U_err>` и `make_unexpected<E, Args...>`), так что простейший вызов фабрики для скалярной ошибки не компилировался. Обе перегрузки удалены; вызовов их в `lumex/`, примерах и тестах не было (поиск `rg make_unexpected` по `lumex/`). Остальные глобальные имена зонтика (`expected`, `bad_expected_access`, `make_unexpected`, `unexpect`, `unexpect_t`, `in_place_tag`) остаются глобальными (решение пользователя). Это несовместимое изменение API: код 1.x, писавший `make_unexpected<T> (err)` или `make_unexpected (err)` ради `expected`, нужно заменить.

Таблица "было -> стало":

| Вид | Было | Стало |
| --- | --- | --- |
| функция | `make_unexpected<T> (error)` -> `expected<T, std::decay_t<Error>>` | `expected<T, E> (unexpect, error)` или `expected<T, E> (unexpected<E> (error))`; возвращаемое значение функции можно писать `return make_unexpected<E> (error);` |
| функция | `make_unexpected (error)` -> `expected<void, Error>` | `expected<void, E> (unexpect, error)` или `return make_unexpected<E> (error);` |
| функция | `make_unexpected<int> (1)` (ошибка компиляции: неоднозначность) | `make_unexpected<int> (1)` -> `unexpected<int>` |

**Проверено:** новый набор `MakeUnexpected` (6 тестов на каждом стандарте: `make_unexpected<int> (1)` без неоднозначности и тип результата, ошибка из нескольких аргументов, без аргументов, `E` без `const` и ссылки, преобразование результата в `expected<T, E>` и `expected<void, E>`, возврат из функции); возврат одной из удаленных перегрузок ломает сборку теста (`static_assert` на тип результата), GCC 13.2 Release, C++11: 484 теста `expected.result.` (6 новых) и 105 `expected.error.`, все проходят.

##### Контракт `serial_port_info_t::path`: имя порта по умолчанию, путь открытия по флагу `need_full_path` (PEW-2313)

**Файлы:** `lumex/applied/serial/enumeration/LumexSerialPortEnumeration.hpp`, `lumex/applied/serial/enumeration/LumexSerialPortEnumeration.cpp`, `lumex/tests/applied/serial/enumeration/LumexSerialPortEnumeration.cxx11.tests.cpp`

**Суть:** `enumerate_serial_ports_detailed` и `enumerate_serial_port_names` получили параметр `bool need_full_path = false`. По умолчанию `path` снова несет каноническое имя порта (`COM12` на Windows, `ttyACM0` на POSIX): форма, которую хранит конфигурация потребителя и принимает `resolve_serial_port_path`. При `need_full_path = true` `path` несет путь, готовый к открытию (`\\.\COM12` на Windows, `/dev/ttyACM0` на POSIX). Ограниченное открытие внутри перечисления в обоих режимах открывает путь; флаг выбирает только содержимое `path`. Это откат неявного изменения контракта из записи 1.0.3.0, из-за которого потребитель на POSIX получал `/dev/ttyACM0` там, где ожидал короткое имя.

**Проверено:** GCC 13.2 Release: `serial.*` 27 из 27 (наборы enumeration и port, включая новый `FullPathFlagSelectsNameOrOpenablePath`), пример `examples.serial.LumexSerialExample` собирается; GCC 8.3 и Clang 23.1.0: те же наборы собираются без предупреждений, 27 из 27. Мутация (флаг не читается): `FullPathFlagSelectsNameOrOpenablePath` падает.

##### Несовместимо: `lumex::path` преобразуется в `std::string` только явно

**Файлы:** `lumex/core/filesystem/fs/LumexFilesystem.hpp`, `lumex/applied/logging/log/LumexLogging.cpp`, `lumex/tests/core/filesystem/fs/LumexFilesystemStdPath.cxx17.tests.cpp`, `lumex/tests/applied/settings/guard/LumexSettingsGuard.cxx11.tests.cpp`, `lumex/tests/applied/settings/ini/LumexSettingsINI.cxx11.tests.cpp`, `lumex/tests/core/temporary/tmp/LumexTemporary.cxx11.tests.cpp`, `lumex/tests/cmake/consumer/path_compile_checks/` (новый: `CMakeLists.txt`, `check.cpp`), `lumex/tests/cmake/cases/wiring_path_compile_checks.cmake` (новый), `lumex/tests/cmake/CMakeLists.txt`

**Суть:** у `lumex::core::filesystem::fs::path` был неявный `operator string_type ()`. После преобразований к `std::filesystem::path` он делал `std_path = lumex_path;` неоднозначным на POSIX (запись «`lumex::path` и `std::filesystem::path` преобразуются друг в друга неявно с C++17»: допускалось только `std_path = std::filesystem::path (lumex_path);`), а с libstdc++ 8 неоднозначной была и прямая инициализация `std::filesystem::path` из `path` (из-за конструктора `path (string_type &&)`). Решение пользователя от 2026-10-08: оператор стал `explicit`. Теперь присваивание, прямая и копирующая инициализация, инициализация в фигурных скобках, `emplace_back` и `push_back` стандартного пути из `path` собираются на всех платформах и компиляторах (GCC 8.3 и libstdc++ 8 тоже); из пути в строку: `p.string ()`, `std::string (p)` или `static_cast<std::string> (p)`. Из строки и из строки C путь строится неявно, как раньше. Несовместимое изменение API (релиз `2.0.0.0`): код, передававший `path` туда, где нужна `std::string` (параметр `std::string const &`, `std::string s = p;`, `s = p;`, `s += p;`, `std::ofstream f (p);` до C++17, возврат из функции, возвращающей `std::string`), перестал собираться. Исправлено в библиотеке: `LumexLogging.cpp` (`get_logs_directory ()` в `std::string`); в тестах - вызовы `load`, `save`, `is_ini_valid` настроек и открытие файлов `std::ofstream` / `std::ifstream` (всего 55 мест: `settings.guard` 2, `settings.ini` 48, `temporary.tmp` 5); примеры, бенчмарки и остальные модули менять не пришлось (проверено сборкой). Смешанные операторы (`==`, `<`, `/` между `path` и `std::filesystem::path`) нужны по-прежнему: оба класса конвертируются друг в друга, и без ограниченных шаблонов выражение неоднозначно между операторами двух библиотек.

Таблица "было -> стало":

| Вид | Было | Стало |
| --- | --- | --- |
| оператор | `operator string_type () const` (неявный) | `explicit operator string_type () const` |
| инициализация | `std::string s = p;` | `std::string s = p.string ();` или `std::string s (p);` |
| присваивание | `s = p;`, `s += p;` | `s = p.string ();`, `s += p.string ();` |
| аргумент | `f (p)`, где `f (std::string const &)` или `f (std::string)` | `f (p.string ())` |
| возврат | `std::string g (path const &p) { return p; }` | `return p.string ();` |
| поток до C++17 | `std::ofstream f (p);`, `std::ifstream f (p);` | `std::ofstream f (p.string ());` |
| `std::filesystem::path`, POSIX | `q = p;` неоднозначно (писали `q = std::filesystem::path (p);`) | `q = p;` работает |
| `std::filesystem::path`, libstdc++ 8 | `std::filesystem::path q (p);` и `emplace_back (p)` неоднозначны | работают |
| `std::string` из `path` | неявно | только явно |
| `path` из `std::string` и строки C | неявно | неявно (без изменений) |

**Проверено:** новый `cmake.path_compile_checks` (GCC 13.2, C++11, 17 и 20: хороший случай и 7 плохих - копирующая инициализация строки, аргумент по ссылке и по значению, присваивание, возврат, `+=`, условное выражение - на каждый стандарт, 24 компиляции; плохой случай обязан упасть с ошибкой преобразования, не с `#error` и не с отсутствующим именем) и `cmake.wiring_path_compile_checks`; тот же `cmake.path_compile_checks` проходит на GCC 8.3, Clang 23.1.0 с libstdc++ и с libc++. `LumexFilesystemStdPath.cxx17.tests.cpp`: убраны макрос `LUMEX_TEST_DIRECT_INIT` (GCC ниже 9) и `static_assert` на неоднозначность; вместо них `static_assert`: `!is_convertible<path, std::string>`, `is_constructible<std::string, path>`, `is_assignable<std::filesystem::path &, path const &>`, `is_constructible<std::filesystem::path, path const &>`, а тесты выполняют присваивание, прямую инициализацию, `static_cast`, инициализацию в фигурных скобках и `emplace_back` без условий по компилятору. Проверка правкой: `explicit` снят - `cmake.path_compile_checks` падает на плохом случае 1 (компилируется), `cmake.wiring_path_compile_checks` падает, набор `filesystem` на C++17 не собирается (три `static_assert`: `!is_convertible<path, std::string>` и присваивание); `explicit` возвращен. Тесты, GCC 13.2 Release: `filesystem`, `temporary`, `settings`, `logging`, `exceptions`, `resource_monitor`, `hardware`, `serial`, `logger` - 981 тест, падают 3 известных (`serial.probe...ThenResponded`; `exceptions...ToCrashReport_ThreadSafe` на C++11 и C++20, нестабильный сам по себе); GCC 8.3 (C++11, 17, `-std=c++2a`) и Clang 23.1.0 с libstdc++ 13: те же модули и `unicode`, 1053 теста, падает только `serial` pty; Clang 23.1.0 с libc++: `filesystem`, `temporary`, `settings.ini` - 412 тестов, все проходят; ASan и UBSan (GCC 13.2, Debug): `filesystem`, `temporary`, `logging`, `unicode`, `settings.ini`, `settings.guard` - 487 тестов (6 `Perf_*` пропущены), чисто. Предупреждения `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast`: новые тесты `LumexFilesystemUnicode`, `LumexUnicodeConvertWideWidths`, `LumexFilesystemStdPath` и хороший случай `path_compile_checks` (29 компиляций: GCC 8.3 C++11 и 17, Clang 23.1.0 C++11, 17, 20, GCC 13.2 C++14, 17, 20) - ни одного. Матрица `create_release.sh --std 11,20` на четырех компиляторах по коммиту с этой правкой: 8 сборок, 0 предупреждений, 0 ошибок, `warns/` пуст. `cmake.*` и `lint.*`: 162 теста проходят на GCC 13.2 после перебазирования на релиз (до него 159 на GCC 13.2, GCC 8.3 и Clang 23.1.0 с libstdc++; на GCC 8.3 пропущен `cmake.format_compile_checks`). Ветка Windows (`StdPath (source.wstring ())`) компилируется MinGW, не запускалась; блоки `LUMEX_OS_WINDOWS` тестов этих модулей уже писали `.string ()`, но не собирались.

##### Несовместимо: режим `wchar_t` модуля xml удален (`LUMEX_XML_WCHAR_MODE`, `LUMEX_XML_CHAR`, `LUMEX_XML_TEXT`)

**Файлы:** `CMakeLists.txt`, `cmake/LumexOptions.cmake`, `lumex/xml/types/XmlTypes.hpp`, `lumex/xml/utility/XmlMacros.hpp`, `lumex/xml/utility/XmlUtils.hpp`, `lumex/xml/text/XmlParser.cpp`, `lumex/xml/text/XmlParser.hpp`, `lumex/xml/document/XmlDocument.cpp`, `lumex/xml/document/XmlDocument.hpp`, `lumex/xml/node/XmlNode.hpp`, `lumex/xml/attribute/XmlAttribute.hpp`, `lumex/xml/text/XmlText.hpp`, остальные файлы `lumex/xml/**`, где стоял `LUMEX_XML_TEXT`, `lumex/examples/xml/example_3.cpp`, `lumex/tests/xml/LumexXml.cxx11.tests.cpp`, `lumex/tests/xml/LumexXmlGlobalNames.cxx11.tests.cpp`, `lumex/tests/xml/LumexXmlStringView.cxx11.tests.cpp`, `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/cases/wiring_xml_no_wchar_mode.cmake` (новый)

**Суть:** параметр `LUMEX_XML_WCHAR_MODE` (по умолчанию выключен) собирал xml с `char_t = wchar_t`. Режим не тестировался, а зонтичный заголовок `LumexXml` в нем не собирался (`XmlUtils.hpp`, `set_value_convert`: `wchar_t *` не приводится к `char *`). По решению пользователя (MAJOR 2.0.0.0) режим удален целиком: параметр CMake и `add_compile_definitions` в корневом `CMakeLists.txt`, 23 ветви `#ifdef LUMEX_XML_WCHAR_MODE` в `XmlUtils.hpp`, `XmlParser.cpp`, `XmlParser.hpp`, `XmlDocument.cpp`, `XmlTypes.hpp` и `XmlMacros.hpp` (остались ветви узкого режима), три вспомогательные функции, нужные только ему, и макросы `LUMEX_XML_CHAR` и `LUMEX_XML_TEXT`, которые существовали только ради него. Широкие перегрузки ввода и вывода (`load (std::basic_istream<wchar_t> &)`, `load_file (wchar_t const *)`, `save`, `print` в `std::wostream`, `encoding_wchar`) остаются: это кодировка данных, а не режим сборки. `lumex_wstring_view` и `portable_wstring_view_t` остаются в модуле `string_view`.

`char_t` оставлен простым псевдонимом `using char_t = char;` (без зависимости от макроса): имя стоит в 714 вхождениях в `lumex/xml`, его замена на `char` тронула бы сотни строк, а потребители пишут `char_t const *` в своем коде. `string_view_t` теперь всегда `portable_string_view_t`.

Таблица "было -> стало":

| Вид | Было | Стало |
| --- | --- | --- |
| параметр CMake | `LUMEX_XML_WCHAR_MODE` (OFF) | удален; `-DLUMEX_XML_WCHAR_MODE=ON` ни на что не влияет |
| определение компиляции | `LUMEX_XML_WCHAR_MODE` (добавлялось корневым `CMakeLists.txt`) | нет |
| макрос | `LUMEX_XML_CHAR` (`char` или `wchar_t`) | `char` |
| макрос | `LUMEX_XML_TEXT ("текст")` (литерал с `L` в широком режиме) | обычный литерал `"текст"` |
| тип | `char_t` (`char` или `wchar_t`) | `char_t` = `char`, псевдоним сохранен |
| тип | `string_view_t` (`portable_string_view_t` или `portable_wstring_view_t`) | `portable_string_view_t` |
| функции `lumex::xml::utility` | `convert_wchar_endian_swap`, `need_endian_swap_utf`, `convert_buffer_endian_swap` (только широкий режим) | удалены |

**Проверено:** экспортируемые символы `libLumexXml.so` (GCC 13.2, Release, `nm -D --defined-only -C`) до правки (родительский коммит `eda337735`) и после: по 614 символов, `diff` пуст (удаленное было только в заголовках и во внутренних ветвях). Матрица `create_release.sh` (GCC 8.3, GCC 13.2, Clang 23.1, MinGW 8.3 posix; C++11 и C++20; 8 сборок): 0 предупреждений, 0 ошибок, каталог `warns/` пуст. GCC 13.2 Release: `xml.*` 628 тестов и 4 примера xml, плюс `lint.*` и `cmake.*` (155) - все проходят. Наборы C++11 `xml`, `settings` и `logger` на GCC 8.3 и Clang 23.1: по 418 пройденных, 0 упавших. ASan и UBSan (GCC 13.2, Debug): `xml.*` и примеры, 599 тестов, 0 упавших. Строгие предупреждения (`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast`, `-fsyntax-only`) на всех `.cpp` модуля xml и на измененных примерах и тестах: GCC 8.3 и Clang 23.1 на C++11, GCC 13.2 на C++14, 17 и 20 - пусто (кроме двух старых предупреждений о неиспользуемых переменных `pr` и `cnt` в `LumexXml.cxx11.tests.cpp:1006`, строки не менялись). Опыты с возвратом: параметр обратно в `LumexOptions.cmake`, `if(LUMEX_XML_WCHAR_MODE)` обратно в корневом `CMakeLists.txt`, `#ifdef LUMEX_XML_WCHAR_MODE` в `XmlTypes.hpp`, `#define LUMEX_XML_TEXT` и `char_t = wchar_t` и `string_view_t` из широкого представления - каждый ловит `cmake.wiring_xml_no_wchar_mode`; последние два еще и не компилируют `LumexXmlStringView.cxx11.tests.cpp` (ошибки в `XmlAttribute.hpp`).

##### XML: ветви для MSVC старше 1400 удалены, `LUMEX_XML_MSVC_CRT_VERSION` убран

**Файлы:** `lumex/xml/utility/XmlMacros.hpp`, `lumex/xml/document/XmlDocument.cpp`, `lumex/tests/cmake/cases/wiring_xml_no_wchar_mode.cmake`

**Суть:** макрос `LUMEX_XML_MSVC_CRT_VERSION` был `_MSC_VER` для MSVC и 1310 для Windows CE, а нужен был в двух местах `XmlDocument.cpp` (`_fseeki64` / `_ftelli64` в `get_file_size` и `fopen_s` в `open_file`) и только в проверке `>= 1400`. LumexLib поддерживает MSVC 2019 и новее (`_MSC_VER` 1920 и выше), поэтому проверка заменена на `defined(_MSC_VER)`, макрос удален. Для поддерживаемых компиляторов (MSVC, clang-cl, GCC, Clang, MinGW) поведение то же; Windows CE и Marmalade (`__S3E__`) шли по запасным веткам `fseek` / `fopen` и идут по ним же.

**Проверено:** `nm -D` экспорт `libLumexXml.so` не менялся (614 символов до и после, см. запись выше); матрица `create_release.sh` с MinGW 8.3 (там `_MSC_VER` не определен, ветвь `__MINGW32__` прежняя): 0 предупреждений. Опыт с возвратом: `#define LUMEX_XML_MSVC_CRT_VERSION` обратно в `XmlMacros.hpp` ловит `cmake.wiring_xml_no_wchar_mode`.

##### XML: заголовки включают то, что используют (`XmlNode.hpp`, `XmlWriterFile.cpp`, `<array>`)

**Файлы:** `lumex/xml/node/XmlNode.hpp`, `lumex/xml/writer/XmlWriterFile.cpp`, `lumex/xml/xpath/memory/XPathStack.hpp`, `lumex/xml/xpath/variable/XPathVariableSet.hpp`, `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/cases/source_std_array_included.cmake` (новый)

**Суть:** `XmlNode.hpp` и `XmlWriterFile.cpp` включали весь зонтик `lumex/core/utility/LumexUtility`. `XmlNode.hpp` использует из него только `LUMEX_ASSERT` (теперь `assert/LumexAssert.hpp`; макросы атрибутов уже включались), плюс `std::size_t` и `std::basic_ostream` (`<cstddef>`, `<iosfwd>`); `XmlWriterFile.cpp` не использует ничего. `XPathStack.hpp` и `XPathVariableSet.hpp` писали `std::array` без `#include <array>` и получали его через `LumexTypeTraits.hpp`, из которого неиспользуемый `<array>` будет убран. Новый тест `source_std_array_included` проверяет по всем файлам `lumex/` (кроме тестов и примеров), что каждый, кто пишет `std::array<` вне комментария, сам включает `<array>`; других нарушителей в коде нет (остальные упоминания в `LumexMemRead.hpp`, `LumexFormatRanges.hpp`, `Encoder.hpp`, `LumexCrcCatalog.hpp`, `Unexpected.hpp`, `Expected.hpp`, `ExpectedVoid.hpp` стоят в комментариях).

**Проверено:** `lint.include_order`, `lint.headers_standalone` и остальные `lint.*` проходят (GCC 13.2), 155 тестов `lint.*` и `cmake.*` без падений. Опыт с возвратом: удаление `#include <array>` из `XPathStack.hpp` ловит `cmake.source_std_array_included` (перечисляет файл). Экспорт `libLumexXml.so` не менялся.

##### Бесконечность упорядочивается по знаку во всех сравнениях `safe_comparator`

**Файлы:** `lumex/core/utility/numeric/LumexSafeNumericComparator.hpp`, `lumex/tests/core/utility/numeric/LumexSafeInfinityOrder.cxx11.tests.cpp` (новый), `lumex/tests/core/utility/numeric/LumexSafeThreeWayCompare.cxx11.tests.cpp`, `lumex/tests/core/utility/numeric/LumexSafeThreeWayCompare.cxx20.tests.cpp`

**Суть:** бесконечность упорядочивается по знаку для любой пары арифметических типов, в `safe_three_way_compare` (функция и член) и в шести сравнениях (`safe_less`, `safe_less_equal`, `safe_greater`, `safe_greater_equal`, `safe_equal`, `safe_not_equal`, а также `safe_compare`, то есть `>=`): `-inf < любое конечное значение < +inf`, `+inf == +inf`, `-inf == -inf`, целое число никогда не равно бесконечности (целое против целого, целое против плавающего, плавающее против другого плавающего типа, `long double` включительно). NaN остается неупорядоченным: все сравнения с ним ложны, кроме `safe_not_equal`, трехстороннее сравнение дает `unordered`. Так же ведут себя встроенные операторы после приведения к общему типу (`1 < INFINITY`, `-INFINITY < -DBL_MAX`, бесконечность `float` равна бесконечности `double`), а компаратор существует, чтобы сравнивать разные типы без сюрпризов приведения, поэтому исключением из этого правила ему быть не следует. Раньше порядок зависел от функции и от пары типов. Для двух разных типов с плавающей точкой специализация отдельно обрабатывала бесконечность: `safe_less`, `safe_greater` для нее всегда отвечали `false`, `safe_less_equal` и `safe_greater_equal` сводились к равенству, трехстороннее сравнение отвечало `unordered`, если бесконечности не равны; для целого и бесконечности трехстороннее сравнение отвечало `unordered`, а шесть булевых функций уже упорядочивали по знаку. Теперь специальной ветки для бесконечности у двух типов с плавающей точкой нет (после приведения к общему, более широкому типу, без потери значения, встроенное сравнение само упорядочивает бесконечности), а в трехстороннем сравнении целого и плавающего бесконечность дает `less` или `greater` по знаку. Поведение для двух значений одного типа и для конечных значений не менялось. Алгоритм C в комментарии заголовка и новый раздел `Infinities` описывают правило. В комментарии класса `safe_comparator` записано, что `safe_comparator<long double, true>` (то есть `std::atomic<long double>`, 16 байт) на x86-64 GCC требует `libatomic` (`__atomic_load_16`) и компонуется с `-latomic`; код не менялся.

Несовместимо по поведению (было -> стало; `finf`, `dinf` - бесконечности `float` и `double`):

| Выражение | Было | Стало |
|---|---|---|
| `safe_three_way_compare (7, dinf)` | `unordered` | `less` |
| `safe_three_way_compare (finf, 7)` | `unordered` | `greater` |
| `safe_three_way_compare (-finf, 1.0)` | `unordered` | `less` |
| `safe_three_way_compare (1.0f, dinf)` | `unordered` | `less` |
| `safe_three_way_compare (-finf, dinf)` | `unordered` | `less` |
| `safe_three_way_compare (finf, dinf)` | `equivalent` | `equivalent` (без изменений) |
| `safe_less (1.0f, dinf)` | `false` | `true` |
| `safe_greater (finf, 1.0)` | `false` | `true` |
| `safe_less_equal (-finf, 1.0)` | `false` | `true` |
| `safe_greater_equal (finf, 1.0)` | `false` | `true` |
| `safe_less (7, dinf)`, `safe_greater (finf, 7)` | `true` | `true` (без изменений) |

Тесты, которые закрепляли прежнее поведение и изменены: `LumexSafeThreeWayCompare.cxx11.tests.cpp` - функция `expected_float_code` удалена, `float_pair_check` и `mixed_pair_check` теперь ожидают точный порядок для бесконечности и проверяют шесть булевых функций на всех парах (параметр `all_booleans` функции `check_one_pair` удален), тест `LumexSafeThreeWayCharacterization.GivenInfinityAgainstAnotherType_WhenCompared_ThenUnorderedUnlessTheSameInfinity` заменен тестом `LumexSafeThreeWayCases.GivenInfinityAgainstAnotherType_WhenCompared_ThenOrderedBySign`; `LumexSafeThreeWayCompare.cxx20.tests.cpp` - `float_pair_check` больше не сверяет с `<=>` только пары без бесконечности, а `mixed_pair_check` не пропускает бесконечность. Тест `LumexSafeThreeWayCharacterization.GivenIntegerBeyondTheFloatMantissa_WhenCompared_ThenConvertedToTheFloatType` (целое после 2^24 приводится к `float`) не менялся.

**Проверено:** до правки 5 из 8 тестов нового набора `LumexSafeInfinityOrder` не проходят на GCC 13.2 (C++11): 1456 расхождений для пар двух типов с плавающей точкой, 288 для целого против плавающего и 256 для плавающего против целого (трехстороннее сравнение и члены, у пар двух типов с плавающей точкой еще булевы функции), пары целых типов и таблица NaN проходят; после правки проходят все 8. Набор: все пары из {NaN, -inf, min, -1, -0, 0, 1, max, +inf} (у целых типов без NaN, бесконечностей и -0, у беззнаковых без -1; `min` - наименьшее значение типа, для плавающих `lowest`) по `int`, `unsigned`, `long long`, `unsigned long long`, `float`, `double`, `long double` (49 пар типов); проверяются `safe_less`, `safe_less_equal`, `safe_greater`, `safe_greater_equal`, `safe_compare`, `safe_equal`, `safe_not_equal`, `safe_three_way_compare` (функция и члены обычного и, кроме `long double`, атомарного компаратора). Эталон математический и ничего не приводит к общему типу: NaN против чего угодно - `unordered`; бесконечность - ее знак (-1, +1), конечное число - 0, и пара с бесконечностью упорядочивается по этим знакам; два целых - знак и модуль; два плавающих - встроенные операторы над `long double`, в который `float` и `double` приводятся без потерь; целое против конечного плавающего - знак, целая часть и наличие дробной части (плавающее от 2^64 больше любого целого до 64 бит по модулю). В `utility.numeric.` на GCC 13.2 (Release): C++11, 14 и 17 - 1435 тестов проходят и 5 пропускаются (прежние тесты отрицательных значений беззнаковых типов), C++20 и 23 - 1463 проходят и 5 пропускаются; GCC 8.3 (C++11, 14, 17, `-std=c++2a`) - по 1435; Clang 23.1.0 с libstdc++ и с libc++ (C++11) - по 1435. ASan и UBSan: тесты новых и измененных наборов (32 теста на C++11-17, 38 на C++20) без замечаний на GCC 13.2 (C++11, 14, 17, 20), GCC 8.3 (C++11), Clang 23.1.0 с libstdc++ и с libc++ (C++11 и C++20). Мутации: 18 правок (возврат к прежним веткам бесконечности в `less`, `greater`, `compare`, `equal`, `not_equal` и трехстороннем сравнении двух типов с плавающей точкой; NaN как `equivalent`; обмен знака в трехстороннем сравнении целого с бесконечностью в обе стороны; `unordered` вместо порядка в обе стороны и только для `+inf`; обмен знака в `safe_compare` и `safe_less` целого с бесконечностью; `safe_equal` и `safe_not_equal` целого с бесконечностью; `unordered` для бесконечности одного типа; NaN как `true` в `safe_less`) - ловятся все 18 одним новым набором, контрольная правка без изменения не ловится. `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast` на новых и измененных тестах и на `float_equal_comparator.cpp` - ни одного замечания на GCC 8.3 (C++11, `-std=c++2a`), Clang 23.1.0 (C++11 с libstdc++ и libc++, C++20 - только замечание `-Wcharacter-conversion` в `gtest-printers.h`), GCC 13.2 (C++14, 17, 20), MinGW 8.3 (C++11). `create_release.sh` (GCC 8.3, GCC 13.2, Clang 23.1.0, MinGW 8.3; C++11 и C++20; `-Werror`): 0 предупреждений и 0 ошибок во всех восьми сборках. `cmake.*` и `lint.*`: 152 теста проходят (в них `lint.headers_standalone` и `cmake.hygiene_compile_checks`). Форматирование (clang-format 21.1.7) и пять проверок `lint.*` без замечаний.

##### Windows-код `serial`, `hardware` и `resource_monitor` переводит UTF-16 в UTF-8 через `core::unicode`

**Файлы:** `lumex/applied/serial/enumeration/LumexSerialPortEnumeration.cpp`, `lumex/applied/serial/resolver/LumexPortProcessResolver.cpp`, `lumex/applied/hardware/caps/LumexHardwareCapabilities.cpp`, `lumex/applied/resource_monitor/process/LumexProcessMonitor.cpp`, `lumex/applied/serial/CMakeLists.txt`, `lumex/applied/hardware/CMakeLists.txt`, `lumex/applied/resource_monitor/CMakeLists.txt`, `cmake/LumexModules.cmake`, `conanfile.py`, `lumex/core/utility/dump/LumexCoreDumpGenerator.hpp` (комментарий), `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/cases/wiring_unicode_users.cmake` (новый), `lumex/tests/cmake/cases/require_fail_serial_without_unicode.cmake`, `require_fail_hardware_without_unicode.cmake`, `require_fail_resource_monitor_without_unicode.cmake` (новые), `lumex/tests/cmake/cases/require_graph_all_edges.cmake`

**Суть:** четыре места вручную вызывали `WideCharToMultiByte`: две копии `convert_wide_to_utf8` в `serial` (перечисление портов и определение процесса, державшего порт), имя адаптера DXGI в `hardware` и `to_utf8` в `LumexProcessMonitor.cpp` (путь и имя процесса). Все четыре вызывают `lumex::core::unicode::convert::to_utf8`, как теперь и `filesystem` (запись выше): одна реализация на все платформы вместо четырех копий. Отличие то же: непарный суррогат UTF-16 пропускается, `WideCharToMultiByte` писал на его месте U+FFFD (имена портов, адаптеров и процессов их не содержат на практике). Модули `serial`, `hardware` и `resource_monitor` получили ребро на `unicode` на уровне конфигурации (`LUMEX_BUILD_SERIAL`, `LUMEX_BUILD_HARDWARE`, `LUMEX_BUILD_RESOURCE_MONITOR` требуют `LUMEX_BUILD_UNICODE`), `lumex::unicode` как PRIVATE (используют только исходники) и `core_unicode` в Conan. **Остается как было:** `_convert_wide_string_to_narrow` в `LumexCoreDumpGenerator.hpp` (модуль `utility`): `unicode` сам требует `utility` (`LUMEX_ASSERT`, `byte_swap`), поэтому заголовок `utility` не может включать `unicode` без цикла в графе модулей; помощник Windows-ветки (имя файла дампа из ASCII) оставлен с комментарием, почему, и закреплен `cmake.wiring_unicode_users` (заголовок не включает `unicode`).

**Проверено:** ни одно из четырех мест не запускается на Linux (все под `LUMEX_OS_WINDOWS` / `_WIN32`): компилируются MinGW 8.3 в матрице, тесты общей части (`to_utf8` для обеих ширин `wchar_t`) в записи выше. Матрица `create_release.sh --std 11,20` на GCC 8.3, GCC 13.2, Clang 23.1.0 и MinGW 8.3 по коммиту с этой правкой: 8 сборок, 0 предупреждений, 0 ошибок, `warns/` пуст. Новые тесты CMake: `cmake.wiring_unicode_users` (ребра, `PRIVATE`-ссылки, `core_unicode` в Conan, порядок подкаталогов, во всех пяти исходниках нет `WideCharToMultiByte`, `MultiByteToWideChar`, `mbstowcs`, `wcstombs`, `convert_wide_to_utf8`, включен заголовок `unicode`, заголовок дампа его не включает) и три `cmake.require_fail_*_without_unicode`; 11 порч (снято каждое из четырех ребер, четыре ссылки, две строки Conan, порядок подкаталогов) роняют `cmake.wiring_unicode_users` и, для ребер, `cmake.require_graph_all_edges` и соответствующий `cmake.require_fail_*_without_unicode`. GCC 13.2 Release: тесты `serial`, `hardware`, `resource_monitor` проходят, кроме известного `serial.probe.LumexSerialProberPty...ThenResponded` (падал и до правки).

##### Несовместимо: `LUMEX_DEFINE_ENUM_TRAITS` и `lumex_enum_traits_t` удалены, перечисления описывает `LUMEX_DEFINE_REFLECTED_ENUM`

**Файлы:** `lumex/core/utility/traits/LumexTypeTraits.hpp`, `lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp` (комментарий), `lumex/tests/core/utility/traits/LumexTypeTraits.cxx20.tests.cpp` (удален), `lumex/tests/core/utility/traits/LumexTypeTraits.cxx11.tests.cpp` (комментарий)

**Суть:** макрос `LUMEX_DEFINE_ENUM_TRAITS (Имя, Тип, A, B)` и шаблон `lumex_enum_traits_t<Имя>` с полями `values`, `first`, `last`, `size` не компилировались ни на одном стандарте ниже C++20 (`using enum` и `std::array{...}` с выводом аргументов в лямбде), лежали в глобальном пространстве имен, требовали вызова только в нем, и у них был один тест (три случая, пропускавшиеся без `using enum`). То же и больше дает `LUMEX_DEFINE_REFLECTED_ENUM`, который работает с C++11: перечислитель и значение пишутся в скобках, вызывается в любом пространстве имен и внутри класса, дает `to_string` и константы `ИмяValues` (`std::array`), `ИмяFirst`, `ИмяLast`, `ИмяSize`, все они константные выражения; его тесты уже проверяют размер, первый, последний, перебор значений и один перечислитель, и других пользователей у макроса не было ни в `lumex/`, ни в тестах, ни в примерах. Старое имя не оставлено ни синонимом, ни псевдонимом; это несовместимое изменение API (релиз `2.0.0.0`). Комментарий в `LumexReflectedEnum.hpp` больше не называет удаленное имя. Генерированная справка `docs/` обновится при выпуске.

Таблица "было -> стало":

| Вид | Было | Стало |
| --- | --- | --- |
| макрос | `LUMEX_DEFINE_ENUM_TRAITS (Color, unsigned char, Red, Green, Blue)` (C++20, глобальное пространство имен) | `LUMEX_DEFINE_REFLECTED_ENUM (Color, unsigned char, (Red), (Green), (Blue))` (C++11, любое пространство имен или класс) |
| класс | `lumex_enum_traits_t<Color>` | константы рядом с перечислением: `ColorValues`, `ColorFirst`, `ColorLast`, `ColorSize` |
| поле | `lumex_enum_traits_t<Color>::values` | `ColorValues` |
| поле | `lumex_enum_traits_t<Color>::first` | `ColorFirst` |
| поле | `lumex_enum_traits_t<Color>::last` | `ColorLast` |
| поле | `lumex_enum_traits_t<Color>::size` | `ColorSize` |

**Проверено:** `rg LUMEX_DEFINE_ENUM_TRAITS lumex_enum_traits_t` по `lumex/`, примерам, бенчмаркам и CMake - нет совпадений (кроме исторических записей этого файла и сгенерированного `docs/`). GCC 13.2 Release: `utility.` 9953 из 9953 (в том числе `utility.traits.`: 102, 104, 105, 122 и 122 теста на C++11, 14, 17, 20, 23 - три теста макроса на C++20 и C++23 удалены), `string.` 441 из 441, `reflection.reflected_enum.` 60 из 60, `reflection.var_info.` 56 из 56, примеры `utility` и `reflection` проходят; `fmt.` 798 тестов, падают 9 (`GivenHexTypeAtLimits_WhenFormat_ThenExactBits` на C++17 и C++20 и 7 сравнений со `std::format` на C++20), то есть те самые "9 `fmt`", что названы в записях о переименовании ниже; `cmake.*` и `lint.*` (в том числе `lint.headers_standalone`) 148 из 148. По всей серии из четырех записей: `create_release.sh` с GCC 8.3, GCC 13.2, Clang 23.1.0 и MinGW 8.3 на C++11 и C++20: 8 сборок, 0 предупреждений, 0 ошибок; `format.py --check` (548 файлов) и пять `check_*.py` без замечаний.

##### Несовместимо: у `Expected` нет глобального `unexpected`

**Файлы:** `lumex/core/expected/error/Unexpected.hpp`, `lumex/core/expected/Expected`, `lumex/core/expected/result/Expected.hpp`, `lumex/core/expected/result/ExpectedTypes.hpp`, `lumex/examples/expected/example_expected.cpp`, `lumex/tests/core/expected/error/ExpectedGlobalNames.cxx11.tests.cpp` (новый), `lumex/tests/cmake/consumer/expected_compile_checks/` (новый: `CMakeLists.txt`, `check.cpp`), `lumex/tests/cmake/cases/wiring_expected_global_names.cmake` (новый), `lumex/tests/cmake/CMakeLists.txt`

**Суть:** `Unexpected.hpp` заканчивался `using lumex::core::expected::error::unexpected;` в глобальном пространстве имен. MinGW объявляет в `<eh.h>` глобальную функцию `void unexpected ()`, поэтому файл, включавший `<eh.h>` и зонтик `lumex/core/expected/Expected`, не компилировался (`conflicts with a previous declaration`, а при обратном порядке включений `redeclared as different kind of symbol`). Тот же конфликт получает любая программа со своим глобальным `unexpected` (функцией или шаблоном класса другой библиотеки). Глобальное `using` удалено, как раньше для `in_place` (запись ниже по списку): писать нужно `lumex::core::expected::error::unexpected` или собственное `using`-объявление. `using error::unexpected;` внутри `lumex::core::expected::result` не глобальное и остается, поэтому `using namespace lumex::core::expected::result;` по-прежнему вводит имя; заголовки модуля используют его внутри своих пространств имен и не менялись. Остальные глобальные имена зонтика (`expected`, `bad_expected_access`, `make_unexpected`, `unexpect`, `unexpect_t`, `in_place_tag`) не менялись; ни одно из них не объявлено в заголовках MinGW (поиск по `/usr/share/mingw-w64/include`, `/usr/x86_64-w64-mingw32/include` и libstdc++ 8.3 для MinGW), так что такого конфликта у них нет. Комментарии зонтика `Expected`, `Expected.hpp`, `ExpectedTypes.hpp` и `Unexpected.hpp`, которые перечисляли глобальные имена, исправлены (`ExpectedTypes.hpp` говорил, что глобальны все теги, хотя `in_place` уже нет); пример `example_expected.cpp` объясняет, откуда берется `unexpected`. Это несовместимое изменение API: код, писавший `unexpected<E>` без `using`-объявления или `using namespace`, нужно исправить. В исходниках на C++ PeakExpertWeb, PeakExpertCE и DChannel глобальный `unexpected` не используется (`DChannel` пишет `std::unexpected`). В таблицу переименований (`core/expected`) добавлена строка.

**Проверено:** MinGW GCC 8.3 (posix), `-fsyntax-only`, файл с `<typeinfo>`, `<eh.h>` и зонтиком `lumex/core/expected/Expected`: до правки ошибка `conflicts with a previous declaration` на `-std=c++11`, `c++17` и `c++2a` (при зонтике перед `<eh.h>` на C++11 - `redeclared as different kind of symbol`), после правки файл компилируется на всех трех стандартах в обоих порядках включений и вместе с `<windows.h>`. Новый набор `ExpectedGlobalNames` (4 теста: своя глобальная функция `unexpected` рядом с зонтиком, класс по полному имени, `result::unexpected`, остальные глобальные имена) входит в наборы `expected.error.` на C++11, 17 и 20; GCC 13.2 Release: 1763 теста `expected` (C++11 - 583, C++17 - 590, C++20 - 590) проходят; GCC 8.3 - 583 теста C++11, Clang 23.1.0 с libstdc++ и с libc++ - по 583 теста C++11; ASan и UBSan (GCC 13.2, RelWithDebInfo) - 105 тестов `expected.error.` на C++11 без замечаний. Новое негативное `cmake.expected_compile_checks` (GCC 13.2, C++11, 17 и 20, по 9 сборок `check.cpp` на стандарт: хорошая, четыре со своим глобальным `unexpected` до и после зонтика - функция как в `<eh.h>` и шаблон класса, четыре плохие: `::unexpected<int>`, `unexpected<int>` без `using`, `using ::unexpected;`, `::in_place`) и `cmake.wiring_expected_global_names`; все 144 теста `cmake.*` и 6 тестов `lint.*` проходят (после перебазирования на релиз). Мутации ловятся: глобальное `using` возвращено - тест не компилируется (`redeclared as different kind of entity`), `cmake.expected_compile_checks` и `cmake.wiring_expected_global_names` падают; убрано глобальное `bad_expected_access` или `unexpect`, либо убрано `using error::unexpected;` из `lumex::core::expected::result` - тест не компилируется и оба кейса падают; возвращено глобальное `in_place` - падают оба кейса. Матрица `create_release.sh --std 11,20` на GCC 8.3, GCC 13.2, Clang 23.1.0 и MinGW 8.3: 8 сборок, 0 предупреждений и 0 ошибок, `warns/` пуст; тест, пример и хороший случай `check.cpp` с `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast -Werror` компилируются на GCC 8.3 (C++11 и `c++2a`), GCC 13.2 (C++14, 17, 20) и Clang 23.1.0 с libstdc++ и с libc++ (C++11 и 20). Для Windows-компиляторов MSVC и clang-cl проверок не было.

##### Нижняя граница трехстороннего сравнения `safe_comparator`: C++11 вместо C++20

**Файлы:** `lumex/core/utility/numeric/LumexSafeNumericComparator.hpp`, `lumex/core/utility/numeric/LumexOrdering.hpp`, `lumex/tests/core/utility/numeric/LumexSafeNumericComparator.cxx20.tests.cpp`

**Суть:** документированная нижняя граница стандарта для `safe_three_way_compare`, `three_way_comparison_result(_t)` и `is_*` с C++20 понижена до C++11: в заголовке 15 строк `@since C++20` стали `@since C++11`, блок `@file` и примеры говорят то же (в примерах нет `#if LUMEX_HAS_THREE_WAY_COMPARISON`). Наблюдаемые изменения: (1) на инструментах без `<compare>` (GCC 8 с `-std=c++2a`, любой стандарт ниже C++20) эти имена теперь есть, и результат - классы `LumexOrdering.hpp`; на C++20 с `<compare>` результат прежний, `std::strong_ordering` и `std::partial_ordering`; (2) тип результата записан явно, а не `auto`; (3) `is_*` принимают и классы библиотеки; (4) охранный макрос `LUMEX_CORE_UTILITY_NUMERIC_HPP` заголовка `LumexSafeNumericComparator.hpp` стал `LUMEX_CORE_UTILITY_NUMERIC_SAFE_NUMERIC_COMPARATOR_HPP`, так как в каталоге теперь два заголовка (правило: к охранному макросу добавляется имя файла). Время компиляции: процессорное время `-fsyntax-only` GCC 13.2 (медиана 15 прогонов, до и после) - заголовок `LumexSafeNumericComparator.hpp` 186 -> 187 мс на C++11 и 400 -> 404 мс на C++20, зонтик `LumexUtility` 421 -> 415 мс на C++11 и 1005 -> 1002 мс на C++20, то есть в пределах разброса. Комментарий в начале `LumexSafeNumericComparator.cxx20.tests.cpp` исправлен (он говорил, что заголовок объявляет эти имена только с `<compare>`); тесты в нем не менялись.

**Проверено:** прежние тесты `SafeComparator...` (в том числе трехсторонняя часть в `LumexSafeNumericComparator.cxx20.tests.cpp`, которая сравнивает результат со `std::strong_ordering` и `std::partial_ordering`) проходят без изменений на GCC 13.2 (C++11 - C++23), Clang 23.1.0 (libc++ и libstdc++, C++11 и C++20) и GCC 8.3; остальное - в записи о классах упорядочения выше.

##### Примеры `base64`, `crc` и `utility` показывают `span`, примеры `crc` и `utility` собираются как C++11

**Файлы:** `lumex/examples/base64/example_base64.cpp`, `lumex/examples/crc/example_crc.cpp`, `lumex/examples/crc/CMakeLists.txt`, `lumex/examples/utility/example_utility.cpp`, `lumex/examples/utility/CMakeLists.txt`

**Суть:** где примеры работали с байтовыми буферами, теперь есть и `lumex::core::span::view::span` (новых интерфейсов нет): `example_base64.cpp` кодирует часть вектора как `span`, `example_crc.cpp` считает CRC-32 и каталожный CRC по подвидам `span`, `example_utility.cpp` читает значения из массива байт через `mem::as` (указатель и размер и `span`; результат - `std::optional` с C++17 и `optional` библиотеки до него). В `example_utility.cpp` убрано условие C++20 вокруг `byte_swap` (он работает с C++11), поэтому закрепление примеров `crc` (было 14) и `utility` (было 20) заменено на C++11, как у примера `span`.

**Проверено:** примеры `base64`, `crc` и `utility` (шесть исполняемых файлов) собираются и проходят как тесты `examples.*` на GCC 13.2 (Release), GCC 8.3, Clang 23.1.0 с libc++ и с libstdc++ 13 и под ASan и UBSan.

##### CRC: библиотека собирается как C++11 и проверяется с C++11

**Файлы:** `lumex/core/crc/CMakeLists.txt`, `lumex/core/crc/catalog/LumexCrcCatalog.cpp`, `lumex/core/crc/LumexCrc`, `lumex/core/crc/catalog/LumexCrcCatalog.hpp`, `lumex/tests/LumexTestStandards.cmake`, `lumex/tests/core/crc/catalog/` (`LumexCrcCatalog.cxx11.tests.cpp`, `LumexCrcCatalogSpan.cxx11.tests.cpp` - переименованы из `.cxx14`; `LumexCrcCatalog.cxx17.tests.cpp`, `CMakeLists.txt`), `lumex/tests/core/crc/parametric/` (`LumexCrcParametric.cxx11.tests.cpp`, `LumexCrcParametricSpan.cxx11.tests.cpp` - переименованы из `.cxx14`; `LumexCrcParametric.cxx20.tests.cpp`, `LumexCrcTestHelpers.hpp`, `CMakeLists.txt`)

**Суть:** модуль считался C++14 из-за `std::make_index_sequence` в таблицах каталога (`LumexCrcCatalog.cpp`), хотя заголовки с перегрузками для `span` и `std::vector` давно компилируются как C++11. Теперь в `LumexCrcCatalog.cpp` свой список индексов (`index_list_t`, `make_index_list_t`), ничего другого из C++14 в библиотеке не нашлось (GCC 8.3 с `-std=c++11` ругался только на `index_sequence`), и цель собирается с `CXX_STANDARD 11` вместо 14. Закрепление стандарта осталось и должно остаться ниже C++17: `LumexCrcCatalog.cpp` определяет статические члены всех спецификаций для потребителей, собранных до C++17 (при `odr`-использовании нужно ровно одно определение, а с C++17 члены встроенные и единица трансляции их не определяет); комментарий в `CMakeLists.txt` говорит об этом. ABI не зависит от стандарта потребителя: экспортируются функции только с фундаментальными типами, `std::vector` и `span`, а перегрузки для `span`, `std::string_view` и `std::span` встроенные. Тесты `.cxx14` переименованы в `.cxx11` (в `LumexCrcCatalog` свой список индексов вместо `std::index_sequence`, в `LumexCrcTestHelpers.hpp` `reflect` стала `LUMEX_CONSTEXPR_CXX14`), строка модуля в `LumexTestStandards.cmake` - `crc 11 14 17 20`: те же сценарии (каталог по индексу и по параметрам с указателем и размером, вектором и `span`, транспортная контрольная сумма, все спецификации по контрольным значениям, `calculate (span)` всех алгоритмов) идут теперь и на C++11, где таблица строится при первом использовании. Тексты "модуль требует C++14" в `LumexCrc`, `CMakeLists.txt` и комментариях наборов заменены.

**Проверено:** новые наборы `LumexCrcCatalogCxx11Tests` (297 тестов) и `LumexCrcParametricCxx11Tests` (341), всего `crc.` на GCC 13.2 (Release) стало 2567 тестов вместо 1929; все проходят на C++11, C++14, C++17 и C++20 против библиотеки, собранной как `-std=c++11`, а также на GCC 8.3 (C++11, C++14, C++17, `-std=c++2a`), Clang 23.1.0 с libc++ и с libstdc++ 13 и под ASan и UBSan (GCC 13.2, Debug). Проверка закрепления: если собрать библиотеку как C++17 или C++20, наборы на C++11 и C++14 не линкуются (неопределенные ссылки на статические члены спецификаций); правка списка индексов (`make_index_list_t<N - 1, N - 2, I...>`) не компилируется. Предупреждений в новых и перенесенных тестах нет (`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast`: GCC 8.3, GCC 13.2, Clang 23, MinGW 8.3), `create_release.sh` на четырех компиляторах - 0 предупреждений и 0 ошибок.

##### `LumexMemRead::as` работает с C++11: результат `optional_t<T>`, ограничения без концептов

**Файлы:** `lumex/core/utility/mem/LumexMemRead.hpp`, `lumex/core/utility/traits/LumexTypeTraits.hpp`, `lumex/core/utility/LumexUtility`, `lumex/core/utility/CMakeLists.txt`, `lumex/core/CMakeLists.txt`, `lumex/CMakeLists.txt`, `cmake/LumexModules.cmake`, `conanfile.py`, `lumex/tests/core/utility/mem/` (`LumexMemRead.cxx11.tests.cpp`, `LumexMemReadSpan.cxx11.tests.cpp` - перенесены из `.cxx20`; новые `LumexMemReadConstraints.cxx11.tests.cpp`, `LumexMemReadResultType.cxx11.tests.cpp`, `LumexMemReadGlobalNames.cxx11.tests.cpp`, `LumexMemRead.cxx17.tests.cpp`, `LumexMemReadStdSpan.cxx20.tests.cpp`, `CMakeLists.txt`), `lumex/tests/core/utility/traits/LumexTypeTraitsTopics.cxx11.tests.cpp`, `LumexTypeTraitsTopics.cxx20.tests.cpp`, `lumex/tests/cmake/cases/` (`require_fail_utility_without_optional.cmake` - новый, `wiring_optional.cmake`, `wiring_span.cmake`, `require_graph_all_edges.cmake`, `require_ok_independents_off.cmake`, `require_ok_only_utility_and_math.cmake`), `lumex/tests/cmake/consumer/` (`cplusplus_macro`, `reflection_umbrella`, `standard_mismatch`), `lumex/tests/cmake/CMakeLists.txt`

**Суть:** `mem::as<T>` читает значение из сырых байт через `std::memcpy` и раньше существовал только в C++20 (концепты `Extractible`, `ByteLike`, `std::optional`, `std::span`; без них заголовок ничего не объявлял). Теперь он работает с C++11. Результат - `mem::optional_t<T>`: `std::optional<T>` с C++17 и `lumex::core::optional::opt::optional<T>` до него, поэтому у вызовов на C++17 и C++20 тип и поведение те же, а пустой результат проверяется через `has_value ()`; `std::nullopt` и `nullopt` модуля скрыты за псевдонимом (функция возвращает `optional_t<T> ()`), заголовок не вносит глобальных имен. Перегрузки: указатель и размер, объект с `get_data ()` и `get_data_size ()` (константные методы), `lumex::core::span::view::span<ByteType const>` на любом стандарте и `std::span<ByteType const>` там, где есть `<span>` (`LUMEX_HAS_STD_SPAN`); прежнее условие `LUMEX_HAS_STD_CONCEPTS && LUMEX_HAS_STD_SPAN` на весь заголовок снято. Элемент байтового спана: `char`, `unsigned char`, `std::byte` (C++17) и `byte` модуля `span` (до C++17 это собственное перечисление, с C++17 - `std::byte`); `signed char`, как и раньше, не принимается. Ограничения одинаковы на любом стандарте и написаны через `std::enable_if`: `Detail::DataSource` (концепт) заменен на `Detail::is_data_source`, а к концептам `Extractible` и `ByteLike` в `LumexTypeTraits.hpp` добавлены их формы для C++11 `traits::meta::is_extractible` и `traits::meta::is_byte_like` (концепты остались для других пользователей, тест сверяет их с признаками). Неподходящий `T` (не тривиально копируемый, указатель, ссылка, не стандартная раскладка) или неподходящий элемент спана не находит перегрузки. Модуль `utility` теперь требует `optional`: `lumex_require_module (LUMEX_BUILD_UTILITY LUMEX_BUILD_OPTIONAL)`, `lumex::optional` в `target_link_libraries`, `core_utility` в Conan требует `core_optional`, в `lumex/core/CMakeLists.txt` `optional` подключается до `utility`; конфигурация с `LUMEX_BUILD_UTILITY=ON` и `LUMEX_BUILD_OPTIONAL=OFF` отклоняется (новый случай `cmake.require_fail_utility_without_optional`). Стоимость включения: единица трансляции с зонтиком `LumexUtility` (`-fsyntax-only`, GCC 13.2, минимум из 9 запусков) стоит +4 мс на C++11, +11 мс на C++14, -1 мс на C++17 и +2 мс на C++20.

**Проверено:** набор `utility.mem` на GCC 13.2 (Release) вырос с 62 до 208 тестов (C++11 - 38, C++14 - 38, C++17 - 40, C++20 и C++23 - по 46): те же сценарии, что были для C++20 (нулевой указатель, короткий буфер, точный размер, невыровненные данные, знаковые, вещественные, структуры, источник данных, спаны `char`, `unsigned char`, `byte`), проверка отказа неподходящих типов через обнаружение выражения (`T`, источников, элементов спана), тип результата (до C++17 - `optional` библиотеки, с C++17 - `std::optional`, для каждой перегрузки) и проба глобальных имен. Все они, а с ними `optional.` (42), `span.` (508), `base64.` (226), `crc.` (2567), `utility.` (8658) и 10 примеров, проходят на GCC 13.2 (12011 тестов), на GCC 8.3 (3842, пропущены 30: нет концептов и `<span>`), на Clang 23.1.0 с libc++ и с libstdc++ 13 (по 4173) и под ASan и UBSan (4025). Все 20325 тестов дерева на GCC 13.2 (Release): не проходят только известные 79 `reflection.*`, 9 `fmt.*`, один `serial.*` (pty) и нестабильный `ToCrashReport_ThreadSafe`. `cmake.*` и `lint.*`: 137 тестов, все проходят. `create_release.sh` (GCC 8.3, GCC 13.2, Clang 23.1.0, MinGW 8.3; C++11 и C++20; `-Werror`): 0 предупреждений и 0 ошибок во всех восьми сборках. Мутации: из 23 правок (размер и нулевой указатель, копирование без последнего байта, отрицательный размер источника, выбор `std::optional` на C++11 и `optional` библиотеки на C++17, лишние и потерянные типы элементов, отсутствие `get_data_size`, неконстантный источник, нулевая длина спанов, потеря проверки `T`, ослабление `is_extractible` и `is_byte_like`, глобальное имя в заголовке `optional`) ловятся 22; не ловится правка, убирающая `!std::is_reference<T>` из `is_extractible`: ссылка и так не тривиально копируема, так что условие лишнее и повторяет концепт.

##### `optional`: типы и глобальные имена в разных заголовках

**Файлы:** `lumex/core/optional/LumexOptional` (зонтик), `lumex/core/optional/opt/LumexOptional.hpp`, `lumex/core/optional/opt/LumexOptionalGlobals.hpp` (новый), `lumex/tests/core/optional/opt/LumexOptionalGlobalNames.cxx11.tests.cpp` (новый), `LumexOptional.cxx11.tests.cpp`, `lumex/tests/cmake/cases/wiring_optional.cmake` (новый), `lumex/tests/cmake/CMakeLists.txt`

**Суть:** `opt/LumexOptional.hpp` объявляет типы модуля в `lumex::core::optional::opt` и больше ничего не кладет в глобальное пространство имен. Глобальные `optional`, `nullopt`, `make_optional` и `lumex_bad_optional_access` перенесены в `opt/LumexOptionalGlobals.hpp`, который включает зонтик `lumex/core/optional/LumexOptional`; у всех, кто включает зонтик (в дереве - все), ничего не меняется. Заголовок, которому нужен тип, но который не должен добавлять имена в каждый файл, включает `opt/LumexOptional.hpp` и пишет пространство имен полностью: так сделан `LumexMemRead.hpp` до C++17 (запись о нем выше по списку). Зачем: зонтик `utility` включает `LumexMemRead.hpp`, а его включают почти все модули; с зонтиком `optional` в нем каждая программа на C++11 и C++14 получила бы глобальный `optional` и могла бы упереться в свой собственный.

**Проверено:** новый тест `LumexOptionalGlobalNames` (программа с собственными глобальными `optional`, `nullopt`, `make_optional` и `lumex_bad_optional_access` включает заголовок типов и работает с обоими) и новый тест в `LumexOptional.cxx11.tests.cpp` (глобальные имена зонтика - это типы модуля); наборы `optional.` - 42 теста, проходят на GCC 13.2 (Release), GCC 8.3, Clang 23.1.0 с libc++ и с libstdc++ 13 и под ASan и UBSan. `cmake.wiring_optional` закрепляет раскладку по файлам. Правка (глобальный `using lumex::core::optional::opt::nullopt;` в заголовке типов) ломает тест.

##### `LumexMemRead::as`: перегрузка для `span` библиотеки; модуль `utility` требует `span`

**Файлы:** `lumex/core/utility/mem/LumexMemRead.hpp`, `lumex/core/utility/CMakeLists.txt`, `lumex/core/CMakeLists.txt` (`span` подключается до `utility`), `lumex/CMakeLists.txt`, `cmake/LumexModules.cmake`, `conanfile.py`, `lumex/tests/core/utility/mem/LumexMemReadSpan.cxx20.tests.cpp` (новый), `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/cases/require_fail_utility_without_span.cmake` (новый), `require_graph_all_edges.cmake`, `require_ok_only_utility_and_math.cmake`, `wiring_span.cmake`

**Суть:** перегрузка `as<T> (std::span<ByteType const>)` остается как есть: у шаблона `ByteType` выводится из аргумента, а при выводе `std::span` не преобразуется в `span` библиотеки. Рядом добавлена `as<T> (lumex::core::span::view::span<ByteType const>)` для `std::byte`, `char` и `unsigned char` с теми же правилами (`std::nullopt` для нулевого указателя и буфера короче `sizeof (T)`). Условие заголовка не менялось (концепты C++20 и `<span>`, иначе он ничего не объявляет), поэтому и новая перегрузка с C++20; `lumex/core/span/LumexSpan` включается только при `__cplusplus > 201703L`: единицы трансляции C++11 - C++17 с зонтиком `LumexUtility` не платят за заголовок, который им не нужен (около 30 мс на единицу трансляции, GCC 13.2). Модуль `utility` теперь требует `span`: `lumex_require_module (LUMEX_BUILD_UTILITY LUMEX_BUILD_SPAN)`, `lumex::span` в `target_link_libraries`, компонент Conan `core_utility` требует `core_math` и `core_span`, в `lumex/core/CMakeLists.txt` `span` подключается до `utility`. Конфигурация с `LUMEX_BUILD_UTILITY=ON` и `LUMEX_BUILD_SPAN=OFF` отклоняется (новый случай `cmake.require_fail_utility_without_span`; ребра `base64` и `crc` отдельно не проверить, потому что `utility` проверяется раньше, их закрепляют `cmake.wiring_span` и `cmake.require_graph_all_edges`). `span` по-прежнему не требует других модулей (включает только заголовки макросов `utility`), цикла нет.

**Проверено:** 12 новых имен CTest (6 тестов на C++20 и C++23: `span` из `std::byte`, из `char` и `unsigned char`, слишком короткий `span`, подвид читается с его начала, `std::span` и `span` библиотеки дают одно значение, статическая протяженность с выводом типа элемента). Все 62 теста `utility.mem` проходят на GCC 13.2 (Release) и под ASan и UBSan (GCC 13.2, Debug), 93 теста на Clang 23.1.0 (libc++ и libstdc++ 13; C++20, C++23 и C++26), на GCC 8.3 два теста пропускаются (нет концептов и `<span>`). Две правки перегрузки (размер ноль, перегрузка убрана) ловятся тестами. `cmake.*` и `lint.*`: 135 тестов, все проходят (два случая, падавшие на прежней базе из-за `LUMEX_BUILD_RESOURCE_MONITOR`, исправлены в `release/v2.0.0.0`). Поверх `release/v2.0.0.0` на GCC 13.2 (Release) проходят все 11840 тестов (`span.`, `base64.`, `crc.`, `utility.` - 8500, `unicode.`, `xml.` и 12 примеров этих модулей), сборка без предупреждений. `create_release.sh` (GCC 8.3, GCC 13.2, Clang 23.1.0, MinGW 8.3; C++11 и C++20; `-Werror`): 0 предупреждений и 0 ошибок во всех восьми сборках.

##### CRC: `compute_crc_catalog`, `compute_crc_with_rev_eng_params` и `calculate` принимают `span` библиотеки, `std::span` по-прежнему принимается

**Файлы:** `lumex/core/crc/catalog/LumexCrcCatalog.hpp`, `lumex/core/crc/parametric/LumexCrcParametric.hpp`, `lumex/core/crc/CMakeLists.txt`, `cmake/LumexModules.cmake`, `conanfile.py`, `lumex/tests/core/crc/catalog/LumexCrcCatalogSpan.cxx14.tests.cpp` (новый), `LumexCrcCatalogStdSpan.cxx20.tests.cpp` (новый), `lumex/tests/core/crc/parametric/LumexCrcParametricSpan.cxx14.tests.cpp` (новый), `LumexCrcParametric.cxx20.tests.cpp`, `lumex/tests/cmake/cases/require_graph_all_edges.cmake`, `wiring_span.cmake`

**Суть:** три перегрузки, которые были только в C++20 и брали `std::span<std::uint8_t const>`: `compute_crc_catalog (index, bytes)`, `compute_crc_with_rev_eng_params (params, bytes)` и статический `calculate (data)` параметрических алгоритмов, теперь берут `lumex::core::span::view::span<std::uint8_t const>` и не зависят от стандарта (модуль собирается с C++14, а сами заголовки с функциями-обертками компилируются и с C++11). `std::span` любой протяженности и константности преобразуется в него неявно, как `std::array`, встроенный массив и подвиды; перегрузки для указателя и размера и для `std::vector` не меняются, `std::vector` берет свою. Экспортируемые функции и ABI те же: перегрузки встроенные. Модуль `crc` требует `span`: ребро `lumex_require_module (LUMEX_BUILD_CRC LUMEX_BUILD_SPAN)`, `lumex::span` в `target_link_libraries`, `core_crc` в Conan требует `core_span`. `LumexCrcCatalog.hpp` и `LumexCrcParametric.hpp` больше не включают `<span>` и `LumexCheckFeatures.hpp`: код, который брал `LUMEX_HAS_STD_SPAN` оттуда, включает его сам (тесты C++20 так и сделаны).

**Проверено:** 33 новых имени тестов: `LumexCrcCatalogSpan` (5 тестов на C++14, C++17 и C++20: все индексы каталога через `span` равны форме с указателем и размером, параметры RevEng, контейнеры, пустой `span`, изменяемые байты только на чтение), `LumexCrcCatalogStdSpan` (2 теста на C++20), `LumexCrcParametricSpan` (5 тестов на C++14, C++17 и C++20: контрольные значения каталога, контейнеры, подвиды, пустой `span`, изменяемые байты только на чтение) и один тест в `LumexCrcParametric.cxx20.tests.cpp`: `std::span` любой протяженности и константности. Все 1929 тестов `crc.` проходят на GCC 13.2 (Release), Clang 23.1.0 с libc++ и с libstdc++ 13, GCC 8.3 (5 пропускаются: нет `<span>`) и под ASan и UBSan (GCC 13.2, Debug; 6 тестов `Perf_*` пропускаются в Debug). Три правки (нулевой размер, потеря первого байта, половина размера) ловятся тестами. Предупреждений в новых тестах и примерах нет (`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast`: GCC 13.2, Clang 23, MinGW 8.3; GCC 8.3 с C++14 и выше).

##### `Base64Encoder::encode`: перегрузка для `span` библиотеки в каждом стандарте, `std::span` по-прежнему принимается

**Файлы:** `lumex/core/base64/encode/Encoder.hpp`, `lumex/core/base64/LumexBase64`, `lumex/core/base64/CMakeLists.txt`, `cmake/LumexModules.cmake`, `conanfile.py`, `lumex/tests/core/base64/encode/LumexBase64EncoderSpan.cxx11.tests.cpp` (новый), `LumexBase64EncoderStdSpan.cxx20.tests.cpp` (новый), `LumexBase64Encoder.cxx20.tests.cpp`, `lumex/tests/cmake/cases/require_graph_all_edges.cmake`, `require_ok_independents_off.cmake`, `wiring_span.cmake`, `lumex/tests/cmake/consumer/standard_mismatch/CMakeLists.txt`

**Суть:** перегрузка `Base64Encoder::encode (std::span<byte_type const>)` существовала только в C++20 (`LUMEX_HAS_STD_SPAN`). Ее место занимает `encode (lumex::core::span::view::span<byte_type const>)` без условия на стандарт: она есть с C++11 и принимает `std::array`, встроенный массив и подвид другого `span`, а `std::span` любой протяженности и константности (с C++20) преобразуется в нее неявно, поэтому вызовы `encode (std_span)` остаются прежними. Перегрузки `encode (void const *, std::size_t)` и `encode (std::vector<byte_type> const &)` не затронуты (`std::vector` по-прежнему берет свою), экспортируемые функции и ABI библиотеки те же: новая перегрузка встроенная. Модуль `base64` теперь требует модуль `span`: `lumex_require_module (LUMEX_BUILD_BASE64 LUMEX_BUILD_SPAN)`, `lumex::span` в `target_link_libraries`, компонент Conan `core_base64` требует `core_span`; конфигурация с `LUMEX_BUILD_BASE64=ON` и `LUMEX_BUILD_SPAN=OFF` отклоняется. `Encoder.hpp` больше не включает `<span>` и `LumexCheckFeatures.hpp`: код, который брал макрос `LUMEX_HAS_STD_SPAN` через этот заголовок, включает `lumex/core/utility/compiler/LumexCheckFeatures.hpp` сам (тесты C++20 так и сделаны).

**Проверено:** 28 новых тестов: `LumexBase64EncoderSpan.cxx11.tests.cpp` (8 тестов на C++11, C++17 и C++20: `span` равен форме с указателем и размером, `std::array`, встроенный массив, подвиды, пустой `span`, векторы RFC 4648, большой буфер, вызовы с `std::vector` и с указателем без неоднозначности) и `LumexBase64EncoderStdSpan.cxx20.tests.cpp` (4 теста на C++20: изменяемый `std::span`, `std::span` статической протяженности, rvalue `std::span`, `std::span` других элементов не принимается). Все 226 тестов `base64.` проходят на GCC 13.2 (Release), Clang 23.1.0 с libc++ и с libstdc++ 13, GCC 8.3 (6 пропускаются: нет `<span>`) и под ASan и UBSan (GCC 13.2, Debug; 9 тестов `Perf_*` пропускаются в Debug). Две правки перегрузки (последний байт не кодируется, перегрузка переименована) ловятся тестами. Предупреждений в новых тестах и примерах нет (`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast`: GCC 8.3, GCC 13.2, Clang 23, MinGW 8.3).

##### Дерево тестов повторяет дерево исходников: префикс CTest называет каталог, а не модуль

**Файлы:** `lumex/tests/core/**`, `lumex/tests/applied/**`, `lumex/tests/xml/xpath/query/` (каталог тестов на каталог исходников; файлы перенесены через `git mv`, отдельными коммитами), `cmake/LumexGoogleTest.cmake` (`lumex_add_standard_suites`), `lumex/tests/LumexTestStandards.cmake`, `lumex/tests/cmake/cases/wiring_standard_suites.cmake`, `wiring_test_names.cmake`, `wiring_atomic.cmake`, `wiring_exception_crash_lock.cmake`, `wiring_field_reflection.cmake`, `wiring_field_reflection_no_leak.cmake`, `lumex/tests/xml/LumexXml.cxx11.tests.cpp`.

**Суть:** `lumex/tests/<путь>` тестирует `lumex/<путь>`: `lumex/tests/core/math/ops` - `lumex/core/math/ops`, `lumex/tests/core/utility/traits` - `lumex/core/utility/traits`, `lumex/tests/applied/json/schema` - `lumex/applied/json/schema` и так далее, включая вложенные `resource_monitor/monitor/detail`, `serial/probe/detail`, `xml/xpath/query`. Имя в CTest берется из каталога, который регистрирует тест, поэтому `ctest -R '^math\.ops\.'` выбирает один каталог исходников, а `ctest -R '^math\.'` - модуль целиком. Меняется только префикс: `base64.Base64EncoderTest.<тест>.cxx11` стало `base64.encode.Base64EncoderTest.<тест>.cxx11`; имена наборов и тестов те же. Таблица стандартов осталась одна на модуль (`LumexTestStandards.cmake`): каждый каталог собирает по исполняемому файлу `Lumex<Модуль><Каталог>Cxx<std>Tests` на каждый стандарт модуля (набор старшего стандарта по-прежнему компилирует файлы младших) и по варианту `lock_based` / `wait_table` у `atomic`, начиная со стандарта самого младшего файла каталога: `utility/cast`, `utility/mem` и `utility/ranges` имеют только файлы C++20 и наборы от C++20, как и раньше. `lumex_add_standard_suites` принимает в `MODULE` ключ модуля, а префикс берет у вызывающего каталога; ошибка, если ключ не начинает префикс, если в каталоге нет `*.tests.cpp` или не собрано ни одного набора. Файлы, которые тестировали несколько каталогов, разделены по наборам с теми же именами: `base64` (`encode`, `decode`, `validate`), `crc` (`catalog`, `parametric`), `exceptions` (`exception`, `stacktrace`), `json` (`schema`, `validation`, `normalization`; схема и документы общие в `LumexJsonSchemaTestFixtures.hpp`), `logger` (`config`, `logger`), `string` (тесты `to_case_insensitive` ушли в `text`), `time` (`clock`, `timer`). Тесты зонтичных заголовков и файлы рядом с зонтиком остались в каталоге модуля: `atomic` (глобальные имена), `exceptions` (`LumexExceptionWrapper`), `settings` (сохранение через три бэкенда), `utility` (макросы `min` / `max`) и `xml` (документ, узел, атрибут и писатель вместе, через зонтик: их нельзя честно разнести по каталогам). У `fmt`, `circular_buffer` и `generators/number_generator` нет подкаталогов исходников, их тесты не менялись; тестов для `math/constants` не было и нет. Тесты `reflected_enum` и `var_info` собираются отдельно от тестов полей и без `nlohmann`: макрос `LUMEX_WITH_FIELD_REFLECTION` и `nlohmann_json` есть только у каталога `field_reflection`, который добавляется при опции и цели. Имена `xml.Xml/AttrBoolParamTest...` содержали 16 байт объекта (в том числе адрес строкового литерала) и менялись от сборки к сборке: у `BoolCase` теперь есть `operator<<`, имена выглядят как `xml.Xml/AttrBoolParamTest.<тест>/s=1,expected=true.cxx11`, а `BoolCaseNameTest` проверяет текст (+3 теста, по одному на стандарт). `cmake.wiring_standard_suites` теперь проверяет, что каталог тестов есть в дереве исходников под тем же путем, что его добавляет `CMakeLists.txt` родителя, что `MODULE` равен ключу модуля, что варианты есть в каждом каталоге модуля и что где-то в модуле есть файл младшего стандарта; `cmake.wiring_test_names` закрепляет префиксы `math.ops.`, `utility.traits.`, `json.schema.`, `resource_monitor.monitor.detail.`, `xml.xpath.query.`. Новый файл с тестами нужно класть в каталог, который называет каталог исходников: файл, оставленный в каталоге модуля без собственного вызова, валит `wiring_standard_suites`.

**Проверено:** GCC 13.2, Release, ctest -j4 по всему дереву. Было 18067 тестов, стало 18070 (+3 `BoolCaseNameTest`); повторяющихся имен 0; после отбрасывания каталога из префикса все 18067 старых имен есть в новом списке, кроме 36 `xml.Xml/AttrBoolParamTest`, которых и не могло быть: их имена с байтами объекта в каждой сборке разные (в двух сборках новые имена совпадают). Падают те же 92 теста, что и до переноса: 79 `reflection.*` (имена полей на GCC 13), 9 `fmt.*` (сравнение с `std::format`), 2 `cmake.require_*`, `serial.LumexSerialProberPty.GivenPreWrittenResponse_...` (pty этой машины) и нестабильный `ToCrashReport_ThreadSafe` (40 запусков в каждом дереве: падает в 67 из 120 запусков теста до и в 59 из 120 после). Исполняемых файлов тестов 62 -> 201 (GCC 8.3: 61 -> 185, Clang 23.1.0: 63 -> 217, конфигурация); число отдельных компиляций 382 -> 413 (деление файла на несколько добавляет трансляций, на каждую по разбору `gtest.h` и заголовков модуля), сумма времени компиляций 2899 -> 2970 с (+2,4%), компоновок 62 -> 201, 49,8 -> 58,0 с. Чистая сборка всех исполняемых файлов тестов (библиотеки и GoogleTest не считаются, 24 с и в том, и в другом), `ninja -j4`, 6 частей: 894 -> 879 с стенного времени (-1,7%), процессорное время (user+sys) 2699 -> 2746 с (+1,7%). Пилот `base64` (3 -> 9 исполняемых, 6 -> 16 компиляций): 6,3 -> 9,3 с стенного времени, 20,0 -> 29,3 с процессорного. `create_release.sh` на GCC 8.3, GCC 13.2, Clang 23.1.0 и MinGW 8.3, C++11 и C++20: 8 сборок, 0 предупреждений, `warns/` пуст. Форматирование `format.py --check`: 450 файлов в порядке. Мутации (каждая валит нужную проверку): каталог тестов без каталога исходников, `MODULE` другого модуля (и в случае, и при конфигурации), каталог без `add_subdirectory` у родителя, пропущенный вариант `atomic`, лишний `*.tests.cpp` в каталоге модуля, файл вне схемы `.cxx15`, нет файла младшего стандарта во всем модуле, убранная проверка `MODULE` в хелпере, правило префикса с двумя уровнями (только новые ожидания `wiring_test_names`), снятая блокировка `crashes` в `stacktrace`, макрос поля на `var_info`, снятая защита `field_reflection`, удаленный `operator<<` у `BoolCase` (`BoolCaseNameTest` падает, имена снова с байтами).

##### Монитор ресурсов собирается с C++11

**Файлы:** `lumex/applied/resource_monitor/**` (оба сэмплера и `LumexProcessMonitor`), `lumex/applied/resource_monitor/CMakeLists.txt`, `cmake/LumexModules.cmake`, `conanfile.py`, `lumex/tests/LumexTestStandards.cmake`, `lumex/tests/applied/resource_monitor/**`

**Суть:** модуль был закреплен на C++17 из-за `std::filesystem`, а его публичный `LumexProcessMonitor.hpp` вводил `std::optional`, так что проект на C++11 или C++14 не мог подключить даже umbrella `LumexResourceMonitor`. Теперь модуль использует средства самой библиотеки: `optional`, `LumexStringView` и `lumex::filesystem`. Закрепление `CXX_STANDARD 17` и флаг `LUMEX_USES_STD_FILESYSTEM` убраны, как у остальных модулей C++11. Модуль требует `optional`, `string_view` и `filesystem` (`lumex_require_module`, цели CMake, компоненты Conan). Наборы тестов переведены с 17 на 11 (`LumexResourceMonitorCxx11Tests`); в Windows-ветке теста имя исполняемого файла берется у запущенного модуля, а не из имени цели.

**Проверено:** сборка с `-DCMAKE_CXX_STANDARD=11` (GCC 13.2) без ошибок, 58 тестов модуля и lint проходят, стресс `ctest -j12 --repeat until-fail:150` трижды без падений.

##### Несовместимо: у `optional` и `Expected` нет глобального `in_place`

**Файлы:** `lumex/core/optional/opt/LumexOptional.hpp`, `lumex/core/expected/result/ExpectedTypes.hpp`, `lumex/tests/core/optional/LumexOptional.cxx11.tests.cpp`, `lumex/tests/core/expected/CMakeLists.txt`, `lumex/tests/core/expected/ExpectedWithOptional.cxx11.tests.cpp` (новый)

**Суть:** с `v1.0.0.0` оба модуля выносили свой `in_place` в глобальное пространство имен (`using lumex::core::optional::opt::in_place;` и `using lumex::core::expected::result::in_place;`). Это два разных объекта (`in_place_t` и `in_place_tag`) под одним именем, поэтому файл, включающий оба umbrella, не компилировался ни на одном стандарте. Оба глобальных `using` удалены: писать нужно `lumex::core::optional::opt::in_place` и `lumex::core::expected::result::in_place` или собственное `using`-объявление. Остальные глобальные имена (`optional`, `nullopt`, `make_optional`, `LumexBadOptionalAccess`, `in_place_tag`, `unexpect`, `unexpect_t`) не менялись. Это несовместимое изменение API, поэтому по `VERSIONING.md` релиз получает номер `2.0.0.0`. В исходниках на C++ PeakExpertWeb, PeakExpertCE и DChannel `in_place` не используется. Комментарий к `in_place` в `LumexOptional.hpp` больше не называет несуществовавшее `lumex::in_place`.

**Проверено:** новый набор `ExpectedWithOptional` (3 теста на каждом из стандартов 11, 17, 20) подключает оба umbrella в одном файле, строит `optional` из `Expected` и наоборот; возврат обоих `using` дает прежнюю ошибку компиляции; 1293 теста `expected` и `optional` проходят (GCC 13.2, `-DCMAKE_CXX_STANDARD=11`).

##### Несовместимо: функции, методы и классы ядра в snake_case

**Файлы:** `lumex/core/**`, `lumex/applied/**`, `lumex/tests/**`, `lumex/examples/**`, `lumex/core/atomic/README.md`, `lumex/core/fmt/README.md`, `lumex/applied/logger/README.md`, `cmake/LumexGoogleTest.cmake`, `Scripts/CodeTools/check_fmt_examples_coverage.py`

**Суть:** все функции и методы библиотеки (модули core, applied и xml, статические и свободные функции тоже) и все классы модулей core записаны в snake_case; приставка `Lumex` у классов осталась (`LumexTime` -> `lumex_time`, `LumexStringView` -> `lumex_string_view`, `BitLockGuard` -> `bit_lock_guard`, `Expected` -> `expected`, `Unexpected` -> `unexpected`), глобальные псевдонимы классов (`using LumexStringView = ...`) переименованы так же. Закрытые функции и методы с префиксом `_` сохраняют его (`_logMessage` -> `_log_message`). Старые имена не оставлены ни синонимами, ни устаревшими псевдонимами: потребитель переходит на новые имена по таблице ниже, а экспортируемые символы скомпонованных модулей меняются вместе с ними; поэтому это несовместимое изменение API и ABI в релизе `2.0.0.0`. Всего переименовано 322 имени: 60 классов и один псевдоним класса (`LumexStacktrace`), 223 функции и метода библиотеки и 38 вспомогательных функций тестов и примеров. В модуле xml функций в camelCase не было. `toString`, которую `LUMEX_DEFINE_REFLECTED_ENUM` создает для перечисления, стала `to_string`: ее находят по ADL трейт `is_reflected_enum`, форматирование `fmt` и помощники JSON и логирования, поэтому собственная функция пользователя для этого трейта тоже должна называться `to_string`. Концепт `DataSource` из `LumexMemRead.hpp` требует от типа пользователя методы `get_data` и `get_data_size` (было `GetData`, `GetDataSize`). `LumexWStringView` стал `lumex_wstring_view`, как `std::wstring_view`. Не переименованы: `Mask` (в `crc`, новое имя `mask` уже занято локальной переменной тех же функций), `INVOKE` (в `LumexTypeTraits.hpp`, `invoke` по ADL столкнулся бы с `std::invoke`), `Get` и пять функций `StacktraceTest_*` в тестах (имена сверяются с текстом стека вызовов). Остались без изменений: классы модулей applied и xml, структуры, перечисления, пространства имен, элементы перечислений, параметры шаблонов, макросы (в том числе `lumDebug`, `lumDemangle`), члены данных, параметры функций, локальные переменные, имена файлов и зонтичных заголовков, строковые литералы (кроме названий операций `lumex_settings_guard`: `ensure_keys_with_defaults`, `ensure_exists_with_defaults`, `repair_if_corrupted`), а также псевдонимы вроде `FormatArgs` и `IntGenerator`, которые не являются глобальным псевдонимом класса.

Таблица "было -> стало" по модулям (вид: класс, псевдоним, функция, метод):

**core/atomic**

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `BitLock` | `bit_lock` |
| класс | `BitLockGuard` | `bit_lock_guard` |
| класс | `LockBasedCell` | `lock_based_cell` |
| класс | `StdBackedCell` | `std_backed_cell` |

**core/base64**

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `Decoder` | `decoder` |
| класс | `Encoder` | `encoder` |
| класс | `Validator` | `validator` |

**core/circular_buffer**

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `CircularBuffer` | `circular_buffer` |

**core/crc**

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `CrcParametric` | `crc_parametric` |
| функция | `AppendCrcLeastSignificantByteFirst` | `append_crc_least_significant_byte_first` |
| функция | `Compute` | `compute` |
| функция | `ComputeBitwise` | `compute_bitwise` |
| функция | `ComputeCrcCatalog` | `compute_crc_catalog` |
| функция | `ComputeCrcWithRevEngParams` | `compute_crc_with_rev_eng_params` |
| функция | `ComputeEntry` | `compute_entry` |
| функция | `ComputeTableDriven` | `compute_table_driven` |
| функция | `ComputeTransportChecksum` | `compute_transport_checksum` |
| функция | `CrcCatalogLegacyIndex` | `crc_catalog_legacy_index` |
| функция | `CrcTransportUsesCustomSpecSentinel` | `crc_transport_uses_custom_spec_sentinel` |
| функция | `GetCrcCatalogBitWidth` | `get_crc_catalog_bit_width` |
| функция | `GetCrcCatalogEntryCount` | `get_crc_catalog_entry_count` |
| функция | `GetTransportCrcCatalogIndex` | `get_transport_crc_catalog_index` |
| функция | `GetTransportCrcMode` | `get_transport_crc_mode` |
| функция | `LookupTableEntry` | `lookup_table_entry` |
| функция | `LsbTableByte` | `lsb_table_byte` |
| функция | `MakeComputeTable` | `make_compute_table` |
| функция | `MakeLookupTable` | `make_lookup_table` |
| функция | `MakeLookupTableImpl` | `make_lookup_table_impl` |
| функция | `MakeWidthTable` | `make_width_table` |
| функция | `MaskForWidth` | `mask_for_width` |
| функция | `MsbTableByte` | `msb_table_byte` |
| функция | `Reflect` | `reflect` |
| метод | `Run` | `run` |
| функция | `SetTransportCrcCatalogIndex` | `set_transport_crc_catalog_index` |
| функция | `SetTransportCrcDefault` | `set_transport_crc_default` |
| функция | `SetTransportCrcRevEngParams` | `set_transport_crc_rev_eng_params` |
| функция | `Transport` | `transport` |
| функция | `TransportMutex` | `transport_mutex` |
| функция | `TryGetTransportCrcRevEngParams` | `try_get_transport_crc_rev_eng_params` |
| функция | `ValidateCrcRevEngParams` | `validate_crc_rev_eng_params` |
| функция | `ValidateSpec` | `validate_spec` |
| метод | `Value` | `value` |
| функция | `WidthEntry` | `width_entry` |

**core/environment**

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `EnvironmentStrategy` | `environment_strategy` |
| класс | `LumexEnvironment` | `lumex_environment` |
| класс | `PosixEnvironmentStrategy` | `posix_environment_strategy` |
| класс | `WindowsEnvironmentStrategy` | `windows_environment_strategy` |

**core/exceptions**

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `DbgHelpInitializer` | `dbg_help_initializer` |
| класс | `LumexBaseException` | `lumex_base_exception` |
| класс | `LumexBasicStacktrace` | `lumex_basic_stacktrace` |
| класс | `LumexCrashHandler` | `lumex_crash_handler` |
| псевдоним | `LumexStacktrace` | `lumex_stacktrace` |
| класс | `LumexStacktraceEntry` | `lumex_stacktrace_entry` |
| функция | `ExceptionWrapper` | `exception_wrapper` |
| метод (также core/utility) | `_generateCoreDump` | `_generate_core_dump` |
| метод (также core/utility) | `_generateDumpFilename` | `_generate_dump_filename` |
| метод | `getStackTrace` | `get_stack_trace` |
| функция | `_handleSEHException` | `_handle_seh_exception` |
| функция | `LumexException_GetStackTraceTrampoline` | `lumex_exception_get_stack_trace_trampoline` |
| метод | `_notifyAndLog` | `_notify_and_log` |
| функция | `_onWindowsCrashHandler` | `_on_windows_crash_handler` |
| метод (также core/utility) | `_setupCoreDumpSettings` | `_setup_core_dump_settings` |
| метод (также core/utility) | `_setupSignalHandlers` | `_setup_signal_handlers` |
| метод | `_signalHandler` | `_signal_handler` |

**core/expected**

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `BadExpectedAccess` | `bad_expected_access` |
| класс | `Expected` | `expected` |
| класс | `Unexpected` | `unexpected` |
| глобальное имя | `unexpected` | `lumex::core::expected::error::unexpected` (глобального имени нет) |
| глобальное имя | `in_place` | `lumex::core::expected::result::in_place` (глобального имени нет) |

**core/filesystem**

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `Impl` | `impl` |
| функция | `checkName` | `check_name` |
| функция | `hasInvalidEnding` | `has_invalid_ending` |
| функция | `isFileExists` | `is_file_exists` |
| функция | `isForbidden` | `is_forbidden` |
| функция | `isReservedName` | `is_reserved_name` |
| функция | `sanitizeName` | `sanitize_name` |

**core/fmt**

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `BasicAppender` | `basic_appender` |
| класс | `BasicFormatArgs` | `basic_format_args` |
| класс | `BasicFormatContext` | `basic_format_context` |
| класс | `BasicFormatParseContext` | `basic_format_parse_context` |
| класс | `BasicFormatString` | `basic_format_string` |
| класс | `BasicStringRef` | `basic_string_ref` |
| класс | `Buffer` | `buffer` |
| класс | `BuiltinFormatter` | `builtin_formatter` |
| класс | `ChronoFormatter` | `chrono_formatter` |
| класс | `CountingBuffer` | `counting_buffer` |
| класс | `ElementFormatter` | `element_formatter` |
| класс | `FormatError` | `format_error` |
| класс | `FormatHandler` | `format_handler` |
| класс | `FormatStringChecker` | `format_string_checker` |
| класс | `Formatter` | `formatter` |
| класс | `IteratorBuffer` | `iterator_buffer` |
| класс | `OstreamFormatter` | `ostream_formatter` |
| класс | `StringBuffer` | `string_buffer` |
| класс | `TruncatingBuffer` | `truncating_buffer` |
| класс | `TupleFormatter` | `tuple_formatter` |

**core/generators**

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `NumberGenerator` | `number_generator` |

**core/math**

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `LumexMathSizeMismatchException` | `lumex_math_size_mismatch_exception` |

**core/optional**

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `LumexBadOptionalAccess` | `lumex_bad_optional_access` |
| глобальное имя | `in_place` | `lumex::core::optional::opt::in_place` (глобального имени нет) |

**core/reflection**

| Вид | Было | Стало |
| --- | --- | --- |
| функция | `FormatValue` | `format_value` |
| функция | `toString` | `to_string` |
| функция | `VarInfo` | `var_info` |

**core/string_view**

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `LumexStringView` | `lumex_string_view` |
| класс | `LumexWStringView` | `lumex_wstring_view` |

**core/temporary**

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `LumexTemporary` | `lumex_temporary` |
| класс | `TemporaryDirectory` | `temporary_directory` |
| класс | `TemporaryFile` | `temporary_file` |

**core/time**

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `LumexTime` | `lumex_time` |
| класс | `LumexTimer` | `lumex_timer` |

**core/utility**

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `BadDownCast` | `bad_down_cast` |
| класс | `CoreDumpGenerator` | `core_dump_generator` |
| класс | `DumpConfiguration` | `dump_configuration` |
| класс | `DumpFactory` | `dump_factory` |
| класс | `LumexCallbackSlot` | `lumex_callback_slot` |
| класс | `OperationGuard` | `operation_guard` |
| класс | `SafeComparator` | `safe_comparator` |
| класс | `Scoped` | `scoped` |
| метод | `_acquireOperationSlot` | `_acquire_operation_slot` |
| метод | `addMemoryFilter` | `add_memory_filter` |
| функция | `As` | `as` |
| метод | `_blockDefaultShutdownSignals` | `_block_default_shutdown_signals` |
| функция | `captureCallerInfoImpl` | `capture_caller_info_impl` |
| функция | `captureStackTrace` | `capture_stack_trace` |
| метод | `clearMemoryFilters` | `clear_memory_filters` |
| функция | `_convertWideStringToNarrow` | `_convert_wide_string_to_narrow` |
| метод | `createConfiguration` | `create_configuration` |
| метод | `_createDirectoryAtomically` | `_create_directory_atomically` |
| метод | `_createDirectoryRecursive` | `_create_directory_recursive` |
| метод | `_createFileAtomically` | `_create_file_atomically` |
| метод | `_createManualCoreDump` | `_create_manual_core_dump` |
| метод | `createUnixConfiguration` | `create_unix_configuration` |
| метод | `createWindowsConfiguration` | `create_windows_configuration` |
| функция | `_createWindowsDump` | `_create_windows_dump` |
| метод | `_customSignalHandlerWrapper` | `_custom_signal_handler_wrapper` |
| функция | `demangleSymbol` | `demangle_symbol` |
| функция | `dumpTypeToString` | `dump_type_to_string` |
| метод | `_endPerformanceMonitoring` | `_end_performance_monitoring` |
| функция | `ensureSymbolsInitialized` | `ensure_symbols_initialized` |
| функция | `formatHex` | `format_hex` |
| функция | `formatTime` | `format_time` |
| метод | `generateDump` | `generate_dump` |
| метод | `_generateFallbackRandomComponent` | `_generate_fallback_random_component` |
| метод | `generateInstanceDump` | `generate_instance_dump` |
| метод | `_generateSecureRandomComponent` | `_generate_secure_random_component` |
| метод | `getCurrentConfiguration` | `get_current_configuration` |
| метод | `getCurrentDumpType` | `get_current_dump_type` |
| метод | `GetData` | `get_data` |
| метод | `GetDataSize` | `get_data_size` |
| функция | `getDbgHelpMutex` | `get_dbg_help_mutex` |
| метод | `getDefaultDumpType` | `get_default_dump_type` |
| метод | `getDescription` | `get_description` |
| метод | `getDirectory` | `get_directory` |
| метод | `getDumpDirectory` | `get_dump_directory` |
| метод | `getDumpDirectoryIfSet` | `get_dump_directory_if_set` |
| метод | `getEstimatedSize` | `get_estimated_size` |
| метод (также applied/logger) | `_getExecutableDirectory` | `_get_executable_directory` |
| функция | `getExeDirectory` | `get_exe_directory` |
| метод | `getFilename` | `get_filename` |
| метод | `getInstanceConfiguration` | `get_instance_configuration` |
| метод | `getInstanceDumpDirectory` | `get_instance_dump_directory` |
| метод | `getMaxSizeBytes` | `get_max_size_bytes` |
| функция | `getMaxValue` | `get_max_value` |
| метод | `getMemoryFilters` | `get_memory_filters` |
| метод | `getMemoryFiltersRange` | `get_memory_filters_range` |
| функция | `_getMinidumpType` | `_get_minidump_type` |
| функция | `getMinValue` | `get_min_value` |
| функция | `GetNearestTo` | `get_nearest_to` |
| метод | `getOptionalDumpDirectory` | `get_optional_dump_directory` |
| метод | `getSupportedTypes` | `get_supported_types` |
| метод | `getType` | `get_type` |
| метод | `getValidationError` | `get_validation_error` |
| метод | `_instantSystemdMonitor` | `_instant_systemd_monitor` |
| метод | `_invalidateCache` | `_invalidate_cache` |
| метод | `isAcquired` | `is_acquired` |
| функция | `_isAdminPrivileges` | `_is_admin_privileges` |
| метод | `isAdminPrivileges` | `is_admin_privileges` |
| метод | `isCompress` | `is_compress` |
| функция | `_isElevatedProcess` | `_is_elevated_process` |
| метод | `isEnableSourceInfo` | `is_enable_source_info` |
| метод | `isEnableSymbols` | `is_enable_symbols` |
| метод | `isIncludeHandleData` | `is_include_handle_data` |
| метод | `isIncludeProcessData` | `is_include_process_data` |
| метод | `isIncludeThreadInfo` | `is_include_thread_info` |
| метод | `isIncludeUnloadedModules` | `is_include_unloaded_modules` |
| метод | `isInitialized` | `is_initialized` |
| метод | `isInstanceInitialized` | `is_instance_initialized` |
| функция | `isKernelType` | `is_kernel_type` |
| метод | `isSupported` | `is_supported` |
| функция | `isUnixType` | `is_unix_type` |
| функция и метод | `isValid` | `is_valid` |
| метод | `isValidDirectory` | `is_valid_directory` |
| метод | `isValidFilename` | `is_valid_filename` |
| метод | `isValidMemoryFilter` | `is_valid_memory_filter` |
| функция | `_isValidMinidumpType` | `_is_valid_minidump_type` |
| функция | `isWindowsType` | `is_windows_type` |
| метод | `_logCoreDumpSize` | `_log_core_dump_size` |
| метод | `_logDumpCreationSuccess` | `_log_dump_creation_success` |
| метод | `_logMessage` | `_log_message` |
| метод | `_logPerformanceMetrics` | `_log_performance_metrics` |
| метод | `_monitorAndCopyCoreDumps` | `_monitor_and_copy_core_dumps` |
| метод | `_platformInitialize` | `_platform_initialize` |
| метод | `_posixSigwaitThread` | `_posix_sigwait_thread` |
| функция | `_redirectedSetUnhandledExceptionFilter` | `_redirected_set_unhandled_exception_filter` |
| метод | `registerCustomConsoleHandler` | `register_custom_console_handler` |
| метод | `registerCustomSignalHandler` | `register_custom_signal_handler` |
| метод | `_releaseOperationSlot` | `_release_operation_slot` |
| метод | `renameCoreFiles` | `rename_core_files` |
| метод | `_restoreCorePattern` | `_restore_core_pattern` |
| метод | `_sanitizeFilenameComponent` | `_sanitize_filename_component` |
| метод | `_sanitizeLogMessage` | `_sanitize_log_message` |
| метод | `_sanitizeLogMessageForAdmin` | `_sanitize_log_message_for_admin` |
| метод | `_sanitizePath` | `_sanitize_path` |
| метод | `setAdminGroupName` | `set_admin_group_name` |
| метод | `setCompress` | `set_compress` |
| метод | `setCorePatternForCrash` | `set_core_pattern_for_crash` |
| метод | `setDirectory` | `set_directory` |
| метод | `setDumpType` | `set_dump_type` |
| метод | `setEnableSourceInfo` | `set_enable_source_info` |
| метод | `setEnableSymbols` | `set_enable_symbols` |
| метод | `setFilename` | `set_filename` |
| метод | `setIncludeHandleData` | `set_include_handle_data` |
| метод | `setIncludeProcessData` | `set_include_process_data` |
| метод | `setIncludeThreadInfo` | `set_include_thread_info` |
| метод | `setIncludeUnloadedModules` | `set_include_unloaded_modules` |
| метод | `setMaxSizeBytes` | `set_max_size_bytes` |
| метод | `setType` | `set_type` |
| метод | `_setupCorePattern` | `_setup_core_pattern` |
| метод | `_setupExceptionHandling` | `_setup_exception_handling` |
| метод | `_setupPosixGracefulShutdown` | `_setup_posix_graceful_shutdown` |
| функция | `_setupWindowsHandlers` | `_setup_windows_handlers` |
| метод | `_startPerformanceMonitoring` | `_start_performance_monitoring` |
| метод | `_unhandledExceptionHandler` | `_unhandled_exception_handler` |
| метод | `_unixSignalHandler` | `_unix_signal_handler` |
| метод | `_updateCache` | `_update_cache` |
| метод | `validateConfiguration` | `validate_configuration` |
| метод | `_validateDirectory` | `_validate_directory` |
| метод | `_validateFilename` | `_validate_filename` |
| метод | `_waitForOperationSlot` | `_wait_for_operation_slot` |
| функция | `_windowsConsoleHandler` | `_windows_console_handler` |
| функция | `_windowsExceptionHandler` | `_windows_exception_handler` |

**applied/logger**

| Вид | Было | Стало |
| --- | --- | --- |
| функция | `AddressToHexLogString` | `address_to_hex_log_string` |
| метод | `_applyLoggerConfigKey` | `_apply_logger_config_key` |
| метод | `_calculateMaxAllowedSize` | `_calculate_max_allowed_size` |
| метод | `_canWriteLog` | `_can_write_log` |
| метод | `_checkLogSizeLimits` | `_check_log_size_limits` |
| метод | `_cleanupOldLogs` | `_cleanup_old_logs` |
| функция | `_createDirectoryIfNotExists` | `_create_directory_if_not_exists` |
| метод | `_createLogFilePath` | `_create_log_file_path` |
| метод | `_createSingleLogFilePath` | `_create_single_log_file_path` |
| метод | `_createTimestampedLogFilePath` | `_create_timestamped_log_file_path` |
| метод | `_extractComponentName` | `_extract_component_name` |
| метод | `_extractDirectoryFromPath` | `_extract_directory_from_path` |
| метод | `_flushBuffer` | `_flush_buffer` |
| метод | `_formatTimestamp` | `_format_timestamp` |
| метод | `_formatTimestampForFilename` | `_format_timestamp_for_filename` |
| метод | `_getLogFilesSortedByTime` | `_get_log_files_sorted_by_time` |
| метод | `_getNormalFormFromComponent` | `_get_normal_form_from_component` |
| метод | `_getShortFormFromComponent` | `_get_short_form_from_component` |
| функция | `_hasPrefix` | `_has_prefix` |
| метод | `_isLoggingEnabledFileExists` | `_is_logging_enabled_file_exists` |
| функция | `_normalizeString` | `_normalize_string` |
| метод | `_parseFunctionNameMode` | `_parse_function_name_mode` |
| функция | `_parsePrefixedValue` | `_parse_prefixed_value` |
| метод | `_parseStackTraceFrames` | `_parse_stack_trace_frames` |
| метод | `_presetMatchesComponent` | `_preset_matches_component` |
| метод | `_readConfigFromFile` | `_read_config_from_file` |
| метод | `_shouldLogByPreset` | `_should_log_by_preset` |
| метод | `_shouldShowFunctionName` | `_should_show_function_name` |
| метод | `_shouldUseTimestampedLogs` | `_should_use_timestamped_logs` |
| метод | `_writeLog` | `_write_log` |

**applied/logging**

| Вид | Было | Стало |
| --- | --- | --- |
| метод | `getLogsDirectory` | `get_logs_directory` |
| метод | `setAppName` | `set_app_name` |
| метод | `toFile` | `to_file` |

**applied/resource_monitor**

| Вид | Было | Стало |
| --- | --- | --- |
| функция | `fileTimeToUint64` | `file_time_to_uint64` |
| функция | `makeLogFilePath` | `make_log_file_path` |
| функция | `sampleCpuLinux` | `sample_cpu_linux` |
| функция | `sampleCpuWin` | `sample_cpu_win` |
| функция | `sampleRamLinux` | `sample_ram_linux` |
| функция | `sampleRamWin` | `sample_ram_win` |
| функция | `sanitizePollInterval` | `sanitize_poll_interval` |
| метод | `startIfEnabled` | `start_if_enabled` |
| функция | `workerLoop` | `worker_loop` |

**applied/settings**

| Вид | Было | Стало |
| --- | --- | --- |
| метод | `ensureExistsWithDefaults` | `ensure_exists_with_defaults` |
| метод | `ensureKeysWithDefaults` | `ensure_keys_with_defaults` |
| метод | `_ensureOrRepairImpl` | `_ensure_or_repair_impl` |
| метод | `repairIfCorrupted` | `repair_if_corrupted` |

**тесты и примеры (вспомогательные функции)**

| Вид | Было | Стало |
| --- | --- | --- |
| функция | `AssignPerfError` | `assign_perf_error` |
| функция | `AssignThreadError` | `assign_thread_error` |
| функция | `_boolToYesNo` | `_bool_to_yes_no` |
| функция | `ChangeSource` | `change_source` |
| функция | `CheckComplexErrorLifetime` | `check_complex_error_lifetime` |
| функция | `CheckCopyAfterCopyAssignment` | `check_copy_after_copy_assignment` |
| функция | `CheckCopyAfterCopyConstruction` | `check_copy_after_copy_construction` |
| функция | `CheckPair` | `check_pair` |
| функция | `CheckThreadError` | `check_thread_error` |
| функция | `ConfigureBad` | `configure_bad` |
| функция | `ConfigureOk` | `configure_ok` |
| метод | `convertToInt` | `convert_to_int` |
| функция | `directoryHasAnyFile` | `directory_has_any_file` |
| функция | `ExpectEqualComplex` | `expect_equal_complex` |
| функция | `ExpectIndependentCopy` | `expect_independent_copy` |
| функция | `ExpectMovedFrom` | `expect_moved_from` |
| функция | `ExpectTransformed` | `expect_transformed` |
| функция | `formatCPUFrequency` | `format_cpu_frequency` |
| функция | `formatMemorySize` | `format_memory_size` |
| метод | `getCerrOutput` | `get_cerr_output` |
| метод | `getClogOutput` | `get_clog_output` |
| метод | `getOutput` | `get_output` |
| метод | `getPlatformName` | `get_platform_name` |
| функция | `InitErrorPair` | `init_error_pair` |
| метод | `isUnix` | `is_unix` |
| метод | `isWindows` | `is_windows` |
| функция | `makeScratchDir` | `make_scratch_dir` |
| функция | `MutateThroughError` | `mutate_through_error` |
| функция | `MutateThroughReference` | `mutate_through_reference` |
| функция | `NoexceptFunction` | `noexcept_function` |
| функция | `PerfThresholdMs` | `perf_threshold_ms` |
| функция | `RunMatrix` | `run_matrix` |
| функция | `ThrowingFunction` | `throwing_function` |
| функция | `TransformSuccess` | `transform_success` |
| функция | `TypeLabel` | `type_label` |
| функция | `TypeName` | `type_name` |
| функция | `ValueBad` | `value_bad` |
| функция | `ValueOk` | `value_ok` |

**Проверено:** GCC 13.2, Release, `-DLUMEX_BUILD_TESTS=ON`: сборка без ошибок и предупреждений, 17980 тестов (столько же, сколько до переименования; имена совпадают), падают те же 91 тест, что и до него (79 `reflection` с именами полей при GCC Release, 9 `fmt`, 2 `cmake`, 1 `serial`), плюс нестабильное семейство `ToCrashReport_ThreadSafe` (до переименования падало в 16 прогонах из 25, после - в 19 из 25); все примеры (`examples.*`, в том числе `examples.fmt.LumexFormatExamplesCoverage`) и `lint.*` проходят. `Scripts/CodeTools/format.py` с clang-format 21.1.7: 432 файла соответствуют, дважды. Библиотека с `-DCMAKE_CXX_STANDARD=11`: GCC 13.2 - без предупреждений, Clang 23.1.0 - те же 177 предупреждений, что и до переименования (`-Wc++14-extensions` в `LumexTypeTraits.hpp` и несколько других). Код Windows (DbgHelp, SEH, `LumexCoreDumpGenerator`, монитор ресурсов, окружение, файловая система): MinGW-w64 GCC 8.3, `-fsyntax-only` для 69 `.cpp` библиотеки на C++11, 17 и 2a - набор ошибок такой же, как до переименования (209 ошибок из-за заголовков MinGW 8.3, например `GetCurrentProcessToken`), новых нет; ветки тестов под Windows не собирались, их 22 измененные строки просмотрены глазами. До переименования не собирается `LumexUtilityMinMaxMacros.cxx11.tests.cpp` (`std::min` в `LumexDebug.hpp` под макросом `min`), а с ним пять утилитных наборов тестов; для сравнения в обоих деревьях применена временная правка `(std::min)`, в коммиты она не входит.

##### Несовместимо: утилиты xml перенесены в модули ядра

**Файлы:** `lumex/core/utility/bit/LumexBit.hpp`, `lumex/core/unicode/**` (новый модуль), `lumex/core/math/ops/LumexMath.hpp`, `lumex/core/utility/ranges/LumexIteratorRange.hpp`, `lumex/core/utility/ranges/LumexRanges.hpp`, `lumex/core/utility/LumexUtility`, `lumex/xml/node/XmlNode.hpp`, `lumex/xml/xpath/ast/XPathAstNode.cpp`, `lumex/xml/xpath/query/XPathQuery.cpp`, `lumex/xml/xpath/variable/XPathVariable.cpp`, `lumex/xml/xpath/constants/XPathConstants.hpp`, `lumex/xml/constants/XmlConstants.hpp`, `lumex/xml/utility/XmlMacros.hpp`, `lumex/xml/utility/XmlCleaner.hpp` (удален), `lumex/xml/text/XmlParseResult.cpp`, `lumex/xml/range/XmlObjectRange.hpp` (удален), `lumex/xml/utility/XmlUtils.hpp`, `lumex/xml/text/XmlParser.cpp`, `lumex/xml/document/XmlDocument.cpp`, `lumex/xml/CMakeLists.txt`, `lumex/xml/LumexXml`, `cmake/LumexOptions.cmake`, `cmake/LumexModules.cmake`, `cmake/LumexLibConfig.cmake.in`, `CMakeLists.txt`, `conanfile.py`, `test_package/**`, `lumex/core/CMakeLists.txt`, `lumex/examples/unicode/**`, `lumex/examples/CMakeLists.txt`, `lumex/tests/core/unicode/**`, `lumex/tests/core/utility/bit/LumexBit.cxx11.tests.cpp`, `lumex/tests/core/utility/ranges/LumexIteratorRange.cxx11.tests.cpp`, `lumex/tests/core/math/ops/LumexMathExactlyEqual.cxx11.tests.cpp`, `lumex/tests/xml/LumexXmlEncodings.cxx11.tests.cpp`, `lumex/tests/xml/LumexXmlRanges.cxx11.tests.cpp`, `lumex/tests/xml/LumexXmlParserOptions.cxx11.tests.cpp`, `lumex/tests/xml/xpath/query/LumexXmlXPathNaN.cxx11.tests.cpp`, `lumex/tests/xml/xpath/query/LumexXmlXPathRelational.cxx11.tests.cpp`, `lumex/tests/xml/LumexXmlGlobalNames.cxx11.tests.cpp`, `lumex/tests/LumexTestStandards.cmake`, `lumex/tests/core/CMakeLists.txt`, `lumex/tests/cmake/**`, `THIRD-PARTY-NOTICES.md`

**Суть:** модуль xml держал внутри себя код, который с XML не связан: перекодировку UTF-8, UTF-16, UTF-32 и Latin-1, перестановку байтов и проверку порядка байтов, диапазон из пары итераторов, сравнение чисел с плавающей точкой без допуска, собственного владельца указателя и копии средств стандартной библиотеки. Этот код переносится в модули, которым он принадлежит, а xml зависит от них. Классы и функции, ушедшие из xml, подчиняются правилам ядра (классы и функции в snake_case, приставки `Xml` и `Object` у ушедших классов нет), поэтому имена меняются в том же коммите, что и место. Старые имена не оставлены ни синонимами, ни псевдонимами в xml, как и в записи про snake_case выше: потребитель переходит на новые имена по таблице ниже, это несовместимое изменение API в релизе `2.0.0.0`. Внутри xml эти имена были деталями реализации (правило потребителя называет `XmlUtils.hpp` и `memory/` не публичным API); ни PeakExpertWeb, ни PeakExpertCE не обращаются к ним (в обоих репозиториях нет ни одного вхождения `lumex::xml` в C++-коде).

Происхождение (pugixml, MIT, Copyright (c) 2006-2026 Arseny Kapoulkine): все утилитные функции, перенесенные из xml (`byte_swap`, `is_little_endian`, функции перекодировки и `to_utf8` / `to_wide` в `unicode`), - собственный код Lumex; из pugixml взята только структура классов (классы-политики перекодировки в `unicode` и `iterator_range`). Это формулировка пользователя от 2026-10-08, она заменила прежнюю ("перенесенный код ведет начало от pugixml, уведомление идет с каждым потребителем"); `THIRD-PARTY-NOTICES.md` и комментарии в перенесенных файлах пересказывают ее. Само уведомление pugixml остается в `THIRD-PARTY-NOTICES.md` для заимствованной структуры.

По модулям:

- `core/utility` (`bit`): `byte_swap` работает с C++11 (раньше только с C++20), добавлена `is_little_endian`; `endian_swap` и `is_little_endian` из xml заменены ими.
- `core/unicode` (новый модуль, только заголовки, C++11, цель `lumex::unicode`, параметр `LUMEX_BUILD_UNICODE`): счетчики, писатели и декодеры UTF-8, UTF-16, UTF-32, Latin-1 и `wchar_t` (`utf/LumexUtf.hpp`) и строковые преобразования `to_utf8` и `to_wide` (`convert/LumexUnicodeConvert.hpp`). `lumex::xml` линкует `lumex::unicode` как PUBLIC, `LUMEX_BUILD_XML` требует `LUMEX_BUILD_UNICODE`, сам `unicode` требует `utility`. В xml остались определение кодировки по метке порядка байтов и объявлению и преобразования буферов (`convert_buffer*`, `convert_buffer_output*`): они зависят от `xml_encoding` и `char_t`. Шаблонный параметр `opt_swap` (типы `opt_true` и `opt_false`) заменен параметром `bool SwapBytes`.
- `core/utility` (`ranges`): `XmlObjectRange` стал `iterator_range` (C++11, `lumex/core/utility/ranges/LumexIteratorRange.hpp`); его возвращают `XmlNode::children ()`, `children (name)` и `attributes ()`. Заголовок больше не включает зонтик `LumexUtility`, а только `LumexAttributes.hpp`. Защита от повторного включения `LumexRanges.hpp` стала `LUMEX_CORE_UTILITY_RANGES_RANGES_HPP`: в каталоге теперь два заголовка. Каталог `lumex/xml/range/` убран.
- `core/math` (`ops`): `exactly_equal` (точное сравнение чисел с плавающей точкой, единственное место с подавлением `-Wfloat-equal`) стало шаблоном для `float`, `double` и `long double`, `constexpr` с C++11. `lumex::xml` линкует `lumex::math` явно, `LUMEX_BUILD_XML` требует `LUMEX_BUILD_MATH`.
- внутри `xml`: то, что есть в стандартной библиотеке или в ядре, заменено ими (таблица "xml" ниже); новых модулей для этого нет. Функциональные объекты `less` и `less_equal` в `XPathAstNode.cpp` стали `std::less<double>` и `std::less_equal<double>`; `equal_to` и `not_equal_to` остались, потому что их вызывают с `bool`, `double` и `XPathString`, а `std::equal_to<void>` есть только с C++14.

Таблица "было -> стало" по модулям (вид: класс, функция, макрос):

**core/utility (bit)**

| Вид | Было | Стало |
| --- | --- | --- |
| функция | `lumex::xml::utility::endian_swap` (для `uint16_t` и `uint32_t`) | `lumex::core::utility::bit::byte_swap` (целые типы размером 1, 2, 4 и 8 байт, кроме `bool`) |
| функция | `lumex::xml::utility::is_little_endian` | `lumex::core::utility::bit::is_little_endian` |

**core/utility (ranges)**

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `lumex::xml::range::XmlObjectRange<Iterator>` (`lumex/xml/range/XmlObjectRange.hpp`) | `lumex::core::utility::ranges::iterator_range<Iterator>` (`lumex/core/utility/ranges/LumexIteratorRange.hpp`) |

**core/math (ops)**

| Вид | Было | Стало |
| --- | --- | --- |
| функция | `lumex::xml::utility::exactly_equal (double, double)` | `lumex::core::math::ops::exactly_equal<T> (T, T)` для `float`, `double`, `long double` |

**xml (внутри модуля)**

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `lumex::xml::utility::XmlCleaner<T>` (`.data`, `.release ()`, `lumex/xml/utility/XmlCleaner.hpp`) | `std::unique_ptr<T, void (*) (T *)>` (`.get ()`, `.release ()`); заголовок удален и убран из `LumexXml` |
| класс | `lumex::xml::utility::opt_true`, `opt_false` (шаблонные параметры разбора `opt_escape`, `opt_trim`, `opt_eol`) | параметры `bool` (`Escape`, `Trim`, `Eol`; `true`, `false`) |
| функция | `lumex::xml::utility::is_nan` | `std::isnan` |
| функция | `lumex::xml::utility::gen_nan` | `std::numeric_limits<double>::quiet_NaN ()`; знаковый бит NaN теперь 0 (раньше деление `0.0 / 0.0` давало NaN со знаком) |
| макрос | `LUMEX_XML_CONSTANT` | `LUMEX_CONSTINIT_CONSTANT` |
| макрос | `LUMEX_XML_UNLIKELY` | `LUMEX_ATTRIBUTE_UNLIKELY_COND` |
| макрос | `LUMEX_XML_SNPRINTF` | `std::snprintf (buf, sizeof (buf), ...)` |

**core/unicode (utf)**, пространство имен `lumex::core::unicode::utf` (раньше `lumex::xml::utility`)

| Вид | Было | Стало |
| --- | --- | --- |
| класс | `utf8_counter` | `utf8_counter` |
| класс | `utf16_counter` | `utf16_counter` |
| класс | `utf32_counter` | `utf32_counter` |
| класс | `utf8_writer` | `utf8_writer` |
| класс | `utf16_writer` | `utf16_writer` |
| класс | `utf32_writer` | `utf32_writer` |
| класс | `latin1_writer` | `latin1_writer` |
| класс | `utf8_decoder` | `utf8_decoder` |
| класс | `utf16_decoder<opt_swap>` (`opt_true` / `opt_false`) | `utf16_decoder<SwapBytes>` (`true` / `false`) |
| класс | `utf32_decoder<opt_swap>` (`opt_true` / `opt_false`) | `utf32_decoder<SwapBytes>` (`true` / `false`) |
| класс | `latin1_decoder` | `latin1_decoder` |
| класс | `wchar_decoder` | `wchar_decoder` |
| класс | `wchar_selector<size>` | `wchar_selector<Size>` |
| псевдоним | `wchar_counter` | `wchar_counter` |
| псевдоним | `wchar_writer` | `wchar_writer` |

**core/unicode (convert)**, пространство имен `lumex::core::unicode::convert`

| Вид | Было | Стало |
| --- | --- | --- |
| функция | `lumex::xml::utility::as_utf8` (`wchar_t const *`, `std::basic_string<wchar_t> const &`) | `to_utf8` (те же две формы и новая `(wchar_t const *, std::size_t)`) |
| функция | `lumex::xml::utility::as_wide` (`char const *`, `std::string const &`) | `to_wide` (те же две формы и новая `(char const *, std::size_t)`) |
| функция | `lumex::xml::utility::as_utf8_begin`, `as_utf8_end`, `as_utf8_impl`, `as_wide_impl` | убраны; их роль у `to_utf8 (wchar_t const *, std::size_t)` и `to_wide (char const *, std::size_t)` |
| функция | `lumex::xml::utility::strlength_wide` | убрана (`to_utf8` берет длину из `std::wcslen`) |

**Проверено:** `bit`: набор `UtilityBit` на GCC 13.2, Release, `-Werror`: сборка без предупреждений, на C++11 17 тестов набора (было 4), на C++20 46; `byte_swap` ниже C++20 сверяется с обратным порядком байтов, записанным вручную через `memcpy`, на 512 псевдослучайных значениях для каждого из шести типов размером 2, 4 и 8 байт, знаковых и беззнаковых, и вычисляется в константном выражении. Проверка правкой: шесть порч (сдвиг 4-байтового обмена, сдвиг 8-байтового, маска 8-байтового, сдвиг 2-байтового, инверсия `is_little_endian`, потеря знакового преобразования) каждая роняет набор. `unicode`: наборы `LumexUnicodeCxx11Tests`, `LumexUnicodeUtfCxx11Tests` и `LumexUnicodeConvertCxx11Tests` на GCC 13.2, Release, `-Werror`, всего 64 теста (политики, преобразования, глобальные имена, `wchar_selector<2>` и `<4>` на любом хосте, 1 000 000 символов туда и обратно). Те же 57 тестов политик и преобразований проходят на реализации до переноса (через временный слой имен), поэтому поведение не изменилось; 20 новых тестов `XmlEncodingTest` (UTF-16 и UTF-32 в обоих порядках, Latin-1, метка порядка байтов, определение кодировки по первым байтам и по объявлению, вывод длиннее буфера сериализатора) проходят и до переноса, и после. Тесты xml (`ctest -R '^xml\.'`): 149 -> 169 на C++11, 151 -> 171 на C++17 и C++20. Проверка правкой: 27 порч заголовков `unicode` (счетчики, писатели, границы, быстрый путь ASCII, пары суррогатов, перестановка байтов) и 10 порч кода xml (определение кодировки, выбор порядка байтов, обрезка последовательности на границе буфера) роняют тесты, 3 порчи сначала выжили и закрыты тестами `GivenALengthThatCutsASequence...`, `GivenAStrayByteInsideAsciiRuns...`, `GivenAHighSurrogateBeforeANonLowSurrogate...`; одна порча (метка UTF-8) не меняет поведение. CMake: два новых случая (`require_fail_xml_without_unicode`, `require_fail_unicode_without_utility`), `cmake.*` и `lint.*` проходят, кроме двух, которые падали и до изменения (`require_ok_independents_off`, `require_fail_json_without_string_view`: в них у `resource_monitor` остаются включенными `expected` и `string_view`; в конце ветки их исправляет отдельный коммит). Оба примера `unicode` завершаются с кодом 0. `iterator_range`: 9 тестов в наборе `UtilityRanges` на C++11 и на C++20 (указатели, итераторы `vector` и `list`, обратные итераторы, однопроходный итератор потока) и 5 тестов `XmlRangeTest` (типы возврата `children`, `children (name)`, `attributes`, обходы, пустой диапазон); 6 порч (`empty`, `begin`, `end`, порядок в конструкторе, типы) роняют тесты. Тесты xml: 169 -> 174 на C++11, 171 -> 176 на C++17 и C++20. `exactly_equal`: 9 тестов в наборах `MathOps` на C++11, 17 и 20 (нули с разными знаками, бесконечности, NaN, соседние числа, `volatile`-операнды, константное выражение, отказ для целых и смешанных типов); 5 порч (`!=`, допуск, без `constexpr`, целые принимаются, без `noexcept`) роняют тесты; сборка `LumexXml` с `-Werror` и `-Wfloat-equal` на GCC 13.2 без предупреждений. Замены внутри xml: до правки добавлены тесты, которые проходят и на старом коде (`XmlParserOptions`: 128 строк по всем 32 сочетаниям пяти параметров разбора на четырех документах, значения записаны со старого парсера, и 6 проверок смысла; `XPathRelational`: 11 тестов `<`, `<=`, `>`, `>=` на числах, строках, булевых значениях и множествах узлов; `XPathNaN`: 7 тестов мест, где движок создает NaN); тесты xml на GCC 13.2: 199 на C++11, 201 на C++17 и C++20 (из них 29 в `xml/xpath/query/`). Проверка правкой: 9 из 10 порч (NaN в трех местах, `isnan` в булевом правиле, `less` и `less_equal`, таблицы разбора атрибута и текста, `release` в `XPathQuery`) роняют тесты; порча `floor` без проверки NaN не меняет поведение (`floor (NaN)` дает NaN). Три из них (`less`, таблица разбора атрибута, флаг экранирования текста) сначала выжили и закрыты тестами `XPathRelational` и `XmlParserOptions`. Итог ветки: `./create_release.sh` с GCC 8.3, GCC 13.2, Clang 23.1.0 и MinGW 8.3 на C++11 и C++20 с `-Werror` (8 сборок): 0 предупреждений и 0 ошибок, `warns/` пуст. Полная сборка (292 цели, 901 шаг, 0 предупреждений) и `ctest -j4` на GCC 13.2, Release, `-Werror`, дерево с тестами в каталогах по каталогам исходников: 18944 теста, падают 91: 79 `reflection`, 9 `fmt`, 1 `serial` (`LumexSerialProberPty`) и 2 нестабильных `ToCrashReport_ThreadSafe` (C++11 и C++20); новых падений нет, все 132 теста `cmake.*` и `lint.*` проходят (два прежних падения закрыты отдельным коммитом ветки). Наборы `Unicode` (три каталога), `UtilityBit`, `UtilityRanges`, `MathOps`, `Xml` и `XmlXpathQuery` собираются с `-Werror` без предупреждений и проходят на всех своих стандартах на Clang 23.1.0 (до C++26) и на GCC 8.3 (до C++2a), 4 примера `xml` и 2 примера `unicode` завершаются с кодом 0 на обоих; новые тесты и оба примера проходят проверку синтаксиса MinGW на C++11 и 17 (проверено до переноса тестов в каталоги: заголовки, исходники и примеры с тех пор не менялись, а у перенесенных файлов тестов изменена только строка с путем и один комментарий). Doxygen 1.8.20: 32 предупреждения, все в `LumexCPUVectorizationCapabilities.hpp`, который не менялся; в новых и измененных файлах предупреждений нет. Не проверялось: сборка и запуск на Windows (`wchar_t` из 2 байт проверен только через `wchar_selector<2>` и компиляцию MinGW).

##### Шаблоны переменных `is_invocable_v`, `is_callable_v`, `is_optional_v`, `is_expected_v` есть только с C++14

**Файлы:** `lumex/core/utility/traits/LumexTypeTraits.hpp`, `lumex/core/expected/result/Expected.hpp`, `lumex/core/exceptions/LumexExceptionWrapper.hpp`, `lumex/core/time/timer/LumexTimer.hpp`, `lumex/tests/core/utility/LumexTypeTraits.cxx11.tests.cpp`, `lumex/tests/core/utility/LumexTypeTraitsTopics.cxx11.tests.cpp`, `lumex/tests/core/exceptions/LumexExceptionWrapper.cxx11.tests.cpp`

**Суть:** четыре шаблона переменных были объявлены без проверки стандарта и на C++11 принимались компиляторами как расширение, с предупреждением `variable templates are a C++14 extension` (4 места в `LumexTypeTraits.hpp`, повторяются в каждой единице трансляции). Теперь они объявлены под `#if LUMEX_HAS_VARIABLE_TEMPLATES`, как `is_ostreamable_v` и `is_streamable_v`; на C++11 код пишет `is_callable<...>::value`, так же пишет и сама библиотека (`Expected`, `ExceptionWrapper`, `Timer`). Код на C++11, который использовал `_v`, нужно перевести на `::value`; на C++14 и выше ничего не меняется.

**Проверено:** сборка наборов `Utility`, `Exceptions`, `Expected`, `Time`, `Optional` на C++11: Clang 23.1.0 и GCC 13.2 до правки давали по 4 предупреждения, после - 0; использование `is_optional_v` на C++11 дает ошибку компиляции, на C++14 компилируется (обе проверки на обоих компиляторах); новый тест `GivenVariableTemplates_WhenRead_ThenEqualToValue` проходит на C++14, 17, 20; наборы C++11 проходят (2176 тестов, кроме плавающего `ToCrashReport_ThreadSafe`).

##### Примеры `resource_monitor` показывают `LumexProcessMonitor`

**Файлы:** `lumex/examples/resource_monitor/example_resource_monitor.cpp`, `lumex/examples/resource_monitor/example_resource_monitor_workflow.cpp`, `lumex/examples/resource_monitor/CMakeLists.txt`

**Суть:** оба примера показывали только `start_if_enabled` и `stop`, а заголовки в тексте еще писали `startIfEnabled`. Теперь `LumexResourceMonitorExample` проходит по шести шагам: `stop` без запуска, `start_if_enabled` и `stop`, один процесс по номеру (первый замер без доли CPU, второй с долей), процессы по имени (`find_by_name`, `sample_by_name`, `total`), номер несуществующего процесса (`process_query_error`), `forget_exited`. `LumexResourceMonitorExampleWorkflow` измеряет стоимость куска работы: замер до, 32 МиБ данных и сложение около 150 мс, замер после (рост резидентной памяти и доля CPU). Привязка `CXX_STANDARD 17` в `CMakeLists.txt` убрана: модуль собирается с C++11, примеры написаны на C++11.

**Проверено:** `examples.resource_monitor.*` проходят в CTest (GCC 13.2, Release); разбор обоих файлов с `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow` без предупреждений: GCC 13.2 и Clang 23.1.0 на C++11, 14, 17, 20, GCC 8.3 и MinGW (GCC 8.3-posix) на C++11, 14, 17; сборка и запуск на Windows не проверялись.

##### `Expected` ближе к `std::expected`: `transform` с `void` и с `Expected`, преобразования, сравнения

**Файлы:** `lumex/core/expected/result/Expected.hpp`, `lumex/core/expected/result/ExpectedVoid.hpp`, `lumex/core/expected/result/ExpectedDetail.hpp` (новый), `lumex/core/expected/error/Unexpected.hpp`, `lumex/core/expected/error/BadExpectedAccess.hpp`, `lumex/core/expected/Expected`, `lumex/core/utility/traits/LumexTypeTraits.hpp`, `lumex/examples/expected/example_expected.cpp`, `lumex/examples/expected/example_expected_workflow.cpp`, `lumex/tests/core/expected/ExpectedParitySupport.hpp` (новый), `lumex/tests/core/expected/error/CMakeLists.txt`, `lumex/tests/core/expected/result/CMakeLists.txt`, в `lumex/tests/core/expected/result/`: `ExpectedTransform.cxx11.tests.cpp`, `ExpectedMonadicParity.cxx11.tests.cpp`, `ExpectedConversion.cxx11.tests.cpp`, `ExpectedComparison.cxx11.tests.cpp`, `ExpectedStdParity.cxx17.tests.cpp`, в `lumex/tests/core/expected/error/`: `UnexpectedParity.cxx11.tests.cpp`, `UnexpectedParity.cxx17.tests.cpp`, `BadExpectedAccessBase.cxx11.tests.cpp` (все новые), `lumex/tests/core/utility/traits/LumexInvokeMember.cxx11.tests.cpp` (новый)

**Суть:** пункт 69 списка дел. `transform` с функцией, возвращающей `void`, и с функцией, возвращающей `Expected`, отвергался (у `Expected<void, E>` `void` принимался только в специализации). Теперь, как у `std::expected`, `transform` с `void` дает `Expected<void, E>` в обеих ветках и в обеих специализациях, а результат-`Expected` дает `Expected<Expected<U, G>, E>` без разворачивания; `transform_error` тоже принимает `Expected` (не `void`, как и в стандарте). Функция в `and_then`, `transform`, `or_else` и `transform_error` теперь пересылаемая ссылка, а не копия, и вызывается как `std::invoke`: работают указатели на функции-члены и на данные-члены, `std::reference_wrapper`, функциональные объекты, которые нельзя копировать, и их `operator()` с ref-квалификаторами; результат `transform` строится на месте, без лишнего перемещения. Результат `and_then` и `or_else` теперь без `const` и ссылки, несовпадение типа ошибки (значения) ловит `static_assert` с текстом. Общие ограничения операций записаны один раз в новом `ExpectedDetail.hpp` и используются как `requires` с C++20 и как `std::enable_if` раньше.

По проходу по всей поверхности `std::expected` добавлено еще: преобразующие конструкторы из `Expected<U, G>` и `Expected<void, G>` (неявные или `explicit` по тем же правилам, что в стандарте; для `bool` по LWG 3836); конструкторы из `unexpected<G>` неявны, если ошибка преобразуется (`return unexpected<E> (e);` работает); явный конструктор значения; конструкторы `in_place` и `unexpect` с `std::initializer_list` и с ограничениями; `emplace` со списком; присваивание значения и `unexpected`; конструктор по умолчанию только для типа, у которого он есть; условный `noexcept` у перемещающего присваивания (раньше безусловный, брошенное исключение завершало программу) и у `swap` (одинаково на всех стандартах); `==` между `Expected` разных типов, со значением и с `unexpected` с любой стороны, `!=` и обратные формы до C++20; `Expected<const T, E>`; проверки `T` и `E` (массив, `void`, `const`, `unexpected`); у `unexpected` пересылающий конструктор, `in_place`, список, `swap`, `==`, `!=`, `noexcept`-`error ()`, `constexpr`, подсказка вывода шаблона (C++17); `bad_expected_access<void>` как общий базовый класс. Остаются осознанные отличия (список в комментарии к классу `expected`): `error ()`, `operator*` и `operator->` проверяют предусловие через `LUMEX_ASSERT`; свои теги `in_place_tag` и `unexpect_t`; присваивания и `emplace` через копию и `swap`. Не закрыто: конструктор копирования не удаляется условно (в C++11 нельзя), класс не тривиально копируем и не используется в константных выражениях. Несовместимо: `unexpected<E&>` (и `void`, массив) не компилируется; перемещающее присваивание не `noexcept` при бросающем перемещении.

Побочная находка: у `traits::invoke::invoke_result` для указателя на функцию-член без аргументов в MinGW GCC 8.3 результатом был тип функции (перегрузка для указателя на данные-член подходила и к функциям-членам; GCC 8 и 13 на Linux ее отвергали). Перегрузка ограничена, тест `LumexInvokeMember` падает на MinGW без правки.

**Проверено:** GCC 13.2, GCC 8.3 и Clang 23.1.0 (libc++), Release, `-DLUMEX_BUILD_TESTS=ON`: тестов `expected` стало 579, 586 и 586 на C++11, 17 и 20 (было 417, 418 и 418), все проходят, оба примера выполняются; MinGW (GCC 8.3-posix): все файлы тестов `expected` и оба примера компилируются на C++11 и C++2a, примеры без предупреждений при `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`; библиотека на четырех компиляторах (GCC 8.3, GCC 13.2, Clang 23.1.0, MinGW) на C++11 и C++20: 0 предупреждений, `warns/` пуст; `Scripts/CodeTools/format.py` с clang-format 21.1.7: 461 файл соответствует, дважды; `lint.*` проходят; Doxygen 1.8.20: 0 предупреждений в измененных файлах. Каждый новый тест проверен 26 мутациями реализации (ограничение `transform_error`, перемещение вместо копии, построение на месте, функция по значению, `explicit`, `noexcept`, правило `bool`, `==`, `!=`, базовый класс исключения и другие): все 26 ловятся тестами. Дифференциальная проверка с `std::expected` из libstdc++ 13 на C++23 (227 проверок: 6 сценариев - `transform`, `and_then`, `or_else`, наблюдатели, модификаторы, преобразования, сравнения, - 5 форм типов результатов и 216 свойств типов): результаты сценариев и типы результатов совпадают; расходятся только удаление копирования (8) и места, где libstdc++ 13.2 не ограничивает сравнения (4) или предшествует LWG 3836 (2). Тесты `cmake.*` проходят, кроме `cmake.require_ok_independents_off` и `cmake.require_fail_json_without_string_view` (оба о графе зависимостей `resource_monitor`: он требует `expected` и `string_view`; `cmake/` и `lumex/tests/cmake/` этими изменениями не тронуты).

##### Base64: перегрузки со строкой есть в каждом стандарте, ниже C++17 они берут `lumex_string_view`

**Файлы:** `lumex/core/base64/codec/Base64.hpp`, `lumex/core/base64/encode/Encoder.hpp`, `lumex/core/base64/decode/Decoder.hpp`, `lumex/core/base64/validate/Validator.hpp`, `lumex/core/base64/LumexBase64`, `lumex/core/base64/CMakeLists.txt`, `lumex/core/CMakeLists.txt`, `cmake/LumexModules.cmake`, `conanfile.py`, `lumex/examples/base64/example_base64.cpp`, `lumex/tests/core/base64/encode/LumexBase64EncoderStringView.cxx11.tests.cpp` (новый), `lumex/tests/core/base64/decode/LumexBase64DecoderStringView.cxx11.tests.cpp` (новый), `lumex/tests/core/base64/validate/LumexBase64ValidatorStringView.cxx11.tests.cpp` (новый), `CMakeLists.txt` в `lumex/tests/core/base64/encode/`, `decode/` и `validate/`, `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/cases/wiring_string_view.cmake` (новый), `require_fail_base64_without_string_view.cmake` (новый), `require_graph_all_edges.cmake`, `require_fail_json_without_string_view.cmake`, `lumex/tests/cmake/consumer/standard_mismatch/CMakeLists.txt`, `main.cpp`

**Суть:** пункт 6 плана из пункта 94 списка дел (каждый класс работает с C++11). `encoder::encode (std::string_view)` была только с C++17: на C++11 и C++14 не собирались `encode ("abc")` и `encode (std::string)` (оставались указатель с размером, вектор и `span`). `decoder::decode (text)`, `decoder::decode (text, out)` и `validator::is_valid_base64 (text)` до C++17 брали `std::string const &` (так был устроен `string_type_t`), и `lumex_string_view` они не принимали ни в каком стандарте. Теперь все четыре перегрузки берут `string_type_t` без условия на стандарт: `std::string_view` с C++17 (как было) и `lumex_string_view` модуля `string_view` ниже. Литерал, `char const *` и `std::string` неявно преобразуются в любой из них, поэтому существующие вызовы собираются прежними. Представление размерное: NUL внутри `std::string` остается символом (при кодировании байтом, при декодировании недопустимым символом), `char const *` кончается на первом NUL, нулевой `char const *` ниже C++17 дает пустой текст (`std::string_view` такого не допускает). Экспортируемые функции и ABI те же: перегрузки встроенные над функциями «указатель, размер», множество экспортируемых символов `libLumexCore_base64` до и после правки совпадает (7 символов, `nm -D`). Набор перегрузок `encode` на C++11 проверен на неоднозначность: литерал, `char const *`, `char[]`, `std::string`, `std::vector<byte_type>`, `std::array`, массив байтов, `span` и `(nullptr, 0)` вызываются без ошибок.

Решение о связи с модулем `string_view` (скомпилированная библиотека, 484 + 526 строк `.cpp`): перегрузкам нужна именно ссылка на нее, а не один заголовок. Конструкторы `lumex_string_view` из `char const *` и из `std::string const &` скомпилированы в библиотеку, а `data ()` и `size ()` встроенные: в единице трансляции, которая преобразует литерал и строку, `nm` показывает ровно два неопределенных символа (эти два конструктора), а сама `libLumexCore_base64` ни одного символа `string_view` не требует. Кроме того класс объявлен `LUMEX_API`, то есть `dllimport` на Windows. Поэтому, как у `span`, добавлены ребро времени конфигурации `lumex_require_module (LUMEX_BUILD_BASE64 LUMEX_BUILD_STRING_VIEW)`, `lumex::string_view` в `target_link_libraries` (PUBLIC, потребитель получает его вместе с `lumex::base64`) и `core_string_view` в требованиях компонента Conan `core_base64`; в `lumex/core/CMakeLists.txt` `string_view` подключается раньше `base64`. Конфигурация с `LUMEX_BUILD_BASE64=ON` и `LUMEX_BUILD_STRING_VIEW=OFF` теперь отклоняется (новый случай `cmake.require_fail_base64_without_string_view`); случай `cmake.require_fail_json_without_string_view` выключает `base64`, чтобы по-прежнему проверять ребро `json`. Новый случай `cmake.wiring_string_view` закрепляет ребро, ссылку, порядок подкаталогов и Conan.

**Несовместимо:** `string_type_t` ниже C++17 был `std::string const &`, теперь это `lumex_string_view` (по значению): тот, кто брал адрес `decoder::decode` или `validator::is_valid_base64` или называл этот тип, правит код. Конфигурация с `LUMEX_BUILD_BASE64=ON` без `LUMEX_BUILD_STRING_VIEW` отклоняется; потребитель без CMake добавляет `LumexCore_string_view` в компоновку. `lumex_string_view` как аргумент с C++17 не принимается (параметр там `std::string_view`).

**Проверено:** 28 новых тестов в трех файлах `*StringView.cxx11.tests.cpp` (кодирование 11, декодирование 10, проверка 7): литерал и векторы RFC 4648, `char const *`, `std::string`, `std::string` с вложенными нулями (`encode`: все байты, `decode` и `is_valid_base64`: недопустимый символ, а литерал с нулем кончается на нем), подвид и пустое представление, все 256 значений байта и все длины от 0 до 69 (строка совпадает с вектором), набор перегрузок без неоднозначности, вызовы `(указатель, размер)`, `(nullptr, 0)`; четыре теста относятся только к ниже C++17 (нулевой `char const *`, подвиды `lumex_string_view`). Тесты модуля `base64.` на GCC 13.2 (Release, CTest): 302 теста проходят, 96, 100 и 106 на C++11, 17 и 20 (было 68, 76 и 82). Наборы C++11 проходят на GCC 8.3 и на Clang 23.1.0 с libc++ и с libstdc++ (96 тестов). Множество экспортируемых символов `libLumexCore_base64.so` до и после правки одно и то же (7 символов, `nm -D`), и сама библиотека не требует ни одного символа `string_view` (требуют только единицы трансляции потребителя). Проверка правкой: девять правок `Encoder.hpp`, `Decoder.hpp` и `Validator.hpp` (размер минус один, длина по `strlen`, нет защиты пустого представления) ловятся тестами на C++11 и C++17, кроме одной, которая не меняет результата (защита пустого представления в `decode (text)`: пустой вектор и без нее). Оба примера `base64` собираются и выполняются (`examples.base64.*`, CTest, GCC 13.2). Предупреждений нет (`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast`, GCC 8.3 и Clang 23.1.0 на C++11, 17, GCC 13.2 на C++11, 14, 17, 20 для новых тестов и примера); библиотека на GCC 8.3, GCC 13.2, Clang 23.1.0 и MinGW (GCC 8.3-posix) на C++11 и C++20: 0 предупреждений, `warns/` пуст. Под ASan и UBSan: 1638 тестов модулей `base64`, `crc`, `exceptions.exception`, `xml` и `string_view` на C++11, 14 и 17 (один прогон на все пять модулей, GCC 13.2, Debug) проходят, сообщений санитайзеров нет, 28 тестов `Perf_*` в Debug пропускаются. Случаи `cmake.*` (в том числе новые и два `consumer_standard_mismatch_*`, где библиотека собрана на C++11 и C++20, а потребитель на C++20 и C++11): 142 теста с метками `cmake` и `lint` проходят (GCC 13.2). Windows (MinGW-DLL, GCC 8.3-posix, таблицы экспорта до и после): `libLumexCore_base64.dll`, `libLumexCore_crc.dll`, `libLumexCore_exceptions.dll` и `libLumexXml.dll` (и `libLumexApplied_settings.dll` через `xml`), собранные на C++11, получили по шесть лишних экспортируемых символов: это шесть операторов сравнения `lumex_string_view`, которые в заголовке `string_view` объявлены `LUMEX_API inline`, а `LUMEX_EXPORTS` задан для каждой DLL (на C++20 лишние символы только у `libLumexCore_crc.dll`, он всегда собирается на C++11); та же картина уже была у `libLumexApplied_resource_monitor.dll`. Ничего из них не импортируется из этих DLL. На ELF множества экспортируемых символов не изменились. Сборка на Windows с MSVC и clang-cl не проверялась.

##### CRC: `compute_crc_catalog` и `compute_crc_with_rev_eng_params` принимают текст в каждом стандарте, `append_crc_least_significant_byte_first` есть с C++11

**Файлы:** `lumex/core/crc/catalog/LumexCrcCatalog.hpp`, `lumex/core/crc/LumexCrc`, `lumex/core/crc/CMakeLists.txt`, `cmake/LumexModules.cmake`, `conanfile.py`, `lumex/examples/crc/example_crc.cpp`, `lumex/tests/core/crc/catalog/LumexCrcCatalogStringView.cxx11.tests.cpp` (новый), `LumexCrcCatalog.cxx17.tests.cpp`, `CMakeLists.txt` в `lumex/tests/core/crc/catalog/`, `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/cases/require_fail_crc_without_string_view.cmake` (новый), `wiring_string_view.cmake`, `wiring_span.cmake`, `require_graph_all_edges.cmake`, `require_fail_json_without_string_view.cmake`, `lumex/tests/cmake/consumer/standard_mismatch/CMakeLists.txt`, `main.cpp`

**Суть:** те же правила, что у `base64`. Перегрузки `compute_crc_catalog (index, std::string_view)` и `compute_crc_with_rev_eng_params (params, std::string_view)` были только с C++17; теперь они есть в каждом стандарте и берут новый псевдоним `lumex::core::crc::catalog::text_view_t`: `std::string_view` с C++17, `lumex_string_view` ниже (литерал, `char const *` и `std::string` преобразуются в оба; размерное представление, пустой текст дает 0, как у указателя и размера). Функция `append_crc_least_significant_byte_first (params, std::vector<std::uint8_t> &)` стояла внутри условия C++17, хотя ей нужен только `std::vector`; она вынесена из него и есть с C++11. Экспортируемые функции и ABI те же (`libLumexCore_crc`: 990 символов до и после, `nm -D`); перегрузки встроенные. Модуль `crc` требует `string_view`: ребро `lumex_require_module (LUMEX_BUILD_CRC LUMEX_BUILD_STRING_VIEW)`, `lumex::string_view` в `target_link_libraries`, `core_string_view` в Conan (причина та же, что у `base64`: конвертирующие конструкторы скомпилированы в библиотеку `string_view`, класс `dllimport` на Windows). Конфигурация с `LUMEX_BUILD_CRC=ON` и `LUMEX_BUILD_STRING_VIEW=OFF` отклоняется (новый случай `cmake.require_fail_crc_without_string_view`).

**Проверено:** 8 новых тестов в `LumexCrcCatalogStringView.cxx11.tests.cpp` (литерал, `char const *`, `std::string`, подвид и представление равны форме с указателем и размером для всех индексов каталога (112); вложенные нули; пустой текст и неверный индекс дают 0; параметры RevEng; набор перегрузок без неоднозначности: литерал, `char[]`, `std::string`, `std::vector`, `std::array`, массив, `span`, указатель; `append_crc_least_significant_byte_first` для ширин 1..64 и нулевой ширины), один из них (тест `append`) перенесен из файла C++17. Тесты модуля `crc.` на GCC 13.2 (Release, CTest): 2597 тестов проходят; набор каталога 305, 305, 309 и 311 тестов на C++11, 14, 17 и 20 (было 297, 297, 302 и 304). Наборы C++11 проходят на GCC 8.3 и на Clang 23.1.0 с libc++ и с libstdc++. Множество экспортируемых символов `libLumexCore_crc.so` одно и то же до и после (990 символов, `nm -D`). Проверка правкой: восемь правок (`compute_crc_catalog` и `compute_crc_with_rev_eng_params`: размер минус один, длина по `strlen`, пустой текст дает 1; `append`: сдвиг на бит, на байт короче) ловятся тестами на C++11 и C++17. Оба примера `crc` собираются и выполняются (`examples.crc.*`, CTest, GCC 13.2). Предупреждений нет (флаги как у `base64`; пример `example_crc.cpp` тоже). Под ASan и UBSan: тот же прогон под ASan и UBSan (1638 тестов пяти модулей на C++11, 14 и 17, GCC 13.2, Debug): все проходят, сообщений санитайзеров нет. Windows: см. запись про `base64` (шесть лишних символов в таблице экспорта `libLumexCore_crc.dll`).

##### `lumex_base_exception`: конструктор от строкового представления есть в каждом стандарте

**Файлы:** `lumex/core/exceptions/exception/LumexException.hpp`, `lumex/core/exceptions/LumexException`, `lumex/core/exceptions/CMakeLists.txt`, `cmake/LumexModules.cmake`, `conanfile.py`, `lumex/tests/core/exceptions/exception/LumexExceptionStringView.cxx11.tests.cpp` (новый), `CMakeLists.txt` в `lumex/tests/core/exceptions/exception/`, `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/cases/require_fail_exceptions_without_string_view.cmake` (новый), `wiring_string_view.cmake`, `require_graph_all_edges.cmake`, `require_fail_json_without_string_view.cmake`, `lumex/tests/cmake/consumer/standard_mismatch/main.cpp`

**Суть:** `lumex_base_exception (std::string_view)` была только с C++17, а `lumex_string_view` не принимался ни в каком стандарте. Теперь ниже C++17 конструктор берет `lumex_string_view` (встроенный, делегирует в экспортируемый `std::string &&`, копирует ровно представление, NUL внутри сохраняется), с C++17 по-прежнему `std::string_view`. Конструкторы `char const *`, `std::string const &` и `std::string &&` и их выбор не изменились (литерал, `std::string` и `lvalue`/`rvalue` строки берут свои). Экспортируемые функции и ABI те же (`libLumexCore_exceptions`: 62 символа до и после). Типы, объявленные `LUMEX_DEFINE_EXCEPTION`, получают по-прежнему три конструктора сообщения, без представления (как и с C++17). Модуль `exceptions` требует `string_view`: ребро `lumex_require_module (LUMEX_BUILD_EXCEPTIONS LUMEX_BUILD_STRING_VIEW)`, `lumex::string_view` в `target_link_libraries`, `core_string_view` в Conan; `LUMEX_BUILD_EXCEPTIONS=ON` с `LUMEX_BUILD_STRING_VIEW=OFF` отклоняется (новый случай `cmake.require_fail_exceptions_without_string_view`).

**Проверено:** 4 новых теста в `LumexExceptionStringView.cxx11.tests.cpp` (копируется ровно представление, пустое и с нулями, все конструкторы дают одно сообщение, `throw` и перехват) и проверки на этапе компиляции, какие аргументы строят исключение. Тесты модуля `exceptions.` на GCC 13.2 (Release, CTest, `-j4`): 171 из 172 проходят; не прошел известный плавающий `LumexBaseException_ToCrashReport_ThreadSafe.cxx20` (отчеты о падении пишутся в каталог `crashes`; отдельно на пяти запусках подряд он не прошел один раз); под ASan и UBSan тот же тест проходит на C++11 и C++17. Тесты `exceptions.exception.` на C++11 (18 тестов, CTest, по одному процессу на тест) проходят на GCC 8.3, на Clang 23.1.0 с libc++ и с libstdc++ и на GCC 13.2. Тестов модуля 56, 58 и 58 на C++11, 17 и 20 (было 52, 54 и 54). Множество экспортируемых символов `libLumexCore_exceptions.so` одно и то же до и после (62 символа, `nm -D`). Проверка правкой: четыре правки (оба конструктора: размер минус один и длина по `strlen`; каждая на своем стандарте) ловятся тестами. Предупреждений нет (флаги как у `base64`). Под ASan и UBSan: тот же прогон под ASan и UBSan (1638 тестов пяти модулей на C++11, 14 и 17, GCC 13.2, Debug): все проходят, сообщений санитайзеров нет. Windows: см. запись про `base64` (шесть лишних символов в `libLumexCore_exceptions.dll`).

##### XML: 23 перегрузки на `string_view_t` есть в каждом стандарте, ниже C++17 это `lumex_string_view` (`lumex_wstring_view` в режиме `wchar_t`)

**Файлы:** `lumex/xml/types/XmlTypes.hpp`, `lumex/xml/node/XmlNode.hpp`, `lumex/xml/attribute/XmlAttribute.hpp`, `lumex/xml/text/XmlText.hpp`, `lumex/xml/utility/XmlUtils.hpp`, `lumex/xml/LumexXml`, `lumex/xml/CMakeLists.txt`, `cmake/LumexModules.cmake`, `conanfile.py`, `lumex/tests/xml/LumexXmlStringView.cxx11.tests.cpp` (новый), `lumex/tests/xml/CMakeLists.txt`, `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/cases/require_fail_xml_without_string_view.cmake` (новый), `wiring_string_view.cmake`, `require_graph_all_edges.cmake`, `require_fail_json_without_string_view.cmake`, `lumex/tests/cmake/consumer/standard_mismatch/main.cpp`

**Суть:** `string_view_t` и 23 перегрузки на нем существовали только с C++17: 17 членов `XmlNode` (`child`, `attribute`, `next_sibling`, `previous_sibling`, `attribute (name, hint)`, `set_name`, `set_value`, `append_attribute`, `prepend_attribute`, `insert_attribute_after`, `insert_attribute_before`, `append_child`, `prepend_child`, `insert_child_after`, `insert_child_before`, `remove_attribute`, `remove_child`), 3 члена `XmlAttribute` (`set_name`, `set_value`, `operator=`), 2 члена `XmlText` (`set`, `operator=`) и `utility::stringview_equal`. Теперь они есть в каждом стандарте: `string_view_t` - это `std::basic_string_view<char_t>` с C++17 (как было), а ниже `lumex_string_view`, при `LUMEX_XML_WCHAR_MODE` `lumex_wstring_view` (выбор по `char_t`, один раз в `XmlTypes.hpp`). Теперь работают вызовы `node.child (std::string)`, `node.append_child (std::string)`, `attr = std::string`, которые на C++11 не собирались; вызовы с `char_t const *` по-прежнему берут свои перегрузки (проверено на неоднозначность: `child ("x")`, `append_child (xml_node_type::node_pcdata)`, присваивания литерала, числа, `bool`, `std::string`). Перегрузки по-прежнему встроенные над функциями «указатель, размер», классы по-прежнему не экспортируются (`LUMEX_API` только на членах `.cpp`), экспортируемые функции и ABI те же (`libLumexXml`: 614 символов до и после, `nm -D`). Модуль `xml` требует `string_view`: ребро `lumex_require_module (LUMEX_BUILD_XML LUMEX_BUILD_STRING_VIEW)`, `lumex::string_view` в `target_link_libraries`, `core_string_view` в Conan; `LUMEX_BUILD_XML=ON` с `LUMEX_BUILD_STRING_VIEW=OFF` отклоняется (новый случай `cmake.require_fail_xml_without_string_view`).

Режим `LUMEX_XML_WCHAR_MODE` (по умолчанию выключен, не тестируется): зонтичный заголовок `LumexXml` в нем не собирается и без этой правки (`XmlUtils.hpp`, `set_value_convert`: `wchar_t *` не приводится к `char *`, одна и та же ошибка на GCC 13.2 с C++11 и C++17 до и после). Отдельные `XmlTypes.hpp`, `XmlAttribute.hpp`, `XmlText.hpp` и `XmlNode.hpp` в этом режиме собираются, и `string_view_t` в нем `lumex_wstring_view` на C++11 и `std::wstring_view` на C++17; все оболочки `XmlNode`, `XmlAttribute`, `XmlText` с `std::wstring` собираются на C++11 и C++17 (разбор без компоновки).

**Проверено:** 7 новых тестов в `LumexXmlStringView.cxx11.tests.cpp` (поиск, правка, присваивание и удаление через `std::string`, через размерные представления внутри большего буфера и через пустые представления; вложенный ноль: `std::string` размерная, литерал кончается на нуле; набор перегрузок без неоднозначности; `stringview_equal`) и проверки на этапе компиляции типа `string_view_t`. Тесты модуля `xml.` на GCC 13.2 (Release, CTest): 622 теста проходят, 206, 208 и 208 на C++11, 17 и 20 (было 199, 201 и 201). Наборы C++11 проходят на GCC 8.3 и на Clang 23.1.0 с libc++ и с libstdc++ (177 тестов основного набора). Множество экспортируемых символов `libLumexXml.so` одно и то же до и после (614 символов, `nm -D`). Проверка правкой: 25 правок (каждая из 23 перегрузок: размер минус один; две перегрузки `child` и `append_child`: длина по `strlen`) ловятся тестами на C++11 и C++17. Предупреждений нет (флаги как у `base64`). Под ASan и UBSan: тот же прогон под ASan и UBSan (1638 тестов пяти модулей на C++11, 14 и 17, GCC 13.2, Debug): все проходят, сообщений санитайзеров нет. Режим `wchar_t`: проверено только без компоновки (см. выше). Windows: см. запись про `base64` (шесть лишних символов в `libLumexXml.dll` и `libLumexApplied_settings.dll`).

##### Windows: представления `string_view` экспортируются только из своей библиотеки (`LUMEX_STRING_VIEW_API`)

**Файлы:** `lumex/LumexExport.hpp`, `lumex/core/string_view/view/LumexStringView.hpp`, `lumex/core/string_view/view/LumexWStringView.hpp`, `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/cases/wiring_string_view_export.cmake` (новый)

**Суть:** после перегрузок со строкой (модули `base64`, `crc`, `exceptions`, `xml` включают заголовок `string_view`) в таблице экспорта MinGW-DLL этих модулей (и `settings`, который линкует `xml`) на C++11 (у `crc` и на C++20) появилось по шесть лишних символов: шесть операторов сравнения `lumex_string_view`, объявленных `LUMEX_API inline`. `LUMEX_API` это `dllexport` в каждой DLL, собранной с `LUMEX_EXPORTS`, а `LUMEX_EXPORTS` задан для каждой (у `resource_monitor` их уже было двенадцать). Теперь классы и вставки в поток (`operator<<`) обоих представлений объявлены `LUMEX_STRING_VIEW_API`: макрос привязан к `LumexCore_string_view_EXPORTS`, как `LUMEX_UTILITY_API` к `LumexCore_utility_EXPORTS` (`dllexport` только в библиотеке `string_view`, `dllimport` в остальных DLL и у потребителей, на ELF видимость по умолчанию там же), а шесть операторов сравнения обычные `inline` без экспорта (MinGW отвергает `dllimport` на встроенной функции). Библиотека `string_view` по-прежнему экспортирует классы и вставки; она и `resource_monitor` теряют по двенадцать экспортов встроенных операторов, таблица экспорта каждой другой DLL равна той, что была до перегрузок со строкой. Новых имен нет, ABI на ELF не меняется.

**Проверено:** таблицы экспорта шестнадцати DLL сборок `create_release.sh` с MinGW 8.3 сравнены с таблицами родительского коммита (`objdump -p`, раздел Ordinal/Name Pointer). На C++11 `base64`, `crc`, `exceptions`, `xml` и `settings` теряют по шесть символов (операторы сравнения узкого представления), `string_view` и `resource_monitor` по двенадцать (узкого и широкого), остальные девять DLL совпадают символ в символ; на C++2a теряют `crc` шесть, `string_view` и `resource_monitor` двенадцать, остальные тринадцать совпадают. До перебазирования на вершину ветки то же сравнение с деревом до перегрузок со строкой дало: все DLL, кроме `string_view` и `resource_monitor` (минус двенадцать), совпадают на обоих стандартах. `nm -D` библиотек ELF (GCC 13.2) `base64` (7 символов), `crc` (990), `exceptions` (62), `filesystem` (148), `string_view` (96), `xml` (614) и `resource_monitor` (80) совпадает с таблицами родительского коммита. `cmake.wiring_string_view_export` читает исходники, раскрывает макрос компилятором MinGW (`-E -P`: `dllimport` в DLL, собранной с `LUMEX_EXPORTS`) и компилирует по одной единице `base64`, `crc`, `exceptions` и `xml` как объект DLL, проверяя, что в его директивах экспорта нет ни одного символа `string_view` (а у объекта самой библиотеки `string_view` они есть); четыре порчи (макрос на `LUMEX_EXPORTS`, класс обратно на `LUMEX_API`, оператор сравнения с макросом, вставка без макроса) роняют кейс; без MinGW кейс проверяет только исходники. `create_release.sh` (GCC 8.3, GCC 13.2, Clang 23.1.0, MinGW 8.3; C++11 и C++20; `-Werror`): 0 предупреждений и 0 ошибок во всех восьми сборках, `warns/` пуст. `cmake.*` и `lint.*`: 149 тестов проходят. Не проверялось: сборка на MSVC и запуск на Windows.

##### Один тип «стандартное представление с C++17, иначе представление библиотеки»: `portable_string_view_t` и `portable_wstring_view_t`

**Файлы:** `lumex/core/string_view/view/LumexPortableStringView.hpp` (новый), `lumex/core/string_view/view/LumexPortableWStringView.hpp` (новый), `lumex/core/string_view/LumexStringView`, `lumex/core/base64/codec/Base64.hpp`, `lumex/core/crc/catalog/LumexCrcCatalog.hpp`, `lumex/core/exceptions/exception/LumexException.hpp`, `lumex/xml/types/XmlTypes.hpp`, `lumex/tests/core/string_view/view/LumexStringViewPortable.cxx11.tests.cpp` (новый)

**Суть:** тип, который `std::string_view` с C++17 и `lumex_string_view` ниже, был записан отдельным `#if` в четырех модулях: `string_type_t` в `base64`, `text_view_t` в `crc`, пара конструкторов `lumex_base_exception`, `string_view_t` в `xml`. Теперь он один: `lumex::core::string_view::view::portable_string_view_t` и широкий близнец `portable_wstring_view_t`, в двух маленьких заголовках, которые включают `<string_view>` с C++17 и заголовок представления ниже, ничего больше. `string_type_t`, `text_view_t` и `string_view_t` остались псевдонимами этого типа (ни одно открытое имя не убрано; `string_view_t` в режиме `wchar_t` это `portable_wstring_view_t`), у `lumex_base_exception` один конструктор на все стандарты. Заголовок `LumexStringView` включает оба новых.

**Проверено:** тест `LumexStringViewPortable` на C++11, 17 и 20 (псевдоним это `lumex_string_view` ниже C++17 и `std::string_view` с C++17, широкий близнец так же; сборка из строки C, `std::string` и представления библиотеки, в `std::string` не преобразуется), тесты строковых перегрузок `base64`, `crc`, `exceptions` и `xml` на C++11, 17 и 20 с теми же результатами, что до замены (GCC 13.2, GCC 8.3, Clang 23.1.0 с libc++ и libstdc++ 13, ASan и UBSan; все проходят, кроме нестабильного `ToCrashReport_ThreadSafe` на GCC 13.2 и под санитайзерами). Порча «псевдоним это широкое стандартное представление» роняет тест. `nm -D` библиотек `base64`, `crc`, `exceptions` и `xml` не изменился, таблицы экспорта MinGW-DLL тоже. `create_release.sh` (четыре компилятора, C++11 и C++20, `-Werror`): 0 предупреждений. Не проверялось: режим `LUMEX_XML_WCHAR_MODE` для `xml` целиком (зонтичный `LumexXml` в этом режиме не собирается и на вершине ветки `release/v2.0.0.0`: `set_value_convert` в `XmlUtils.hpp` присваивает `wchar_t *` в `char *`); заголовок `XmlTypes.hpp` в нем собирается, и `string_view_t` равен `portable_wstring_view_t` (C++11, 17 и 20).

#### Исправлено

##### Имена полей компилятора на GCC 13: разбор берет последний идентификатор после последнего `.`, `->` или `::`

**Файлы:** `lumex/core/reflection/field_reflection/LumexAggregateFields.hpp`, `lumex/tests/core/reflection/field_reflection/` (`LumexFieldNamesParser.cxx11.tests.cpp`, `LumexFieldNamesLive.cxx20.tests.cpp` - новые; `LumexFieldNamesAgreement.cxx20.tests.cpp`)

**Суть:** с C++20 автоматические имена (`names_as_array`, `to_json` агрегата без регистрации) берутся из `LUMEX_FUNCTION_NAME` шаблона с указателем на подобъект в аргументе. GCC 13 печатает в нем член с квалификацией классом (`...::value.ns::Type::id`), а разбор брал текст после последней точки и читал идентификатор до `:`, то есть пространство имен: `names[0]` был `"lumex_field_reflection_tests"` вместо `"id"` (79 тестов `reflection.field_reflection` на C++20 и 4 сравнения зарегистрированных имен с именами компилятора; todo 55a). Теперь имя - идентификатор после последней операции доступа (`.`, `->`) или области (`::`) строки, после которой стоит начало идентификатора (не цифра); то, что печатается перед ней, не важно: GCC 13 `.ns::Type::id`, прежний GCC и Clang `.value.id`, MSVC `->value->id`. Разбор работает для вложенных и безымянных пространств имен, шаблонных типов (в том числе с дробными аргументами шаблона), членов вложенных агрегатов, имен с цифрами, подчеркиваниями и байтами UTF-8 и указателя на член базового класса; строка без члена дает пустое имя; буфер размера 0 не трогается. Имя поля хранится в статическом объекте с конструктором (`parsed_field_name`), поэтому первое обращение из нескольких потоков безопасно (раньше буфер заполнялся без защиты). Для Clang в заголовке подавлено `-Wundefined-internal` на фантомный объект агрегата из безымянного пространства имен. GCC 8.3 (и MinGW GCC 8.3) не принимают указатель на подобъект как аргумент шаблона вообще, поэтому у них имен компилятора нет (как и раньше, `__cplusplus` 201709L при `-std=c++2a`), работает регистрация.

**Проверено:** GCC 13.2 Release, `reflection.field_reflection`: C++11 111 тестов, C++14 156, C++17 162, C++20 261, все проходят (до правки 83 падения на C++20). Те же наборы проходят на Clang 23.1.0 с libstdc++ и с libc++ и под ASan с UBSan и в Debug (GCC 13.2); GCC 8.3: ошибок нет, тесты имен компилятора пропускаются (`-std=c++2a` дает `__cplusplus` 201709L). `cmake.*` и `lint.*` (169 тестов) проходят. Мутации (все пойманы): разбор берет текст после последней точки (прежняя ошибка; ловят 102 теста из 261 на C++20), цифра допустима в начале имени (`GivenStringWithoutMember`), байты UTF-8 не входят в имя (`GivenUtf8Identifier`), нет защиты буфера размера 0 (`GivenLongName_WhenParsedIntoSmallBuffer`), побеждает первая операция вместо последней. Новые тесты: `LumexFieldNamesParserTest` (разбор записанных строк `__PRETTY_FUNCTION__` GCC 13.2, Clang 23.1.0 с libstdc++ и libc++, строки MSVC по форме из кода, не снятые с настоящего MSVC, и граничные случаи) идет на каждой платформе на каждом стандарте; `LumexFieldNamesLiveTest` (живые имена в разных областях видимости и хвост строки) с C++20.
##### Четыре константы `LUMEX_MATH_CONSTANTS_*` с неверным хвостом цифр

**Файлы:** `lumex/core/math/constants/LumexMathConstants.hpp`

**Суть:** заголовок обещает 50 знаков, но у четырех констант часть знаков была неверной. Это нашли тесты констант (пункт 86а списка дел), которые сравнивают текст каждого макроса с цифрами, посчитанными вне библиотеки. `LUMEX_MATH_CONSTANTS_SQRT_5`: неверны знаки после 46-го (было `...835960414`, стало `...835961152`). `LUMEX_MATH_CONSTANTS_PLASTIC_NUMBER` и `LUMEX_MATH_CONSTANTS_INVERSE_SQRT_PI`: неверны знаки после 41-го (было `...440794569502`, стало `...440405690173`; было `...405905105952`, стало `...405062932899`). `LUMEX_MATH_CONSTANTS_COPERNICUS_CONSTANT` (pi/180): знаки после 22-го были `2222...`, стали `3690768488612713442871888541` (`0.0174532925199432957692369...`). Макросы остаются литералами типа `double`, поэтому значение типа `double` не изменилось ни у одной из четырех констант (проверено: округление старых и новых цифр до `float`, `double` и 64-битного `long double` одно и то же); меняются только цифры, которые получает `long double` через суффикс `L` (`LUMEX_MATH_CONSTANTS_SQRT_5` с вклеенным `L` и подобное) и все, кто читает текст макроса.

**Проверено:** до исправления новый тест цифр (следующий коммит) падает на этих четырех макросах и больше ни на одном из 25; после исправления проходит. Эталон: `Decimal` Python на 140 знаков по формулам, не использующим заголовок (`LumexMathConstantsReference.py`).

##### `core_dump_generator::instance ()` возвращает заполненный объект; `get_memory_filters_range` берет мьютекс

**Файлы:** `lumex/core/utility/dump/LumexCoreDumpGenerator.hpp`, `lumex/tests/core/utility/dump/LumexCoreDumpInstance.cxx11.tests.cpp`

**Суть:** `instance ()` создавал объект закрытым конструктором по умолчанию, и ничто не записывало `m_dumpDirectory`, `m_currentConfig` и `m_isInitialized` (закрытый конструктор с каталогом и настройкой не использовался). Поэтому `get_instance_dump_directory ()` и `get_optional_dump_directory ()` всегда были пусты, а `generate_instance_dump` всегда возвращал `false` (`invalid_argument` с кодом ошибки). Теперь:
1. `instance ()` при первом вызове создает объект закрытой функцией `_create_instance ()`: под `s_mutex` через закрытый конструктор копирует каталог и настройку, которые записал `initialize ()`, и ставит флаг. Объект создается лениво, а не в `initialize ()`: деструктор экземпляра при выходе из процесса останавливает монитор и пишет прежний `core_pattern`, обращаясь к статическим объектам (`s_monitorThread`, `s_originalCorePattern`), которые к этому моменту могут быть уже уничтожены; создание в `initialize ()` добавило бы этот путь каждой программе, которая инициализирует генератор и не вызывает `instance ()`.
2. `initialize ()` и `set_dump_type ()` после записи статического состояния вызывают закрытую `_refresh_instance ()` (под их `s_mutex`; берет только мьютекс экземпляра, порядок блокировок всегда `s_mutex`, затем мьютекс экземпляра): существующий экземпляр получает новый каталог, настройку и флаг. В `initialize ()` экземпляра еще нет, вызов страхует на случай повторной инициализации; `set_dump_type ()` без этого оставил бы в экземпляре прежний тип дампа.
3. `get_memory_filters_range ()` читает статическую настройку под `s_mutex`, тем же мьютексом, под которым `initialize ()` и `set_dump_type ()` ее заменяют (в описании записано «потокобезопасно»). Мьютекс защищает построение представления, но не обход: представление ссылается на список фильтров и недействительно после замены настройки (`set_dump_type ()` сбрасывает фильтры); это записано в описании. Тип `memory_filters_range_t` не менялся (`std::ranges::ref_view` с C++20).
4. Описания `initialize`, `instance`, `set_dump_type`, методов экземпляра и файла обновлены.

**Проверено:** GCC 13.2 Release, `utility.dump.`: 95 тестов на C++11 (было 74), 105 на C++17 (84), 112 на C++20 (91); 21 новый тест в `LumexCoreDumpInstance` (заполнение, обновление, `set_dump_type`, `instance ()`, ветки `generate_instance_dump`, ожидание `get_memory_filters_range` на занятом мьютексе); GCC 8.3 и Clang 23 (C++11) - 95, ASan/UBSan Debug - 95. Два прежних теста «неинициализированный экземпляр» теперь берут свежий экземпляр с сброшенным флагом (`instance ()` такого объекта больше не возвращает). 6 мутаций, все пойманы: пустой конструктор, забытый флаг, нет обновления в `set_dump_type`, нет мьютекса в `get_memory_filters_range`, повторный захват `s_mutex` в обновлении (взаимная блокировка, тест остановлен по таймауту 150 с), `instance ()` без создания (ошибка сегментации). Не покрыто тестами: вызов `_refresh_instance ()` из `initialize ()` (экземпляра там еще нет, а сам `initialize ()` в тестах не запускается - ставит обработчики и пишет `core_pattern` через sudo).

##### `LumexFormat*`: `-Wpedantic` молчит про `__int128` на GCC

**Файлы:** `lumex/core/fmt/LumexFormat.hpp`, `lumex/tests/cmake/consumer/hygiene_compile_checks/` (`CMakeLists.txt`, `fmt_pedantic.cpp` - новый)

**Суть:** `LumexFormat.hpp` (а значит и `LumexFormatChrono.hpp`, `LumexFormatRanges.hpp` и зонтик `LumexFormat`) писал `__int128` и `unsigned __int128` в 11 местах, и GCC 8 и 13 при `-Wpedantic` выдавали `ISO C++ does not support '__int128'` на каждом (на C++11 и C++20; Clang молчит). Теперь тип назван один раз: `__extension__ typedef __int128 int128_type;` и `__extension__ typedef unsigned __int128 uint128_type;` в `Detail` (под `LUMEX_FORMAT_HAS_INT128`), остальной заголовок использует псевдонимы. `__extension__` понимают GCC, Clang, clang-cl и MinGW; у MSVC `__SIZEOF_INT128__` нет. Проверка в `cmake.hygiene_compile_checks`: каждый из четырех заголовков (включая зонтик) собирается под `-Wall -Wextra -Wpedantic -Werror` на C++11, 14, 17 и 20 с форматированием 128-битных чисел; на GCC голый `__int128` в той же единице должен быть отвергнут (иначе проверка ничего бы не доказывала; Clang его молча принимает).

Прогон строгой проверки заголовков Clang 23 с libstdc++ 13 (`check_headers_standalone.py --cxx-flags="-Wall -Wextra -Wpedantic -Werror"`, C++11, 182 заголовка): замечаний нет; то же с `-Wconversion -Wsign-conversion -Wshadow -Wold-style-cast`, на GCC 8 и 13 (C++11) и на GCC 13 и Clang 23 (C++20) с `-Wall -Wextra -Wpedantic -Werror`: по 0 из 182. Проверка прогона: заголовок с неиспользуемой переменной скрипт отвергает.

**Проверено:** заголовки `fmt_pedantic.cpp` (4 заголовка) чисты под `-Wall -Wextra -Wpedantic -Werror` на GCC 8.3, GCC 13.2 и Clang 23 (C++11, 17, 20), MinGW GCC 8.3 (C++11, `-fsyntax-only`); до правки GCC 8.3 и 13.2 выдавали 11 предупреждений. С прежним `LumexFormat.hpp` `cmake.hygiene_compile_checks` падает на «the fmt CORE header under -Wpedantic (C++11) must compile» (`ISO C++ does not support '__int128'`). `fmt` на GCC 13.2: 194 теста на C++11 и 14, 195 на C++17, 200 на C++20 (известные падения без изменений: 1 на C++17 и 9 на C++20, сравнение с `std::format` и `GivenHexTypeAtLimits`; 1 пропуск локали); GCC 8.3 и Clang 23 на C++11 - 194. `cmake.*` и `lint.*` (включая `lint.headers_standalone`): 162 из 162 проходят.

##### `mem::as<T>` и `traits::meta::is_extractible` не принимают `const` и `volatile` `T`

**Файлы:** `lumex/core/utility/traits/LumexTypeTraits.hpp`, `lumex/core/utility/mem/LumexMemRead.hpp` (описание), `lumex/tests/core/utility/mem/LumexMemReadConstraints.cxx11.tests.cpp`, `lumex/tests/core/utility/traits/LumexTypeTraitsTopics.cxx11.tests.cpp`, `lumex/tests/core/utility/traits/LumexTypeTraitsTopics.cxx20.tests.cpp`

**Суть:** `is_extractible<int const>` было истинным (константный тип тривиально копируем), поэтому ограничение `mem::as<int const>` пропускало вызов, а тело (`std::memcpy` в `T res{}`, то есть в константный объект) не компилировалось: ошибка из глубины функции вместо отсутствия перегрузки. Теперь признак и концепт `Extractible` требуют еще `!std::is_const` и `!std::is_volatile`; `as<int const>` и `as<int volatile>` не находят перегрузку через указатель с размером, объект-источник и `span` (проверка детектором). Концепт и признак по-прежнему совпадают (тест согласия C++20 получил пять типов с cv). Версия без квалификаторов не изменилась.

**Проверено:** GCC 13.2 Release: `utility.mem.` 40 тестов на C++11 (было 38), 42 на C++17, 48 на C++20; `utility.traits.` 103 на C++11 (было 102), 123 на C++20 (согласие признака и концепта: 5 типов с cv в существующем тесте); GCC 8.3 и Clang 23 на C++11 - те же 40 и 103; ASan/UBSan Debug - 40 и 103. 2 мутации (убрана проверка `const`, убрана проверка `volatile`): пойманы тестами признака и `mem`. `as<int const>` теперь дает «no matching function» вместо ошибки в теле. Находка, не менялась: `is_extractible` по-прежнему истинно для `int[4]` (`as<int[4]>` падает внутри `std::optional`) и для типа без конструктора по умолчанию (`T res{}`).

##### Тесты `optional` собираются и проходят на C++14, 17, 20 и 23

**Файлы:** `lumex/tests/core/optional/opt/LumexOptional.cxx11.tests.cpp`, `lumex/tests/LumexTestStandards.cmake` (строка `optional`)

**Суть:** набор `LumexOptional.cxx11.tests.cpp` не собирался на C++17 и выше: вызов `make_optional (s)` с `std::string` через глобальное имя из зонтичного заголовка находил еще и `std::make_optional` по ADL, и вызов был неоднозначным. Вызовы записаны с полным именем `lumex::core::optional::opt::make_optional`; добавлены тесты двух остальных перегрузок (`make_optional<T> (args...)` и со списком инициализации) и проверка типа результата. Строка `optional` таблицы стандартов тестов была `11`, теперь `11 14 17 20 23`: набор проходит на каждом, и исправление ADL больше не может вернуться незамеченным.

**Проверено:** GCC 13.2 Release, `optional.opt.`: по 51 тесту на C++11, 14, 17, 20 и 23 (было 42 на C++11 без других стандартов), все проходят; GCC 8.3 - то же на C++11, 14, 17, 20; Clang 23 (libc++) - на C++11, 17, 23. До правки на GCC 13.2 при `-std=c++17` и выше: «call of overloaded 'make_optional(std::string&)' is ambiguous». `cmake.wiring_standard_suites` проходит.

##### `optional`: значение лежит в буфере `alignas`, а не в `std::aligned_storage` (C++23)

**Файлы:** `lumex/core/optional/opt/LumexOptional.hpp`, `lumex/tests/core/optional/opt/LumexOptionalStorage.cxx11.tests.cpp` (новый), `lumex/tests/cmake/consumer/hygiene_compile_checks/` (`CMakeLists.txt`, `optional_aligned_storage.cpp` и `aligned_storage_baseline.cpp` - новые)

**Суть:** `std::aligned_storage` объявлен устаревшим в C++23 ([depr.meta.types]), и заголовок `optional` выдавал `-Wdeprecated-declarations` на GCC 13 и Clang 23 при `-std=c++23` на каждом использовании. Теперь член хранения - `alignas (T) unsigned char m_storage[sizeof (T)]` (`alignas` есть в C++11); указатель на значение получается через `static_cast<T *> (static_cast<void *> (m_storage))`, поэтому `reinterpret_cast` и его `NOLINT` не нужны. Размер и выравнивание `optional<T>` те же, что давал прежний член: буфер идет первым, флаг после него (проверено на 18 типах до и после правки, вывод совпал). Проверка в `cmake.hygiene_compile_checks`: заголовок собирается под `-Wall -Wextra -Wpedantic -Wdeprecated-declarations -Werror` на C++11, 14, 17, 20 и (если компилятор знает C++23) 23; `std::aligned_storage` в отдельной единице на C++23 должен быть отвергнут как устаревший, иначе проверка на C++23 пропускается с сообщением (библиотека без устаревания ничего не доказывает).

**Проверено:** GCC 13.2 Release, `optional.opt.` на C++11: 7 новых тестов `LumexOptionalStorageTest` (расположение для 18 типов, включая типы с выравниванием 16 и 32, нечетные размеры 3, 5, 7, выравнивание адреса значения, копирование и перемещение, константный доступ; до C++23 еще сравнение размера и выравнивания с прежним расположением `aligned_storage` + флаг через `static_assert`), все проходят; 51 тест вместе со следующим пунктом. Программа с 18 типами печатает те же `sizeof` и `alignof` для `optional<T>` с прежним и с новым заголовком (GCC 13, C++17). `-std=c++23 -Wall -Wextra -Wpedantic -Werror` без предупреждений на GCC 13.2 и Clang 23 (libstdc++ 13 и libc++); с прежним заголовком те же компиляторы давали `-Wdeprecated-declarations` (GCC одно, Clang два). На GCC 8.3 и Clang 23 (C++11) 51 тест, ASan/UBSan Debug - 51. `cmake.hygiene_compile_checks` проходит, с прежним заголовком падает на «the optional header under -Wdeprecated-declarations (C++23) must compile»; `aligned_storage_baseline.cpp` отвергается как устаревший на GCC 13.2 и Clang 23 (C++23). 2 мутации (без `alignas`, буфер на 8 байт длиннее), обе пойманы `static_assert` при сборке тестов.

##### `bit::count_leading_zeros` для `unsigned char` и `unsigned short`: без `-Wconversion` и `-Wsign-conversion`

**Файлы:** `lumex/core/utility/bit/LumexBit.hpp`, `lumex/tests/core/utility/bit/LumexBit.cxx11.tests.cpp`, `lumex/tests/cmake/consumer/hygiene_compile_checks/` (`CMakeLists.txt`, `bit_warnings.cpp` - новый)

**Суть:** для типов в 1 и 2 байта функция вычитала `32 - sizeof (T) * 8` (`std::size_t`) из результата `__builtin_clz` (`int`) и возвращала сумму как `std::uint8_t`: GCC 8 и 13 давали `-Wsign-conversion` и `-Wconversion`, Clang 23 - `-Wsign-conversion` и `-Wimplicit-int-conversion` (ветка `__builtin_clzll` для узких типов тоже). В обычной сборке и в `create_release -Werror` предупреждений нет, потому что строгие флаги там не включены, но потребитель с `-Wconversion -Werror` получал ошибку при первом вызове. Теперь арифметика ведется в `int` (`32 - static_cast<int> (sizeof (T) * 8)`) и результат сужается одним явным `static_cast<std::uint8_t>`; так же записаны возврат для нулевого значения, ветка `__builtin_clzll`, три возврата ветки MSVC (`_BitScanReverse`, `_BitScanReverse64`) и запасная ветка без встроенных функций. В ветке 32-битного MSVC значение теперь сдвигается как `std::uint64_t` (`static_cast<std::uint64_t> (value) >> 32`; для узкого типа прежний `value >> 32` был сдвигом `int` на 32). Результаты функции не меняются. Проверка в `cmake.hygiene_compile_checks`: единица `bit_warnings.cpp` создает функцию для `unsigned char`, `std::uint8_t`, `unsigned short`, `std::uint32_t`, `unsigned long` и `unsigned long long`, `byte_swap` для знаковых и беззнаковых 2, 4 и 8 байт и `is_little_endian` под `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast -Werror` на C++11, 14, 17 и 20.

**Проверено:** GCC 13.2 Release, `utility.bit.`: 21 тест на C++11, 50 на C++20, 51 на C++23 (было 17, 46, 47; 4 новых теста сравнивают результат со счетом по битам без встроенных функций: все 256 значений в 1 байт, все 65536 в 2 байта, степени двойки с соседями для 32 и 64 бит и 2048 случайных значений), все проходят; GCC 8.3 и Clang 23 на C++11 - 21, ASan/UBSan Debug - 21. `bit_warnings.cpp` чисто на GCC 8.3, GCC 13.2 и Clang 23 на C++11, 14, 17 и 20; с прежним заголовком `cmake.hygiene_compile_checks` падает на «the bit header under the strictest flags (C++11) must compile» (`-Wsign-conversion`, `-Wconversion`). Запасная ветка (без встроенных функций) проверена на копии заголовка с `#elif 0` вместо условия GCC/Clang: 256 + 65536 значений и 200000 случайных 32- и 64-битных совпали со счетом по битам, чисто под теми же флагами на GCC 8.3 и Clang 23; ветки MSVC не собирались (нет компилятора). 3 мутации (смещение 31 вместо 32, `+ 1` у `__builtin_clzll`, ширина минус 1 для нуля), все пойманы тестами.
##### `resolve_serial_port_path` возвращает абсолютный путь без изменений (PEW-2313, PEW-2308)

**Файлы:** `lumex/applied/serial/port/LumexSerialPort.cpp`, `lumex/tests/applied/serial/port/LumexSerialPort.cxx11.tests.cpp`

**Суть:** на POSIX значение, которое уже является абсолютным путем (`/dev/ttyACM0`, `/dev/serial/by-id/...`), возвращается как есть. До правки повторный вызов дописывал префикс, и получался несуществующий `/dev/serial/by-id//dev/ttyACM0`: у потребителя (PeakExpertWeb) падали все пробы и подключения, которые резолвили результат перечисления (PEW-2313 BLOCKER, PEW-2308 MAJOR). Ветка Windows такие значения и так возвращала без изменений; короткие имена, by-id алиасы и сетевые каналы не изменились.

**Проверено:** GCC 13.2 Release, GCC 8.3, Clang 23.1.0: `serial.port.LumexSerialPort.AbsoluteDevPathIsReturnedUnchanged` проходит на всех трех (27 из 27 в наборе). Мутация (guard убран): тест падает. Существующие тесты коротких имен, by-id и пустого имени не менялись и проходят.

##### Примеры xml пишут файлы со своими именами и удаляют их

**Файлы:** `lumex/examples/xml/example_1.cpp`, `lumex/examples/xml/example_2.cpp`, `lumex/examples/xml/example_4.cpp`, `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/cases/source_xml_examples_own_files.cmake` (новый)

**Суть:** `examples.xml.LumexXmlExample1` и `LumexXmlExample2` записывали и читали один файл `./xgconsole.xml` в одном рабочем каталоге: при параллельном `ctest` один перезаписывал файл, который читал другой, и первый пример время от времени падал (один раз в прогоне, поодиночке и в 30 повторах проходил). Теперь у каждого свое имя (`lumex_xml_example_1.xml`, `lumex_xml_example_2.xml`; у четвертого уже было `lumex_xml_example_4.xml`), и каждый удаляет файл сразу после загрузки (`std::remove`), как примеры `json` и `settings`. Пример 3 только читает файл, который лежит рядом с исходниками.

**Проверено:** `ctest -j4 -R '^examples\.xml' --repeat until-fail:20` (GCC 13.2 Release и ASan/UBSan Debug): 4 из 4 в каждом из 20 повторов, в каталоге `bin/` остается только `file_for_example_3.xml`. Опыт с возвратом (оба примера снова на `xgconsole.xml`, удаление после загрузки оставлено): `ctest -j4 -R '^examples\.xml' --repeat until-fail:300` остановился на падении `LumexXmlExample1` (гонка воспроизведена), `cmake.source_xml_examples_own_files` падает на повторе имени; без `std::remove` в примерах 2 и 4 он падает на оставленном файле.

##### `-Wfloat-equal` внутри `LumexSafeNumericComparator.hpp`: сравнение на равенство идет через `exactly_equal`

**Файлы:** `lumex/core/utility/numeric/LumexSafeNumericComparator.hpp`, `lumex/tests/core/utility/numeric/LumexSafeEqualExactly.cxx11.tests.cpp` (новый), `lumex/tests/cmake/consumer/hygiene_compile_checks/CMakeLists.txt`, `lumex/tests/cmake/consumer/hygiene_compile_checks/float_equal_comparator.cpp` (новый)

**Суть:** `safe_equal` и `safe_not_equal` для `float` и `double` (одинаковые типы), сравнения двух разных типов с плавающей точкой, целого с плавающим, трехстороннее сравнение двух разных типов с плавающей точкой при бесконечности и `compare_and_set` обычного (не атомарного) `safe_comparator` с плавающим типом записывали `==` и `!=` прямо в заголовке. GCC предупреждает об этом (`-Wfloat-equal`) в единице трансляции потребителя, если заголовок не системный, а пометить место для GCC нечем (у него нет прагмы, которая гасит предупреждение из шаблона вне системного каталога; был только `#pragma clang diagnostic ignored "-Wfloat-equal"`), поэтому сборка потребителя с `-Wfloat-equal -Werror` ломалась при первом же `safe_equal (double, double)`. Теперь все 14 мест вызывают `::lumex::core::math::ops::exactly_equal` (модуль `core/math`, `utility` его и так подключает через `LumexRanges.hpp`), прагма Clang из заголовка удалена, ответы не изменились. Для целых типов `compare_and_set` по-прежнему пишет `==` (две перегрузки закрытой функции `same_value`).

**Проверено:** до правки `float_equal_comparator.cpp` (все шесть сравнений, `safe_compare`, трехстороннее сравнение и `compare_and_set` для `float`, `double`, `long double`, их пар друг с другом и с `int` и `unsigned long long`; `-Wall -Wextra -Wfloat-equal -Werror`) не компилируется на GCC 13.2: 14 разных строк заголовка предупреждают, на C++11 и на C++20; после правки компилируется без замечаний на GCC 8.3 (C++11, 14, 17, `-std=c++2a`), GCC 13.2, Clang 23.1.0 с libstdc++ и с libc++ (C++11, 14, 17, 20) и MinGW 8.3 (C++11, 14, 17, `-std=c++2a`), и это при удаленной прагме Clang. Единица с собственным `lhs == rhs` для `double` (`-DLUMEX_HYGIENE_RAW_FLOAT_EQUAL`) отвергается на тех же компиляторах и стандартах с `comparing floating-point with '==' or '!='` (иначе проверка ничего не доказывала бы). `cmake.hygiene_compile_checks` проходит: на каждом стандарте C++11-20 одна единица компилируется и одна отвергается. Новый набор `LumexSafeEqualExactly` (5 тестов в каждом наборе `utility.numeric.`): `safe_equal` и `safe_not_equal` (функции и члены) совпадают с `exactly_equal` на всех 81 парах значений 0, -0, 1, -1, 1.5, +inf, -inf и двух NaN для `float`, `double`, `long double`, на всех парах разных типов с плавающей точкой (81 пара на пару типов) и на целых 0, 1, -1 против этих значений в обе стороны; `compare_and_set` не меняет значение при NaN и при другой бесконечности и меняет при -0 против 0. Мутации: возврат любого из 14 мест к прямому `==` ловится проверкой `float_equal_comparator.cpp` на GCC 13.2 (14 из 14, проверка вызывает ошибку компиляции с `-Wfloat-equal`); отрицание вызова `exactly_equal` ловится новым набором в 11 местах из 14, остальные три (бесконечность в `>=`, `<=` и в трехстороннем сравнении двух разных типов с плавающей точкой) исчезают в записи о порядке бесконечностей ниже.

##### `lumex_filesystem::to_wide_string`, `from_wide_string` и `path::wstring` не зависят от локали C: путь в UTF-8 больше не превращается в пустую строку

**Файлы:** `lumex/core/filesystem/fs/LumexFilesystem.cpp`, `lumex/core/filesystem/fs/LumexFilesystem.hpp` (комментарии), `lumex/core/filesystem/CMakeLists.txt`, `cmake/LumexModules.cmake`, `conanfile.py`, `lumex/tests/core/filesystem/fs/LumexFilesystemUnicode.cxx11.tests.cpp` (новый), `lumex/tests/core/unicode/convert/LumexUnicodeConvertWideWidths.cxx11.tests.cpp` (новый), `lumex/tests/core/unicode/convert/CMakeLists.txt`, `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/cases/require_fail_filesystem_without_unicode.cmake` (новый), `lumex/tests/cmake/cases/require_graph_all_edges.cmake`

**Суть:** на POSIX обе функции звали `mbstowcs` и `wcstombs`, а те читают локаль C процесса: в локали "C" (так стартует любая программа, не вызвавшая `setlocale`) байт выше 0x7F это ошибка, поэтому путь в UTF-8 (кириллица, любой не ASCII) давал пустую строку, а с ним пустым был и `path::wstring ()`. На Windows функции звали `MultiByteToWideChar` и `WideCharToMultiByte`. Теперь на всех платформах это `lumex::core::unicode::convert::to_wide` и `to_utf8` (модуль `unicode`, добавленный переносом утилит из `xml`): результат не зависит ни от локали, ни от операционной системы; `wchar_t` читается как UTF-16 там, где он занимает 2 байта (Windows), и как UTF-32 там, где 4 (Linux).

**Отличие в обработке недопустимого входа** (решение пользователя от 2026-10-08): недопустимый вход пропускается, ошибкой не считается. Байт, который не начинает полную последовательность UTF-8, и непарный суррогат UTF-16 не оставляют следа в результате. На Windows `MultiByteToWideChar` и `WideCharToMultiByte` писали на их месте U+FFFD; прежние вызовы POSIX возвращали пустую строку на всем входе. Модуль `unicode` не проверяет UTF-32: на Linux значение U+D800 в `wchar_t` записывается в UTF-8 тремя байтами, а не пропускается. Описано в комментариях `to_wide_string`, `from_wide_string` и `path::wstring`. Открытый интерфейс не менялся: у `filesystem` появилась зависимость на уровне конфигурации (`lumex_check_module_dependencies`: `FILESYSTEM` требует `UNICODE`), `lumex::unicode` подключен к `LumexCore_filesystem` как PRIVATE (заголовочный модуль нужен только исходнику, он же дает путь включения), в Conan `core_filesystem` требует `core_unicode`; в `lumex/core/CMakeLists.txt` `unicode` и так добавляется раньше `filesystem`.

**Проверено:** 12 новых тестов `LumexFilesystemUnicode` на C++11, 17 и 20 (36 тестов; кириллица, символ из 4 байт UTF-8 (одна единица или пара), смешанный путь туда и обратно, пустая строка, недопустимые байты и обрезанная последовательность пропускаются, непарный суррогат в зависимости от ширины `wchar_t`, встроенный нуль, `path::wstring`, локаль "C" через `setlocale` с возвратом прежней, перебор доступных локалей: "C", "POSIX", "C.UTF-8", "ru_RU.utf8", "russian" (ISO-8859-5) и других, не установленные пропускаются). 8 новых тестов `LumexUnicodeConvertWideWidths` на C++11: оба конвейера `to_utf8` и `to_wide` (2-байтовые единицы Windows и 4-байтовые Linux) на любой платформе, непарные суррогаты, несовпадающая пара, недопустимый UTF-8, 50 случайных кругов; на Windows код этого пути не запускался. Проверка правкой: прежняя реализация POSIX (`mbstowcs` и `wcstombs`) возвращена в `LumexFilesystem.cpp` - падают 10 из 12 тестов (все, кроме ASCII и пустой строки), вместе с тестом локали "C"; еще 4 порчи (байты расширяются до `wchar_t`, `wchar_t` сужаются до `char`, пустой результат на не ASCII в каждую сторону) роняют от 6 до 8 тестов; 4 порчи `wchar_selector<2>` и `utf16_decoder` (`utf16_decoder<true>` вместо `<false>`, 32-битный счетчик, ведущий суррогат сохраняется при неверной второй единице, одиночный суррогат сохраняется) роняют от 1 до 3 тестов `WideWidths`. GCC 13.2 Release: тесты `filesystem` 255 -> 291 (C++11 - 89, C++17 - 101, C++20 - 101). Конфигурационные проверки: `cmake.require_fail_filesystem_without_unicode` (новый) и `cmake.require_graph_all_edges`; снятие ребра, ссылки `lumex::unicode` в `filesystem/CMakeLists.txt`, строки в `conanfile.py` или порядка подкаталогов ловят `cmake.require_graph_all_edges`, `cmake.require_fail_filesystem_without_unicode` и `cmake.wiring_unicode_users` (описан в записи ниже). Матрица `create_release.sh --std 11,20` на GCC 8.3, GCC 13.2, Clang 23.1.0 и MinGW 8.3 по коммиту с этой правкой и следующей: 8 сборок, 0 предупреждений, 0 ошибок, `warns/` пуст (ветка Windows файла `LumexFilesystem.cpp` теперь одинакова с POSIX и компилируется MinGW, но не запускалась).

##### `-Wnon-template-friend` в `LumexAggregateFields.hpp` на GCC (C++14)

**Файлы:** `lumex/core/reflection/field_reflection/LumexAggregateFields.hpp`, `lumex/tests/cmake/consumer/hygiene_compile_checks/CMakeLists.txt`, `lumex/tests/cmake/consumer/hygiene_compile_checks/field_reflection_warnings.cpp` (новый)

**Суть:** на C++14 GCC считает поля агрегата через лазейку CWG 2118: `loophole_tag<T, N>` объявляет дружественную функцию `loophole_fn`, не являющуюся шаблоном, а ее определяет другой класс, `loophole_set`. GCC предупреждает об этом объявлении по умолчанию (`-Wnon-template-friend`) в каждой единице трансляции, которая включает заголовок. Другого написания у лазейки нет (функция объявляется на каждую конкретизацию и определяется другим классом), поэтому предупреждение подавлено в коде вокруг одного объявления: `#pragma GCC diagnostic ignored` внутри `#if defined(__GNUC__) && !defined(__clang__)`. Определение лазейки и подсчет полей не изменились.

**Проверено:** до правки `field_reflection_warnings.cpp` (`tuple_size` и `get<1>` над агрегатом из двух полей, `-Wall -Wextra -Wpedantic -Werror`) не компилируется на GCC 8.3 и GCC 13.2 на C++14 (`friend declaration ... declares a non-template function`, строка 205) и компилируется на C++11, 17, 20 и на Clang 23.1.0 (libstdc++ и libc++); после правки компилируется на всех четырех на C++11, 14, 17 и 20 (прямая матрица).

##### `-Wpedantic`: пустой аргумент `...` в `LUMEX_DEFINE_REFLECTED_ENUM` и `LUMEX_MEASURE_TIME`

**Файлы:** `lumex/core/reflection/reflected_enum/LumexReflectedEnum.hpp`, `lumex/core/time/timer/LumexTimer.hpp`, `lumex/tests/cmake/consumer/hygiene_compile_checks/CMakeLists.txt`, `lumex/tests/cmake/consumer/hygiene_compile_checks/variadic_macros_pedantic.cpp` (новый), `lumex/tests/cmake/consumer/hygiene_compile_checks/json_umbrella.cpp`

**Суть:** макросы, выбирающие перегрузку по числу аргументов (`LUMEX_PP_ARG_N`, `LUMEX_PP_ENUM_BODY_PICK`, `LUMEX_PP_ENUM_VALUE_PICK`, `LUMEX_MEASURE_TIME_GET_OVERLOAD`), принимают в конце `...`, а вызывались так, что для записи без значения `(Name)`, для перечисления из одного элемента и для формы `LUMEX_MEASURE_TIME (expr)` он оставался пустым. До C++20 это расширение: GCC с `-Wpedantic` и Clang с `-Wvariadic-macro-arguments-omitted` сообщали по строке на каждое такое место (в единице трансляции с зонтичным `LumexJson` - от `LumexJsonDiagnostics.hpp`, `LumexJsonSchemaException.hpp` и `LumexJsonSchemaTraverser.hpp`). Вызовы получили лишний аргумент `0` в конце, поэтому `...` никогда не пуст; синтаксис макросов и результат раскрытия не изменились. `LUMEX_DEFINE_EXCEPTION` передает `LUMEX_DEFINE_EXCEPTION_WITH_BODY` пустой аргумент через запятую - это допустимо с C++11 и не менялось.

**Проверено:** до правки `variadic_macros_pedantic.cpp` и зонтичный `LumexJson` с `-Wall -Wextra -Wpedantic -Werror` не компилируются на GCC 8.3, GCC 13.2 и Clang 23.1.0 (libstdc++ и libc++) на C++11, 14 и 17 и компилируются на C++20; после правки компилируются на всех четырех на C++11, 14, 17 и 20 (прямая матрица и `cmake.hygiene_compile_checks`).

##### `-Wunused-result` на GCC: `LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR` не гасит результат `LUMEX_ATTRIBUTE_NODISCARD`

**Файлы:** `lumex/core/utility/attr/LumexAttributes.hpp`, `lumex/tests/core/utility/attr/LumexAttributesMaybeUnusedVar.cxx11.tests.cpp` (новый), `lumex/tests/cmake/consumer/hygiene_compile_checks/CMakeLists.txt`, `lumex/tests/cmake/consumer/hygiene_compile_checks/unused_result.cpp` (новый), `lumex/tests/cmake/consumer/hygiene_compile_checks/json_umbrella.cpp` (новый)

**Суть:** на C++11 и C++14 `LUMEX_ATTRIBUTE_NODISCARD` - это `__attribute__ ((warn_unused_result))`, а `LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (expr)` был `(void)(expr)`. Clang принимает приведение к `void` как осознанное отбрасывание, GCC 8.3 и 13.2 - нет, и `LumexJsonSchemaValidator.hpp` (`_check`) и `LumexJsonSchemaNormalizer.hpp` (`validate_against_schema`) давали `-Wunused-result` в каждой единице трансляции, которая включает зонтичный `LumexJson`. Теперь на GCC (не Clang) ниже C++17 макрос раскрывается в `(void)((expr), sink)`, где `sink` - объект `lumex::core::utility::attr::detail::unused_value_sink_t`, а перегруженная запятая `operator,` принимает значение слева как аргумент функции, то есть использует его, и ничего с ним не делает. Тип не класс (скаляр, массив, функция) передается по значению, чтобы массив неизвестного границы (`int expanded[] = { ... }` в шаблоне, как в `stringify`) и функция распадались в указатель, а тип класса или объединения - по константной ссылке, без копирования и перемещения. Выражение вычисляется один раз, остается выражением типа `void` и принимает `void`-выражения. На C++17 и позже, на Clang и на MSVC раскрытие прежнее.

**Проверено:** до правки GCC 8.3 и 13.2 не компилируют результат функции с `LUMEX_ATTRIBUTE_NODISCARD`, переданный макросу, и зонтичный `LumexJson` с `-Wall -Wextra -Werror` на C++11 и C++14 (на C++17, C++20 и на Clang 23.1.0 компилируют); после правки компилируют на GCC 8.3, GCC 13.2 и Clang 23.1.0 (libstdc++ и libc++) на C++11, 14, 17 и 20; `cmake.hygiene_compile_checks` проходит на GCC 13.2, GCC 8.3 и Clang 23.1.0. Новые 9 тестов `LumexAttributesMaybeUnusedVarTest` (один вызов, `void`, запятые, массивы, перечисления, объединения, класс без копирования, `volatile`, указатели на члены, `nullptr`, массив неизвестной границы) проходят: набор `utility.attr.` - 100 тестов на GCC 13.2 (C++11-23), 80 на GCC 8.3 (C++11-20), 120 на Clang 23.1.0 (C++11-26). Мутации: макрос без действия и макрос, вычисляющий выражение дважды, валят тесты (`GivenACallWithSideEffects_WhenDiscarded_ThenItRunsOnce` и другие, бесконечный цикл в тесте оператора `for`); возврат к `(void)(expr)` оставляет тесты зелеными, но валит `cmake.hygiene_compile_checks`. Без перегрузки по значению (константная ссылка для всех типов) `unused_result.cpp` не компилируется на GCC 8.3 на C++14 (`invalid initialization of reference of type 'const int (&)[]'`). Все 8 сборок `create_release.sh --std 11,20` (GCC 8.3, GCC 13.2, Clang 23.1.0, MinGW 8.3, `-Werror`) идут без предупреждений. Наборы тестов по всем модулям на C++11 и затронутых модулей на старших стандартах после правок 1-6: GCC 13.2 - 6590 тестов проходят, 80 падают (79 имен полей `reflection` на GCC 13 из пункта 55 todo и `LumexSerialProberPty`, как до правок), GCC 8.3 - 5812 проходят, падает только `LumexSerialProberPty`, Clang 23.1.0 с libc++ - 5933 проходят, падает только `LumexSerialProberPty`; `cmake.*` и `lint.*` - 148 тестов проходят.

##### `LumexLogger.hpp` объявляет `std::make_unique` и `std::exchange` ниже C++14

**Файлы:** `lumex/applied/logger/logger/LumexLogger.hpp`, `lumex/tests/applied/logger/logger/LumexLoggerStdPolyfill.cxx11.tests.cpp` (новый)

**Суть:** ниже C++14 заголовок логгера вставлял в `namespace std` шаблоны `make_unique` (для объекта и для массива) и `exchange`. Добавлять объявления в `std` - неопределенное поведение, а программа, которая сама объявляет их для C++11 (так делают старые проекты), получала `redefinition` при включении заголовка. Эти функции не вызывал ни логгер, ни другой код `lumex/` (единственный `std::exchange` - в ветке `join` для C++20), поэтому вспомогательные функции взамен не нужны: блок удален вместе с `<memory>`, который подключался только ради него; комментарий файла больше не обещает их.

**Проверено:** новый тест `LumexLoggerStdPolyfill` объявляет в `std` свои `make_unique` и `exchange` после заголовка логгера и вызывает их; на прежнем заголовке `LumexLoggerLoggerCxx11Tests` не собирается на GCC 13.2 (`redefinition of ... std::make_unique`, `redefinition of ... std::exchange`), на новом проходят все 128 тестов `logger.` на GCC 13.2, GCC 8.3 и Clang 23.1.0 (libc++) на C++11, 14, 17 и 20.

##### Заголовки `xml` не компилируются поодиночке; проверка `lint.headers_standalone`

**Файлы:** `lumex/xml/writer/IXmlWriter.hpp`, `lumex/xml/writer/XmlWriterFile.hpp`, `lumex/xml/xpath/constants/XPathConstants.hpp`, `lumex/xml/tree/XmlTreeWalker.hpp`, `Scripts/CodeTools/check_headers_standalone.py` (новый), `lumex/tests/CMakeLists.txt`, `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/cases/lint_headers_standalone_script.cmake` (новый), `lumex/tests/cmake/cases/wiring_headers_standalone_ctest.cmake` (новый), `lumex/tests/cmake/fixtures/headers_standalone/` (новый)

**Суть:** четыре публичных заголовка не были самодостаточными ни в одном стандарте и собирались только после того, как соседний заголовок что-то подключил. `IXmlWriter.hpp` и `XmlWriterFile.hpp` пишут `std::size_t` без `<cstddef>`; `XPathConstants.hpp` пишет `std::size_t` без `<cstddef>` (libstdc++ приносит его через `<cstdint>`, libc++ нет) и `uintptr_t` без `std::`; в `XmlTreeWalker.hpp` имя `XmlNode` находилось только через `using namespace node`, которое писал другой заголовок, а `friend class XmlNode` объявлял не тот класс (`tree::XmlNode`). Теперь заголовки подключают то, чем пользуются, а `XmlTreeWalker.hpp` объявляет `node::XmlNode` вперед и пишет `node::XmlNode` в `begin`, `for_each`, `end` и в `friend`. Новый `check_headers_standalone.py` (Python 3.7, только стандартная библиотека; `--dir`, `--check`, `--compiler`, `--std`, `--cxx-flags`, `--jobs`) компилирует каждый публичный заголовок и каждый зонтичный файл `lumex/` как единицу из одной строки `#include` с `-fsyntax-only` (`/Zs` для cl и clang-cl); заголовок, который работает только с макросом (`LumexLoggerConfigFormat.hpp`: `#error` без макроса формата), компилируется с этим макросом по таблице скрипта. CTest `lint.headers_standalone` (метка `lint`, C++11, компилятор, флаги и `-stdlib=` текущей сборки, 4 параллельных компиляции) входит в обычный прогон; `cmake.lint_headers_standalone_script` проверяет сам скрипт на двух малых деревьях, `cmake.wiring_headers_standalone_ctest` - регистрацию.

**Проверено:** на прежнем коде (`git archive`) скрипт находит 3 из 176 заголовков на GCC 8.3, GCC 13.2, Clang 23.1.0 с libstdc++ и MinGW 8.3 и 4 из 176 на Clang 23.1.0 с libc++ (добавляется `XPathConstants.hpp`); после правки 0 из 176 на GCC 8.3, GCC 13.2, Clang 23.1.0 (libstdc++ и libc++) и MinGW 8.3 на C++11, 14, 17 и 20 (24 запуска), по 8-24 с на запуск при 4 компиляциях. Мутации: убранный `#include <cstddef>` из `IXmlWriter.hpp` валит `lint.headers_standalone`; скрипт, который не сообщает об ошибках, и скрипт без таблицы макросов валят `cmake.lint_headers_standalone_script`; переименованная регистрация, `--std 14` вместо 11 и убранный фильтр `-stdlib=` валят `cmake.wiring_headers_standalone_ctest`.

##### `LumexNumberGenerator.hpp` не компилируется после `LumexTime`

**Файлы:** `lumex/core/generators/number_generator/LumexNumberGenerator.hpp`, `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/consumer/hygiene_compile_checks/CMakeLists.txt` (новый), `lumex/tests/cmake/consumer/hygiene_compile_checks/header_order.cpp` (новый)

**Суть:** оба конструктора `number_generator` берут запасное зерно из `time (nullptr)` без `std::`. `LumexTime` вводит пространство имен `lumex::core::time`, поэтому в единице трансляции, где `LumexTime` стоит раньше `LumexGenerators`, неквалифицированное `time` находило пространство имен и заголовок не компилировался (`expected primary-expression before '(' token`); в обратном порядке имя находилось в глобальном пространстве и все работало. Вызов записан как `std::time (nullptr)` (`<ctime>` уже включен). Остальные заголовки и исходники `lumex/` просмотрены на неквалифицированные имена библиотеки C, которые может перехватить пространство имен библиотеки с тем же именем (26 имен пространств совпадают с идентификатором C или POSIX: `time`, `clock`, `log`, `error`, `sync`, `math`, `bit`, ...): других вызовов нет, остальные совпадения - функции-члены и конструкторы с тем же именем.

**Проверено:** новый случай `cmake.hygiene_compile_checks` собирает `header_order.cpp` (оба порядка включения, оба конструктора) на C++11, 14, 17 и 20; на прежнем коде падает на GCC 13.2 (C++11, `LumexTime` перед `LumexGenerators`), на новом проходит на GCC 13.2, GCC 8.3 и Clang 23.1.0 (libstdc++ и libc++).

##### Тесты, примеры и потребительские кейсы с компилятором вне системных путей запускаются без `LD_LIBRARY_PATH`

**Файлы:** `cmake/LumexBuild.cmake`, `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/consumer/run_consumer.cmake`, `lumex/tests/cmake/cases/wiring_toolchain_rpath.cmake` (новый); подмодуль `CMakeRoutines`: `deployment/ToolchainRuntimeRpath.cmake` (новый), `README.md`, `tests/CMakeLists.txt`, `tests/cases/toolchain_runtime_rpath_helpers.cmake` (новый), `tests/cases/toolchain_runtime_rpath_invalid_stdlib.cmake` (новый), `tests/target/CMakeLists.txt`, `tests/target/cases/toolchain_runtime_rpath.cmake` (новый), `tests/target/cases/toolchain_runtime_rpath_build.cmake` (новый), `tests/target/fixtures/toolchain_runtime_rpath/` (новый)

**Суть:** бинарники, собранные GCC 13.2 из `/opt/gcc-13.2.0` или Clang 23.1.0 из `/opt/llvm-23.1.0`, не находили рантайм своего компилятора: загрузчик брал из системы `libstdc++.so.6` от GCC 8 или `libc++.so.1` от Clang 19, и программа падала при запуске (`GLIBCXX_3.4.32 not found`) или на первом хеше строки (`undefined symbol: std::__1::__hash_memory`). CMake сам такой путь не добавляет: каталоги, которые компилятор и так просматривает, он в RPATH не кладет. Поэтому `ctest` приходилось запускать с `LD_LIBRARY_PATH=/opt/llvm-23.1.0/lib/x86_64-unknown-linux-gnu:/opt/gcc-13.2.0/lib64`. Теперь в подмодуле `CMakeRoutines` есть `configure_toolchain_runtime_rpath` и `get_toolchain_runtime_directories`: они спрашивают у компилятора, где лежат `libstdc++` и `libgcc_s` (GCC) или `libc++`, `libc++abi`, `libunwind` и `libgcc_s` (Clang с `-stdlib=libc++`), через `-print-file-name` с флагами `CMAKE_CXX_FLAGS` (так учитывается `--gcc-install-dir` из конфигурации Clang 23), и дописывают каталоги вне `/lib*` и `/usr/lib*` в `BUILD_RPATH` цели. `lumex_configure_target` вызывает ее для каждой настроенной цели после `configure_optimization_level`. Только дерево сборки: при `cmake --install` CMake заменяет RPATH целиком на `INSTALL_RPATH` (у пакетов `create_release.sh` это по-прежнему `$ORIGIN`). Системные компиляторы (GCC 8, Clang 19 из `/usr/lib/llvm-19`) не получают ничего; MSVC, Windows, MinGW, Apple и кросс-сборки пропускаются. Вложенные потребительские кейсы (`cmake.consumer_*`, `cmake.install_header_only_without_utility`) - отдельные проекты: раннер передает им каталоги как `CMAKE_BUILD_RPATH` и запускает исполняемый файл без `LD_LIBRARY_PATH` в окружении, так что кейс проверяет именно RUNPATH. Следствие для пакетов: `file(GET_RUNTIME_DEPENDENCIES)` в `PublishDistr.cmake` читает RUNPATH библиотек и теперь видит настоящий `libc++.so.1` от Clang 23, а не системный; поэтому в пакет Clang добавился `lib/libatomic.so.1` из GCC 13.2 (его требует этот `libc++.so.1`, раньше зависимость оставалась незамеченной).

**Проверено:** до правки (Clang 23.1.0 с libc++, Release, без `LD_LIBRARY_PATH`): из 104 тестов `base64.`, `optional.` и примеров `base64` и `math` 100 падают с `undefined symbol: _ZNSt3__113__hash_memoryEPKvm`, 2 проходят, 2 не запускаются (примеры не собирались); `cmake.install_header_only_without_utility` падает с `GLIBCXX_3.4.32 not found`; GCC 13.2: `ctest` не может перечислить тесты (`GLIBCXX_3.4.29 not found`), ни один не запускается. После правки, без `LD_LIBRARY_PATH`: Clang 23.1.0 с libc++, полная сборка и `ctest -j4` - 19814 тестов, 10 не проходят (6 `fmt` из todo 55 b и c, `LumexSerialProberPty.GivenPreWrittenResponse_WhenProbe_ThenResponded`, плавающий `LumexExceptionTest.LumexBaseException_ToCrashReport_ThreadSafe`, а также `cmake.require_ok_independents_off` и `cmake.require_fail_json_without_string_view`, которые падают и на `9521c921d`: граф зависимостей `resource_monitor` не отражен в этих кейсах); GCC 13.2, Release - 18068 тестов, 91 не проходит (79 `reflection`, 9 `fmt`, `LumexSerialProberPty` и те же два `cmake.*`; все известны); все 9 кейсов с меткой `consumer` проходят на обоих (7 из них запускают исполняемый файл). GCC 8.3 и Clang 19 с libc++ (системные): RUNPATH целей только каталог сборки, как раньше; 3 потребительских кейса проходят. `create_release.sh --formats tar.gz --std 17` для GCC 13.2, Clang 23.1.0 и GCC 8.3 до и после: список файлов и RUNPATH каждого ELF-файла пакетов GCC 13.2 и GCC 8.3 совпадают, у Clang 23.1.0 добавился один файл `lib/libatomic.so.1`; у всех 16 библиотек RUNPATH `$ORIGIN`. Сборка четырьмя компиляторами (GCC 8.3, GCC 13.2, Clang 23.1.0, MinGW 8.3) на C++11 и C++20: 0 предупреждений, `warns/` пуст. Самотесты CMakeRoutines: 23 из 23 в режиме скрипта, целевые - ALL PASSED на GCC 13.2 (280 проверок), Clang 23.1.0 (284), GCC 8.3 и Clang 19 (по 276), включая сборку фикстуры с чтением RUNPATH и запуском без `LD_LIBRARY_PATH`. Мутации: 19 правок модуля (системный каталог всегда ложь, нет проверки конца пути, флаги не доходят до компилятора, нет кэша, GCC ищет libc++, `-stdlib=` цели не читается, свойство не ставится и еще 12) ловятся самотестами; 9 правок проводки LumexLib (вызов убран, вызов до `configure_optimization_level`, `-Wl,-rpath` в `LumexBuild.cmake`, каталоги не доходят до раннера, раннер не передает их или оставляет `LD_LIBRARY_PATH`) ловит `cmake.wiring_toolchain_rpath`; раннер без `BUILD_RPATH` при заданном `LD_LIBRARY_PATH` в окружении падает на `install_header_only_without_utility`, с ним проходит.

##### `LumexResourceMonitor::stop()` не ждет конца паузы

**Файлы:** `lumex/applied/resource_monitor/monitor/LumexResourceMonitor.cpp`, `lumex/applied/resource_monitor/monitor/LumexResourceMonitor.hpp`, `lumex/tests/applied/resource_monitor/LumexResourceMonitor.cxx17.tests.cpp`

**Суть:** поток семплера спал через `std::this_thread::sleep_for` и в стартовой паузе (2 с), и между замерами, а `stop()` ждал его `join`: остановка хоста задерживалась до конца паузы или интервала (по умолчанию до 10 с). Теперь обе паузы - ожидание на `std::condition_variable`, и `stop()` будит поток сразу.

**Проверено:** новые тесты `StopDuringTheStartupPauseReturnsAtOnce` и `StopBetweenSamplesReturnsAtOnce` (интервал 60 с, бюджет 1 с) падают на прежнем коде (2 с и 62 с) и проходят на новом; весь набор модуля - 5,6 с вместо 21 с (GCC 13.2, Release).

##### Занятая память и загрузка CPU в журнале монитора на Linux

**Файлы:** `lumex/applied/resource_monitor/monitor/LumexResourceMonitor.cpp`, `lumex/applied/resource_monitor/monitor/detail/LumexProcFs.hpp` (новый), `lumex/applied/resource_monitor/monitor/detail/LumexProcFs.cpp` (новый), `lumex/tests/applied/resource_monitor/LumexProcFs.cxx17.tests.cpp` (новый)

**Суть:** занятая память на Linux считалась как `totalram - freeram` из `sysinfo()`, то есть вместе со страничным кэшем; теперь это `MemTotal - MemAvailable` из `/proc/meminfo`, как на Windows (`ullAvailPhys`), а `sysinfo()` остался запасным путем для ядер без `MemAvailable` (до 3.14). В загрузке CPU время `iowait` считалось занятостью, а `guest` и `guest_nice` входили в общее время второй раз (они уже есть в `user` и `nice`); теперь простой - `idle + iowait`, а общее время - восемь полей без гостевых. Разбор `/proc/stat` и `/proc/meminfo` вынесен в функции `detail::parse_proc_stat_cpu` и `detail::parse_proc_meminfo`, которые принимают текст файла.

**Проверено:** 11 новых тестов разбора (полная строка `cpu`, старое ядро с четырьмя полями, строки не того вида, ключи с общим префиксом, нет `MemAvailable`, живые `/proc` машины); три мутации разбора (простой без `iowait`, общее время с гостевыми полями, ключи по префиксу) ловятся. На машине с 31,11 ГиБ журнал показывает 12,41 ГиБ занятой памяти, ровно `MemTotal - MemAvailable`; прежняя формула дала бы 21,89 ГиБ.

##### `LumexDebug.hpp` не собирается при функциональных макросах `min` и `max`

**Файлы:** `lumex/core/utility/debug/LumexDebug.hpp`

**Суть:** `capture_stack_trace` на Linux вызывала `std::min (...)` без скобок. Если в единице трансляции определен функциональный макрос `min` (как делает `<windows.h>` без `NOMINMAX`), вызов раскрывался макросом и заголовок не компилировался. Вызов записан как `(std::min)(...)`, как и в остальной библиотеке.

**Проверено:** `LumexUtilityMinMaxMacros.cxx11.tests.cpp` не собирался на прежнем коде (ошибка компиляции, набор `LumexUtilityCxx11Tests`) и проходит на новом на C++11, 14, 17, 20 и 23; вся группа `utility.` - 8354 теста, 0 падений, `lint.*` проходит (GCC 13.2, Release).

##### Описание пакета Conan называет старые требования к стандарту

**Файлы:** `conanfile.py`

**Суть:** `description` рецепта говорило, что `math` требует C++20, а `resource_monitor` требует C++17. `math` работает с C++11 с 2026-09-25, `resource_monitor` с 2.0.0.0. В описании остались только `to_json` и проверки форматной строки при компиляции, которым нужен C++20.

**Проверено:** `python3 -m py_compile conanfile.py`; в `CMakeLists.txt` модулей `math` и `resource_monitor` нет закрепленного стандарта.


##### Библиотека собирается MinGW (GCC 8.3, потоки posix) и GCC 8 на Linux

**Файлы:** `cmake/LibraryVersioning.cmake`, `lumex/core/utility/CMakeLists.txt`, `lumex/applied/hardware/CMakeLists.txt`, `lumex/applied/logger/logger/LumexLogger.hpp`, `lumex/applied/hardware/caps/LumexCPUVectorizationCapabilities.hpp`, `lumex/applied/resource_monitor/process/LumexProcessMonitor.cpp`, `lumex/applied/serial/probe/detail/LumexSerialBoundedOpen.cpp`, `lumex/core/utility/dump/LumexCoreDumpGenerator.hpp`, `lumex/core/environment/env/LumexEnvironment.cpp`, `lumex/core/filesystem/fs/LumexFilesystem.cpp`, `lumex/tests/applied/hardware/LumexHardwareCapabilities.cxx11.tests.cpp`, `lumex/tests/applied/logger/LumexLogger.cxx11.tests.cpp`

**Суть:** кросс-сборка под Windows компилятором `x86_64-w64-mingw32-g++-posix` (MinGW-w64, GCC 8.3) падала на девяти местах, которых не видят MSVC и clang-cl; теперь все 16 библиотек собираются. (1) Версионный ресурс компилировался командой `rc.exe` (`/nologo /fo`); для `windres` (`MINGW`) аргументы теперь `-O coff -i ... -o ...`. (2) Логгер выбирал `__FUNCSIG__` по целевой системе (`_WIN32`), а он есть только у MSVC и clang-cl; `LOGGER_FUNCTION_NAME` теперь равен `LUMEX_FUNCTION_NAME`, который выбирает по компилятору. (3) `GetCurrentProcessToken` нет в старых заголовках MinGW-w64; генератор дампов использует собственную функцию с тем же документированным псевдодескриптором `(HANDLE)-4`. (4) `dbghelp`, `shlwapi`, `iphlpapi`, `d3d11`, `dxgi` подключались через `#pragma comment(lib)`, который GCC игнорирует; для MinGW они указаны в CMake (`utility` - PUBLIC, потому что их вызывает встроенный код заголовка). (5) `#include <Shlwapi.h>` написан с заглавной буквы, в `LumexFilesystem.cpp` теперь `<shlwapi.h>`. (6) `_dupenv_s` нет в стандартной среде выполнения MinGW; `windows_environment_strategy::get_variable` в этом случае сразу вызывает `GetEnvironmentVariableA`, который и так был запасным путем. (7) `std::thread::native_handle()` у winpthreads - это `pthread_t`, а не `HANDLE`; для `CancelSynchronousIo` дескриптор получается через `pthread_gethandle`. (8) `kReservedNames.size()` не константное выражение для массива из `std::string` на C++11 в GCC 8; счетчик убран, цикл идет по массиву. (9) `localtime_r` не объявлен в MinGW; под ним используется `std::localtime` под уже взятым мьютексом. Отдельно, на GCC 8 любой платформы `alignas` перед атрибутом экспорта в заголовке структуры не компилировался (`cpu_vectorization_info_t`); теперь вне MSVC выравнивание задано атрибутом `aligned`, а `psapi.h` и `tlhelp32.h` включаются после `windows.h`.

**Проверено:** MinGW-w64 GCC 8.3 posix, C++11, Release, `-DCMAKE_TOOLCHAIN_FILE` (Linux): все 16 библиотек собраны, ошибок нет (`package` не собирается: ему нужен CPack); GCC 13.2, Release: наборы `hardware`, `logger`, `filesystem`, `environment`, `serial`, `exceptions`, `resource_monitor`, `utility` - 8674 теста, падают только `ToCrashReport_ThreadSafe` и `LumexSerialProberPty.GivenPreWrittenResponse...` (обе падали и до правок); GCC 8.3 на Linux: `LumexCPUVectorizationCapabilities.cpp` на дереве релиза не компилируется (`expected identifier before __attribute__`), на этой ветке компилируется; новые тесты `CpuVectorizationInfoTest.IsAlignedToACacheLine` и `LoggerFunctionNameMacroTest.FollowsTheCompilerAndNotTheTarget` проходят; запуск полученных DLL на Windows не проверялся.


##### xml: предупреждения `-Wswitch-enum` и `-Wfloat-equal` GCC

**Файлы:** `lumex/xml/node/XmlNode.cpp`, `lumex/xml/utility/XmlUtils.hpp`, `lumex/xml/xpath/ast/XPathAstNode.cpp`, `lumex/xml/xpath/ast/XPathAstNode.hpp`, `lumex/xml/xpath/parser/XPathParser.cpp`, `lumex/xml/xpath/string/XPathString.cpp`, `lumex/xml/xpath/variable/XPathVariable.cpp`, `lumex/xml/xpath/variable/XPathVariable.hpp`, `lumex/tests/xml/LumexXmlXPathNumbers.cxx11.tests.cpp` (новый), `lumex/tests/xml/CMakeLists.txt`

**Суть:** сборка с закрепленным стандартом (так делает `create_release.sh`) давала на GCC 8 и 13 по 66-68 предупреждений в `xml`: 11 `switch` по перечислениям с `default`, но без всех значений (`-Wswitch-enum`, GCC печатает по строке на каждое пропущенное значение), и 12 точных сравнений `double` (`-Wfloat-equal`). В `switch` недостающие значения теперь перечислены перед `default` и ведут в ту же ветку, поведение не меняется. Сравнения с нулем, NaN и бесконечностью идут через одну функцию `utility::exactly_equal`, где отключено только это предупреждение; операторы XPath `=` и `!=` для чисел (они по определению сравнивают точно) остались шаблонными функторами в `XPathAstNode.cpp` под своим `pragma`. Проверка на NaN в `floor` и `ceiling` теперь вызывает `utility::is_nan`.

**Проверено:** предупреждений xml после правки 0 на GCC 8.3, GCC 13.2, Clang 23.1.0 и MinGW 8.3 (C++11 и C++20); все 426 тестов `xml` проходят; 11 новых тестов `XPathNumbersTest` (булево и строка из числа, `floor` и `ceiling` от NaN, числовой предикат, `=` и `!=`) проходят и на прежнем коде, то есть поведение не изменилось, а при замене `==` на `!=` в `exactly_equal` 6 из 11 падают (до правки мутация не ловилась ни одним тестом).


##### serial: предупреждения `-Wswitch-default`, `-Wsign-conversion`, `-Wcast-function-type`, `-Wold-style-cast`

**Файлы:** `lumex/applied/serial/probe/LumexSerialProber.cpp`, `lumex/applied/serial/probe/detail/LumexSerialBoundedOpen.cpp`, `lumex/applied/serial/resolver/LumexPortProcessResolver.cpp`

**Суть:** (1) У девяти `switch` по `enum class` (четность, стоп-биты, управление потоком, статус транспорта, исход открытия) не было ветки `default`. Теперь она есть: для настроек порта значение вне перечисления означает неверную настройку и дает `false`, для статуса транспорта и исхода открытия оно считается ошибкой, как значение `failed`. (2) Маски `termios` (`c_iflag`, `c_oflag`, `c_lflag`, `c_cflag`) считались как `~(int)`, то есть отрицательное число, и неявно превращались в `tcflag_t`; теперь приведение явное, набор битов тот же. (3) Только на Windows: адреса функций `ntdll` приводились из `FARPROC` напрямую (`-Wcast-function-type`), теперь через функцию `exported_function`, а запасной `NT_SUCCESS` использует `static_cast`.

**Проверено:** предупреждений в `serial` после правки 0 на GCC 8.3, GCC 13.2 и MinGW 8.3, у Clang 23.1.0 остались 2 из другого модуля (`logger`); тесты `serial` на GCC 13.2: 49 из 50, не проходит `LumexSerialProberPty.GivenPreWrittenResponse_WhenProbe_ThenResponded`, он не проходил и до правок (6 запусков из 6 на сборке до переименования); Windows-ветка собрана MinGW, не запускалась.


##### MinGW: MSVC-прагмы под `_MSC_VER` и класс генератора дампов без `dllimport`

**Файлы:** 17 файлов `lumex/applied/` и `lumex/core/` с `#pragma warning` и `#pragma comment` (`LumexHardwareCapabilities.hpp/.cpp`, `LumexCPUVectorizationCapabilities.hpp`, `LumexLogging.hpp`, `LumexSettingsGuard.hpp`, `LumexSettingsINI.hpp`, `LumexSettingsJSON.hpp`, `LumexSettingsXML.hpp`, `LumexEnvironment.hpp`, `LumexCrashHandler.hpp`, `LumexException.hpp`, `LumexStacktrace.hpp/.cpp`, `LumexStacktraceEntry.hpp`, `LumexFilesystem.hpp/.cpp`), `lumex/core/utility/dump/LumexCoreDumpGenerator.hpp`, `Scripts/CodeTools/check_msvc_pragmas.py` (новый), `lumex/tests/CMakeLists.txt`, `lumex/tests/cmake/CMakeLists.txt`, `lumex/tests/cmake/cases/lint_msvc_pragmas_script.cmake` (новый), `lumex/tests/cmake/fixtures/msvc_pragmas/` (новые)

**Суть:** (1) `#pragma warning` и `#pragma comment(lib)` есть только у MSVC и clang-cl (оба задают `_MSC_VER`); для GCC это неизвестные прагмы, и сборка с `-Werror` останавливалась на первой из 56. Они были под `_WIN32`, который задает и MinGW. Теперь все 56 стоят под `_MSC_VER`. (2) Классы `dump_factory` и `core_dump_generator` целиком были помечены `LUMEX_UTILITY_API` (`dllimport` у потребителей), а все их методы inline и определены вне класса; GCC на MinGW для каждого метода, которым пользуются в теле класса, выдает `redeclared without dllimport attribute after being referenced`, и это предупреждение нельзя отключить ни флагом, ни `pragma` (32 места). Под MinGW атрибут теперь стоит только на статических данных-членах, которые определяет `.cpp` (макросы `LUMEX_DUMP_CLASS_API` и `LUMEX_DUMP_DATA_API`); на MSVC, clang-cl и ELF все как было. (3) Новая проверка `lint.msvc_pragmas` (`check_msvc_pragmas.py`) находит прагму вне `_MSC_VER`, в ветке `#else` или в условии с `||`; кейс `cmake.lint_msvc_pragmas_script` проверяет саму проверку на двух фикстурах.

**Проверено:** MinGW 8.3, C++11: 115 предупреждений до правки, 27 после (все 56 `-Wunknown-pragmas` и 32 `redeclared` ушли; остаток другой природы, он в следующих записях); проверка на дереве релиза находит ровно 56 прагм, на этой ветке 0; если в проверке убрать правило про `||`, кейс на фикстурах падает; GCC 13.2, Release: все 4079 теста C++11 проходят, кроме `LumexSerialProberPty.GivenPreWrittenResponse...`, который не проходил и до правок; на MSVC не собиралось (на других компиляторах макросы раскрываются в прежний текст).


##### Остальные предупреждения Windows-веток и логгера на MinGW и Clang

**Файлы:** `lumex/applied/hardware/caps/LumexHardwareCapabilities.cpp`, `lumex/applied/logger/logger/LumexLogger.cpp`, `lumex/applied/logger/logger/LumexLogger.hpp`, `lumex/core/exceptions/stacktrace/LumexStacktrace.cpp`, `lumex/core/filesystem/fs/LumexFilesystem.cpp`, `lumex/core/utility/dump/LumexCoreDumpGenerator.hpp`

**Суть:** (1) Запасная `std::exchange` логгера для стандартов до C++14 была объявлена `constexpr` при двух операторах в теле, что Clang на C++11 принимает только как расширение (`-Wc++14-extensions`, 2 предупреждения); теперь она обычная шаблонная функция. (2) В Windows-ветках неявные преобразования знаков и размеров (`DWORD` в `LONG`, `int` в `size_t`, `size_t` в `DWORD` при расчете DACL, маска `~FILE_ATTRIBUTE_READONLY`) и приведения в стиле C заменены на `static_cast` и `reinterpret_cast`; значения те же. (3) `get_file_status_windows` вызывал одну и ту же `GetFileAttributesExA` в обеих ветках тернарного оператора по `follow`; вызов один, параметр помечен неиспользуемым с пояснением (функция описывает сам файл или ссылку). (4) `_get_minidump_type` перечисляет `CORE_DUMP_FULL` и `DEFAULT_AUTO` перед `default` (`-Wswitch-enum`), `DEFAULT_UNIX` равен `CORE_DUMP_FULL`. (5) У явного инстанцирования `lumex_basic_stacktrace` под MinGW нет атрибута экспорта: GCC его игнорирует с предупреждением, класс уже объявлен с ним. (6) `capture_stack_trace` логгера явно использует параметры и константу, которые не нужны на платформе без обхода стека.

**Проверено:** все 16 комбинаций сборки библиотеки (GCC 8.3, GCC 13.2, Clang 23.1.0, MinGW 8.3, C++11, 14, 17, 20, `LUMEX_WERROR=OFF`): 0 предупреждений и 0 ошибок (до серии правок: 11-13 у Clang, 76-78 у GCC, 192 у MinGW); GCC 13.2: группы `logger`, `logging`, `utility`, `filesystem`, `hardware`, `exceptions` - 8833 теста, 0 падений, `lint.*` 5 из 5; Windows-ветки собраны MinGW и не запускались.


##### Определение возможностей процессора: `__cpuid` выбирается по компилятору, а не по системе

**Файлы:** `lumex/applied/hardware/caps/LumexCPUVectorizationCapabilities.cpp`

**Суть:** файл подключал `<intrin.h>` и вызывал `__cpuid` по условию `_WIN32` / `LUMEX_OS_WINDOWS`. `<intrin.h>` из MinGW-w64 заново объявляет встроенные функции GCC (`__builtin_ia32_crc32*`), и GCC печатает `redundant redeclaration ... [-Wredundant-decls]` без номера строки (место `<built-in>`); с `-Werror` сборка MinGW падала. Теперь `<intrin.h>` и `__cpuid` только у MSVC и clang-cl (`_MSC_VER`), у GCC, в том числе на Windows, `<cpuid.h>` и `__cpuid_count`, как на Linux.

**Проверено:** MinGW 8.3 с `LUMEX_WERROR=ON` до правки падает на этом файле, после проходит; GCC 13.2 с `LUMEX_WERROR=ON` проходит (Linux использует тот же путь, что и раньше); значения CPUID на Windows не сверялись.


##### Установка без DLL, если задан только `CMAKE_INSTALL_LIBDIR`

**Файлы:** `CMakeLists.txt`, `lumex/tests/cmake/cases/wiring_install_dirs_order.cmake` (новый), `lumex/tests/cmake/CMakeLists.txt`

**Суть:** `include(GNUInstallDirs)` стоял в корневом `CMakeLists.txt` после `add_subdirectory(lumex)`. Правила установки модулей читают `CMAKE_INSTALL_BINDIR` при обработке своей папки, поэтому при конфигурации, где задан только `-DCMAKE_INSTALL_LIBDIR=lib` (так вызывает `create_release.sh`), `BINDIR` был пуст и CMake молча отбрасывал правило `RUNTIME`, то есть сами DLL не попадали в `install`. На Linux это не видно (библиотеки `.so` ставятся по `LIBRARY`), на Windows и MinGW установка не содержала ни одной DLL. Теперь `include(GNUInstallDirs)` стоит перед `add_subdirectory(lumex)`.

**Проверено:** первая конфигурация под MinGW 8.3 с `-DCMAKE_INSTALL_PREFIX=/LumexLib -DCMAKE_INSTALL_LIBDIR=lib`: правил установки DLL было 0, стало 16 (без аргументов и с одним `LIBDIR` тоже 16); сгенерированные правила установки на Linux (GCC 13.2, те же аргументы) совпадают построчно до и после; новый кейс `cmake.wiring_install_dirs_order` падает, если вернуть `include` после `add_subdirectory`.

---

## [v1.0.3.1] - в разработке

> Изменения поверх `v1.0.3.0`.

### [v1.0.3.1]

#### Изменено

##### Релизный скрипт Windows по схеме Linux-скрипта

**Файлы:** `create_release.ps1`, `create_release.sh`

**Суть:** `create_release.ps1` приведен к схеме `create_release.sh`: одна сборка на компилятор и ISA вместо четырех сборок (стандарты 11, 14, 17, 20) на архитектуру. Компиляторы задаются списком `-Compilers`: ид набора MSVC (`v120`..`v145`; артефакт помечается `msvc<год Visual Studio>`, например `msvc2026`) или `clang-cl` (имя в PATH либо путь к `clang-cl.exe`; сборка через Ninja, пометка `clang-cl-<major.minor.patch>` из вывода `--version`). Версия читается из `project(LumexLib VERSION)` (`-Version` меняет только имена артефактов и предупреждает о расхождении); пакеты `LumexLib-<version>_win_<ISA>_<compiler>.<zip|tar.gz>` кладутся в `release/` (по умолчанию) и содержат одну верхнюю папку с именем пакета, как в Linux-пакетах; есть `-Arch`, `-Formats`, `-OutputDir`, `-KeepWork`, `-Help`. Все проверки (python, cmake, ninja для clang-cl, установленные наборы MSVC с компилятором нужной архитектуры, компиляторы clang-cl, tar для tar.gz) выполняются до первой сборки; `<repo>/x64` удаляется после каждого пакета; в конце печатается предупреждение о секции `[vX.Y.Z.W]` в `CHANGELOG.md`, помеченной "в разработке", и итог по времени - как в Linux-скрипте. Обязательная передача `-DLUMEX_BUILD_XML=ON` убрана: модуль включен по умолчанию (`cmake/LumexOptions.cmake`). В `create_release.sh` комментарий о проверке секции CHANGELOG переформулирован без ссылки на внутренние документы процесса.

Проверено локально: `-Compilers v145` дает `LumexLib-1.0.3.1_win_x64_msvc2026.zip` (1 мин 34 с), `-Compilers D:\local\LLVM\bin\clang-cl.exe` - `LumexLib-1.0.3.1_win_x64_clang-cl-21.1.5.zip` (1 мин 30 с); в обоих архивах 214 записей, верхняя папка - имя пакета.

##### Установщик Windows (`.exe`) на CPack/NSIS

**Файлы:** `CMakeLists.txt`, `lumex/tests/cmake/cases/cpack_nsis.cmake` (новый), `lumex/tests/cmake/CMakeLists.txt`, `create_release.ps1`

**Суть:** корневой `CMakeLists.txt` получил блок CPack (только для top-level сборки с `LUMEX_INSTALL`): генератор `NSIS` на Windows и `TGZ` на остальных платформах, имя, версия из `project(LumexLib VERSION)`, контакт, имя файла `LumexLib-<версия>-win`, каталог установки по умолчанию `LumexLib/<версия>` и запись в Add/Remove Programs; `CPACK_PACKAGE_INSTALL_DIRECTORY`, `CPACK_PACKAGE_INSTALL_REGISTRY_KEY` и `CPACK_NSIS_DISPLAY_NAME` объявлены CACHE-переменными, чтобы релизный скрипт дописывал суффикс компилятора. `create_release.ps1` получил формат `exe`: к сборке добавляется `compile.py -i` (CPack строит NSIS-инсталлятор из того же install-дерева, `--cmake-args` передает `LumexLib/<версия>_<компилятор>` в каталог установки, реестр и Add/Remove), готовый `build/LumexLib-<версия>-win_<ISA>_*_cpp*.exe` копируется в `release/LumexLib-<версия>_win_<ISA>_<компилятор>.exe`; проверка NSIS выполняется до первой сборки: `makensis` ищется через переменную окружения `MAKENSIS_EXE`, PATH, ключ реестра NSIS, значения PATH из реестра (машинный и пользовательский) и обычные каталоги установки (в том числе Chocolatey и Scoop), а найденный каталог ставится в начало PATH для CPack, чтобы он взял тот же бинарь; zip/tar.gz собираются из того же прогона. Инсталлятор ставит дерево в `%ProgramFiles%\LumexLib\<версия>_<компилятор>` (сборки разных компиляторов стоят рядом, как `/opt/LumexLib/...` на Linux) и оставляет uninstaller с записью в Add/Remove Programs. Кейс `cmake.cpack_nsis` фиксирует строки подключения CPack.

**Проверено:** кейс `cmake.cpack_nsis` (через `cmake -P`), два прогона `create_release.ps1 -Formats zip,exe` - `msvc2026` (1 мин 34 с) и `clang-cl-21.1.5` (1 мин 16 с); оба инсталлятора несут сигнатуру NSIS (`Nullsoft Install System`), 1.53 и 1.56 МБ. Установку и удаление прогоняет пользователь: инсталлятор требует прав администратора (per-machine, как у продуктов). Повторный прогон `msvc2026` после расширенного поиска `makensis` - 2 мин 02 с.

##### Перечисления в параметрах релизного скрипта

**Файлы:** `create_release.ps1`

**Суть:** `-Compilers`, `-Arch` и `-Formats` объявлены `[string[]]` и склеиваются в строку перед разбором: работают и обычное перечисление PowerShell (`-Formats zip,exe`), и одна строка с запятыми (`-Formats 'zip,exe'`). Раньше без кавычек перечисление падало с ошибкой привязки аргументов (`Cannot convert value to type System.String`), то есть пример из `-Help` (`-Compilers v143,D:\local\LLVM\bin\clang-cl.exe`) в таком виде не работал.

**Проверено:** оба вида вызова дают один и тот же список (сборка `msvc2026` с некавыченным `-Formats zip,exe` выше); `-Help`, пустой `-Compilers`, неизвестный формат и неверный `MAKENSIS_EXE` завершаются с exit 1 и внятным сообщением; ошибка привязки воспроизводилась в Windows PowerShell 5.1 и pwsh 7.6.6.

##### dev-каталог с линковочными файлами и отладочными символами

**Файлы:** `cmake/LumexModules.cmake`, `CMakeLists.txt`, `lumex/tests/cmake/cases/dev_artifacts.cmake` (новый), `lumex/tests/cmake/CMakeLists.txt`, `create_release.ps1`, `create_release.sh`

**Суть:** в установочном дереве появился каталог `dev/`: `lumex_install_dev_artifacts` обходит реальные цели из `LUMEX_SHARED_LIBRARY_CANDIDATES` и кладет туда для каждой библиотеки импортный `.lib`, `.exp` и `.pdb` (MSVC; clang-cl тоже, он выставляет `MSVC=TRUE`) либо `<файл>.debug` (GCC/Clang на ELF - тот, что линковочный лаунчер `configure_optimization_level` вырезает из релизного бинаря). Правила - `install(FILES ... OPTIONAL)`: чего платформа не производит (`.exp` на ELF, PDB у MinGW), просто не попадает в пакет, интерфейсные модули пропускаются. Каталог едет со всем остальным деревом, то есть попадает и в `zip`/`tar.gz`, и в deb/rpm, и в NSIS-инсталлятор; импортные `.lib` остаются и в `lib/` - на них ссылаются экспортированные цели CMake. Кейс `cmake.dev_artifacts` фиксирует правила и вызов из корневого файла.

**Проверено:** `cmake.dev_artifacts` и `cmake.cpack_nsis` (через `cmake -P`) зеленые; прогон `create_release.ps1 -Compilers v145 -Formats zip,exe`: в `LumexLib-1.0.3.1_win_x64_msvc2026.zip` 262 записи (214 до правки), из них 48 в `dev/` (16 PDB, 16 импортных `.lib`, 16 `.exp`), zip 10.8 МБ вместо 1.6, инсталлятор 7.27 МБ вместо 1.53; `project.nsi` в CPack-стейджинге перечисляет те же 48 файлов. Что clang-cl получает те же файлы, подтверждено сборкой (его `build/bin` несет PDB, `build/lib` - `.lib`/`.exp`) и тем, что clang-cl выставляет `MSVC=TRUE` (проверено мини-конфигурацией: `MSVC_VAR=1`, `SIMULATE_ID=MSVC`). Прогон на Linux - за пользователем: правила те же, `.debug` пишет сплиттер CMakeRoutines.

##### MSVC-рантайм в bin/ релизных пакетов

**Файлы:** `cmake/InstallBinRuntime.cmake` (новый), `CMakeLists.txt`, `create_release.ps1`, `lumex/tests/cmake/cases/install_bin_runtime.cmake` (новый), `lumex/tests/cmake/CMakeLists.txt`

**Суть:** в установочное дерево добавлен install-хук (только Windows): после раскладки файлов `cmake/InstallBinRuntime.cmake` берет первые `Lumex*.dll` из `bin/` и прогоняет их через `file(GET_RUNTIME_DEPENDENCIES)` в `CMakeRoutines/deployment/CopyRuntimeDependencies.cmake` с дефолтным набором имен Windows - тот же механизм и тот же набор (`msvcp140.dll`, `vcruntime140*.dll`), что `publish_distr` уже стейджит в `<platform>/Distr<Config>`. Раньше пакет нес только Lumex-DLL, и потребителю требовался отдельно установленный распространяемый пакет MSVC; теперь библиотеки грузятся на машине без него. Хук попадает и в zip (дерево собирает `compile.py --install-prefix`), и в NSIS-инсталлятор (CPack прогоняет те же install-правила); на ELF скрипт сразу выходит, и раскладка рантайма остается за `create_release.sh` (его набор не меняется). Кейс `cmake.install_bin_runtime` фиксирует обвязку.

**Проверено:** кейс падал до обвязки и зелен после; прогон `create_release.ps1 -Compilers v145 -Formats zip,exe` (2 мин 47 с): в `LumexLib-1.0.3.1_win_x64_msvc2026.zip` 265 записей (было 262), `bin/` - 19 файлов (было 16): + `msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll`; те же три файла в CPack-стейджинге и в `project.nsi` инсталлятора. Linux-сторона не затронута: хук под `if(WIN32)`, скрипт выходит при `NOT WIN32`.

---

## [v1.0.3.0] - в разработке

> Изменения поверх `v1.0.2.0`.

### [v1.0.3.0]

#### Добавлено

##### Опрос последовательного порта с общим дедлайном (`SerialProber`)

**Файлы:** `lumex/applied/serial/probe/LumexSerialProber.hpp`, `lumex/applied/serial/probe/LumexSerialProber.cpp`, `lumex/applied/serial/probe/detail/LumexSerialTransport.hpp`, `lumex/applied/serial/probe/detail/LumexSerialProbeEngine.hpp`, `lumex/applied/serial/probe/detail/LumexSerialBoundedOpen.hpp`, `lumex/applied/serial/probe/detail/LumexSerialBoundedOpen.cpp`, `lumex/applied/serial/LumexSerialPort` (зонтичный заголовок), `lumex/tests/applied/serial/LumexSerialProbeEngine.cxx11.tests.cpp`, `lumex/tests/applied/serial/LumexSerialProber.cxx11.tests.cpp`, `lumex/tests/applied/serial/LumexSerialProberPty.cxx11.tests.cpp`, `lumex/examples/serial/example_serial.cpp`, `lumex/examples/serial/example_serial_workflow.cpp`

**Суть:** класс `SerialProber` (пространство имен `lumex::applied::serial::probe`): конструктор от пути и `serial_port_settings_t` (по умолчанию 9600, 8 бит данных, без паритета, один стоп-бит, без flow control); `open` с ограниченным по времени открытием (по умолчанию 500 мс); `probe (request, response_complete, deadline = 500 мс, max_response_bytes = 8192)` открывает порт при необходимости, чистит буферы, пишет запрос и читает до подтверждения предикатом, лимита или дедлайна; `read`/`write` с дедлайном. Шесть исходов: `responded`, `incomplete`, `no_response`, `open_timeout`, `open_failed`, `io_error`; настройки, недоступные на платформе, дают `open_failed` с текстом. На Windows открытие выполняется в рабочем потоке и при просрочке отменяется через `CancelSynchronousIo` (зависший Bluetooth-порт не блокирует вызывающего дольше дедлайна), обмен - overlapped-вызовы с `CancelIoEx`; на POSIX - `O_NONBLOCK` и `poll`. Класс перемещаемый, не потокобезопасный, ошибки - статусами, без исключений; примеры дополнены безопасной демонстрацией на заведомо отсутствующем порту.

#### Изменено

##### Перечисление портов больше не может зависнуть

**Файлы:** `lumex/applied/serial/enumeration/LumexSerialPortEnumeration.hpp`, `lumex/applied/serial/enumeration/LumexSerialPortEnumeration.cpp`, `lumex/tests/applied/serial/LumexSerialPortEnumeration.cxx11.tests.cpp`, `lumex/tests/cmake/cases/source_enumeration_uses_bounded_open.cmake`, `lumex/tests/cmake/CMakeLists.txt`

**Суть:** каждая открывающая проба (классификация порта и брутфорс `COM1`..`COM255`) переведена на общий ограниченный `bounded_open_serial_port` с дедлайном 500 мс; новое состояние `serial_port_state::unresponsive` (строка `"Unresponsive"`) для открытий, не уложившихся в дедлайн, с текстом таймаута в `system_error`; `print_serial_ports_info` печатает отдельную секцию unresponsive. Bluetooth-порты по-прежнему не открываются, значения по умолчанию флагов не изменились. Исходный кейс `source_enumeration_uses_bounded_open` запрещает прямые открытия в файле перечисления и требует наличия `CancelSynchronousIo` и `CancelIoEx`.

#### Исправлено

##### Заголовки utility переживают макросы `min` и `max` из `windows.h`

**Файлы:** `lumex/core/utility/numeric/LumexSafeNumericComparator.hpp`, `lumex/tests/core/utility/LumexUtilityMinMaxMacros.cxx11.tests.cpp`

**Суть:** 58 вызовов `std::numeric_limits<...>::min ()` и `max ()` заменены на идиому `(std::numeric_limits<...>::max) ()` - то же правило, что уже действует в `core/fmt` и закреплено тестом `LumexFormatMinMaxMacros.cxx11.tests.cpp`. Потребитель, который подключает `LumexUtility` после `windows.h` без `NOMINMAX` (так делает потребительская фикстура `consumer_standard_mismatch_*`), снова компилируется; новый `LumexUtilityMinMaxMacros.cxx11.tests.cpp` фиксирует это, определяя макросы до включения умбреллы.

##### Раннер потребительских CMake-кейсов распознает скип при конфигурации

**Файлы:** `lumex/tests/cmake/consumer/run_consumer.cmake`

**Суть:** фикстура, которая печатает `LUMEX_CONSUMER_SKIP` и завершает конфигурацию (например, `atomic_cross_module` на Windows), теперь пропускается CTest, а не падает с "executable not found": раннер проверяет вывод конфигурации на маркер до сборки, а не только в ветке `CONFIGURE_ONLY`.

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

##### `LumexStringView` и `LumexWStringView` не включают зонтик `LumexUtility`

**Файлы:** `lumex/core/string_view/view/LumexStringView.hpp`, `lumex/core/string_view/view/LumexWStringView.hpp`, `lumex/core/string_view/view/LumexStringView.cpp`

**Суть:** оба заголовка включали `lumex/core/utility/LumexUtility` и неиспользуемый `LumexConstantMacros.hpp`. Зонтик тянет отладочные заголовки, генератор дампа (`windows.h`, `aclapi.h`, `dbghelp.h` и соседние заголовки Windows) и идентификатор процесса. Классам нужны `LUMEX_CONSTEXPR`, `LUMEX_CONSTEXPR_CTOR`, `LUMEX_NOEXCEPT` и `LUMEX_ATTRIBUTE_NODISCARD`, поэтому остаются `LumexKeywords.hpp` и `LumexAttributes.hpp`. `LumexStringView.cpp` сам включает `<algorithm>` и `<stdexcept>`: `std::min`, `std::swap` и `std::out_of_range` приходили через зонтик. Потребитель, который включал только `string_view` и пользовался отсюда символами дампа, Windows или `stringify`, больше их не видит и включает нужный заголовок сам.

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
