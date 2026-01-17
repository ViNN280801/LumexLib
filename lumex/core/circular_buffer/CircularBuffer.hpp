#ifndef LUMEX_CIRCULAR_BUFFER_HPP
#define LUMEX_CIRCULAR_BUFFER_HPP

// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays, cppcoreguidelines-pro-type-reinterpret-cast)

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "lumex/core/utility/LumexKeywords.hpp"

#if __cplusplus >= 202002L
  #include <compare>  // Для std::strong_ordering
  #include <concepts> // Для std::three_way_comparable
#endif

namespace Lumex
{
  namespace Core
  {
    namespace CircularBuffer
    {
      /**
       * @brief Фиксированной емкости круговой буфер с семантикой перезаписи при полном буфере (подобно
       * Boost's `circular_buffer`).
       *
       * @tparam T Элемент типа, хранящийся в буфере.
       * @tparam Allocator Тип аллокатора; по умолчанию `std::allocator<T>`.
       *
       * Этот контейнер пытается имитировать основное поведение `Boost`'s `circular_buffer`
       * при сохранении реализации только на C++11 и header-only. Емкость фиксирована при создании.
       *
       * @section characteristics Характеристики
       *  - Вставка, когда буфер заполнен, перезаписывает самый старый элемент.
       *  - Не требуется `default-constructibility` для `T` (элементы создаются по запросу).
       *  - Сильная безопасность исключений для вставки, когда не перезаписывается;
       *    базовая безопасность исключений при перезаписи (перезаписанный элемент уничтожен перед
       *    вставкой).
       *  - Предоставляет случайный доступ к индексам (`0..size()-1`), где `0` относится к самому старому
       *    элементу.
       *  - Не потокобезопасный.
       * @section guarantees Гарантии, инвалидация и сложность
       *
       * @subsection invalidation Инвалидация указателей/итераторов/ссылок
       * Итераторы у этого контейнера индексные (хранят логический индекс), поэтому любые операции,
       * которые меняют расположение "головы" (head) - меняют соответствие "логический индекс → объект".
       * Такие операции инвалидируют все итераторы. Сами объекты при этом не перемещаются.
       *
       * Правила по операциям:
       *  - `emplace_back` / `push_back` при НЕполном буфере:
       *      - iterators: валидны (кроме `end()` - он меняется);
       *      - references/pointers к существующим элементам: валидны.
       *  - `emplace_front` / `push_front` при НЕполном буфере:
       *      - iterators: все инвалидируются (меняется `head`, логические индексы сдвигаются);
       *      - references/pointers: валидны (объекты не двигаются).
       *  - `emplace_back` / `push_back` при ПОЛНОМ буфере (перезапись самого старого):
       *      - iterators: все инвалидируются (меняется `head`);
       *      - references/pointers: к уничтоженному front-элементу - инвалид; к остальным - валидны.
       *  - `emplace_front` / `push_front` при ПОЛНОМ буфере (перезапись самого нового):
       *      - iterators: все инвалидируются (меняется `head`);
       *      - references/pointers: к уничтоженному back-элементу - инвалид; к остальным - валидны.
       *  - `pop_front`:
       *      - iterators: все инвалидируются (меняется `head`);
       *      - references/pointers: к удаленному элементу - инвалид; к остальным - валидны.
       *  - `pop_back`:
       *      - iterators: инвалидируются итераторы на последний элемент и `end()`; остальные валидны;
       *      - references/pointers: к удаленному элементу - инвалид; к остальным - валидны.
       *  - `clear`:
       *      - iterators/references/pointers: все инвалидируются.
       *  - `swap`:
       *      - iterators: все инвалидируются (итераторы привязаны к конкретному объекту контейнера);
       *      - references/pointers: продолжают указывать на те же объекты, но владельцем памяти станет другой буфер.
       *
       * @subsection complexity Сложность
       * Все основные операции - O(1): доступ по индексу, `front`/`back`, `push_*`/`emplace_*`/`pop_*`,
       * включая случаи перезаписи. Итерация - O(n). `swap` - O(1).
       *
       * @subsection exceptions Гарантии исключений
       *  - Конструирование/вставка при НЕполном буфере: сильная гарантия (commit-или-rollback).
       *  - Перезапись (полный буфер): базовая гарантия - сначала уничтожается заменяемый элемент,
       *    затем выполняется конструирование нового; при исключении буфер остается согласованным.
       *  - `pop_*`, `clear`, `swap` - не выбрасывают (согласно объявленным `noexcept`/макросам).
       *
       * @subsection threads Потокобезопасность
       * Контейнер не потокобезопасен. Одновременный доступ из нескольких потоков требует внешней синхронизации.
       *
       * @subsection iterators Итераторы
       * Итераторы - случайного доступа. Из-за индексной природы итераторов любое изменение `head`
       * делает их несогласованными с исходными элементами (см. раздел об инвалидации).
       *
       * @subsection contiguity Непрерывность памяти
       *             Контейнер не гарантирует непрерывное размещение объектов `T` как `std::vector`.
       *             Интерфейса `data()` нет. Адреса существующих элементов стабильны между операциями,
       *             которые не уничтожают данный элемент; при перезаписи/удалении адрес становится невалидным.
       *
       * @subsection ordering Сравнение
       *  - `==`, `!=`, `<`, `<=`, `>`, `>=` - лексикографическое сравнение логической последовательности;
       *    требуют соответствующих операций для `T`.
       *  - Оператор `<=>` доступен в C++20 и новее и требует `std::totally_ordered<T>`.
       *
       * @subsection allocator Аллокатор и swap
       * При `swap` аллокаторы обмениваются только если
       * `std::allocator_traits<Allocator>::propagate_on_container_swap::value == true`.
       * Иначе предполагается их эквивалентность; элементы не перемещаются.
       */
      template <class T, class Allocator = std::allocator<T>> class CircularBuffer
      {
        using alloc_traits = std::allocator_traits<Allocator>;

      public:
        // ============= public member types =============

        using value_type      = T;                  ///< Тип элементов, хранящихся в буфере.
        using allocator_type  = Allocator;          ///< Тип аллокатора, используемого для управления памятью элементов.
        using size_type       = std::size_t;        ///< Тип для представления размера или количества элементов.
        using difference_type = std::ptrdiff_t;     ///< Тип для представления разницы между двумя итераторами.
        using reference       = value_type &;       ///< Ссылка на элемент буфера.
        using const_reference = value_type const &; ///< Константная ссылка на элемент буфера.
        using pointer         = typename alloc_traits::pointer;       ///< Указатель на элемент буфера.
        using const_pointer   = typename alloc_traits::const_pointer; ///< Константный указатель на элемент буфера.

      private:
        // Неинициализированное хранилище для элементов T
        struct Slot {
          alignas(T) unsigned char data[sizeof(T)];
        };

        static_assert(alignof(Slot) >= alignof(T), "Slot alignment must be >= T alignment");
        static_assert(sizeof(Slot) >= sizeof(T), "Slot size must be >= sizeof(T)");

        using slot_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Slot>;
        using slot_traits    = std::allocator_traits<slot_allocator>;

        static constexpr bool is_swap_noexcept
          = (std::allocator_traits<Allocator>::propagate_on_container_swap::value
               ? (noexcept(std::swap(std::declval<Allocator &>(), std::declval<Allocator &>()))
                  && noexcept(std::swap(std::declval<slot_allocator &>(), std::declval<slot_allocator &>())))
               : std::allocator_traits<Allocator>::is_always_equal::value);

      public:
        /**
         * @brief Итератор случайного доступа над логической последовательностью элементов.
         *
         * Этот итератор предоставляет функциональность для обхода элементов в CircularBuffer
         * с возможностью случайного доступа. Он ведет себя как обычные указатели или итераторы
         * стандартных контейнеров, таких как `std::vector`.
         */
        class iterator
        {
        public:
          using iterator_category = std::random_access_iterator_tag; ///< Категория итератора.
          using value_type        = T;                               ///< Тип элементов, на которые указывает итератор.
          using difference_type   = std::ptrdiff_t; ///< Тип для представления разницы между двумя итераторами.
          using pointer           = T *;            ///< Тип указателя на элемент.
          using reference         = T &;            ///< Тип ссылки на элемент.

          /// @brief Конструктор по умолчанию. Создает неинициализированный итератор.
          iterator() = default;

          /**
           * @brief Оператор разыменования.
           * @return Ссылка на элемент, на который указывает итератор.
           */
          reference
          operator*() const
          {
            return (*m_buf)[m_idx];
          }

          /**
           * @brief Оператор доступа к члену.
           * @return Указатель на элемент, на который указывает итератор.
           */
          pointer
          operator->() const
          {
            return std::addressof((*m_buf)[m_idx]);
          }

          /**
           * @brief Префиксный оператор инкремента. Перемещает итератор к следующему элементу.
           * @return Ссылка на текущий итератор после инкремента.
           */
          iterator &
          operator++()
          {
            ++m_idx;
            return *this;
          }

          /**
           * @brief Постфиксный оператор инкремента. Перемещает итератор к следующему элементу.
           * @return Копия итератора до инкремента.
           */
          iterator
          operator++(int)
          {
            iterator tmp(*this);
            ++*this;
            return tmp;
          }

          /**
           * @brief Префиксный оператор декремента. Перемещает итератор к предыдущему элементу.
           * @return Ссылка на текущий итератор после декремента.
           */
          iterator &
          operator--()
          {
            --m_idx;
            return *this;
          }

          /**
           * @brief Постфиксный оператор декремента. Перемещает итератор к предыдущему элементу.
           * @return Копия итератора до декремента.
           */
          iterator
          operator--(int)
          {
            iterator tmp(*this);
            --*this;
            return tmp;
          }

          /**
           * @brief Оператор сложения с присваиванием. Перемещает итератор на `n` позиций вперед.
           * @param n Количество позиций для перемещения.
           * @return Ссылка на текущий итератор после перемещения.
           */
          iterator &
          operator+=(difference_type n)
          {
            m_idx += n;
            return *this;
          }

          /**
           * @brief Оператор вычитания с присваиванием. Перемещает итератор на `n` позиций назад.
           * @param n Количество позиций для перемещения.
           * @return Ссылка на текущий итератор после перемещения.
           */
          iterator &
          operator-=(difference_type n)
          {
            m_idx -= n;
            return *this;
          }

          /**
           * @brief Оператор сложения. Создает новый итератор, смещенный на `n` позиций вперед.
           * @param n Количество позиций для смещения.
           * @return Новый итератор, смещенный на `n` позиций.
           */
          iterator
          operator+(difference_type n) const
          {
            iterator tmp(*this);
            return tmp += n;
          }

          /**
           * @brief Дружественный оператор сложения. Создает новый итератор, смещенный на `n` позиций вперед.
           * @param n Количество позиций для смещения.
           * @param iter Исходный итератор.
           * @return Новый итератор, смещенный на `n` позиций.
           */
          friend iterator
          operator+(difference_type n, iterator iter)
          {
            return iter += n;
          }

          /**
           * @brief Оператор вычитания. Создает новый итератор, смещенный на `n` позиций назад.
           * @param n Количество позиций для смещения.
           * @return Новый итератор, смещенный на `n` позиций.
           */
          iterator
          operator-(difference_type n) const
          {
            iterator tmp(*this);
            return tmp -= n;
          }

          /**
           * @brief Оператор разности. Вычисляет расстояние между двумя итераторами.
           * @param other Другой итератор.
           * @return Разница в позициях между текущим и другим итератором.
           */
          difference_type
          operator-(iterator const &other) const
          {
            return difference_type(m_idx) - difference_type(other.m_idx);
          }

          /**
           * @brief Оператор сравнения на равенство.
           * @param other Другой итератор.
           * @return `true`, если итераторы указывают на один и тот же элемент в одном буфере, иначе `false`.
           */
          bool
          operator==(iterator const &other) const
          {
            return m_buf == other.m_buf && m_idx == other.m_idx;
          }

          /**
           * @brief Оператор сравнения на неравенство.
           * @param other Другой итератор.
           * @return `true`, если итераторы указывают на разные элементы или находятся в разных буферах, иначе `false`.
           */
          bool
          operator!=(iterator const &other) const
          {
            return !(*this == other);
          }

          /**
           * @brief Оператор "меньше". Сравнивает позиции двух итераторов.
           * @param other Другой итератор.
           * @return `true`, если текущий итератор предшествует `other`, иначе `false`.
           */
          bool
          operator<(iterator const &other) const
          {
            return m_idx < other.m_idx;
          }

          /**
           * @brief Оператор "больше". Сравнивает позиции двух итераторов.
           * @param other Другой итератор.
           * @return `true`, если текущий итератор следует за `other`, иначе `false`.
           */
          bool
          operator>(iterator const &other) const
          {
            return other < *this;
          }

          /**
           * @brief Оператор "меньше или равно". Сравнивает позиции двух итераторов.
           * @param other Другой итератор.
           * @return `true`, если текущий итератор предшествует `other` или равен ему, иначе `false`.
           */
          bool
          operator<=(iterator const &other) const
          {
            return !(other < *this);
          }

          /**
           * @brief Оператор "больше или равно". Сравнивает позиции двух итераторов.
           * @param other Другой итератор.
           * @return `true`, если текущий итератор следует за `other` или равен ему, иначе `false`.
           */
          bool
          operator>=(iterator const &other) const
          {
            return !(*this < other);
          }

        private:
          friend class CircularBuffer;

          /// @brief Приватный конструктор.
          /// @param buf Указатель на CircularBuffer, которому принадлежит итератор.
          /// @param idx Логический индекс элемента.
          iterator(CircularBuffer *buf, size_type idx) : m_buf(buf), m_idx(idx) {}

          CircularBuffer *m_buf{}; ///< Указатель на буфер, которым владеет этот итератор.
          size_type m_idx{};       ///< Логический индекс элемента в буфере.
        };

        /**
         * @brief Константный итератор случайного доступа.
         *
         * Этот итератор предоставляет функциональность для обхода элементов в CircularBuffer
         * с возможностью случайного доступа, но не позволяет модифицировать элементы.
         * Он ведет себя как `const` указатели или итераторы стандартных контейнеров.
         */
        class const_iterator
        {
        public:
          using iterator_category = std::random_access_iterator_tag; ///< Категория итератора.
          using value_type        = T;                               ///< Тип элементов, на которые указывает итератор.
          using difference_type   = std::ptrdiff_t; ///< Тип для представления разницы между двумя итераторами.
          using pointer           = T const *;      ///< Тип константного указателя на элемент.
          using reference         = T const &;      ///< Тип константной ссылки на элемент.

          /// @brief Конструктор по умолчанию. Создает неинициализированный константный итератор.
          const_iterator() : m_buf(0), m_idx(0) {}

          /// @brief Конструктор копирования из неконстантного итератора.
          /// @param iter Исходный итератор.
          const_iterator(iterator const &iter) : m_buf(iter.m_buf), m_idx(iter.m_idx) {}

          /**
           * @brief Оператор разыменования.
           * @return Константная ссылка на элемент, на который указывает итератор.
           */
          reference
          operator*() const
          {
            return (*m_buf)[m_idx];
          }

          /**
           * @brief Оператор доступа к члену.
           * @return Константный указатель на элемент, на который указывает итератор.
           */
          pointer
          operator->() const
          {
            return std::addressof((*m_buf)[m_idx]);
          }

          /**
           * @brief Префиксный оператор инкремента. Перемещает итератор к следующему элементу.
           * @return Ссылка на текущий итератор после инкремента.
           */
          const_iterator &
          operator++()
          {
            ++m_idx;
            return *this;
          }

          /**
           * @brief Постфиксный оператор инкремента. Перемещает итератор к следующему элементу.
           * @return Копия итератора до инкремента.
           */
          const_iterator
          operator++(int)
          {
            const_iterator tmp(*this);
            ++*this;
            return tmp;
          }

          /**
           * @brief Префиксный оператор декремента. Перемещает итератор к предыдущему элементу.
           * @return Ссылка на текущий итератор после декремента.
           */
          const_iterator &
          operator--()
          {
            --m_idx;
            return *this;
          }

          /**
           * @brief Постфиксный оператор декремента. Перемещает итератор к предыдущему элементу.
           * @return Копия итератора до декремента.
           */
          const_iterator
          operator--(int)
          {
            const_iterator tmp(*this);
            --*this;
            return tmp;
          }

          /**
           * @brief Оператор сложения с присваиванием. Перемещает итератор на `n` позиций вперед.
           * @param n Количество позиций для перемещения.
           * @return Ссылка на текущий итератор после перемещения.
           */
          const_iterator &
          operator+=(difference_type n)
          {
            m_idx += n;
            return *this;
          }

          /**
           * @brief Оператор вычитания с присваиванием. Перемещает итератор на `n` позиций назад.
           * @param n Количество позиций для перемещения.
           * @return Ссылка на текущий итератор после перемещения.
           */
          const_iterator &
          operator-=(difference_type n)
          {
            m_idx -= n;
            return *this;
          }

          /**
           * @brief Оператор сложения. Создает новый константный итератор, смещенный на `n` позиций вперед.
           * @param n Количество позиций для смещения.
           * @return Новый константный итератор, смещенный на `n` позиций.
           */
          const_iterator
          operator+(difference_type n) const
          {
            const_iterator tmp(*this);
            return tmp += n;
          }

          /**
           * @brief Дружественный оператор сложения. Создает новый константный итератор, смещенный на `n` позиций
           * вперед.
           * @param n Количество позиций для смещения.
           * @param iter Исходный константный итератор.
           * @return Новый константный итератор, смещенный на `n` позиций.
           */
          friend const_iterator
          operator+(difference_type n, const_iterator iter)
          {
            return iter += n;
          }

          /**
           * @brief Оператор вычитания. Создает новый константный итератор, смещенный на `n` позиций назад.
           * @param n Количество позиций для смещения.
           * @return Новый константный итератор, смещенный на `n` позиций.
           */
          const_iterator
          operator-(difference_type n) const
          {
            const_iterator tmp(*this);
            return tmp -= n;
          }

          /**
           * @brief Оператор разности. Вычисляет расстояние между двумя константными итераторами.
           * @param other Другой константный итератор.
           * @return Разница в позициях между текущим и другим итератором.
           */
          difference_type
          operator-(const_iterator const &other) const
          {
            return difference_type(m_idx) - difference_type(other.m_idx);
          }

          /**
           * @brief Оператор сравнения на равенство.
           * @param other Другой константный итератор.
           * @return `true`, если итераторы указывают на один и тот же элемент в одном буфере, иначе `false`.
           */
          bool
          operator==(const_iterator const &other) const
          {
            return m_buf == other.m_buf && m_idx == other.m_idx;
          }

          /**
           * @brief Оператор сравнения на неравенство.
           * @param other Другой константный итератор.
           * @return `true`, если итераторы указывают на разные элементы или находятся в разных буферах, иначе `false`.
           */
          bool
          operator!=(const_iterator const &other) const
          {
            return !(*this == other);
          }

          /**
           * @brief Оператор "меньше". Сравнивает позиции двух константных итераторов.
           * @param other Другой константный итератор.
           * @return `true`, если текущий итератор предшествует `other`, иначе `false`.
           */
          bool
          operator<(const_iterator const &other) const
          {
            return m_idx < other.m_idx;
          }

          /**
           * @brief Оператор "больше". Сравнивает позиции двух константных итераторов.
           * @param other Другой константный итератор.
           * @return `true`, если текущий итератор следует за `other`, иначе `false`.
           */
          bool
          operator>(const_iterator const &other) const
          {
            return other < *this;
          }

          /**
           * @brief Оператор "меньше или равно". Сравнивает позиции двух константных итераторов.
           * @param other Другой константный итератор.
           * @return `true`, если текущий итератор предшествует `other` или равен ему, иначе `false`.
           */
          bool
          operator<=(const_iterator const &other) const
          {
            return !(other < *this);
          }

          /**
           * @brief Оператор "больше или равно". Сравнивает позиции двух константных итераторов.
           * @param other Другой константный итератор.
           * @return `true`, если текущий итератор следует за `other` или равен ему, иначе `false`.
           */
          bool
          operator>=(const_iterator const &other) const
          {
            return !(*this < other);
          }

        private:
          friend class CircularBuffer;

          /// @brief Приватный конструктор.
          /// @param buf Указатель на константный CircularBuffer, которому принадлежит итератор.
          /// @param idx Логический индекс элемента.
          const_iterator(CircularBuffer const *buf, size_type idx) : m_buf(buf), m_idx(idx) {}

          CircularBuffer const *m_buf; ///< Указатель на константный буфер, которым владеет этот итератор.
          size_type m_idx;             ///< Логический индекс элемента в буфере.
        };

        using reverse_iterator = std::reverse_iterator<iterator>; ///< Тип для обратного итератора.
        using const_reverse_iterator
          = std::reverse_iterator<const_iterator>; ///< Тип для константного обратного итератора.

        // ============= ctors / dtors =============

        /**
         * @brief Создает буфер с указанной емкостью.
         *
         * @param capacity Положительное максимальное количество элементов, которое может хранить буфер.
         * @param alloc Аллокатор, используемый для выделения памяти.
         * @throws std::length_error если `capacity == 0`, так как буфер не может иметь нулевую емкость.
         * @note Буфер инициализируется пустым, элементы добавляются позже.
         */
        explicit CircularBuffer(size_type capacity, allocator_type const &alloc = allocator_type())
            : m_allocator(alloc), m_slots_alloc(m_allocator), m_capacity(capacity)
        {
          if(m_capacity == 0) throw std::length_error("CircularBuffer capacity must be > 0");
          allocate_storage();
        }

        /**
         * @brief Конструктор копирования.
         *
         * Создает новый круговой буфер, который является копией `other`.
         * Элементы копируются в логическом порядке из `other` в новый буфер.
         * Если аллокатор имеет `propagate_on_container_copy_construction`, он также копируется.
         *
         * @param other Буфер, из которого производится копирование.
         * @throws Исключения, выбрасываемые конструкторами копирования элементов `T` или аллокатором.
         */
        CircularBuffer(CircularBuffer const &other)
            : m_allocator(alloc_traits::select_on_container_copy_construction(other.m_allocator)),
              m_slots_alloc(m_allocator), // Инициализируем из новой m_allocator
              m_capacity(other.m_capacity)
        {
          allocate_storage();
          // Копируем существующие элементы в логическом порядке
          for(size_type i = 0; i < other.m_size; ++i) emplace_back(other[i]);
        }

        /**
         * @brief Оператор присваивания копированием.
         *
         * Присваивает содержимое `other` текущему буферу.
         * Использует идиому "copy-and-swap" для обеспечения сильной гарантии исключений.
         *
         * @param other Буфер, из которого производится присваивание.
         * @return Ссылка на текущий буфер.
         * @throws Исключения, выбрасываемые конструктором копирования `CircularBuffer` или аллокатором.
         */
        CircularBuffer &
        operator=(CircularBuffer const &other)
        {
          if(this == &other) return *this;
          CircularBuffer tmp(other);
          swap(tmp);
          return *this;
        }

        /**
         * @brief Конструктор перемещения.
         *
         * Создает новый круговой буфер, перемещая ресурсы из `other`.
         * После вызова `other` будет находиться в действительном, но неопределенном состоянии.
         *
         * @param other Буфер, из которого перемещаются ресурсы.
         * @note Этот конструктор помечен `noexcept`, чтобы обеспечить корректное поведение при перемещении.
         */
        CircularBuffer(CircularBuffer &&other) LUMEX_NOEXCEPT_FUNCTION : m_allocator(std::move(other.m_allocator)),
                                                                         m_slots_alloc(std::move(other.m_slots_alloc)),
                                                                         m_slots(other.m_slots),
                                                                         m_capacity(other.m_capacity),
                                                                         m_size(other.m_size),
                                                                         m_head(other.m_head)
        {
          other.m_slots    = 0;
          other.m_capacity = 0;
          other.m_size     = 0;
          other.m_head     = 0;
        }

        /**
         * @brief Оператор присваивания перемещением.
         *
         * Присваивает содержимое `other` текущему буферу, перемещая ресурсы.
         * Все существующие элементы в текущем буфере уничтожаются, а память освобождается.
         * После вызова `other` будет находиться в действительном, но неопределенном состоянии.
         *
         * @param other Буфер, из которого перемещаются ресурсы.
         * @return Ссылка на текущий буфер.
         * @note Этот оператор помечен `noexcept`, чтобы обеспечить корректное поведение при перемещении.
         */
        CircularBuffer &
        operator=(CircularBuffer &&other) LUMEX_NOEXCEPT_FUNCTION
        {
          if(this == &other) return *this;
          clear();
          deallocate_storage();
          m_allocator      = std::move(other.m_allocator);
          m_slots_alloc    = std::move(other.m_slots_alloc);
          m_slots          = other.m_slots;
          m_capacity       = other.m_capacity;
          m_size           = other.m_size;
          m_head           = other.m_head;
          other.m_slots    = 0;
          other.m_capacity = 0;
          other.m_size     = 0;
          other.m_head     = 0;
          return *this;
        }

        /**
         * @brief Деструктор.
         *
         * Уничтожает все элементы в буфере и освобождает выделенную память.
         */
        ~CircularBuffer()
        {
          clear();
          deallocate_storage();
        }

        // ============= capacity / state =============

        /**
         * @brief Возвращает максимальное количество элементов, которое может хранить буфер.
         * @return Емкость буфера.
         * @note Эта функция помечена `noexcept`.
         */
        size_type
        capacity() const LUMEX_NOEXCEPT_FUNCTION
        {
          return m_capacity;
        }

        /**
         * @brief Возвращает текущее количество элементов в буфере.
         * @return Количество элементов в буфере.
         * @note Эта функция помечена `noexcept`.
         */
        size_type
        size() const LUMEX_NOEXCEPT_FUNCTION
        {
          return m_size;
        }

        /**
         * @brief Проверяет, пуст ли буфер.
         * @return `true`, если буфер не содержит элементов, иначе `false`.
         * @note Эта функция помечена `noexcept`.
         */
        bool
        empty() const LUMEX_NOEXCEPT_FUNCTION
        {
          return m_size == 0;
        }

        /**
         * @brief Проверяет, полон ли буфер.
         * @return `true`, если буфер содержит максимальное количество элементов (равное емкости), иначе `false`.
         * @note Эта функция помечена `noexcept`.
         */
        bool
        full() const LUMEX_NOEXCEPT_FUNCTION
        {
          return m_size == m_capacity;
        }

        /**
         * @brief Удаляет все элементы из буфера.
         *
         * Уничтожает все элементы в буфере, но не изменяет его емкость.
         * После вызова `clear()` буфер становится пустым (`size()` будет `0`).
         *
         * @invalidation Инвалидирует все итераторы, ссылки и указатели.
         * @complexity O(size()) вызовов деструктора `T` (или O(1), если `T` тривиально разрушаем).
         *
         * @note Эта функция помечена `noexcept`.
         */
        void
        clear() LUMEX_NOEXCEPT_FUNCTION
        {
          if(!std::is_trivially_destructible<T>::value)
            for(size_type i = 0; i < m_size; ++i) alloc_traits::destroy(m_allocator, std::addressof(at_slot(i)));

          m_size = 0;
          m_head = 0;
        }

        // ============= element access =============

        /**
         * @brief Доступ к элементу по логическому индексу с проверкой границ.
         *
         * Возвращает ссылку на элемент по указанному логическому индексу.
         * Логический индекс `0` всегда указывает на самый старый элемент.
         *
         * @param idx Логический индекс элемента (`0 <= idx < size()`).
         * @return Ссылка на элемент по указанному индексу.
         * @throws std::out_of_range если `idx` находится вне допустимого диапазона (`idx >= size()`).
         */
        reference
        at(size_type idx)
        {
          if(idx >= m_size) throw std::out_of_range("CircularBuffer::at: index out of range");
          return at_slot_ref(idx);
        }

        /**
         * @brief Константный доступ к элементу по логическому индексу с проверкой границ.
         *
         * Возвращает константную ссылку на элемент по указанному логическому индексу.
         * Логический индекс `0` всегда указывает на самый старый элемент.
         *
         * @param idx Логический индекс элемента (`0 <= idx < size()`).
         * @return Константная ссылка на элемент по указанному индексу.
         * @throws std::out_of_range если `idx` находится вне допустимого диапазона (`idx >= size()`).
         */
        const_reference
        at(size_type idx) const
        {
          if(idx >= m_size) throw std::out_of_range("CircularBuffer::at: index out of range");
          return at_slot_cref(idx);
        }

        /**
         * @brief Доступ к элементу по логическому индексу без проверки границ.
         *
         * Возвращает ссылку на элемент по указанному логическому индексу.
         * Логический индекс `0` всегда указывает на самый старый элемент.
         *
         * @warning Использование этого оператора с недопустимым индексом (`idx >= size()`)
         *          приводит к неопределенному поведению (UB).
         * @param idx Логический индекс элемента (`0 <= idx < size()`).
         * @return Ссылка на элемент по указанному индексу.
         */
        reference
        operator[](size_type idx)
        {
          return at_slot_ref(idx);
        }

        /**
         * @brief Константный доступ к элементу по логическому индексу без проверки границ.
         *
         * Возвращает константную ссылку на элемент по указанному логическому индексу.
         * Логический индекс `0` всегда указывает на самый старый элемент.
         *
         * @warning Использование этого оператора с недопустимым индексом (`idx >= size()`)
         *          приводит к неопределенному поведению (UB).
         * @param idx Логический индекс элемента (`0 <= idx < size()`).
         * @return Константная ссылка на элемент по указанному индексу.
         */
        const_reference
        operator[](size_type idx) const
        {
          return at_slot_cref(idx);
        }

        /**
         * @brief Доступ к самому старому элементу (front).
         *
         * Возвращает ссылку на первый (самый старый) элемент в буфере.
         *
         * @see front_safe() - версия с проверкой границ и выбрасыванием std::out_of_range.
         * @pre Буфер не должен быть пустым (`!empty()`) - UB.
         * @return Ссылка на первый элемент.
         * @throws Поведение неопределено, если буфер пуст.
         */
        reference
        front()
        {
#ifndef NDEBUG
          assert(!empty() && "CircularBuffer::front() on empty buffer");
#endif
          return (*this)[0];
        }

        /**
         * @brief Доступ к самому старому элементу (front) с проверкой границ.
         *
         * Возвращает ссылку на первый (самый старый) элемент в буфере.
         *
         * @see front() - версия без проверки границ, но с UB.
         * @return Ссылка на первый элемент.
         * @throws Поведение неопределено, если буфер пуст.
         */
        reference
        front_safe()
        {
          if(empty()) throw std::out_of_range("CircularBuffer::front_safe() on empty buffer");
          return (*this)[0];
        }

        /**
         * @brief Константный доступ к самому старому элементу (front).
         *
         * Возвращает константную ссылку на первый (самый старый) элемент в буфере.
         *
         * @see front_safe() - версия с проверкой границ и выбрасыванием std::out_of_range.
         * @pre Буфер не должен быть пустым (`!empty()`) - UB.
         * @return Константная ссылка на первый элемент.
         * @throws Поведение неопределено, если буфер пуст.
         */
        const_reference
        front() const
        {
#ifndef NDEBUG
          assert(!empty() && "CircularBuffer::front() on empty buffer");
#endif
          return (*this)[0];
        }

        /**
         * @brief Константный доступ к самому старому элементу (front) с проверкой границ.
         *
         * Возвращает константную ссылку на первый (самый старый) элемент в буфере.
         *
         * @see back() - версия без проверки границ, но с UB.
         * @return Константная ссылка на первый элемент.
         * @throws Поведение неопределено, если буфер пуст.
         */
        const_reference
        front_safe() const
        {
          if(empty()) throw std::out_of_range("CircularBuffer::front_safe() on empty buffer");
          return (*this)[0];
        }

        /**
         * @brief Доступ к самому новому элементу (back).
         *
         * Возвращает ссылку на последний (самый новый) элемент в буфере.
         *
         * @pre Буфер не должен быть пустым (`!empty()`) - UB.
         * @return Ссылка на последний элемент.
         * @throws Поведение неопределено, если буфер пуст.
         */
        reference
        back()
        {
#ifndef NDEBUG
          assert(!empty() && "CircularBuffer::back() on empty buffer");
#endif
          return (*this)[m_size - 1];
        }

        /**
         * @brief Доступ к самому новому элементу (back) с проверкой границ.
         *
         * Возвращает ссылку на последний (самый новый) элемент в буфере.
         *
         * @see back() - версия без проверки границ, но с UB.
         * @return Ссылка на последний элемент.
         * @throws std::out_of_range если буфер пуст.
         */
        reference
        back_safe()
        {
          if(empty()) throw std::out_of_range("CircularBuffer::back_safe() on empty buffer");
          return (*this)[m_size - 1];
        }

        /**
         * @brief Константный доступ к самому новому элементу (back).
         *
         * Возвращает константную ссылку на последний (самый новый) элемент в буфере.
         *
         * @pre Буфер не должен быть пустым (`!empty()`) - UB.
         * @see back_safe() - версия, выбрасывающая std::out_of_range.
         * @return Константная ссылка на последний элемент.
         * @throws Поведение неопределено, если буфер пуст.
         */
        const_reference
        back() const
        {
#ifndef NDEBUG
          assert(!empty() && "CircularBuffer::back() on empty buffer");
#endif
          return (*this)[m_size - 1];
        }

        /**
         * @brief Константный доступ к самому новому элементу (back) с проверкой границ.
         *
         * Возвращает константную ссылку на последний (самый новый) элемент в буфере.
         *
         * @see back() - версия без проверки границ, но с UB.
         * @return Константная ссылка на последний элемент.
         * @throws Поведение неопределено, если буфер пуст.
         */
        const_reference
        back_safe() const
        {
          if(empty()) throw std::out_of_range("CircularBuffer::back_safe() on empty buffer");
          return (*this)[m_size - 1];
        }

        // ============= modifiers =============

        /**
         * @brief Добавляет копию элемента в конец буфера.
         *
         * Если буфер полон, самый старый элемент перезаписывается.
         *
         * @param value Значение, которое нужно добавить.
         * @throws Исключения, выбрасываемые конструктором копирования `T` или аллокатором.
         */
        void
        push_back(T const &value)
        {
          emplace_back(value);
        }

        /**
         * @brief Добавляет перемещенный элемент в конец буфера.
         *
         * Если буфер полон, самый старый элемент перезаписывается.
         *
         * @param value Значение, которое нужно добавить (будет перемещено).
         * @throws Исключения, выбрасываемые конструктором перемещения `T` или аллокатором.
         */
        void
        push_back(T &&value)
        {
          emplace_back(std::move(value));
        }

        /**
         * @brief Добавляет копию элемента в начало буфера.
         *
         * Если буфер полон, самый новый элемент перезаписывается (теряется).
         * Это отличается от `push_back`, где перезаписывается самый старый элемент.
         *
         * @param value Значение, которое нужно добавить.
         * @throws Исключения, выбрасываемые конструктором копирования `T` или аллокатором.
         * @note Элемент становится новым "front" элементом.
         */
        void
        push_front(T const &value)
        {
          emplace_front(value);
        }

        /**
         * @brief Добавляет перемещенный элемент в начало буфера.
         *
         * Если буфер полон, самый новый элемент перезаписывается (теряется).
         * Это отличается от `push_back`, где перезаписывается самый старый элемент.
         *
         * @param value Значение, которое нужно добавить (будет перемещено).
         * @throws Исключения, выбрасываемые конструктором перемещения `T` или аллокатором.
         * @note Элемент становится новым "front" элементом.
         */
        void
        push_front(T &&value)
        {
          emplace_front(std::move(value));
        }

        /**
         * @brief Создает элемент на месте в конце буфера.
         *
         * Конструирует новый элемент непосредственно в буфере, используя переданные аргументы.
         * Если буфер полон, самый старый элемент уничтожается перед вставкой нового.
         *
         * @tparam Args Типы аргументов, передаваемых конструктору `T`.
         * @param args Аргументы, переданные конструктору `T`.
         * @return Ссылка на только что вставленный элемент.
         * @throws Исключения, выбрасываемые конструктором `T` или аллокатором.
         * @note Обеспечивает базовую безопасность исключений: если конструктор `T` выбрасывает исключение
         *       при перезаписи, старый элемент уже уничтожен, и буфер остается в согласованном состоянии.
         *       Если буфер не был полон, обеспечивается сильная безопасность исключений.
         *
         * @invalidation
         *  - Неполный буфер: итераторы остаются валидными, кроме `end()`.
         *  - Полный буфер: все итераторы инвалидируются; ссылки/указатели к старейшему элементу — инвалид.
         * @complexity O(1).
         * @throws Конструктор `T` или аллокатор.
         */
        template <class... Args>
        reference
        emplace_back(Args &&...args)
        {
          if(full())
          {
            // эквивалент pop_front(): удалить старейший, сделать буфер неполным
            alloc_traits::destroy(m_allocator, std::addressof(at_slot(0)));
            m_head = next_index(m_head);
            --m_size;
          }
          size_type const pos = physical_index(m_size); // теперь неполный буфер
          alloc_traits::construct(m_allocator, reinterpret_cast<T *>(slot_ptr(pos)), std::forward<Args>(args)...);
          ++m_size;
          return *reinterpret_cast<T *>(slot_ptr(pos));
        }

        /**
         * @brief Создает элемент на месте в начале буфера.
         *
         * Конструирует новый элемент непосредственно в буфере, используя переданные аргументы.
         * Если буфер полон, самый новый элемент уничтожается перед вставкой нового.
         *
         * @tparam Args Типы аргументов, передаваемых конструктору `T`.
         * @param args Аргументы, переданные конструктору `T`.
         * @return Ссылка на только что вставленный элемент.
         * @throws Исключения, выбрасываемые конструктором `T` или аллокатором.
         * @note Обеспечивает базовую безопасность исключений: если конструктор `T` выбрасывает исключение
         *       при перезаписи, старый элемент уже уничтожен, и буфер остается в согласованном состоянии.
         *       Если буфер не был полон, обеспечивается сильная безопасность исключений.
         */
        template <class... Args>
        reference
        emplace_front(Args &&...args)
        {
          size_type const pos = (m_size == 0) ? m_head : prev_index(m_head);

          if(full())
          {
            // освободить целевой слот: это именно prev_index(m_head)
            alloc_traits::destroy(m_allocator, std::addressof(at_slot(0 + m_size - 1))); // старый самый новый
            --m_size; // теперь capacity-1, head НЕ трогаем
          }

          // конструируем в pos, при неудаче инварианты не нарушены
          alloc_traits::construct(m_allocator, reinterpret_cast<T *>(slot_ptr(pos)), std::forward<Args>(args)...);
          m_head = (m_size == 0) ? m_head : pos;
          ++m_size;
          return *reinterpret_cast<T *>(slot_ptr(m_head));
        }

        /**
         * @brief Удаляет самый старый элемент из буфера.
         *
         * Если буфер не пуст, уничтожает первый (самый старый) элемент и уменьшает размер буфера.
         * Если буфер пуст, функция ничего не делает.
         *
         * @invalidation Все итераторы инвалидируются; ссылки/указатели к удалённому элементу — инвалид.
         * @complexity O(1).
         *
         * @note Эта функция помечена `noexcept`.
         */
        void
        pop_front()
        {
          if(empty()) return;
          alloc_traits::destroy(m_allocator, std::addressof(at_slot(0)));
          m_head = next_index(m_head);
          --m_size;
        }

        /**
         * @brief Удаляет самый новый элемент из буфера.
         *
         * Если буфер не пуст, уничтожает последний (самый новый) элемент и уменьшает размер буфера.
         * Если буфер пуст, функция ничего не делает.
         *
         * @invalidation Инвалидируются итераторы на последний элемент и `end()`;
         * остальные итераторы и ссылки/указатели (кроме удалённого) остаются валидны.
         * @complexity O(1).
         *
         * @note Эта функция помечена `noexcept`.
         */
        void
        pop_back()
        {
          if(empty()) return;
          alloc_traits::destroy(m_allocator, std::addressof(at_slot(m_size - 1)));
          --m_size;
        }

        /**
         * @brief Возвращает копию аллокатора, используемого буфером.
         * @return Копия объекта аллокатора.
         * @note Эта функция помечена `noexcept`.
         */
        allocator_type
        get_allocator() const LUMEX_NOEXCEPT_FUNCTION
        {
          return m_allocator;
        }

        /**
         * @brief Обменивает содержимое двух буферов.
         *
         * @details Меняет местами все элементы между текущим буфером и `other`.
         *          Если `std::allocator_traits<Allocator>::propagate_on_container_swap::value` равно `true`,
         *          аллокаторы также обмениваются. В противном случае аллокаторы не обмениваются, и
         *          предполагается, что они эквивалентны.
         *
         * @invalidation Инвалидирует все итераторы (они привязаны к объекту контейнера).
         * Ссылки/указатели остаются валидны, но элементы переходят во владение другого буфера.
         * @complexity O(1).
         * @throws Не бросает при выполнении условий noexcept, зафиксированных в объявлении.
         *
         * @param other Буфер, с которым производится обмен.
         * @note Эта функция помечена `noexcept` при определенных условиях, зависящих от характеристик аллокатора.
         */
        void
        swap(CircularBuffer &other) LUMEX_NOEXCEPT_FUNCTION_CONDITIONAL(is_swap_noexcept)
        {
          if(std::allocator_traits<Allocator>::propagate_on_container_swap::value)
          {
            // Разрешено и корректно обменивать аллокаторы (и ребайн-аллокатор):
            std::swap(m_allocator, other.m_allocator);
            std::swap(m_slots_alloc, other.m_slots_alloc);
          }
          else
          {
            // Аллокаторы не пропагируются. Корректность обмена буферов требует их эквивалентности.
            // В релизе полагаемся на документированное пред-условие; в отладке - проверим.
#ifndef NDEBUG
            // Требования Allocator (C++11): сравнение на равенство допустимо.
            assert(m_allocator == other.m_allocator);
#endif
            // Аллокаторы оставляем на местах.
          }

          // Обмениваем внутренние данные (указатель и мета-данные)
          std::swap(m_slots, other.m_slots);
          std::swap(m_capacity, other.m_capacity);
          std::swap(m_size, other.m_size);
          std::swap(m_head, other.m_head);
        }

        /**
         * @brief Не член-функция `swap`, обменивающая содержимое двух буферов.
         *
         * Перегрузка глобальной функции `swap` для `CircularBuffer`, обеспечивающая
         * эффективный обмен содержимым двух буферов.
         *
         * @param other_1 Первый буфер.
         * @param other_2 Второй буфер.
         * @note Эта функция помечена `noexcept` при определенных условиях, зависящих от характеристик аллокатора.
         */
        friend void
        swap(CircularBuffer &other_1, CircularBuffer &other_2) LUMEX_NOEXCEPT_FUNCTION_CONDITIONAL(is_swap_noexcept)
        {
          other_1.swap(other_2);
        }

        // ============= iteration =============

        /**
         * @brief Возвращает итератор на начало логической последовательности элементов.
         * @return Итератор, указывающий на самый старый элемент буфера.
         */
        iterator
        begin()
        {
          return iterator(this, 0);
        }

        /**
         * @brief Возвращает итератор на конец логической последовательности элементов.
         * @return Итератор, указывающий на позицию за последним элементом буфера.
         */
        iterator
        end()
        {
          return iterator(this, m_size);
        }

        /**
         * @brief Возвращает константный итератор на начало логической последовательности элементов.
         * @return Константный итератор, указывающий на самый старый элемент буфера.
         * @note Эта перегрузка позволяет использовать `begin()` в `const` контекстах.
         */
        const_iterator
        begin() const
        {
          return cbegin();
        }

        /**
         * @brief Возвращает константный итератор на конец логической последовательности элементов.
         * @return Константный итератор, указывающий на позицию за последним элементом буфера.
         * @note Эта перегрузка позволяет использовать `end()` в `const` контекстах.
         */
        const_iterator
        end() const
        {
          return cend();
        }

        /**
         * @brief Возвращает константный итератор на начало логической последовательности элементов.
         * @return Константный итератор, указывающий на самый старый элемент буфера.
         */
        const_iterator
        cbegin() const
        {
          return const_iterator(this, 0);
        }

        /**
         * @brief Возвращает константный итератор на конец логической последовательности элементов.
         * @return Константный итератор, указывающий на позицию за последним элементом буфера.
         */
        const_iterator
        cend() const
        {
          return const_iterator(this, m_size);
        }

        /**
         * @brief Возвращает обратный итератор на начало обратной логической последовательности элементов.
         * @return Обратный итератор, указывающий на самый новый элемент буфера.
         */
        reverse_iterator
        rbegin()
        {
          return reverse_iterator(end());
        }

        /**
         * @brief Возвращает обратный итератор на конец обратной логической последовательности элементов.
         * @return Обратный итератор, указывающий на позицию перед самым старым элементом буфера.
         */
        reverse_iterator
        rend()
        {
          return reverse_iterator(begin());
        }

        /**
         * @brief Возвращает константный обратный итератор на начало обратной логической последовательности элементов.
         * @return Константный обратный итератор, указывающий на самый новый элемент буфера.
         */
        const_reverse_iterator
        rbegin() const
        {
          return const_reverse_iterator(end());
        }

        /**
         * @brief Возвращает константный обратный итератор на конец обратной логической последовательности элементов.
         * @return Константный обратный итератор, указывающий на позицию перед самым старым элементом буфера.
         */
        const_reverse_iterator
        rend() const
        {
          return const_reverse_iterator(begin());
        }

        /**
         * @brief Возвращает константный обратный итератор на начало обратной логической последовательности элементов.
         * @return Константный обратный итератор, указывающий на самый новый элемент буфера.
         */
        const_reverse_iterator
        crbegin() const
        {
          return const_reverse_iterator(cend());
        }

        /**
         * @brief Возвращает константный обратный итератор на конец обратной логической последовательности элементов.
         * @return Константный обратный итератор, указывающий на позицию перед самым старым элементом буфера.
         */
        const_reverse_iterator
        crend() const
        {
          return const_reverse_iterator(cbegin());
        }

      private:
        allocator_type m_allocator; ///< Аллокатор для элементов типа `T`.
        slot_allocator
          m_slots_alloc;      ///< Аллокатор для внутренних слотов (`Slot`), используемый для выделения сырой памяти.
        Slot *m_slots{};      ///< Указатель на массив слотов (сырой памяти), где хранятся элементы.
        size_type m_capacity; ///< Максимальное количество элементов, которое может хранить буфер.
        size_type m_size{};   ///< Текущее количество созданных (активных) элементов в буфере.
        size_type m_head{}; ///< Физический индекс в массиве `m_slots`, указывающий на начало логического буфера (самый
                            ///< старый элемент).

        /**
         * @brief Вычисляет физический индекс в базовом массиве для заданного логического индекса.
         * @param logical_idx Логический индекс элемента (от `0` до `m_size - 1`).
         * @return Физический индекс в массиве `m_slots`.
         * @note Эта функция помечена `noexcept`.
         */
        size_type
        physical_index(size_type logical_idx) const LUMEX_NOEXCEPT_FUNCTION
        {
          return (m_head + logical_idx) % m_capacity;
        }

        // Helpers to move head circularly
        /**
         * @brief Вычисляет следующий физический индекс в круговом порядке.
         * @param idx Текущий физический индекс.
         * @return Следующий физический индекс.
         * @note Эта функция помечена `noexcept`.
         */
        size_type
        next_index(size_type idx) const LUMEX_NOEXCEPT_FUNCTION
        {
          return (idx + 1) % m_capacity;
        }

        /**
         * @brief Вычисляет предыдущий физический индекс в круговом порядке.
         * @param idx Текущий физический индекс.
         * @return Предыдущий физический индекс.
         * @note Эта функция помечена `noexcept`.
         */
        size_type
        prev_index(size_type idx) const LUMEX_NOEXCEPT_FUNCTION
        {
          return (idx + m_capacity - 1) % m_capacity;
        }

        /**
         * @brief Возвращает изменяемую ссылку на элемент по логическому индексу без проверки границ.
         * @param logical_idx Логический индекс элемента.
         * @return Изменяемая ссылка на элемент.
         */
        reference
        at_slot_ref(size_type logical_idx)
        {
          return at_slot(logical_idx);
        }

        /**
         * @brief Возвращает константную ссылку на элемент по логическому индексу без проверки границ.
         * @param logical_idx Логический индекс элемента.
         * @return Константная ссылка на элемент.
         */
        const_reference
        at_slot_cref(size_type logical_idx) const
        {
          return at_slot_const(logical_idx);
        }

        /**
         * @brief Возвращает указатель на сырое хранилище (байт) для элемента по физическому индексу.
         * @param phys_idx Физический индекс слота.
         * @return `void*` указатель на сырые данные слота.
         * @note Эта функция помечена `noexcept`.
         */
        void *
        slot_ptr(size_type phys_idx) LUMEX_NOEXCEPT_FUNCTION
        {
          // адрес именно байтового "кармана" для T, а не адрес объекта Slot
          return static_cast<void *>(m_slots[phys_idx].data); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        /**
         * @brief Возвращает константный указатель на сырое хранилище (байт) для элемента по физическому индексу.
         * @param phys_idx Физический индекс слота.
         * @return `void const*` указатель на сырые данные слота.
         * @note Эта функция помечена `noexcept`.
         */
        void const *
        slot_ptr(size_type phys_idx) const LUMEX_NOEXCEPT_FUNCTION
        {
          // адрес именно байтового "кармана" для T, а не адрес объекта Slot
          return static_cast<void const *>(
            m_slots[phys_idx].data); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        /**
         * @brief Возвращает изменяемую ссылку на элемент по логическому индексу.
         * @param logical_idx Логический индекс элемента.
         * @return Изменяемая ссылка на элемент типа `T`.
         */
        reference
        at_slot(size_type logical_idx)
        {
          return *reinterpret_cast<T *>(slot_ptr(physical_index(logical_idx)));
        }

        /**
         * @brief Возвращает константную ссылку на элемент по логическому индексу.
         * @param logical_idx Логический индекс элемента.
         * @return Константная ссылка на элемент типа `T`.
         */
        const_reference
        at_slot_const(size_type logical_idx) const
        {
          return *reinterpret_cast<T const *>(slot_ptr(physical_index(logical_idx)));
        }

        /**
         * @brief Выделяет память для `m_capacity` слотов.
         *
         * Использует `m_slots_alloc` для выделения необходимого объема памяти.
         *
         * @throws Исключения, выбрасываемые аллокатором при выделении памяти.
         */
        void
        allocate_storage()
        {
          m_slots = slot_traits::allocate(m_slots_alloc, m_capacity);
        }

        /**
         * @brief Освобождает выделенную память для слотов.
         *
         * Если `m_slots` не является `nullptr`, память освобождается с использованием `m_slots_alloc`.
         * После освобождения `m_slots` устанавливается в `nullptr`.
         *
         * @note Эта функция помечена `noexcept`.
         */
        void
        deallocate_storage() LUMEX_NOEXCEPT_FUNCTION
        {
          if(m_slots) slot_traits::deallocate(m_slots_alloc, m_slots, m_capacity);
          m_slots = nullptr;
        }
      };

      // ============= non-member comparison operators =============

      /**
       * @brief Оператор сравнения на равенство для двух `CircularBuffer`.
       *
       * Сравнивает два `CircularBuffer` на равенство поэлементно. Буферы считаются равными,
       * если они имеют одинаковый размер и все их элементы в логическом порядке равны.
       *
       * @tparam T Тип элементов в буфере.
       * @tparam Alloc Тип аллокатора буфера.
       * @param lhs Первый `CircularBuffer`.
       * @param rhs Второй `CircularBuffer`.
       * @return `true`, если буферы равны, иначе `false`.
       * @note Использует `std::equal` для поэлементного сравнения.
       */
      template <class T, class Alloc>
      inline bool
      operator==(CircularBuffer<T, Alloc> const &lhs, CircularBuffer<T, Alloc> const &rhs)
      {
        if(lhs.size() != rhs.size()) return false;
        return std::equal(lhs.begin(), lhs.end(), rhs.begin());
      }

      /**
       * @brief Оператор сравнения на неравенство для двух `CircularBuffer`.
       *
       * Сравнивает два `CircularBuffer` на неравенство. Буферы считаются неравными,
       * если они имеют разный размер или хотя бы один элемент в логическом порядке отличается.
       *
       * @tparam T Тип элементов в буфере.
       * @tparam Alloc Тип аллокатора буфера.
       * @param lhs Первый `CircularBuffer`.
       * @param rhs Второй `CircularBuffer`.
       * @return `true`, если буферы неравны, иначе `false`.
       */
      template <class T, class Alloc>
      inline bool
      operator!=(CircularBuffer<T, Alloc> const &lhs, CircularBuffer<T, Alloc> const &rhs)
      {
        return !(lhs == rhs);
      }

      /**
       * @brief Оператор "меньше" для двух `CircularBuffer`.
       *
       * Сравнивает два `CircularBuffer` лексикографически.
       *
       * @tparam T Тип элементов в буфере.
       * @tparam Alloc Тип аллокатора буфера.
       * @param lhs Первый `CircularBuffer`.
       * @param rhs Второй `CircularBuffer`.
       * @return `true`, если `lhs` лексикографически меньше `rhs`, иначе `false`.
       * @note Использует `std::lexicographical_compare` для сравнения.
       */
      template <class T, class Alloc>
      inline bool
      operator<(CircularBuffer<T, Alloc> const &lhs, CircularBuffer<T, Alloc> const &rhs)
      {
        return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
      }

      /**
       * @brief Оператор "меньше или равно" для двух `CircularBuffer`.
       *
       * Сравнивает два `CircularBuffer` лексикографически.
       *
       * @tparam T Тип элементов в буфере.
       * @tparam Alloc Тип аллокатора буфера.
       * @param lhs Первый `CircularBuffer`.
       * @param rhs Второй `CircularBuffer`.
       * @return `true`, если `lhs` лексикографически меньше или равен `rhs`, иначе `false`.
       */
      template <class T, class Alloc>
      inline bool
      operator<=(CircularBuffer<T, Alloc> const &lhs, CircularBuffer<T, Alloc> const &rhs)
      {
        return !(rhs < lhs);
      }

      /**
       * @brief Оператор "больше" для двух `CircularBuffer`.
       *
       * Сравнивает два `CircularBuffer` лексикографически.
       *
       * @tparam T Тип элементов в буфере.
       * @tparam Alloc Тип аллокатора буфера.
       * @param lhs Первый `CircularBuffer`.
       * @param rhs Второй `CircularBuffer`.
       * @return `true`, если `lhs` лексикографически больше `rhs`, иначе `false`.
       */
      template <class T, class Alloc>
      inline bool
      operator>(CircularBuffer<T, Alloc> const &lhs, CircularBuffer<T, Alloc> const &rhs)
      {
        return rhs < lhs;
      }

      /**
       * @brief Оператор "больше или равно" для двух `CircularBuffer`.
       *
       * Сравнивает два `CircularBuffer` лексикографически.
       *
       * @tparam T Тип элементов в буфере.
       * @tparam Alloc Тип аллокатора буфера.
       * @param lhs Первый `CircularBuffer`.
       * @param rhs Второй `CircularBuffer`.
       * @return `true`, если `lhs` лексикографически больше или равен `rhs`, иначе `false`.
       */
      template <class T, class Alloc>
      inline bool
      operator>=(CircularBuffer<T, Alloc> const &lhs, CircularBuffer<T, Alloc> const &rhs)
      {
        return !(lhs < rhs);
      }

#if __cplusplus >= 202002L
      /**
       * @brief Оператор "трехстороннего сравнения" (`<=>`) для двух `CircularBuffer` (C++20).
       *
       * Выполняет лексикографическое сравнение двух `CircularBuffer` с использованием `std::strong_ordering`.
       *
       * @tparam Alloc Тип аллокатора буфера.
       * @param lhs Первый `CircularBuffer`.
       * @param rhs Второй `CircularBuffer`.
       * @return Результат сравнения `std::strong_ordering::less`, `std::strong_ordering::equal` или
       *         `std::strong_ordering::greater`.
       * @note Этот оператор доступен только в C++20 и более поздних версиях.
       */
      template <class T, class Alloc>
        requires std::three_way_comparable<T>
      inline std::strong_ordering
      operator<=>(CircularBuffer<T, Alloc> const &lhs, CircularBuffer<T, Alloc> const &rhs)
      {
        // Реализация, основанная на поочередном лексикографическом сравнении элементов
        auto it1  = lhs.begin();
        auto end1 = lhs.end();
        auto it2  = rhs.begin();
        auto end2 = rhs.end();

        for(; it1 != end1 && it2 != end2; ++it1, ++it2)
          if(auto cmp = (*it1 <=> *it2); cmp != std::strong_ordering::equal) return cmp;

        // После сравнения общих элементов, сравниваем размеры
        if(it1 == end1 && it2 == end2) return std::strong_ordering::equal;
        if(it1 == end1)
        { // lhs короче rhs
          return std::strong_ordering::less;
        }
        // В противном случае (it2 == end2), rhs короче lhs
        return std::strong_ordering::greater;
      }
#endif
    } // namespace CircularBuffer
  } // namespace Core
} // namespace Lumex

// NOLINTEND(cppcoreguidelines-avoid-c-arrays, cppcoreguidelines-pro-type-reinterpret-cast)

#endif // !LUMEX_CIRCULAR_BUFFER_HPP
