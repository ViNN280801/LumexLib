#ifndef LUMEX_EXPECTED_TYPES_HPP
#define LUMEX_EXPECTED_TYPES_HPP

namespace Lumex
{
  namespace Core
  {
    namespace Expected
    {
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
    } // namespace Expected
  } // namespace Core
} // namespace Lumex

#endif // !LUMEX_EXPECTED_TYPES_HPP
