#ifndef LUMEX_UTILITY_CRC_CRC_HPP
#define LUMEX_UTILITY_CRC_CRC_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#if __cplusplus >= 202002L
  #include <span>
#endif

#include "lumex/core/utility/LumexAttributes.hpp"
#include "lumex/core/utility/LumexConstantMacros.hpp"

#include "lumex/LumexExport.hpp"

namespace Lumex
{
  namespace Core
  {
    namespace crc
    {
      namespace Constants
      {
        LUMEX_CONST_NUM uint8_t kUpperNibbleMask
          = 0xF0; ///< Константа для извлечения старших 4 бит из байта (сдвиг вправо на 4 позиции)
        LUMEX_CONST_NUM uint8_t kLowerNibbleMask
          = 0x0F; ///< Константа для извлечения младших 4 бит из байта (маска 0x0F)
        LUMEX_CONST_NUM std::size_t kCrc4TableSize = 16UL; ///< Размер таблицы CRC4 (16 элементов для 4-битных значений)

        LUMEX_CONST_NUM std::size_t kCrc8TableSize
          = 256UL; ///< Размер таблицы CRC8 по умолчанию (256 элементов для 8-битных значений).

        /**
         * @brief Статическая константная таблица для быстрого вычисления CRC4.
         * @details Эта таблица содержит предварительно вычисленные значения CRC4
         *          для каждого возможного 4-битного значения (0-15), что позволяет
         *          эффективно вычислять контрольную сумму, используя побитовую операцию XOR
         *          и индексацию таблицы. Таблица инициализируется на этапе компиляции.
         * @note Используется для вычисления 4-битных контрольных сумм в коммуникационных протоколах.
         *       Размер таблицы: 16 элементов (0x00 - 0x0F).
         */
        std::array<uint8_t, kCrc4TableSize> const kCrc4Table = {0, 13, 7, 10, 14, 3, 9, 4, 1, 12, 6, 11, 15, 2, 8, 5};

        /**
         * @brief Статическая константная таблица для быстрого вычисления CRC8.
         * @details Эта таблица содержит предварительно вычисленные значения CRC8
         *          для каждого возможного байта, что позволяет эффективно
         *          вычислять контрольную сумму, используя побитовую операцию XOR
         *          и индексацию таблицы. Таблица инициализируется на этапе компиляции.
         * @note Использует полином 0x8C (или 0x07 в прямой нотации).
         */
        LUMEX_CONSTINIT_CONSTANT std::array<uint8_t, kCrc8TableSize> kCrc8Table = {
          0,   94,  188, 226, 97,  63,  221, 131, 194, 156, 126, 32,  163, 253, 31,  65,  157, 195, 33,  127, 252, 162,
          64,  30,  95,  1,   227, 189, 62,  96,  130, 220, 35,  125, 159, 193, 66,  28,  254, 160, 225, 191, 93,  3,
          128, 222, 60,  98,  190, 224, 2,   92,  223, 129, 99,  61,  124, 34,  192, 158, 29,  67,  161, 255, 70,  24,
          250, 164, 39,  121, 155, 197, 132, 218, 56,  102, 229, 187, 89,  7,   219, 133, 103, 57,  186, 228, 6,   88,
          25,  71,  165, 251, 120, 38,  196, 154, 101, 59,  217, 135, 4,   90,  184, 230, 167, 249, 27,  69,  198, 152,
          122, 36,  248, 166, 68,  26,  153, 199, 37,  123, 58,  100, 134, 216, 91,  5,   231, 185, 140, 210, 48,  110,
          237, 179, 81,  15,  78,  16,  242, 172, 47,  113, 147, 205, 17,  79,  173, 243, 112, 46,  204, 146, 211, 141,
          111, 49,  178, 236, 14,  80,  175, 241, 19,  77,  206, 144, 114, 44,  109, 51,  209, 143, 12,  82,  176, 238,
          50,  108, 142, 208, 83,  13,  239, 177, 240, 174, 76,  18,  145, 207, 45,  115, 202, 148, 118, 40,  171, 245,
          23,  73,  8,   86,  180, 234, 105, 55,  213, 139, 87,  9,   235, 181, 54,  104, 138, 212, 149, 203, 41,  119,
          244, 170, 72,  22,  233, 183, 85,  11,  136, 214, 52,  106, 43,  117, 151, 201, 74,  20,  246, 168, 116, 42,
          200, 150, 21,  75,  169, 247, 182, 232, 10,  84,  215, 137, 107, 53};
      } // namespace Constants

      /**
       * @brief Класс для вычисления CRC4 контрольных сумм.
       * @details Предоставляет статические методы для вычисления 4-битных циклических
       *          избыточных кодов (CRC4). Используется для проверки целостности данных
       *          в коммуникационных протоколах с высокой надежностью.
       */
      class LUMEX_PUBLIC_API Crc4
      {
      public:
        /**
         * @brief Вычисляет CRC4 для заданного 8-битного значения.
         * @details Функция принимает один байт данных и вычисляет 4-битную контрольную сумму
         *          используя предварительно вычисленную таблицу CRC4. Результат используется
         *          для проверки целостности данных в коммуникационных пакетах.
         * @param data 8-битное значение для вычисления CRC4. Должно быть в диапазоне 0-255.
         * @return 4-битное значение CRC4 (0-15). Результат всегда корректный для любого входного значения.
         * @note Функция является потокобезопасной и реентерабельной.
         * @see Constants::kCrc4Table
         * @example
         * uint8_t packetNumber = 42;
         * uint8_t crc4 = Crc4::calculateCrc4(packetNumber);
         * // crc4 содержит 4-битную контрольную сумму для значения 42
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value is a CRC4 checksum; crucial for data integrity verification.")
        static uint8_t calculateCrc4(uint8_t data) LUMEX_NOEXCEPT_FUNCTION;
      };

      class LUMEX_PUBLIC_API Crc8
      {
      public:
        /**
         * @brief Вычисляет CRC8 для заданного буфера данных, представленного raw ptr и размером.
         * @details Эта функция является базовой реализацией CRC8. Она принимает raw ptr на данные
         *          и их размер. В случае некорректных входных данных (nullptr для данных или нулевой размер),
         *          она возвращает 0.
         * @param data Указатель на начало буфера данных. Должен быть действительным указателем, если `size` > 0.
         * @param size Размер буфера данных в байтах.
         * @return Вычисленное значение CRC8 (`uint8_t`) или ошибка. Возвращает 0, если `data == nullptr`
         * или `size
         * == 0`.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value is a CRC checksum; crucial for data integrity verification.")
        static uint8_t calculateCrc8(uint8_t const *data, std::size_t size) LUMEX_NOEXCEPT_FUNCTION;

        /**
         * @brief Вычисляет CRC8 для заданного буфера данных, представленного `std::vector<uint8_t>`.
         * @details Эта функция является удобной перегрузкой для работы со `std::vector`.
         *          В случае пустого вектора она возвращает 0.
         * @param data Константная ссылка на `std::vector<uint8_t>` с данными.
         * @return Вычисленное значение CRC8 (`uint8_t`) или ошибка. Возвращает 0, если `data` пуст.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value is a CRC checksum; crucial for data integrity verification.")
        static uint8_t calculateCrc8(std::vector<uint8_t> const &data) LUMEX_NOEXCEPT_FUNCTION;

#if __cplusplus >= 202002L
        /**
         * @brief Вычисляет CRC8 для заданного буфера данных, представленного `std::span<uint8_t>`.
         * @param data Константная ссылка на `std::span<uint8_t>` с данными.
         * @return Вычисленное значение CRC8 (`uint8_t`) или ошибка. Возвращает 0, если `data` пуст.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value is a CRC checksum; crucial for data integrity verification.")
        static uint8_t calculateCrc8(std::span<uint8_t const> data) LUMEX_NOEXCEPT_FUNCTION;
#endif
      };
    } // namespace CRC
  } // namespace Core
} // namespace Lumex

#endif // !LUMEX_UTILITY_CRC_CRC_HPP
