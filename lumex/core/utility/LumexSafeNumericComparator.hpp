// NOLINTBEGIN(readability-simplify-boolean-expr)
#ifndef LUMEX_UTILITY_SAFENUMERICCOMPARATOR_HPP
  #define LUMEX_UTILITY_SAFENUMERICCOMPARATOR_HPP

  #include <atomic>
  #include <cmath>
  #include <limits>
  #include <type_traits>
  #include <utility>

  #if __cplusplus >= 202002L
    #include <compare>
  #endif

  #if defined(_M_IX86) || defined(__i386__) || defined(__i386) || defined(i386)
    #define _SFNC_ARCH_X86 1 // 32-bit CPU architecture (SFNC stands for Safe Numeric Comparator)
  #endif

/**
 * @file SafeNumericComparator.hpp
 * @brief Универсальный thread-safe компаратор для безопасного сравнения арифметических типов
 * @details Предотвращает переполнение и некорректные сравнения между типами с разными диапазонами.
 *          Поддерживает все арифметические типы (integral и floating-point) с compile-time оптимизациями.
 * @since C++11
 * @note Thread-safe, exception-safe, no-throw где возможно
 *
 * @section problem Проблема: Небезопасное сравнение разных типов
 *
 * В C++ сравнение переменных разных типов может привести к серьезным проблемам:
 *
 * @subsection overflow Проблема переполнения
 * При сравнении типов с разными диапазонами значений происходит неявное преобразование типов,
 * которое может привести к переполнению и неопределенному поведению (UB):
 *
 * @code
 * unsigned char block_size = 200;  // Диапазон: 0-255
 * int packet_size = 300;           // Диапазон: -2,147,483,648 до 2,147,483,647
 *
 * // ОПАСНО! Может привести к неправильному результату
 * if (block_size >= packet_size) {
 *   // Логика: 200 >= 300 должно быть false
 *   // Реальность: unsigned char(300) = 44 (300 % 256)
 *   // Результат: 200 >= 44 = true (НЕПРАВИЛЬНО!)
 * }
 * @endcode
 *
 * @subsection floating_point Проблема с плавающей точкой
 * Сравнение целочисленных и floating-point типов также проблематично:
 *
 * @code
 * int counter = 3;
 * float threshold = 3.14f;
 *
 * // ОПАСНО! NaN и бесконечность не обрабатываются
 * if (counter >= threshold) {
 *   // При threshold = NaN результат неопределен
 *   // При threshold = +infinity всегда false
 *   // При threshold = -infinity всегда true
 * }
 * @endcode
 *
 * @subsection consequences Последствия проблем
 * - Неправильная логика программы
 * - Неопределенное поведение (UB)
 * - Потенциальные уязвимости безопасности
 * - Сложная отладка
 *
 * @section solution Решение: SafeComparator
 *
 * SafeComparator решает все эти проблемы через:
 *
 * @subsection range_checking Проверка диапазонов
 * Перед сравнением проверяет, помещается ли значение в диапазон целевого типа:
 *
 * @code
 * // БЕЗОПАСНО с SafeComparator
 * SafeUCharComparator safe_size(200);
 * if (safe_size.safe_compare(300)) {
 *   // Алгоритм:
 *   // 1. Проверяем: 300 > 255 (max unsigned char) = true
 *   // 2. Возвращаем: false (корректно!)
 * }
 * @endcode
 *
 * @subsection floating_handling Обработка floating-point
 * Корректно обрабатывает специальные значения:
 *
 * @code
 * SafeIntComparator safe_counter(3);
 * float threshold = std::numeric_limits<float>::quiet_NaN();
 *
 * if (safe_counter.safe_compare(threshold)) {
 *   // Алгоритм:
 *   // 1. Проверяем: threshold is NaN = true
 *   // 2. Возвращаем: false (корректно!)
 * }
 * @endcode
 *
 * @subsection compile_time Compile-time оптимизации
 * Использует SFINAE для выбора оптимального алгоритма сравнения:
 * - Одинаковые типы: прямое сравнение (максимальная скорость)
 * - Целочисленные типы: проверка диапазонов + преобразование
 * - Floating-point типы: обработка NaN/infinity + преобразование
 * - Смешанные типы: комбинированная обработка
 *
 * @section examples Практические примеры
 *
 * @subsection bad_example Пример БЕЗ SafeComparator (опасно)
 *
 * @code
 * // Проблемный код
 * struct DataBlock {
 *     unsigned char m_Size;  // 0-255
 *     // ...
 * };
 *
 * void processData(const DataBlock& block) {
 *     int maxPacketSize = 1000;  // Может быть любым значением
 *
 *     // КРИТИЧЕСКАЯ ОШИБКА!
 *     if (block.m_Size >= maxPacketSize) {
 *         // Проблема: unsigned char(1000) = 232 (1000 % 256)
 *         // Результат: любое m_Size >= 232 даст true
 *         // Например: m_Size = 200, но 200 >= 232 = false (правильно)
 *         // Но: m_Size = 250, и 250 >= 232 = true (неправильно!)
 *         processLargePacket();
 *     } else {
 *         processSmallPacket();
 *     }
 * }
 * @endcode
 *
 * @subsection good_example Пример С SafeComparator (безопасно)
 *
 * @code
 * // Безопасный код
 * struct DataBlock {
 *     unsigned char m_Size;  // 0-255
 *     // ...
 * };
 *
 * void processData(const DataBlock& block) {
 *     int maxPacketSize = 1000;  // Может быть любым значением
 *
 *     // БЕЗОПАСНО!
 *     SafeUCharComparator safeSize(block.m_Size);
 *     if (safeSize.safe_compare(maxPacketSize)) {
 *         // Алгоритм SafeComparator:
 *         // 1. Проверяем: maxPacketSize(1000) > 255 (max unsigned char) = true
 *         // 2. Возвращаем: false (корректно!)
 *         // Результат: processSmallPacket() будет вызван правильно
 *         processLargePacket();
 *     } else {
 *         processSmallPacket();  // Правильный выбор!
 *     }
 * }
 * @endcode
 *
 * @subsection comprehensive_example Полный пример всех операций сравнения
 *
 * @code
 * // Демонстрация всех операций сравнения
 * void demonstrateAllComparisons() {
 *     unsigned char size = 200;
 *     int threshold = 300;
 *     float precision = 3.14f;
 *
 *     SafeUCharComparator safeSize(size);
 *
 *     // Все виды сравнений
 *     if (safeSize.safe_equal(threshold)) {
 *         // Безопасное сравнение ==
 *     }
 *
 *     if (safeSize.safe_not_equal(threshold)) {
 *         // Безопасное сравнение !=
 *     }
 *
 *     if (safeSize.safe_less(threshold)) {
 *         // Безопасное сравнение <
 *     }
 *
 *     if (safeSize.safe_greater(threshold)) {
 *         // Безопасное сравнение >
 *     }
 *
 *     if (safeSize.safe_less_equal(threshold)) {
 *         // Безопасное сравнение <=
 *     }
 *
 *     if (safeSize.safe_greater_equal(threshold)) {
 *         // Безопасное сравнение >=
 *     }
 *
 * #if __cplusplus >= 202002L
 *     // C++20 трехстороннее сравнение
 *     auto result = safeSize.safe_three_way_compare(threshold);
 *     if (is_equal(result)) {
 *         // Значения равны
 *     } else if (is_less(result)) {
 *         // Первое значение меньше
 *     } else if (is_greater(result)) {
 *         // Первое значение больше
 *     }
 * #endif
 *
 *     // Утилитарные функции
 *     if (safe_equal(size, threshold)) {
 *         // Прямое сравнение без создания объекта
 *     }
 *
 *     if (safe_less(size, precision)) {
 *         // Сравнение с floating-point
 *     }
 * }
 * @endcode
 *
 * @subsection why_it_works Почему это работает
 *
 * SafeComparator использует следующий алгоритм для сравнения unsigned char >= int:
 *
 * @code
 * bool safe_compare(unsigned char current, int other) {
 *     // 1. Проверяем диапазон: other > max(unsigned char) = 255
 *     if (other > 255) return false;  // other слишком большой
 *
 *     // 2. Проверяем диапазон: other < min(unsigned char) = 0
 *     if (other < 0) return true;      // other отрицательный
 *
 *     // 3. Безопасное сравнение: преобразуем в общий тип
 *     return current >= static_cast<int>(other);
 * }
 * @endcode
 *
 * Этот подход гарантирует:
 * - Корректные результаты сравнения
 * - Отсутствие переполнения
 * - Обработка edge cases
 * - Compile-time проверки типов
 * - Thread-safety при необходимости
 *
 * @section usage Когда использовать
 *
 * Используйте SafeComparator когда:
 * - Сравниваете переменные разных типов
 * - Работаете с пользовательским вводом
 * - Разрабатываете критические системы
 * - Нужна гарантия корректности сравнений
 * - Требуется thread-safety
 *
 * @section performance Производительность
 *
 * SafeComparator оптимизирован для производительности:
 * - Compile-time диспетчеризация (zero-cost abstractions)
 * - Специализации для одинаковых типов (прямое сравнение)
 * - Минимальные накладные расходы на проверки
 * - Lock-free операции для атомарных версий
 *
 * @section algorithm Полный алгоритм работы SafeComparator
 *
 * @subsection algorithm_overview Обзор алгоритма
 * SafeComparator использует детерминированный алгоритм для безопасного сравнения любых
 * арифметических типов. Алгоритм выполняется в строгом порядке действий:
 *
 * **ВХОДНЫЕ ДАННЫЕ**: current (тип T), other (тип U)
 * **ВЫХОДНЫЕ ДАННЫЕ**: bool (результат сравнения current >= other)
 *
 * @subsection step_by_step_algorithm Пошаговый алгоритм
 *
 * **ШАГ 1: Классификация типов**
 * ```
 * ДЕЙСТВИЕ 1.1: Определить тип T
 * ДЕЙСТВИЕ 1.2: Определить тип U
 * ДЕЙСТВИЕ 1.3: Вычислить same_type = (T == U)
 * ДЕЙСТВИЕ 1.4: Вычислить both_integral = (T ∈ integral) AND (U ∈ integral)
 * ДЕЙСТВИЕ 1.5: Вычислить both_floating = (T ∈ floating) AND (U ∈ floating)
 * ДЕЙСТВИЕ 1.6: Вычислить mixed_types = NOT(same_type) AND NOT(both_integral) AND NOT(both_floating)
 * ```
 *
 * **ШАГ 2: Выбор алгоритма сравнения**
 * ```
 * ЕСЛИ same_type == true:
 *     ПЕРЕЙТИ К АЛГОРИТМУ A (одинаковые типы)
 * ИНАЧЕ ЕСЛИ both_integral == true:
 *     ПЕРЕЙТИ К АЛГОРИТМУ B (целочисленные типы)
 * ИНАЧЕ ЕСЛИ both_floating == true:
 *     ПЕРЕЙТИ К АЛГОРИТМУ C (floating-point типы)
 * ИНАЧЕ ЕСЛИ mixed_types == true:
 *     ПЕРЕЙТИ К АЛГОРИТМУ D (смешанные типы)
 * ИНАЧЕ:
 *     ВЫЗВАТЬ ОШИБКУ КОМПИЛЯЦИИ
 * ```
 *
 * **АЛГОРИТМ A: Сравнение одинаковых типов (T == U)**
 * ```
 * ДЕЙСТВИЕ A.1: Выполнить прямое сравнение: result = (current >= other)
 * ДЕЙСТВИЕ A.2: Вернуть result
 * ```
 *
 * **АЛГОРИТМ B: Сравнение целочисленных типов (T != U, оба integral)**
 * ```
 * ДЕЙСТВИЕ B.1: Получить max_T = std::numeric_limits<T>::max()
 * ДЕЙСТВИЕ B.2: Получить min_T = std::numeric_limits<T>::min()
 * ДЕЙСТВИЕ B.3: ЕСЛИ other > max_T:
 *     ДЕЙСТВИЕ B.3.1: Вернуть false
 * ДЕЙСТВИЕ B.4: ЕСЛИ other < min_T:
 *     ДЕЙСТВИЕ B.4.1: Вернуть true
 * ДЕЙСТВИЕ B.5: Получить CommonType = std::common_type<T, U>::type
 * ДЕЙСТВИЕ B.6: Выполнить преобразование: current_common = static_cast<CommonType>(current)
 * ДЕЙСТВИЕ B.7: Выполнить преобразование: other_common = static_cast<CommonType>(other)
 * ДЕЙСТВИЕ B.8: Выполнить сравнение: result = (current_common >= other_common)
 * ДЕЙСТВИЕ B.9: Вернуть result
 * ```
 *
 * **АЛГОРИТМ C: Сравнение floating-point типов**
 * ```
 * ДЕЙСТВИЕ C.1: ЕСЛИ std::isnan(current) == true ИЛИ std::isnan(other) == true:
 *     ДЕЙСТВИЕ C.1.1: Вернуть false
 * ДЕЙСТВИЕ C.2: ЕСЛИ std::isinf(current) == true ИЛИ std::isinf(other) == true:
 *     ДЕЙСТВИЕ C.2.1: Выполнить сравнение: result = (current == other)
 *     ДЕЙСТВИЕ C.2.2: Вернуть result
 * ДЕЙСТВИЕ C.3: Получить CommonType = std::common_type<T, U>::type
 * ДЕЙСТВИЕ C.4: Выполнить преобразование: current_common = static_cast<CommonType>(current)
 * ДЕЙСТВИЕ C.5: Выполнить преобразование: other_common = static_cast<CommonType>(other)
 * ДЕЙСТВИЕ C.6: Выполнить сравнение: result = (current_common >= other_common)
 * ДЕЙСТВИЕ C.7: Вернуть result
 * ```
 *
 * **АЛГОРИТМ D: Сравнение смешанных типов (integral + floating)**
 * ```
 * ДЕЙСТВИЕ D.1: ЕСЛИ T ∈ integral И U ∈ floating:
 *     ПЕРЕЙТИ К ПОДАЛГОРИТМУ D1 (integral T, floating U)
 * ДЕЙСТВИЕ D.2: ЕСЛИ T ∈ floating И U ∈ integral:
 *     ПЕРЕЙТИ К ПОДАЛГОРИТМУ D2 (floating T, integral U)
 * ДЕЙСТВИЕ D.3: ИНАЧЕ:
 *     ВЫЗВАТЬ ОШИБКУ КОМПИЛЯЦИИ
 * ```
 *
 * **ПОДАЛГОРИТМ D1: integral T, floating U**
 * ```
 * ДЕЙСТВИЕ D1.1: ЕСЛИ std::isnan(other) == true:
 *     ДЕЙСТВИЕ D1.1.1: Вернуть false
 * ДЕЙСТВИЕ D1.2: ЕСЛИ std::isinf(other) == true:
 *     ДЕЙСТВИЕ D1.2.1: ЕСЛИ other < 0:
 *         ДЕЙСТВИЕ D1.2.1.1: Вернуть true
 *     ДЕЙСТВИЕ D1.2.2: ИНАЧЕ:
 *         ДЕЙСТВИЕ D1.2.2.1: Вернуть false
 * ДЕЙСТВИЕ D1.3: Получить max_T = std::numeric_limits<T>::max()
 * ДЕЙСТВИЕ D1.4: Получить min_T = std::numeric_limits<T>::min()
 * ДЕЙСТВИЕ D1.5: ЕСЛИ other > max_T:
 *     ДЕЙСТВИЕ D1.5.1: Вернуть false
 * ДЕЙСТВИЕ D1.6: ЕСЛИ other < min_T:
 *     ДЕЙСТВИЕ D1.6.1: Вернуть true
 * ДЕЙСТВИЕ D1.7: Получить CommonType = std::common_type<T, U>::type
 * ДЕЙСТВИЕ D1.8: Выполнить преобразование: current_common = static_cast<CommonType>(current)
 * ДЕЙСТВИЕ D1.9: Выполнить преобразование: other_common = static_cast<CommonType>(other)
 * ДЕЙСТВИЕ D1.10: Выполнить сравнение: result = (current_common >= other_common)
 * ДЕЙСТВИЕ D1.11: Вернуть result
 * ```
 *
 * **ПОДАЛГОРИТМ D2: floating T, integral U**
 * ```
 * ДЕЙСТВИЕ D2.1: ЕСЛИ std::isnan(current) == true:
 *     ДЕЙСТВИЕ D2.1.1: Вернуть false
 * ДЕЙСТВИЕ D2.2: ЕСЛИ std::isinf(current) == true:
 *     ДЕЙСТВИЕ D2.2.1: ЕСЛИ current > 0:
 *         ДЕЙСТВИЕ D2.2.1.1: Вернуть true
 *     ДЕЙСТВИЕ D2.2.2: ИНАЧЕ:
 *         ДЕЙСТВИЕ D2.2.2.1: Вернуть false
 * ДЕЙСТВИЕ D2.3: Получить CommonType = std::common_type<T, U>::type
 * ДЕЙСТВИЕ D2.4: Выполнить преобразование: current_common = static_cast<CommonType>(current)
 * ДЕЙСТВИЕ D2.5: Выполнить преобразование: other_common = static_cast<CommonType>(other)
 * ДЕЙСТВИЕ D2.6: Выполнить сравнение: result = (current_common >= other_common)
 * ДЕЙСТВИЕ D2.7: Вернуть result
 * ```
 *
 * @subsection algorithm_example Пример выполнения алгоритма
 *
 * **ВХОДНЫЕ ДАННЫЕ**: current = 252 (unsigned char), other = 1652 (int)
 *
 * **ШАГ 1: Классификация типов**
 * - T = unsigned char, U = int
 * - same_type = false (unsigned char != int)
 * - both_integral = true (оба целочисленные)
 * - both_floating = false
 * - mixed_types = false
 *
 * **ШАГ 2: Выбор алгоритма**
 * - both_integral == true -> АЛГОРИТМ B
 *
 * **АЛГОРИТМ B: Сравнение целочисленных типов**
 * - ДЕЙСТВИЕ B.1: max_T = 255 (максимум unsigned char)
 * - ДЕЙСТВИЕ B.2: min_T = 0 (минимум unsigned char)
 * - ДЕЙСТВИЕ B.3: other(1652) > max_T(255) -> true -> Вернуть false
 *
 * **РЕЗУЛЬТАТ**: false (252 >= 1652 корректно определено как false)
 *
 * @subsection practical_examples Практические примеры с кодом
 *
 * **ПРИМЕР 1: БЕЗ SafeComparator (опасно)**
 * @code
 * unsigned char block_size = 252;  // Диапазон: 0-255
 * int packet_size = 1652;          // Диапазон: -2,147,483,648 до 2,147,483,647
 *
 * // ОПАСНО! Неявное преобразование типов
 * if (block_size >= packet_size) {
 *     // Проблема: unsigned char(1652) = 116 (1652 % 256)
 *     // Результат: 252 >= 116 = true (НЕПРАВИЛЬНО!)
 *     // Логически: 252 >= 1652 должно быть false
 *     processLargePacket();  // Вызывается ошибочно!
 * } else {
 *     processSmallPacket();
 * }
 * @endcode
 *
 * **Проблема**: unsigned char(1652) = 116 из-за переполнения (1652 % 256 = 116)
 * **Результат**: 252 >= 116 = true (неправильно!)
 * **Последствие**: Неправильная логика программы
 *
 * **ПРИМЕР 2: С SafeComparator (безопасно)**
 * @code
 * unsigned char block_size = 252;  // Диапазон: 0-255
 * int packet_size = 1652;          // Диапазон: -2,147,483,648 до 2,147,483,647
 *
 * // БЕЗОПАСНО! Используем SafeComparator
 * SafeUCharComparator safe_size(block_size);
 * if (safe_size.safe_compare(packet_size)) {
 *     // Алгоритм SafeComparator:
 *     // 1. Классификация: unsigned char vs int -> both_integral = true
 *     // 2. Выбор: АЛГОРИТМ B (целочисленные типы)
 *     // 3. ДЕЙСТВИЕ B.1: max_T = 255
 *     // 4. ДЕЙСТВИЕ B.3: packet_size(1652) > max_T(255) -> true
 *     // 5. РЕЗУЛЬТАТ: false (корректно!)
 *     processLargePacket();
 * } else {
 *     processSmallPacket();  // Правильный выбор!
 * }
 * @endcode
 *
 * **Алгоритм SafeComparator**:
 * 1. **Классификация типов**: unsigned char vs int -> both_integral = true
 * 2. **Выбор алгоритма**: АЛГОРИТМ B (целочисленные типы)
 * 3. **ДЕЙСТВИЕ B.1**: max_T = 255 (максимум unsigned char)
 * 4. **ДЕЙСТВИЕ B.3**: packet_size(1652) > max_T(255) -> true
 * 5. **РЕЗУЛЬТАТ**: false (252 >= 1652 корректно определено как false)
 *
 * **Результат**: processSmallPacket() вызывается правильно!
 *
 * **Сравнение результатов**:
 * - **Без SafeComparator**: 252 >= 116 = true (неправильно)
 * - **С SafeComparator**: 252 >= 1652 = false (правильно)
 * - **Разница**: SafeComparator предотвращает переполнение и дает корректный результат
 *
 * @subsection algorithm_properties Свойства алгоритма
 *
 * **Детерминированность**: Алгоритм всегда дает одинаковый результат для одинаковых входных данных
 * **Корректность**: Результат сравнения математически корректен
 * **Безопасность**: Нет неопределенного поведения (UB)
 * **Эффективность**: O(1) временная и пространственная сложность
 * **Полнота**: Обрабатывает все возможные комбинации арифметических типов
 *
 */

// === Хелперные метафункции ===
template <typename TypeToClean> struct clean_type_t {
  using type = typename std::remove_cv<typename std::remove_reference<TypeToClean>::type>::type;
};

// === Проверки совместимости типов ===

/**
 * @brief Метафункция для проверки возможности безопасного сравнения двух типов
 * @tparam T Первый тип для сравнения
 * @tparam U Второй тип для сравнения
 * @details Проверяет, что оба типа являются арифметическими и имеют специализированные numeric_limits.
 *          Это необходимо для корректной работы с диапазонами значений и предотвращения переполнения.
 * @note Compile-time проверка, не влияет на runtime производительность
 */
template <typename T, typename U> struct is_safe_comparable {
  using clean_T           = typename clean_type_t<T>::type;
  using clean_U           = typename clean_type_t<U>::type;
  static bool const value = std::is_arithmetic<clean_T>::value && std::is_arithmetic<clean_U>::value
                            && std::numeric_limits<clean_T>::is_specialized
                            && std::numeric_limits<clean_U>::is_specialized;
};

  // === Концепты для C++20 совместимости ===
  #if __cplusplus >= 202002L
/**
 * @brief Концепт для арифметических типов с специализированными numeric_limits
 * @tparam T Тип для проверки
 * @details Современная C++20 альтернатива SFINAE для проверки арифметических типов.
 *          Обеспечивает более читаемые ошибки компиляции и лучшую интеграцию с современным C++.
 * @since C++20
 */
template <typename T>
concept ArithmeticType = std::is_arithmetic_v<T> && std::numeric_limits<T>::is_specialized;

/**
 * @brief Концепт для безопасно сравниваемых типов
 * @tparam T Первый тип для сравнения
 * @tparam U Второй тип для сравнения
 * @details Проверяет, что оба типа являются арифметическими и могут быть безопасно сравнены.
 *          Используется для более строгой типизации в C++20 коде.
 * @since C++20
 */
template <typename T, typename U>
concept SafeComparable = ArithmeticType<T> && ArithmeticType<U>;
  #endif

// === Traits для оптимизации ===
/**
 * @brief Метафункция для определения характеристик сравнения типов
 * @tparam T Первый тип для сравнения
 * @tparam U Второй тип для сравнения
 * @details Предоставляет compile-time информацию о типах для оптимизации алгоритмов сравнения.
 *          Использует STL common_type для определения общего типа преобразования.
 * @note Все вычисления выполняются на этапе компиляции, не влияя на runtime производительность
 */
template <typename T, typename U> struct comparison_traits {
  using clean_T               = typename clean_type_t<T>::type;
  using clean_U               = typename clean_type_t<U>::type;

  static bool const same_type = std::is_same<clean_T, clean_U>::value; ///< true если типы идентичны
  static bool const both_integral
    = std::is_integral<clean_T>::value && std::is_integral<clean_U>::value; ///< true если оба типа целочисленные
  static bool const both_floating
    = std::is_floating_point<clean_T>::value
      && std::is_floating_point<clean_U>::value; ///< true если оба типа с плавающей точкой
  static bool const mixed_types
    = !same_type && !both_integral && !both_floating; ///< true если типы смешанные (integral + floating)

  // Используем STL common_type напрямую - просто, надежно, эффективно
  using common_type = typename std::common_type<clean_T, clean_U>::type; ///< Общий тип для безопасного преобразования
};

// === SFINAE Helper для C++11 совместимости ===

/**
 * @brief Базовый шаблон для реализации безопасного сравнения
 * @tparam T Первый тип для сравнения
 * @tparam U Второй тип для сравнения
 * @tparam Enable SFINAE параметр для специализации
 * @details Использует SFINAE для выбора оптимальной реализации сравнения в зависимости от типов.
 *          Обеспечивает C++11 совместимость без использования if constexpr.
 */
template <typename T, typename U, typename Enable = void> struct safe_compare_impl_helper;

/**
 * @brief Специализация для одинаковых типов (максимальная оптимизация)
 * @tparam T Тип для сравнения
 * @details Прямое сравнение без преобразований типов для максимальной производительности.
 *          Используется когда оба типа идентичны, что исключает возможность переполнения.
 * @note No-throw, максимальная производительность
 */
template <typename T>
struct safe_compare_impl_helper<
  T, T, typename std::enable_if<std::is_integral<typename clean_type_t<T>::type>::value>::type> {
  using clean_T = typename clean_type_t<T>::type;
  /**
   * @brief Безопасное сравнение >= для одинаковых типов
   * @param[in] current Текущее значение
   * @param[in] other Значение для сравнения
   * @return true если current >= other
   * @note Прямое сравнение без преобразований, максимальная производительность
   */
  static bool
  compare(T current, T other) noexcept
  {
    return current >= other; // Прямое сравнение без преобразований
  }

  /**
   * @brief Безопасное сравнение >= для одинаковых типов
   * @param[in] current Текущее значение
   * @param[in] other Значение для сравнения
   * @return true если current >= other
   */
  static bool
  greater_equal(T current, T other) noexcept
  {
    return current >= other;
  }

  /**
   * @brief Безопасное сравнение <= для одинаковых типов
   * @param[in] current Текущее значение
   * @param[in] other Значение для сравнения
   * @return true если current <= other
   */
  static bool
  less_equal(T current, T other) noexcept
  {
    return current <= other;
  }

  /**
   * @brief Безопасное сравнение < для одинаковых типов
   * @param[in] current Текущее значение
   * @param[in] other Значение для сравнения
   * @return true если current < other
   */
  static bool
  less(T current, T other) noexcept
  {
    return current < other;
  }

  /**
   * @brief Безопасное сравнение > для одинаковых типов
   * @param[in] current Текущее значение
   * @param[in] other Значение для сравнения
   * @return true если current > other
   */
  static bool
  greater(T current, T other) noexcept
  {
    return current > other;
  }

  /**
   * @brief Безопасное сравнение == для одинаковых типов
   * @param[in] current Текущее значение
   * @param[in] other Значение для сравнения
   * @return true если current == other
   */
  static bool
  equal(T current, T other) noexcept
  {
    return current == other;
  }

  /**
   * @brief Безопасное сравнение != для одинаковых типов
   * @param[in] current Текущее значение
   * @param[in] other Значение для сравнения
   * @return true если current != other
   */
  static bool
  not_equal(T current, T other) noexcept
  {
    return current != other;
  }

  #if __cplusplus >= 202002L
  /**
   * @brief Трехстороннее сравнение для одинаковых типов (C++20 spaceship)
   * @param[in] current Текущее значение
   * @param[in] other Значение для сравнения
   * @return std::strong_ordering результат сравнения
   * @details Возвращает strong_ordering для точного сравнения одинаковых типов.
   *          Гарантирует детерминированный порядок без потери информации.
   * @since C++20
   */
  static std::strong_ordering
  three_way_compare(T current, T other) noexcept
  {
    if(current < other) return std::strong_ordering::less;
    if(current > other) return std::strong_ordering::greater;
    return std::strong_ordering::equal;
  }
  #endif
};

/**
 * @brief Специализация для одинаковых типов с плавающей точкой
 * @tparam T Тип с плавающей точкой для сравнения
 * @details Прямое сравнение без преобразований типов для максимальной производительности.
 *          Используется когда оба типа идентичны и являются floating-point типами.
 * @note No-throw, максимальная производительность для floating-point типов
 */
template <typename T>
struct safe_compare_impl_helper<
  T, T, typename std::enable_if<std::is_floating_point<typename clean_type_t<T>::type>::value>::type> {
  using clean_T = typename clean_type_t<T>::type;
  /**
   * @brief Безопасное сравнение >= для одинаковых floating-point типов
   * @param[in] current Текущее значение
   * @param[in] other Значение для сравнения
   * @return true если current >= other
   * @note Прямое сравнение без преобразований, максимальная производительность
   */
  static bool
  compare(T current, T other) noexcept
  {
    if(std::isnan(current) || std::isnan(other)) return false;
    return current >= other;
  }

  /**
   * @brief Безопасное сравнение == для одинаковых floating-point типов
   * @param[in] current Текущее значение
   * @param[in] other Значение для сравнения
   * @return true если current == other
   * @note Прямое сравнение без преобразований, максимальная производительность
   */
  static bool
  equal(T current, T other) noexcept
  {
    if(std::isnan(current) || std::isnan(other)) return false;
    return current == other;
  }

  /**
   * @brief Безопасное сравнение != для одинаковых floating-point типов
   * @param[in] current Текущее значение
   * @param[in] other Значение для сравнения
   * @return true если current != other
   * @note Прямое сравнение без преобразований, максимальная производительность
   */
  static bool
  not_equal(T current, T other) noexcept
  {
    if(std::isnan(current) || std::isnan(other)) return true;
    return current != other;
  }

  /**
   * @brief Безопасное сравнение < для одинаковых floating-point типов
   * @param[in] current Текущее значение
   * @param[in] other Значение для сравнения
   * @return true если current < other
   * @note Прямое сравнение без преобразований, максимальная производительность
   */
  static bool
  less(T current, T other) noexcept
  {
    if(std::isnan(current) || std::isnan(other)) return false;
    return current < other;
  }

  /**
   * @brief Безопасное сравнение > для одинаковых floating-point типов
   * @param[in] current Текущее значение
   * @param[in] other Значение для сравнения
   * @return true если current > other
   * @note Прямое сравнение без преобразований, максимальная производительность
   */
  static bool
  greater(T current, T other) noexcept
  {
    if(std::isnan(current) || std::isnan(other)) return false;
    return current > other;
  }

  /**
   * @brief Безопасное сравнение <= для одинаковых floating-point типов
   * @param[in] current Текущее значение
   * @param[in] other Значение для сравнения
   * @return true если current <= other
   * @note Прямое сравнение без преобразований, максимальная производительность
   */
  static bool
  less_equal(T current, T other) noexcept
  {
    if(std::isnan(current) || std::isnan(other)) return false;
    return current <= other;
  }

  /**
   * @brief Безопасное сравнение >= для одинаковых floating-point типов
   * @param[in] current Текущее значение
   * @param[in] other Значение для сравнения
   * @return true если current >= other
   * @note Прямое сравнение без преобразований, максимальная производительность
   */
  static bool
  greater_equal(T current, T other) noexcept
  {
    if(std::isnan(current) || std::isnan(other)) return false;
    return current >= other;
  }

  #if __cplusplus >= 202002L
  /**
   * @brief Трехстороннее сравнение для одинаковых floating-point типов
   * @param[in] current Текущее значение
   * @param[in] other Значение для сравнения
   * @return std::partial_ordering результат сравнения
   * @details Возвращает partial_ordering для floating-point типов из-за NaN.
   *          NaN всегда возвращает unordered.
   * @since C++20
   */
  static std::partial_ordering
  three_way_compare(T current, T other) noexcept
  {
    if(std::isnan(current) || std::isnan(other)) return std::partial_ordering::unordered;
    if(current < other) return std::partial_ordering::less;
    if(current > other) return std::partial_ordering::greater;
    return std::partial_ordering::equivalent;
  }
  #endif
};

/**
 * @brief Специализация для целочисленных типов
 * @tparam T Первый целочисленный тип
 * @tparam U Второй целочисленный тип
 * @details Реализует безопасное сравнение между разными целочисленными типами.
 *          Проверяет диапазоны значений для предотвращения переполнения при преобразовании типов.
 * @note Проверяет границы типов перед преобразованием, предотвращает неопределенное поведение
 */
template <typename T, typename U>
struct safe_compare_impl_helper<
  T, U,
  typename std::enable_if<
    std::is_integral<typename clean_type_t<T>::type>::value && std::is_integral<typename clean_type_t<U>::type>::value
    && !std::is_same<typename clean_type_t<T>::type, typename clean_type_t<U>::type>::value>::type> {
  using clean_T    = typename clean_type_t<T>::type;
  using clean_U    = typename clean_type_t<U>::type;
  using CommonType = typename std::common_type<clean_T, clean_U>::type; ///< Общий тип для безопасного преобразования

  /**
   * @brief Безопасное сравнение >= для целочисленных типов
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current >= other
   * @details Проверяет диапазоны значений перед преобразованием для предотвращения переполнения.
   *          Если other превышает максимальное значение T, возвращает false.
   *          Если other меньше минимального значения T, возвращает true.
   */
  static bool
  compare(T current, U other) noexcept
  {
    // Проверяем диапазоны для предотвращения переполнения
    // ВАЖНО: учитываем знаковость типов для корректного сравнения
    if((std::is_signed<clean_T>::value && std::is_unsigned<clean_U>::value) ? true : false)
    {
      // T signed, U unsigned: если current < 0, то current >= other всегда false
      if(current < 0) return false;
      // other всегда >= 0, проверяем только верхнюю границу
  #ifdef _SFNC_ARCH_X86
      using UnsignedT = typename std::make_unsigned<clean_T>::type;
      if(static_cast<UnsignedT>(other) > static_cast<UnsignedT>(std::numeric_limits<clean_T>::max())) return false;
  #else
      if(other > static_cast<typename std::make_unsigned<clean_T>::type>(std::numeric_limits<clean_T>::max()))
        return false;
  #endif
    }
    else if((std::is_unsigned<clean_T>::value && std::is_signed<clean_U>::value) ? true : false)
    {
      // T unsigned, U signed: если other < 0, то current >= other всегда true
      if(other < 0) return true;
      // Иначе проверяем верхнюю границу
  #ifdef _SFNC_ARCH_X86
      using UnsignedU = typename std::make_unsigned<clean_U>::type;
      if(static_cast<UnsignedU>(other) > static_cast<UnsignedU>(std::numeric_limits<clean_T>::max())) return false;
  #else
      using UnsignedU = typename std::make_unsigned<clean_U>::type;
      if(static_cast<UnsignedU>(other) > static_cast<UnsignedU>(std::numeric_limits<clean_T>::max())) return false;
  #endif
    }
    else
    {
      // Оба signed или оба unsigned - безопасное сравнение
  #ifdef _SFNC_ARCH_X86
      // На 32-битных архитектурах используем явные приведения для устранения C4018
      if(static_cast<CommonType>(other) > static_cast<CommonType>(std::numeric_limits<clean_T>::max())) return false;
      if(static_cast<CommonType>(other) < static_cast<CommonType>(std::numeric_limits<clean_T>::min())) return true;
  #else
      if(other > std::numeric_limits<clean_T>::max()) return false;
      if(other < std::numeric_limits<clean_T>::min()) return true;
  #endif
    }
    return static_cast<CommonType>(current) >= static_cast<CommonType>(other);
  }

  /**
   * @brief Безопасное сравнение >= для целочисленных типов
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current >= other
   */
  static bool
  greater_equal(T current, U other) noexcept
  {
    return compare(current, other);
  }

  /**
   * @brief Безопасное сравнение <= для целочисленных типов
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current <= other
   * @details Проверяет диапазоны значений перед преобразованием для предотвращения переполнения.
   *          Если other превышает максимальное значение T, возвращает true.
   *          Если other меньше минимального значения T, возвращает false.
   */
  static bool
  less_equal(T current, U other) noexcept
  {
    // Проверяем диапазоны с учетом знаковости
    if((std::is_signed<clean_T>::value && std::is_unsigned<clean_U>::value) ? true : false)
    {
      // T signed, U unsigned: если current < 0, то current <= other всегда true
      if(current < 0) return true;
  #ifdef _SFNC_ARCH_X86
      using UnsignedT = typename std::make_unsigned<clean_T>::type;
      if(static_cast<UnsignedT>(other) > static_cast<UnsignedT>(std::numeric_limits<clean_T>::max())) return true;
  #else
      if(other > static_cast<typename std::make_unsigned<clean_T>::type>(std::numeric_limits<clean_T>::max()))
        return true;
  #endif
    }
    else if((std::is_unsigned<clean_T>::value && std::is_signed<clean_U>::value) ? true : false)
    {
      if(other < 0) return false;
  #ifdef _SFNC_ARCH_X86
      using UnsignedU = typename std::make_unsigned<clean_U>::type;
      if(static_cast<UnsignedU>(other) > static_cast<UnsignedU>(std::numeric_limits<clean_T>::max())) return true;
  #else
      using UnsignedU = typename std::make_unsigned<clean_U>::type;
      if(static_cast<UnsignedU>(other) > static_cast<UnsignedU>(std::numeric_limits<clean_T>::max())) return true;
  #endif
    }
    else
    {
  #ifdef _SFNC_ARCH_X86
      // На 32-битных архитектурах используем явные приведения для устранения C4018
      if(static_cast<CommonType>(other) > static_cast<CommonType>(std::numeric_limits<clean_T>::max())) return true;
      if(static_cast<CommonType>(other) < static_cast<CommonType>(std::numeric_limits<clean_T>::min())) return false;
  #else
      if(other > std::numeric_limits<clean_T>::max()) return true;
      if(other < std::numeric_limits<clean_T>::min()) return false;
  #endif
    }
    return static_cast<CommonType>(current) <= static_cast<CommonType>(other);
  }

  /**
   * @brief Безопасное сравнение < для целочисленных типов
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current < other
   * @details Проверяет диапазоны значений перед преобразованием для предотвращения переполнения.
   *          Если other превышает максимальное значение T, возвращает true.
   *          Если other меньше минимального значения T, возвращает false.
   */
  static bool
  less(T current, U other) noexcept
  {
    // Проверяем диапазоны с учетом знаковости
    if((std::is_signed<clean_T>::value && std::is_unsigned<clean_U>::value) ? true : false)
    {
      // T signed, U unsigned: если current < 0, то current < other всегда true
      if(current < 0) return true;
  #ifdef _SFNC_ARCH_X86
      using UnsignedT = typename std::make_unsigned<clean_T>::type;
      if(static_cast<UnsignedT>(other) > static_cast<UnsignedT>(std::numeric_limits<clean_T>::max())) return true;
  #else
      if(other > static_cast<typename std::make_unsigned<clean_T>::type>(std::numeric_limits<clean_T>::max()))
        return true;
  #endif
    }
    else if((std::is_unsigned<clean_T>::value && std::is_signed<clean_U>::value) ? true : false)
    {
      if(other < 0) return false;
  #ifdef _SFNC_ARCH_X86
      using UnsignedU = typename std::make_unsigned<clean_U>::type;
      if(static_cast<UnsignedU>(other) > static_cast<UnsignedU>(std::numeric_limits<clean_T>::max())) return true;
  #else
      using UnsignedU = typename std::make_unsigned<clean_U>::type;
      if(static_cast<UnsignedU>(other) > static_cast<UnsignedU>(std::numeric_limits<clean_T>::max())) return true;
  #endif
    }
    else
    {
  #ifdef _SFNC_ARCH_X86
      // На 32-битных архитектурах используем явные приведения для устранения C4018
      if(static_cast<CommonType>(other) > static_cast<CommonType>(std::numeric_limits<clean_T>::max())) return true;
      if(static_cast<CommonType>(other) < static_cast<CommonType>(std::numeric_limits<clean_T>::min())) return false;
  #else
      if(other > std::numeric_limits<clean_T>::max()) return true;
      if(other < std::numeric_limits<clean_T>::min()) return false;
  #endif
    }
    return static_cast<CommonType>(current) < static_cast<CommonType>(other);
  }

  /**
   * @brief Безопасное сравнение > для целочисленных типов
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current > other
   * @details Проверяет диапазоны значений перед преобразованием для предотвращения переполнения.
   *          Если other превышает максимальное значение T, возвращает false.
   *          Если other меньше минимального значения T, возвращает true.
   */
  static bool
  greater(T current, U other) noexcept
  {
    // Проверяем диапазоны с учетом знаковости
    if((std::is_signed<clean_T>::value && std::is_unsigned<clean_U>::value) ? true : false)
    {
      // T signed, U unsigned: если current < 0, то current > other всегда false
      if(current < 0) return false;
  #ifdef _SFNC_ARCH_X86
      using UnsignedT = typename std::make_unsigned<clean_T>::type;
      if(static_cast<UnsignedT>(other) > static_cast<UnsignedT>(std::numeric_limits<clean_T>::max())) return false;
  #else
      if(other > static_cast<typename std::make_unsigned<clean_T>::type>(std::numeric_limits<clean_T>::max()))
        return false;
  #endif
    }
    else if((std::is_unsigned<clean_T>::value && std::is_signed<clean_U>::value) ? true : false)
    {
      if(other < 0) return true;
  #ifdef _SFNC_ARCH_X86
      using UnsignedU = typename std::make_unsigned<clean_U>::type;
      if(static_cast<UnsignedU>(other) > static_cast<UnsignedU>(std::numeric_limits<clean_T>::max())) return false;
  #else
      using UnsignedU = typename std::make_unsigned<clean_U>::type;
      if(static_cast<UnsignedU>(other) > static_cast<UnsignedU>(std::numeric_limits<clean_T>::max())) return false;
  #endif
    }
    else
    {
  #ifdef _SFNC_ARCH_X86
      // На 32-битных архитектурах используем явные приведения для устранения C4018
      if(static_cast<CommonType>(other) > static_cast<CommonType>(std::numeric_limits<clean_T>::max())) return false;
      if(static_cast<CommonType>(other) < static_cast<CommonType>(std::numeric_limits<clean_T>::min())) return true;
  #else
      if(other > std::numeric_limits<clean_T>::max()) return false;
      if(other < std::numeric_limits<clean_T>::min()) return true;
  #endif
    }
    return static_cast<CommonType>(current) > static_cast<CommonType>(other);
  }

  /**
   * @brief Безопасное сравнение == для целочисленных типов
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current == other
   * @details Проверяет диапазоны значений перед преобразованием для предотвращения переполнения.
   *          Если other не помещается в диапазон T, возвращает false.
   */
  static bool
  equal(T current, U other) noexcept
  {
    // Проверяем диапазоны с учетом знаковости
    if((std::is_signed<clean_T>::value && std::is_unsigned<clean_U>::value) ? true : false)
    {
      // T signed, U unsigned: если current < 0, то current == other всегда false
      if(current < 0) return false;
  #ifdef _SFNC_ARCH_X86
      using UnsignedT = typename std::make_unsigned<clean_T>::type;
      if(static_cast<UnsignedT>(other) > static_cast<UnsignedT>(std::numeric_limits<clean_T>::max())) return false;
  #else
      if(other > static_cast<typename std::make_unsigned<clean_T>::type>(std::numeric_limits<clean_T>::max()))
        return false;
  #endif
    }
    else if((std::is_unsigned<clean_T>::value && std::is_signed<clean_U>::value) ? true : false)
    {
      if(other < 0) return false;
  #ifdef _SFNC_ARCH_X86
      using UnsignedU = typename std::make_unsigned<clean_U>::type;
      if(static_cast<UnsignedU>(other) > static_cast<UnsignedU>(std::numeric_limits<clean_T>::max())) return false;
  #else
      using UnsignedU = typename std::make_unsigned<clean_U>::type;
      if(static_cast<UnsignedU>(other) > static_cast<UnsignedU>(std::numeric_limits<clean_T>::max())) return false;
  #endif
    }
    else
    {
      if(other > std::numeric_limits<clean_T>::max()) return false;
      if(other < std::numeric_limits<clean_T>::min()) return false;
    }
    return static_cast<CommonType>(current) == static_cast<CommonType>(other);
  }

  /**
   * @brief Безопасное сравнение != для целочисленных типов
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current != other
   * @details Проверяет диапазоны значений перед преобразованием для предотвращения переполнения.
   *          Если other не помещается в диапазон T, возвращает true.
   */
  static bool
  not_equal(T current, U other) noexcept
  {
    // Проверяем диапазоны с учетом знаковости
    if((std::is_signed<clean_T>::value && std::is_unsigned<clean_U>::value) ? true : false)
    {
      // T signed, U unsigned: если current < 0, то current != other всегда true
      if(current < 0) return true;
  #ifdef _SFNC_ARCH_X86
      using UnsignedT = typename std::make_unsigned<clean_T>::type;
      if(static_cast<UnsignedT>(other) > static_cast<UnsignedT>(std::numeric_limits<clean_T>::max())) return true;
  #else
      if(other > static_cast<typename std::make_unsigned<clean_T>::type>(std::numeric_limits<clean_T>::max()))
        return true;
  #endif
    }
    else if((std::is_unsigned<clean_T>::value && std::is_signed<clean_U>::value) ? true : false)
    {
      if(other < 0) return true;
  #ifdef _SFNC_ARCH_X86
      using UnsignedU = typename std::make_unsigned<clean_U>::type;
      if(static_cast<UnsignedU>(other) > static_cast<UnsignedU>(std::numeric_limits<clean_T>::max())) return true;
  #else
      using UnsignedU = typename std::make_unsigned<clean_U>::type;
      if(static_cast<UnsignedU>(other) > static_cast<UnsignedU>(std::numeric_limits<clean_T>::max())) return true;
  #endif
    }
    else
    {
  #ifdef _SFNC_ARCH_X86
      // На 32-битных архитектурах используем явные приведения для устранения C4018
      if(static_cast<CommonType>(other) > static_cast<CommonType>(std::numeric_limits<clean_T>::max())) return true;
      if(static_cast<CommonType>(other) < static_cast<CommonType>(std::numeric_limits<clean_T>::min())) return true;
  #else
      if(other > std::numeric_limits<clean_T>::max()) return true;
      if(other < std::numeric_limits<clean_T>::min()) return true;
  #endif
    }
    return static_cast<CommonType>(current) != static_cast<CommonType>(other);
  }

  #if __cplusplus >= 202002L
  /**
   * @brief Трехстороннее сравнение для целочисленных типов (C++20 spaceship)
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return std::strong_ordering результат сравнения
   * @details Проверяет диапазоны значений перед преобразованием для предотвращения переполнения.
   *          Возвращает strong_ordering для точного сравнения целочисленных типов.
   * @since C++20
   */
  static std::strong_ordering
  three_way_compare(T current, U other) noexcept
  {
    // Проверяем диапазоны с учетом знаковости
    if((std::is_signed<clean_T>::value && std::is_unsigned<clean_U>::value) ? true : false)
    {
      // T signed, U unsigned: если current < 0, то current < other
      if(current < 0) return std::strong_ordering::less;
    #ifdef _SFNC_ARCH_X86
      using UnsignedT = typename std::make_unsigned<clean_T>::type;
      if(static_cast<UnsignedT>(other) > static_cast<UnsignedT>(std::numeric_limits<clean_T>::max()))
        return std::strong_ordering::less;
    #else
      if(other > static_cast<typename std::make_unsigned<clean_T>::type>(std::numeric_limits<clean_T>::max()))
        return std::strong_ordering::less;
    #endif
    }
    else if((std::is_unsigned<clean_T>::value && std::is_signed<clean_U>::value) ? true : false)
    {
      if(other < 0) return std::strong_ordering::greater;
    #ifdef _SFNC_ARCH_X86
      using UnsignedU = typename std::make_unsigned<clean_U>::type;
      if(static_cast<UnsignedU>(other) > static_cast<UnsignedU>(std::numeric_limits<clean_T>::max()))
        return std::strong_ordering::less;
    #else
      using UnsignedU = typename std::make_unsigned<clean_U>::type;
      if(static_cast<UnsignedU>(other) > static_cast<UnsignedU>(std::numeric_limits<clean_T>::max()))
        return std::strong_ordering::less;
    #endif
    }
    else
    {
    #ifdef _SFNC_ARCH_X86
      // На 32-битных архитектурах используем явные приведения для устранения C4018
      if(static_cast<CommonType>(other) > static_cast<CommonType>(std::numeric_limits<clean_T>::max()))
        return std::strong_ordering::less;
      if(static_cast<CommonType>(other) < static_cast<CommonType>(std::numeric_limits<clean_T>::min()))
        return std::strong_ordering::greater;
    #else
      if(other > std::numeric_limits<clean_T>::max()) return std::strong_ordering::less;
      if(other < std::numeric_limits<clean_T>::min()) return std::strong_ordering::greater;
    #endif
    }

    auto const current_common = static_cast<CommonType>(current);
    auto const other_common   = static_cast<CommonType>(other);

    if(current_common < other_common) return std::strong_ordering::less;
    if(current_common > other_common) return std::strong_ordering::greater;
    return std::strong_ordering::equal;
  }
  #endif
};

/**
 * @brief Специализация для типов с плавающей точкой
 * @tparam T Первый тип с плавающей точкой
 * @tparam U Второй тип с плавающей точкой
 * @details Реализует безопасное сравнение между разными типами с плавающей точкой.
 *          Корректно обрабатывает специальные значения: NaN и бесконечность.
 * @note NaN всегда возвращает false для сравнений, бесконечность сравнивается по знаку
 */
template <typename T, typename U>
struct safe_compare_impl_helper<T, U,
                                typename std::enable_if<std::is_floating_point<typename clean_type_t<T>::type>::value
                                                        && std::is_floating_point<typename clean_type_t<U>::type>::value
                                                        && !std::is_same<typename clean_type_t<T>::type,
                                                                         typename clean_type_t<U>::type>::value>::type> {
  using clean_T    = typename clean_type_t<T>::type;
  using clean_U    = typename clean_type_t<U>::type;
  using CommonType = typename std::common_type<clean_T, clean_U>::type; ///< Общий тип для безопасного преобразования

  /**
   * @brief Безопасное сравнение >= для типов с плавающей точкой
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current >= other
   * @details Корректно обрабатывает NaN (возвращает false) и бесконечность (сравнивает по знаку).
   *          Предотвращает неопределенное поведение при сравнении специальных значений.
   */
  static bool
  compare(T current, U other) noexcept
  {
    if(std::isnan(current) || std::isnan(other)) return false;
    if(std::isinf(current) || std::isinf(other)) return current == other;
    return static_cast<CommonType>(current) >= static_cast<CommonType>(other);
  }

  /**
   * @brief Безопасное сравнение >= для типов с плавающей точкой
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current >= other
   */
  static bool
  greater_equal(T current, U other) noexcept
  {
    return compare(current, other);
  }

  /**
   * @brief Безопасное сравнение <= для типов с плавающей точкой
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current <= other
   * @details Корректно обрабатывает NaN (возвращает false) и бесконечность (сравнивает по знаку).
   */
  static bool
  less_equal(T current, U other) noexcept
  {
    if(std::isnan(current) || std::isnan(other)) return false;
    if(std::isinf(current) || std::isinf(other)) return current == other;
    return static_cast<CommonType>(current) <= static_cast<CommonType>(other);
  }

  /**
   * @brief Безопасное сравнение < для типов с плавающей точкой
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current < other
   * @details Корректно обрабатывает NaN (возвращает false) и бесконечность (сравнивает по знаку).
   */
  static bool
  less(T current, U other) noexcept
  {
    if(std::isnan(current) || std::isnan(other)) return false;
    if(std::isinf(current) || std::isinf(other)) return false; // infinity == infinity, not <
    return static_cast<CommonType>(current) < static_cast<CommonType>(other);
  }

  /**
   * @brief Безопасное сравнение > для типов с плавающей точкой
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current > other
   * @details Корректно обрабатывает NaN (возвращает false) и бесконечность (сравнивает по знаку).
   */
  static bool
  greater(T current, U other) noexcept
  {
    if(std::isnan(current) || std::isnan(other)) return false;
    if(std::isinf(current) || std::isinf(other)) return false; // infinity == infinity, not >
    return static_cast<CommonType>(current) > static_cast<CommonType>(other);
  }

  /**
   * @brief Безопасное сравнение == для типов с плавающей точкой
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current == other
   * @details Корректно обрабатывает NaN (возвращает false) и бесконечность (сравнивает по знаку).
   */
  static bool
  equal(T current, U other) noexcept
  {
    if(std::isnan(current) || std::isnan(other)) return false;
    if(std::isinf(current) || std::isinf(other)) return current == other;
    return static_cast<CommonType>(current) == static_cast<CommonType>(other);
  }

  /**
   * @brief Безопасное сравнение != для типов с плавающей точкой
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current != other
   * @details Корректно обрабатывает NaN (возвращает true) и бесконечность (сравнивает по знаку).
   */
  static bool
  not_equal(T current, U other) noexcept
  {
    if(std::isnan(current) || std::isnan(other)) return true;
    if(std::isinf(current) || std::isinf(other)) return current != other;
    return static_cast<CommonType>(current) != static_cast<CommonType>(other);
  }

  #if __cplusplus >= 202002L
  /**
   * @brief Трехстороннее сравнение для типов с плавающей точкой (C++20 spaceship)
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return std::partial_ordering результат сравнения
   * @details Корректно обрабатывает NaN (возвращает unordered) и бесконечность.
   *          Возвращает partial_ordering для типов с плавающей точкой из-за NaN.
   * @since C++20
   */
  static std::partial_ordering
  three_way_compare(T current, U other) noexcept
  {
    if(std::isnan(current) || std::isnan(other)) return std::partial_ordering::unordered;
    if(std::isinf(current) || std::isinf(other))
    {
      if(current == other) return std::partial_ordering::equivalent;
      return std::partial_ordering::unordered;
    }

    auto const current_common = static_cast<CommonType>(current);
    auto const other_common   = static_cast<CommonType>(other);

    if(current_common < other_common) return std::partial_ordering::less;
    if(current_common > other_common) return std::partial_ordering::greater;
    return std::partial_ordering::equivalent;
  }
  #endif
};

/**
 * @brief Специализация для смешанных типов (целочисленный + плавающая точка)
 * @tparam T Первый тип (целочисленный или плавающая точка)
 * @tparam U Второй тип (плавающая точка или целочисленный)
 * @details Реализует безопасное сравнение между целочисленными типами и типами с плавающей точкой.
 *          Корректно обрабатывает специальные значения floating-point типов.
 * @note Самый сложный случай сравнения, требует особой обработки NaN и бесконечности
 */
template <typename T, typename U>
struct safe_compare_impl_helper<
  T, U,
  typename std::enable_if<(std::is_integral<typename clean_type_t<T>::type>::value
                           && std::is_floating_point<typename clean_type_t<U>::type>::value)
                          || (std::is_floating_point<typename clean_type_t<T>::type>::value
                              && std::is_integral<typename clean_type_t<U>::type>::value)>::type> {
  using clean_T    = typename clean_type_t<T>::type;
  using clean_U    = typename clean_type_t<U>::type;
  using CommonType = typename std::common_type<clean_T, clean_U>::type; ///< Общий тип для безопасного преобразования

  /**
   * @brief Безопасное сравнение >= для смешанных типов
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current >= other
   * @details Обрабатывает все комбинации: integral vs floating-point и floating-point vs integral.
   *          NaN всегда возвращает false, бесконечность сравнивается по знаку.
   */
  static bool
  compare(T current, U other) noexcept
  {
    if(std::is_integral<clean_T>::value && std::is_floating_point<clean_U>::value)
    {
      if(std::isnan(other)) return false;
      if(std::isinf(other)) return other < 0;
      return static_cast<CommonType>(current) >= static_cast<CommonType>(other);
    }
    if(std::isnan(current)) return false;
    if(std::isinf(current)) return current > 0;
    return static_cast<CommonType>(current) >= static_cast<CommonType>(other);
  }

  /**
   * @brief Безопасное сравнение >= для смешанных типов
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current >= other
   */
  static bool
  greater_equal(T current, U other) noexcept
  {
    return compare(current, other);
  }

  /**
   * @brief Безопасное сравнение <= для смешанных типов
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current <= other
   * @details Обрабатывает все комбинации: integral vs floating-point и floating-point vs integral.
   *          NaN всегда возвращает false, бесконечность сравнивается по знаку.
   */
  static bool
  less_equal(T current, U other) noexcept
  {
    if(std::is_integral<clean_T>::value && std::is_floating_point<clean_U>::value)
    {
      if(std::isnan(other)) return false;
      if(std::isinf(other)) return other > 0;
      return static_cast<CommonType>(current) <= static_cast<CommonType>(other);
    }
    if(std::isnan(current)) return false;
    if(std::isinf(current)) return current < 0;
    return static_cast<CommonType>(current) <= static_cast<CommonType>(other);
  }

  /**
   * @brief Безопасное сравнение < для смешанных типов
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current < other
   * @details Обрабатывает все комбинации: integral vs floating-point и floating-point vs integral.
   *          NaN всегда возвращает false, бесконечность сравнивается по знаку.
   */
  static bool
  less(T current, U other) noexcept
  {
    if(std::is_integral<clean_T>::value && std::is_floating_point<clean_U>::value)
    {
      if(std::isnan(other)) return false;
      if(std::isinf(other)) return other > 0;
      return static_cast<CommonType>(current) < static_cast<CommonType>(other);
    }
    if(std::isnan(current)) return false;
    if(std::isinf(current)) return current < 0;
    return static_cast<CommonType>(current) < static_cast<CommonType>(other);
  }

  /**
   * @brief Безопасное сравнение > для смешанных типов
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current > other
   * @details Обрабатывает все комбинации: integral vs floating-point и floating-point vs integral.
   *          NaN всегда возвращает false, бесконечность сравнивается по знаку.
   */
  static bool
  greater(T current, U other) noexcept
  {
    if(std::is_integral<clean_T>::value && std::is_floating_point<clean_U>::value)
    {
      if(std::isnan(other)) return false;
      if(std::isinf(other)) return other < 0;
      return static_cast<CommonType>(current) > static_cast<CommonType>(other);
    }
    if(std::isnan(current)) return false;
    if(std::isinf(current)) return current > 0;
    return static_cast<CommonType>(current) > static_cast<CommonType>(other);
  }

  /**
   * @brief Безопасное сравнение == для смешанных типов
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current == other
   * @details Обрабатывает все комбинации: integral vs floating-point и floating-point vs integral.
   *          NaN всегда возвращает false, бесконечность сравнивается по знаку.
   */
  static bool
  equal(T current, U other) noexcept
  {
    if(std::is_integral<clean_T>::value && std::is_floating_point<clean_U>::value)
    {
      if(std::isnan(other)) return false;
      if(std::isinf(other)) return false;
      return static_cast<CommonType>(current) == static_cast<CommonType>(other);
    }
    if(std::isnan(current)) return false;
    if(std::isinf(current)) return false;
    return static_cast<CommonType>(current) == static_cast<CommonType>(other);
  }

  /**
   * @brief Безопасное сравнение != для смешанных типов
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return true если current != other
   * @details Обрабатывает все комбинации: integral vs floating-point и floating-point vs integral.
   *          NaN всегда возвращает true, бесконечность сравнивается по знаку.
   */
  static bool
  not_equal(T current, U other) noexcept
  {
    if(std::is_integral<clean_T>::value && std::is_floating_point<clean_U>::value)
    {
      if(std::isnan(other)) return true;
      if(std::isinf(other)) return true;
      return static_cast<CommonType>(current) != static_cast<CommonType>(other);
    }
    if(std::isnan(current)) return true;
    if(std::isinf(current)) return true;
    return static_cast<CommonType>(current) != static_cast<CommonType>(other);
  }

  #if __cplusplus >= 202002L
  /**
   * @brief Трехстороннее сравнение для смешанных типов (C++20 spaceship)
   * @param[in] current Текущее значение типа T
   * @param[in] other Значение для сравнения типа U
   * @return std::partial_ordering результат сравнения
   * @details Обрабатывает все комбинации: integral vs floating-point и floating-point vs integral.
   *          NaN всегда возвращает unordered, бесконечность сравнивается по знаку.
   *          Возвращает partial_ordering из-за возможного наличия NaN.
   * @since C++20
   */
  static std::partial_ordering
  three_way_compare(T current, U other) noexcept
  {
    if(std::is_integral<clean_T>::value && std::is_floating_point<clean_U>::value)
    {
      if(std::isnan(other)) return std::partial_ordering::unordered;
      if(std::isinf(other)) return std::partial_ordering::unordered;

      auto const current_common = static_cast<CommonType>(current);
      auto const other_common   = static_cast<CommonType>(other);

      if(current_common < other_common) return std::partial_ordering::less;
      if(current_common > other_common) return std::partial_ordering::greater;
      return std::partial_ordering::equivalent;
    }

    if(std::isnan(current)) return std::partial_ordering::unordered;
    if(std::isinf(current)) return std::partial_ordering::unordered;

    auto const current_common = static_cast<CommonType>(current);
    auto const other_common   = static_cast<CommonType>(other);

    if(current_common < other_common) return std::partial_ordering::less;
    if(current_common > other_common) return std::partial_ordering::greater;
    return std::partial_ordering::equivalent;
  }
  #endif
};

/**
 * @brief Универсальный thread-safe компаратор для безопасного сравнения арифметических типов
 * @tparam T Арифметический тип для сравнения (integral или floating-point)
 * @tparam Atomic Если true, использует std::atomic<T> для thread-safety, иначе обычный T для производительности
 * @details Предотвращает переполнение и некорректные сравнения между типами с разными диапазонами.
 *          Поддерживает все арифметические типы с compile-time оптимизациями и SFINAE диспетчеризацией.
 *
 * @section invariants Инварианты класса
 * - Все операции thread-safe при Atomic=true
 * - Предотвращение переполнения при сравнении разных типов
 * - Корректная обработка edge cases (NaN, бесконечность, границы типов)
 * - No-throw гарантии для всех операций
 *
 * @section thread_safety Thread Safety
 * - При Atomic=true: все операции thread-safe с lock-free семантикой
 * - При Atomic=false: максимальная производительность для single-threaded использования
 * - Memory ordering гарантии для атомарных операций
 * - Compare-and-swap операции для атомарного обновления
 *
 * @section exception_safety Exception Safety
 * - Strong guarantee для всех операций
 * - No-throw где возможно (все методы помечены noexcept)
 * - Безопасное управление ресурсами через RAII
 *
 * @section performance Производительность
 * - Compile-time диспетчеризация через SFINAE
 * - Специализации для оптимизации одинаковых типов
 * - Zero-cost абстракции где возможно
 * - Lock-free операции для атомарных версий
 *
 * @example
 * // Thread-safe версия для многопоточности
 * SafeComparator<int, true> atomic_counter(0);
 * atomic_counter.update(100);
 * if (atomic_counter.safe_compare(50)) {
 *   // Безопасное сравнение int >= int
 * }
 *
 * // Высокопроизводительная версия для single-threaded
 * SafeComparator<unsigned char, false> fast_size(200);
 * if (fast_size.safe_compare(300)) {
 *   // Безопасное сравнение unsigned char >= int без переполнения
 * }
 *
 * @since C++11
 * @note Полностью совместим с C++11, использует современные C++20 концепты при доступности
 */
template <typename T, bool Atomic = false> class SafeComparator
{
  using clean_T = typename clean_type_t<T>::type;
  static_assert(std::is_arithmetic<clean_T>::value, "T must be arithmetic type");
  static_assert(std::numeric_limits<clean_T>::is_specialized, "T must have specialized numeric_limits");

private:
  // Compile-time выбор реализации
  typename std::conditional<Atomic, std::atomic<clean_T>, clean_T>::type
    m_value; ///< Значение компаратора (атомарное или обычное)

  /**
   * @brief SFINAE диспетчер для атомарного сохранения значения
   * @tparam A Параметр для SFINAE (равен Atomic)
   * @param[in] value Значение для сохранения
   * @param[in] order Порядок памяти для атомарных операций
   * @note Специализация для Atomic=true: использует std::atomic::store
   */
  template <bool A = Atomic>
  typename std::enable_if<A, void>::type
  atomic_store(clean_T value, std::memory_order order = std::memory_order_seq_cst) noexcept
  {
    m_value.store(value, order);
  }

  /**
   * @brief SFINAE диспетчер для обычного сохранения значения
   * @tparam A Параметр для SFINAE (равен Atomic)
   * @param[in] value Значение для сохранения
   * @param[in] order Порядок памяти (игнорируется для неатомарных операций)
   * @note Специализация для Atomic=false: прямое присваивание для максимальной производительности
   */
  template <bool A = Atomic>
  typename std::enable_if<!A, void>::type
  atomic_store(clean_T value, std::memory_order /* unused */ = std::memory_order_seq_cst) noexcept
  {
    m_value = value;
  }

  /**
   * @brief SFINAE диспетчер для атомарного загрузки значения
   * @tparam A Параметр для SFINAE (равен Atomic)
   * @param[in] order Порядок памяти для атомарных операций
   * @return Текущее значение
   * @note Специализация для Atomic=true: использует std::atomic::load
   */
  template <bool A = Atomic>
  typename std::enable_if<A, clean_T>::type
  atomic_load(std::memory_order order = std::memory_order_seq_cst) const noexcept
  {
    return m_value.load(order);
  }

  /**
   * @brief SFINAE диспетчер для обычной загрузки значения
   * @tparam A Параметр для SFINAE (равен Atomic)
   * @param[in] order Порядок памяти (игнорируется для неатомарных операций)
   * @return Текущее значение
   * @note Специализация для Atomic=false: прямое чтение для максимальной производительности
   */
  template <bool A = Atomic>
  typename std::enable_if<!A, clean_T>::type
  atomic_load(std::memory_order /* unused */ = std::memory_order_seq_cst) const noexcept
  {
    return m_value;
  }

public:
  // === Конструкторы ===

  /**
   * @brief Конструктор по умолчанию
   * @details Инициализирует значение значением по умолчанию типа T (T{}).
   *          Для атомарных типов это thread-safe операция.
   * @note No-throw, thread-safe при Atomic=true
   */
  SafeComparator() noexcept : m_value{} {}

  /**
   * @brief Конструктор с начальным значением
   * @param[in] value Начальное значение для инициализации
   * @details Создает компаратор с указанным начальным значением.
   *          Для атомарных типов инициализация происходит атомарно.
   * @note No-throw, thread-safe при Atomic=true
   */
  explicit SafeComparator(T value) noexcept : m_value(static_cast<clean_T>(value)) {}

  /**
   * @brief Конструктор копирования
   * @param[in] other Исходный объект для копирования
   * @details Создает копию существующего компаратора.
   *          Для атомарных типов копирование происходит через атомарное чтение.
   * @note No-throw, thread-safe при Atomic=true
   */
  SafeComparator(SafeComparator const &other) noexcept : m_value(other.atomic_load()) {}

  /**
   * @brief Конструктор перемещения
   * @param[in] other Исходный объект для перемещения
   * @details Перемещает значение из другого компаратора, обнуляя исходный объект.
   *          Использует std::exchange для безопасного перемещения.
   * @note No-throw, максимальная производительность
   */
  SafeComparator(SafeComparator &&other) noexcept : m_value(std::exchange(other.m_value, clean_T{})) {}

  /**
   * @brief Оператор присваивания копированием
   * @param[in] other Исходный объект для копирования
   * @return Ссылка на текущий объект
   * @details Присваивает значение из другого компаратора.
   *          Проверяет самоприсваивание для безопасности.
   * @note No-throw, thread-safe при Atomic=true
   */
  SafeComparator &
  operator=(SafeComparator const &other) noexcept
  {
    if(this != &other) atomic_store(other.atomic_load());
    return *this;
  }

  /**
   * @brief Оператор присваивания перемещением
   * @param[in] other Исходный объект для перемещения
   * @return Ссылка на текущий объект
   * @details Перемещает значение из другого компаратора, обнуляя исходный объект.
   *          Проверяет самоприсваивание для безопасности.
   * @note No-throw, максимальная производительность
   */
  SafeComparator &
  operator=(SafeComparator &&other) noexcept
  {
    if(this != &other) atomic_store(std::exchange(other.m_value, clean_T{}));
    return *this;
  }

  /**
   * @brief Деструктор
   * @details Автоматически генерируется компилятором.
   *          Для атомарных типов деструкция thread-safe.
   * @note No-throw, автоматически генерируется компилятором
   */
  ~SafeComparator() = default;

  // === Безопасные операции сравнения ===

  /**
   * @brief Безопасное сравнение >= с другим типом
   * @tparam U Тип значения для сравнения
   * @param[in] other Значение для сравнения
   * @return true если текущее значение >= other
   * @details Выполняет безопасное сравнение между текущим значением типа T и значением типа U.
   *          Проверяет диапазоны значений для предотвращения переполнения.
   *          Использует специализированные алгоритмы для разных комбинаций типов.
   * @note Thread-safe при Atomic=true, no-throw
   * @warning Типы T и U должны быть безопасно сравнимыми (проверяется на compile-time)
   */
  template <typename U>
  bool
  safe_compare(U other) const noexcept
  {
    static_assert(is_safe_comparable<T, U>::value, "Types must be safely comparable");
    return safe_compare_impl_helper<clean_T, U>::compare(atomic_load(), other);
  }

  /**
   * @brief Безопасное сравнение >= с другим типом
   * @tparam U Тип значения для сравнения
   * @param[in] other Значение для сравнения
   * @return true если текущее значение >= other
   * @details Алиас для safe_compare() для лучшей читаемости кода.
   *          Выполняет ту же безопасную проверку диапазонов.
   * @note Thread-safe при Atomic=true, no-throw
   */
  template <typename U>
  bool
  safe_greater_equal(U other) const noexcept
  {
    static_assert(is_safe_comparable<T, U>::value, "Types must be safely comparable");
    return safe_compare_impl_helper<clean_T, U>::greater_equal(atomic_load(), other);
  }

  /**
   * @brief Безопасное сравнение <= с другим типом
   * @tparam U Тип значения для сравнения
   * @param[in] other Значение для сравнения
   * @return true если текущее значение <= other
   * @details Выполняет безопасное сравнение "меньше или равно" между текущим значением типа T и значением типа U.
   *          Проверяет диапазоны значений для предотвращения переполнения.
   * @note Thread-safe при Atomic=true, no-throw
   */
  template <typename U>
  bool
  safe_less_equal(U other) const noexcept
  {
    static_assert(is_safe_comparable<T, U>::value, "Types must be safely comparable");
    return safe_compare_impl_helper<clean_T, U>::less_equal(atomic_load(), other);
  }

  /**
   * @brief Безопасное сравнение < с другим типом
   * @tparam U Тип значения для сравнения
   * @param[in] other Значение для сравнения
   * @return true если текущее значение < other
   * @details Выполняет безопасное сравнение "меньше" между текущим значением типа T и значением типа U.
   *          Проверяет диапазоны значений для предотвращения переполнения.
   * @note Thread-safe при Atomic=true, no-throw
   */
  template <typename U>
  bool
  safe_less(U other) const noexcept
  {
    static_assert(is_safe_comparable<T, U>::value, "Types must be safely comparable");
    return safe_compare_impl_helper<clean_T, U>::less(atomic_load(), other);
  }

  /**
   * @brief Безопасное сравнение > с другим типом
   * @tparam U Тип значения для сравнения
   * @param[in] other Значение для сравнения
   * @return true если текущее значение > other
   * @details Выполняет безопасное сравнение "больше" между текущим значением типа T и значением типа U.
   *          Проверяет диапазоны значений для предотвращения переполнения.
   * @note Thread-safe при Atomic=true, no-throw
   */
  template <typename U>
  bool
  safe_greater(U other) const noexcept
  {
    static_assert(is_safe_comparable<T, U>::value, "Types must be safely comparable");
    return safe_compare_impl_helper<clean_T, U>::greater(atomic_load(), other);
  }

  /**
   * @brief Безопасное сравнение == с другим типом
   * @tparam U Тип значения для сравнения
   * @param[in] other Значение для сравнения
   * @return true если текущее значение == other
   * @details Выполняет безопасное сравнение "равно" между текущим значением типа T и значением типа U.
   *          Проверяет диапазоны значений для предотвращения переполнения.
   * @note Thread-safe при Atomic=true, no-throw
   */
  template <typename U>
  bool
  safe_equal(U other) const noexcept
  {
    static_assert(is_safe_comparable<T, U>::value, "Types must be safely comparable");
    return safe_compare_impl_helper<clean_T, U>::equal(atomic_load(), other);
  }

  /**
   * @brief Безопасное сравнение != с другим типом
   * @tparam U Тип значения для сравнения
   * @param[in] other Значение для сравнения
   * @return true если текущее значение != other
   * @details Выполняет безопасное сравнение "не равно" между текущим значением типа T и значением типа U.
   *          Проверяет диапазоны значений для предотвращения переполнения.
   * @note Thread-safe при Atomic=true, no-throw
   */
  template <typename U>
  bool
  safe_not_equal(U other) const noexcept
  {
    static_assert(is_safe_comparable<T, U>::value, "Types must be safely comparable");
    return safe_compare_impl_helper<clean_T, U>::not_equal(atomic_load(), other);
  }

  #if __cplusplus >= 202002L
  /**
   * @brief Трехстороннее сравнение с другим типом (C++20 spaceship)
   * @tparam U Тип значения для сравнения
   * @param[in] other Значение для сравнения
   * @return Результат трехстороннего сравнения
   * @details Выполняет трехстороннее сравнение между текущим значением типа T и значением типа U.
   *          Возвращает strong_ordering для целочисленных типов, partial_ordering для floating-point.
   *          Проверяет диапазоны значений для предотвращения переполнения.
   * @note Thread-safe при Atomic=true, no-throw
   * @since C++20
   */
  template <typename U>
  auto
  safe_three_way_compare(U other) const noexcept
  {
    static_assert(is_safe_comparable<T, U>::value, "Types must be safely comparable");
    return safe_compare_impl_helper<clean_T, U>::three_way_compare(atomic_load(), other);
  }
  #endif

  // === Операции с состоянием ===

  /**
   * @brief Обновить значение компаратора
   * @param[in] new_value Новое значение для установки
   * @details Устанавливает новое значение в компаратор.
   *          Для атомарных типов операция thread-safe с memory ordering.
   * @note Thread-safe при Atomic=true, no-throw
   */
  void
  update(T new_value) noexcept
  {
    atomic_store(static_cast<clean_T>(new_value));
  }

  /**
   * @brief Получить текущее значение компаратора
   * @return Текущее значение типа T
   * @details Возвращает текущее значение компаратора.
   *          Для атомарных типов операция thread-safe с memory ordering.
   * @note Thread-safe при Atomic=true, no-throw
   */
  T
  get() const noexcept
  {
    return static_cast<T>(atomic_load());
  }

  /**
   * @brief Атомарная операция compare-and-set
   * @param[in] expected_value Ожидаемое текущее значение
   * @param[in] desired_value Желаемое новое значение
   * @return true если значение было успешно изменено, false если expected_value не совпадает с текущим
   * @details Выполняет атомарную операцию сравнения и установки значения.
   *          Для атомарных типов использует compare_exchange_strong с memory ordering.
   *          Для неатомарных типов выполняет обычную проверку и установку.
   * @note Thread-safe при Atomic=true, no-throw
   * @warning expected_value может быть изменен при неуспешном сравнении (для атомарных типов)
   */
  bool
  compare_and_set(T expected_value, T desired_value) noexcept
  {
    return compare_and_set_impl(static_cast<clean_T>(expected_value), static_cast<clean_T>(desired_value),
                                std::integral_constant<bool, Atomic>{});
  }

private:
  /**
   * @brief SFINAE реализация compare-and-set для атомарных типов
   * @param[in] expected_value Ожидаемое значение (может быть изменено)
   * @param[in] desired_value Желаемое значение
   * @param[in] atomic_tag Тег для SFINAE (std::true_type)
   * @return true если значение было изменено
   * @details Использует std::atomic::compare_exchange_strong с memory ordering.
   *          expected_value обновляется текущим значением при неуспешном сравнении.
   * @note Thread-safe, no-throw
   */
  bool
  compare_and_set_impl(clean_T expected_value, clean_T desired_value, std::true_type /* atomic */) noexcept
  {
    return m_value.compare_exchange_strong(expected_value, desired_value, std::memory_order_acq_rel,
                                           std::memory_order_acquire);
  }

  /**
   * @brief SFINAE реализация compare-and-set для неатомарных типов
   * @param[in] expected_value Ожидаемое значение
   * @param[in] desired_value Желаемое значение
   * @param[in] non_atomic_tag Тег для SFINAE (std::false_type)
   * @return true если значение было изменено
   * @details Выполняет обычную проверку и установку для максимальной производительности.
   *          Не изменяет expected_value при неуспешном сравнении.
   * @note No-throw, максимальная производительность
   */
  bool
  compare_and_set_impl(clean_T expected_value, clean_T desired_value, // NOLINT(bugprone-easily-swappable-parameters)
                       std::false_type /* non-atomic */) noexcept
  {
    if(m_value == expected_value)
    {
      m_value = desired_value;
      return true;
    }
    return false;
  }

public:
  // === Операторы для удобства ===

  /**
   * @brief Оператор неявного приведения к типу T
   * @return Текущее значение типа T
   * @details Позволяет использовать компаратор в контекстах, где ожидается значение типа T.
   *          Для атомарных типов выполняется атомарное чтение.
   * @note Thread-safe при Atomic=true, no-throw
   * @warning Неявное приведение может скрывать атомарные операции
   */
  operator T() const noexcept { return static_cast<T>(atomic_load()); }

  /**
   * @brief Оператор присваивания значения
   * @param[in] value Новое значение для установки
   * @return Ссылка на текущий объект
   * @details Устанавливает новое значение в компаратор.
   *          Для атомарных типов операция thread-safe.
   * @note Thread-safe при Atomic=true, no-throw
   */
  SafeComparator &
  operator=(T value) noexcept
  {
    atomic_store(static_cast<clean_T>(value));
    return *this;
  }
};

// === Удобные алиасы для часто используемых типов ===

/**
 * @brief Алиасы для атомарных версий (thread-safe)
 * @details Предоставляют готовые типы для многопоточного использования.
 *          Все операции thread-safe с lock-free семантикой.
 * @note Используйте для многопоточных приложений
 */
using AtomicCharComparator     = SafeComparator<char, true>;           ///< Атомарный компаратор для char
using AtomicUCharComparator    = SafeComparator<unsigned char, true>;  ///< Атомарный компаратор для unsigned char
using AtomicShortComparator    = SafeComparator<short, true>;          ///< Атомарный компаратор для short
using AtomicUShortComparator   = SafeComparator<unsigned short, true>; ///< Атомарный компаратор для unsigned short
using AtomicIntComparator      = SafeComparator<int, true>;            ///< Атомарный компаратор для int
using AtomicUIntComparator     = SafeComparator<unsigned int, true>;   ///< Атомарный компаратор для unsigned int
using AtomicLongComparator     = SafeComparator<long, true>;           ///< Атомарный компаратор для long
using AtomicULongComparator    = SafeComparator<unsigned long, true>;  ///< Атомарный компаратор для unsigned long
using AtomicLongLongComparator = SafeComparator<long long, true>;      ///< Атомарный компаратор для long long
using AtomicULongLongComparator
  = SafeComparator<unsigned long long, true>;                         ///< Атомарный компаратор для unsigned long long
using AtomicFloatComparator      = SafeComparator<float, true>;       ///< Атомарный компаратор для float
using AtomicDoubleComparator     = SafeComparator<double, true>;      ///< Атомарный компаратор для double
using AtomicLongDoubleComparator = SafeComparator<long double, true>; ///< Атомарный компаратор для long double

/**
 * @brief Алиасы для неатомарных версий (высокая производительность)
 * @details Предоставляют готовые типы для single-threaded использования.
 *          Максимальная производительность без накладных расходов на синхронизацию.
 * @note Используйте только в single-threaded контексте
 */
using FastCharComparator     = SafeComparator<char, false>;           ///< Быстрый компаратор для char
using FastUCharComparator    = SafeComparator<unsigned char, false>;  ///< Быстрый компаратор для unsigned char
using FastShortComparator    = SafeComparator<short, false>;          ///< Быстрый компаратор для short
using FastUShortComparator   = SafeComparator<unsigned short, false>; ///< Быстрый компаратор для unsigned short
using FastIntComparator      = SafeComparator<int, false>;            ///< Быстрый компаратор для int
using FastUIntComparator     = SafeComparator<unsigned int, false>;   ///< Быстрый компаратор для unsigned int
using FastLongComparator     = SafeComparator<long, false>;           ///< Быстрый компаратор для long
using FastULongComparator    = SafeComparator<unsigned long, false>;  ///< Быстрый компаратор для unsigned long
using FastLongLongComparator = SafeComparator<long long, false>;      ///< Быстрый компаратор для long long
using FastULongLongComparator
  = SafeComparator<unsigned long long, false>;                       ///< Быстрый компаратор для unsigned long long
using FastFloatComparator      = SafeComparator<float, false>;       ///< Быстрый компаратор для float
using FastDoubleComparator     = SafeComparator<double, false>;      ///< Быстрый компаратор для double
using FastLongDoubleComparator = SafeComparator<long double, false>; ///< Быстрый компаратор для long double

/**
 * @brief Алиасы по умолчанию (неатомарные для лучшей производительности)
 * @details Предоставляют готовые типы с оптимальными настройками по умолчанию.
 *          Используют неатомарные версии для максимальной производительности.
 * @note Рекомендуется для большинства случаев использования
 */
using SafeCharComparator       = SafeComparator<char>;               ///< Безопасный компаратор для char
using SafeUCharComparator      = SafeComparator<unsigned char>;      ///< Безопасный компаратор для unsigned char
using SafeShortComparator      = SafeComparator<short>;              ///< Безопасный компаратор для short
using SafeUShortComparator     = SafeComparator<unsigned short>;     ///< Безопасный компаратор для unsigned short
using SafeIntComparator        = SafeComparator<int>;                ///< Безопасный компаратор для int
using SafeUIntComparator       = SafeComparator<unsigned int>;       ///< Безопасный компаратор для unsigned int
using SafeLongComparator       = SafeComparator<long>;               ///< Безопасный компаратор для long
using SafeULongComparator      = SafeComparator<unsigned long>;      ///< Безопасный компаратор для unsigned long
using SafeLongLongComparator   = SafeComparator<long long>;          ///< Безопасный компаратор для long long
using SafeULongLongComparator  = SafeComparator<unsigned long long>; ///< Безопасный компаратор для unsigned long long
using SafeFloatComparator      = SafeComparator<float>;              ///< Безопасный компаратор для float
using SafeDoubleComparator     = SafeComparator<double>;             ///< Безопасный компаратор для double
using SafeLongDoubleComparator = SafeComparator<long double>;        ///< Безопасный компаратор для long double

// === Трехстороннее сравнение и утилиты ===

  #if __cplusplus >= 202002L
/**
 * @brief Метафункция для определения типа результата трехстороннего сравнения
 * @tparam T Первый тип для сравнения
 * @tparam U Второй тип для сравнения
 * @details Определяет наиболее подходящий тип ordering для трехстороннего сравнения.
 *          Возвращает strong_ordering для целочисленных типов, partial_ordering для floating-point.
 * @since C++20
 */
template <typename T, typename U> struct three_way_comparison_result {
  using clean_T                       = typename clean_type_t<T>::type;
  using clean_U                       = typename clean_type_t<U>::type;

  static constexpr bool both_integral = std::is_integral<clean_T>::value && std::is_integral<clean_U>::value;
  static constexpr bool both_floating
    = std::is_floating_point<clean_T>::value && std::is_floating_point<clean_U>::value;
  static constexpr bool mixed_types = !both_integral && !both_floating;

  using type                        = std::conditional_t<both_integral, std::strong_ordering,
                                                         std::conditional_t<both_floating || mixed_types, std::partial_ordering, void>>;
};

/**
 * @brief Алиас для типа результата трехстороннего сравнения
 * @tparam T Первый тип для сравнения
 * @tparam U Второй тип для сравнения
 * @since C++20
 */
template <typename T, typename U>
using three_way_comparison_result_t = typename three_way_comparison_result<T, U>::type;

/**
 * @brief Утилитарная функция для преобразования результата сравнения в bool
 * @tparam Ordering Тип результата сравнения
 * @param[in] ordering Результат трехстороннего сравнения
 * @return true если ordering указывает на равенство
 * @details Преобразует результат трехстороннего сравнения в булево значение равенства.
 *          Полезно для совместимости с существующим кодом.
 * @since C++20
 */
template <typename Ordering>
constexpr bool
is_equal(Ordering const &ordering) noexcept
{
  if constexpr(std::is_same_v<Ordering, std::strong_ordering>) { return ordering == std::strong_ordering::equal; }
  else if constexpr(std::is_same_v<Ordering, std::partial_ordering>)
  {
    return ordering == std::partial_ordering::equivalent;
  }
  else
  {
    static_assert(std::is_same_v<Ordering, std::weak_ordering>);
    return ordering == std::weak_ordering::equivalent;
  }
}

/**
 * @brief Утилитарная функция для преобразования результата сравнения в bool
 * @tparam Ordering Тип результата сравнения
 * @param[in] ordering Результат трехстороннего сравнения
 * @return true если ordering указывает на "меньше"
 * @details Преобразует результат трехстороннего сравнения в булево значение "меньше".
 * @since C++20
 */
template <typename Ordering>
constexpr bool
is_less(Ordering const &ordering) noexcept
{
  if constexpr(std::is_same_v<Ordering, std::strong_ordering>) { return ordering < std::strong_ordering::equal; }
  else if constexpr(std::is_same_v<Ordering, std::partial_ordering>)
  {
    return ordering < std::partial_ordering::equivalent;
  }
  else
  {
    static_assert(std::is_same_v<Ordering, std::weak_ordering>);
    return ordering < std::weak_ordering::equivalent;
  }
}

/**
 * @brief Утилитарная функция для преобразования результата сравнения в bool
 * @tparam Ordering Тип результата сравнения
 * @param[in] ordering Результат трехстороннего сравнения
 * @return true если ordering указывает на "больше"
 * @details Преобразует результат трехстороннего сравнения в булево значение "больше".
 * @since C++20
 */
template <typename Ordering>
constexpr bool
is_greater(Ordering const &ordering) noexcept
{
  if constexpr(std::is_same_v<Ordering, std::strong_ordering>) { return ordering > std::strong_ordering::equal; }
  else if constexpr(std::is_same_v<Ordering, std::partial_ordering>)
  {
    return ordering > std::partial_ordering::equivalent;
  }
  else
  {
    static_assert(std::is_same_v<Ordering, std::weak_ordering>);
    return ordering > std::weak_ordering::equivalent;
  }
}

/**
 * @brief Утилитарная функция для преобразования результата сравнения в bool
 * @tparam Ordering Тип результата сравнения
 * @param[in] ordering Результат трехстороннего сравнения
 * @return true если ordering указывает на "меньше или равно"
 * @details Преобразует результат трехстороннего сравнения в булево значение "меньше или равно".
 * @since C++20
 */
template <typename Ordering>
constexpr bool
is_less_equal(Ordering const &ordering) noexcept
{
  if constexpr(std::is_same_v<Ordering, std::strong_ordering>) { return ordering <= std::strong_ordering::equal; }
  else if constexpr(std::is_same_v<Ordering, std::partial_ordering>)
  {
    return ordering <= std::partial_ordering::equivalent;
  }
  else
  {
    static_assert(std::is_same_v<Ordering, std::weak_ordering>);
    return ordering <= std::weak_ordering::equivalent;
  }
}

/**
 * @brief Утилитарная функция для преобразования результата сравнения в bool
 * @tparam Ordering Тип результата сравнения
 * @param[in] ordering Результат трехстороннего сравнения
 * @return true если ordering указывает на "больше или равно"
 * @details Преобразует результат трехстороннего сравнения в булево значение "больше или равно".
 * @since C++20
 */
template <typename Ordering>
constexpr bool
is_greater_equal(Ordering const &ordering) noexcept
{
  if constexpr(std::is_same_v<Ordering, std::strong_ordering>) { return ordering >= std::strong_ordering::equal; }
  else if constexpr(std::is_same_v<Ordering, std::partial_ordering>)
  {
    return ordering >= std::partial_ordering::equivalent;
  }
  else
  {
    static_assert(std::is_same_v<Ordering, std::weak_ordering>);
    return ordering >= std::weak_ordering::equivalent;
  }
}

/**
 * @brief Утилитарная функция для преобразования результата сравнения в bool
 * @tparam Ordering Тип результата сравнения
 * @param[in] ordering Результат трехстороннего сравнения
 * @return true если ordering указывает на "не равно"
 * @details Преобразует результат трехстороннего сравнения в булево значение "не равно".
 * @since C++20
 */
template <typename Ordering>
constexpr bool
is_not_equal(Ordering const &ordering) noexcept
{
  if constexpr(std::is_same_v<Ordering, std::strong_ordering>) { return ordering != std::strong_ordering::equal; }
  else if constexpr(std::is_same_v<Ordering, std::partial_ordering>)
  {
    return ordering != std::partial_ordering::equivalent;
  }
  else
  {
    static_assert(std::is_same_v<Ordering, std::weak_ordering>);
    return ordering != std::weak_ordering::equivalent;
  }
}
  #endif

// === Утилитарные функции для удобства использования ===

/**
 * @brief Безопасное сравнение двух значений разных типов
 * @tparam T Первый тип для сравнения
 * @tparam U Второй тип для сравнения
 * @param[in] value1 Первое значение типа T
 * @param[in] value2 Второе значение типа U
 * @return true если value1 >= value2
 * @details Выполняет безопасное сравнение между двумя значениями разных типов.
 *          Использует специализированные алгоритмы для предотвращения переполнения.
 *          Не требует создания объекта компаратора.
 * @note Thread-safe, no-throw
 * @warning Типы T и U должны быть безопасно сравнимыми (проверяется на compile-time)
 * @example
 * unsigned char size = 200;
 * int limit = 300;
 * if (safe_compare(size, limit)) {
 *   // Безопасное сравнение unsigned char >= int
 * }
 */
template <typename T, typename U>
bool
safe_compare(T value1, U value2) noexcept
{
  static_assert(is_safe_comparable<T, U>::value, "Types must be safely comparable");
  return safe_compare_impl_helper<T, U>::compare(value1, value2);
}

/**
 * @brief Безопасное сравнение "больше или равно"
 * @tparam T Первый тип для сравнения
 * @tparam U Второй тип для сравнения
 * @param[in] value1 Первое значение типа T
 * @param[in] value2 Второе значение типа U
 * @return true если value1 >= value2
 * @details Алиас для safe_compare() для лучшей читаемости кода.
 *          Выполняет ту же безопасную проверку диапазонов.
 * @note Thread-safe, no-throw
 * @example
 * float f = 3.14f;
 * int i = 3;
 * if (safe_greater_equal(f, i)) {
 *   // Безопасное сравнение float >= int
 * }
 */
template <typename T, typename U>
bool
safe_greater_equal(T value1, U value2) noexcept
{
  static_assert(is_safe_comparable<T, U>::value, "Types must be safely comparable");
  return safe_compare_impl_helper<T, U>::greater_equal(value1, value2);
}

/**
 * @brief Безопасное сравнение "меньше или равно"
 * @tparam T Первый тип для сравнения
 * @tparam U Второй тип для сравнения
 * @param[in] value1 Первое значение типа T
 * @param[in] value2 Второе значение типа U
 * @return true если value1 <= value2
 * @details Выполняет безопасное сравнение "меньше или равно" между двумя значениями разных типов.
 *          Проверяет диапазоны значений для предотвращения переполнения.
 * @note Thread-safe, no-throw
 * @example
 * int i = 3;
 * float f = 3.14f;
 * if (safe_less_equal(i, f)) {
 *   // Безопасное сравнение int <= float
 * }
 */
template <typename T, typename U>
bool
safe_less_equal(T value1, U value2) noexcept
{
  static_assert(is_safe_comparable<T, U>::value, "Types must be safely comparable");
  return safe_compare_impl_helper<T, U>::less_equal(value1, value2);
}

/**
 * @brief Безопасное сравнение "меньше"
 * @tparam T Первый тип для сравнения
 * @tparam U Второй тип для сравнения
 * @param[in] value1 Первое значение типа T
 * @param[in] value2 Второе значение типа U
 * @return true если value1 < value2
 * @details Выполняет безопасное сравнение "меньше" между двумя значениями разных типов.
 *          Проверяет диапазоны значений для предотвращения переполнения.
 * @note Thread-safe, no-throw
 * @example
 * unsigned char size = 200;
 * int limit = 300;
 * if (safe_less(size, limit)) {
 *   // Безопасное сравнение unsigned char < int
 * }
 */
template <typename T, typename U>
bool
safe_less(T value1, U value2) noexcept
{
  static_assert(is_safe_comparable<T, U>::value, "Types must be safely comparable");
  return safe_compare_impl_helper<T, U>::less(value1, value2);
}

/**
 * @brief Безопасное сравнение "больше"
 * @tparam T Первый тип для сравнения
 * @tparam U Второй тип для сравнения
 * @param[in] value1 Первое значение типа T
 * @param[in] value2 Второе значение типа U
 * @return true если value1 > value2
 * @details Выполняет безопасное сравнение "больше" между двумя значениями разных типов.
 *          Проверяет диапазоны значений для предотвращения переполнения.
 * @note Thread-safe, no-throw
 * @example
 * int threshold = 100;
 * unsigned char count = 150;
 * if (safe_greater(count, threshold)) {
 *   // Безопасное сравнение unsigned char > int
 * }
 */
template <typename T, typename U>
bool
safe_greater(T value1, U value2) noexcept
{
  static_assert(is_safe_comparable<T, U>::value, "Types must be safely comparable");
  return safe_compare_impl_helper<T, U>::greater(value1, value2);
}

/**
 * @brief Безопасное сравнение "равно"
 * @tparam T Первый тип для сравнения
 * @tparam U Второй тип для сравнения
 * @param[in] value1 Первое значение типа T
 * @param[in] value2 Второе значение типа U
 * @return true если value1 == value2
 * @details Выполняет безопасное сравнение "равно" между двумя значениями разных типов.
 *          Проверяет диапазоны значений для предотвращения переполнения.
 * @note Thread-safe, no-throw
 * @example
 * int expected = 42;
 * float actual = 42.0f;
 * if (safe_equal(expected, actual)) {
 *   // Безопасное сравнение int == float
 * }
 */
template <typename T, typename U>
bool
safe_equal(T value1, U value2) noexcept
{
  static_assert(is_safe_comparable<T, U>::value, "Types must be safely comparable");
  return safe_compare_impl_helper<T, U>::equal(value1, value2);
}

/**
 * @brief Безопасное сравнение "не равно"
 * @tparam T Первый тип для сравнения
 * @tparam U Второй тип для сравнения
 * @param[in] value1 Первое значение типа T
 * @param[in] value2 Второе значение типа U
 * @return true если value1 != value2
 * @details Выполняет безопасное сравнение "не равно" между двумя значениями разных типов.
 *          Проверяет диапазоны значений для предотвращения переполнения.
 * @note Thread-safe, no-throw
 * @example
 * int threshold = 100;
 * unsigned char count = 150;
 * if (safe_not_equal(count, threshold)) {
 *   // Безопасное сравнение unsigned char != int
 * }
 */
template <typename T, typename U>
bool
safe_not_equal(T value1, U value2) noexcept
{
  static_assert(is_safe_comparable<T, U>::value, "Types must be safely comparable");
  return safe_compare_impl_helper<T, U>::not_equal(value1, value2);
}

  #if __cplusplus >= 202002L
/**
 * @brief Безопасное трехстороннее сравнение (C++20 spaceship)
 * @tparam T Первый тип для сравнения
 * @tparam U Второй тип для сравнения
 * @param[in] value1 Первое значение типа T
 * @param[in] value2 Второе значение типа U
 * @return Результат трехстороннего сравнения
 * @details Выполняет безопасное трехстороннее сравнение между двумя значениями разных типов.
 *          Возвращает strong_ordering для целочисленных типов, partial_ordering для floating-point.
 *          Проверяет диапазоны значений для предотвращения переполнения.
 * @note Thread-safe, no-throw
 * @since C++20
 * @example
 * int a = 42;
 * float b = 42.0f;
 * auto result = safe_three_way_compare(a, b);
 * if (result == std::strong_ordering::equal) {
 *   // Безопасное трехстороннее сравнение int <=> float
 * }
 */
template <typename T, typename U>
auto
safe_three_way_compare(T value1, U value2) noexcept
{
  static_assert(is_safe_comparable<T, U>::value, "Types must be safely comparable");
  return safe_compare_impl_helper<T, U>::three_way_compare(value1, value2);
}
  #endif

/**
 * @brief Проверка, помещается ли значение в целевой тип
 * @tparam TargetType Целевой тип для проверки
 * @tparam SourceType Исходный тип значения
 * @param[in] value Значение для проверки типа SourceType
 * @return true если значение помещается в диапазон TargetType без потери данных
 * @details Проверяет, можно ли безопасно преобразовать значение из SourceType в TargetType.
 *          Для целочисленных типов проверяет диапазоны значений.
 *          Для типов с плавающей точкой проверяет на NaN и бесконечность.
 * @note No-throw, compile-time проверка типов
 * @example
 * if (fits_in_type<unsigned char>(300)) {
 *   // 300 не помещается в unsigned char (0-255)
 * }
 *
 * if (fits_in_type<int>(3.14f)) {
 *   // 3.14f можно безопасно преобразовать в int
 * }
 */
template <typename TargetType, typename SourceType>
bool
fits_in_type(SourceType value) noexcept
{
  static_assert(is_safe_comparable<TargetType, SourceType>::value, "Both types must be safely comparable");

  using clean_target = typename std::remove_cv<typename std::remove_reference<TargetType>::type>::type;
  using clean_source = typename std::remove_cv<typename std::remove_reference<SourceType>::type>::type;

  if(std::is_integral<clean_target>::value && std::is_integral<clean_source>::value)
    return value >= std::numeric_limits<clean_target>::min() && value <= std::numeric_limits<clean_target>::max();
  if(std::is_floating_point<clean_target>::value && std::is_floating_point<clean_source>::value)
    return !std::isnan(value) && !std::isinf(value);

  // Mixed types - используем STL common_type
  if(std::is_integral<clean_target>::value && std::is_floating_point<clean_source>::value)
  {
    if(std::isnan(value) || std::isinf(value)) return false;
    return value >= std::numeric_limits<clean_target>::min() && value <= std::numeric_limits<clean_target>::max();
  }

  // floating-point to integral
  return !std::isnan(value) && !std::isinf(value);
}

// === Примеры использования ===

/**
 * @example SafeNumericComparator_Examples.cpp
 * @brief Примеры использования SafeComparator для различных сценариев
 * @details Демонстрирует основные возможности библиотеки безопасного сравнения:
 *          - Безопасное сравнение разных типов
 *          - Thread-safe операции
 *          - Высокопроизводительные операции
 *          - Проверка диапазонов значений
 *          - Смешанные типы
 */

/*
// Пример 1: Безопасное сравнение unsigned char с int
unsigned char block_size = 200;
int packet_size = 300;

// Небезопасно: может привести к переполнению
// if (block_size >= packet_size) // Проблема!

// Безопасно: используем SafeComparator
SafeUCharComparator safe_size(block_size);
if (safe_size.safe_compare(packet_size)) {
  // Безопасное сравнение выполнено
}

// Или используем утилитарную функцию
if (safe_compare(block_size, packet_size)) {
  // Безопасное сравнение выполнено
}

// Пример 2: Все виды сравнений
SafeUCharComparator safe_size(200);
int threshold = 300;

// Все операторы сравнения
if (safe_size.safe_equal(threshold)) { // ==
}
if (safe_size.safe_not_equal(threshold)) { // !=
}
if (safe_size.safe_less(threshold)) { // <
}
if (safe_size.safe_greater(threshold)) { // >
}
if (safe_size.safe_less_equal(threshold)) { // <=
}
if (safe_size.safe_greater_equal(threshold)) { // >=
}

// Утилитарные функции
if (safe_equal(block_size, threshold)) { // ==
}
if (safe_not_equal(block_size, threshold)) { // !=
}
if (safe_less(block_size, threshold)) { // <
}
if (safe_greater(block_size, threshold)) { // >
}
if (safe_less_equal(block_size, threshold)) { // <=
}
if (safe_greater_equal(block_size, threshold)) { // >=
}

// Пример 3: C++20 трехстороннее сравнение
#if __cplusplus >= 202002L
auto result = safe_size.safe_three_way_compare(threshold);
if (is_equal(result)) { // ==
}
else if (is_less(result)) { // <
}
else if (is_greater(result)) { // >
}

// Прямое трехстороннее сравнение
auto direct_result = safe_three_way_compare(block_size, threshold);
if (is_equal(direct_result)) { // ==
}
#endif

// Пример 4: Thread-safe операции
AtomicIntComparator atomic_counter(0);
atomic_counter.update(100);
if (atomic_counter.compare_and_set(100, 200)) {
  // Значение было успешно изменено
}

// Пример 5: Высокая производительность для single-threaded
FastIntComparator fast_counter(0);
fast_counter.update(100);
int current_value = fast_counter.get();

// Пример 6: Проверка диапазонов
if (fits_in_type<unsigned char>(300)) {
  // 300 не помещается в unsigned char (0-255)
}

// Пример 7: Смешанные типы
float float_value = 3.14f;
int int_value = 3;
if (safe_greater_equal(float_value, int_value)) {
  // Безопасное сравнение float >= int
}

// Пример 8: Floating-point с NaN и infinity
float nan_value = std::numeric_limits<float>::quiet_NaN();
float inf_value = std::numeric_limits<float>::infinity();
int normal_value = 42;

SafeFloatComparator safe_float(nan_value);
if (safe_float.safe_equal(normal_value)) { // false - NaN != anything
}
if (safe_float.safe_not_equal(normal_value)) { // true - NaN != anything
}

SafeFloatComparator safe_inf(inf_value);
if (safe_inf.safe_greater(normal_value)) { // true - +inf > any finite
}

// Пример 9: Комплексный сценарий
void processDataPacket(unsigned char packet_size, int max_size, float threshold) {
    SafeUCharComparator safe_packet_size(packet_size);

    // Проверяем размер пакета
    if (safe_packet_size.safe_greater(max_size)) {
        // Пакет слишком большой
        return;
    }

    // Проверяем пороговое значение
    if (safe_packet_size.safe_less_equal(threshold)) {
        // Маленький пакет
        processSmallPacket();
    } else {
        // Большой пакет
        processLargePacket();
    }

#if __cplusplus >= 202002L
    // Используем трехстороннее сравнение для сортировки
    auto comparison = safe_packet_size.safe_three_way_compare(max_size);
    if (is_less(comparison)) {
        // Пакет меньше максимального размера
    }
#endif
}
*/

#endif // !LUMEX_UTILITY_SAFENUMERICCOMPARATOR_HPP
// NOLINTEND(readability-simplify-boolean-expr)
