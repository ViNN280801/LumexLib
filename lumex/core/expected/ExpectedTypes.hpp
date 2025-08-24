#ifndef EXPECTED_TYPES_HPP
#define EXPECTED_TYPES_HPP

#include <type_traits>

// ====================== Вспомогательные теги и типы ======================
/**
 * @brief C++11 не имеет std::in_place. Тег для указания создания значения на месте.
 * @details Используется для прямого конструирования содержащегося значения внутри `Expected`,
 *          избегая ненужных копирований или перемещений. Это аналог `std::in_place_tag` из C++17.
 */
struct in_place_tag {};

/**
 * @brief Глобальная константная переменная для тега `in_place_tag`.
 * @details Используется как аргумент для конструктора `Expected` для указания
 *          конструирования значения на месте.
 * @note Это аналог `std::in_place` из C++17.
 */
constexpr in_place_tag in_place{};

/**
 * @brief Вспомогательный тип для специализации `Expected<void, ErrorType>`.
 * @details Представляет собой "пустое" успешное значение, когда `Expected` не содержит
 *          никаких данных, но находится в успешном состоянии. Это позволяет имитировать
 *          `std::expected<void, E>` из C++23.
 * @note Используется как заглушка для типа успешного значения в специализации `Expected<void, ErrorType>`.
 */
struct Unit {};

/// @brief C++23-совместимый тег "сконструировать ошибку на месте"
struct unexpect_t {};

/// @brief Глобальная константа-тег (как std::unexpect)
constexpr unexpect_t unexpect{};

// Forward declaration for is_expected type trait
template <typename SuccessType, typename ErrorType> class Expected;

/// @brief Мета-признак для определения, является ли тип Expected, по умолчанию false, т.к. любой тип не является
/// Expected.
template <typename T> struct is_expected : std::false_type {};

/// @brief Мета-признак для определения, является ли тип Expected. Тут true, т.к. Expected<S, E> и Expected<void, E>
/// являются Expected.
template <typename S, typename E> struct is_expected<Expected<S, E>> : std::true_type {};

/// @brief Мета-признак для определения, является ли тип Expected. Тут true, т.к. специализация для void -
/// Expected<void, E> является Expected.
template <typename E> struct is_expected<Expected<void, E>> : std::true_type {};

#if __cplusplus >= 202002L
// C++20 helper type traits and concepts
template <typename T>
concept is_expected_concept                        = is_expected<T>::value;

template <typename T> constexpr bool is_expected_v = is_expected<T>::value;
#else
// Pre-C++20 helper type traits
template <typename T> constexpr bool is_expected_v = is_expected<T>::value;
#endif

#endif // !EXPECTED_TYPES_HPP
