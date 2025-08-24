#ifndef LUMEX_EXPECTED_HPP
#define LUMEX_EXPECTED_HPP

#include <cassert>
#include <type_traits>
#include <utility>

#include "lumex/core/utility/LumexAttributes.hpp"
#include "lumex/core/utility/LumexKeywords.hpp"

#include "BadExpectedAccess.hpp"
#include "ExpectedTypes.hpp"
#include "Unexpected.hpp"

namespace Lumex
{
  namespace Core
  {
    namespace Expected
    {
      /**
       * @brief Класс, имитирующий std::expected из С++23.
       * @link https://en.cppreference.com/w/cpp/utility/expected
       * @details Представляет собой объект, который может содержать либо значение типа SuccessType,
       *          либо ошибку типа ErrorType. Это позволяет функциям возвращать либо успешный результат,
       *          либо информацию об ошибке без использования исключений для ожидаемых сбоев.
       *          Реализация использует union для экономии памяти и вручную управляет
       *          жизненным циклом хранимых объектов.
       * @tparam SuccessType Тип успешного значения.
       * @tparam ErrorType Тип ошибки.
       * @note Эта реализация не является потокобезопасной по умолчанию. Доступ к `Expected` из нескольких потоков
       *       без внешней синхронизации может привести к неопределенному поведению, если `SuccessType` или `ErrorType`
       *       не являются потокобезопасными.
       * @warning Использование `Expected` с типами, которые имеют нетривиальные конструкторы/деструкторы или
       *          выделение памяти, может быть менее производительным, чем `std::expected` из C++23,
       *          из-за ручного управления жизненным циклом объектов.
       */
      template <typename SuccessType, typename ErrorType> class Expected
      {
      public:
        // ====================== Static assertions ====================== //
        // Запрещаем быть ссылочным типом, так как:
        // 1. Данный класс (и его специализации) предназначены для
        //    владения хранимым значением - успешным или ошибочным. Если бы типы SuccessType или ErrorType были
        //    ссылочными, например `int&`, то Expected не владел бы объектом, а лишь ссылался бы на него.
        //    Такое поведение может привести к висячим ссылкам, если оригинальный объект, на который ссылается
        //    SuccessType (или ErrorType), будет уничтожен раньше, чем Expected.
        // 2. Внутренние ограничения типа `union`: стандарт С++ запрещает членам `union` быть ссылочными типами,
        //    так как ссылки не могут быть перемещаемыми или копируемыми.
        // 3. Соответствие C++23: стандартная библиотека std::expected из C++23 также явно запрещает ссылочные типы для
        //    параметра ошибки по тем же причинам. Это обеспечивает согласованность и предсказуемость поведения.
        static_assert(!std::is_reference<SuccessType>::value, "Expected<T,E>: T must not be a reference");
        static_assert(!std::is_function<SuccessType>::value, "Expected<T,E>: T must not be a function type");
        static_assert(!std::is_same<typename std::decay<SuccessType>::type, in_place_tag>::value,
                      "Expected<T,E>: T must not be in_place_tag");
        static_assert(!std::is_same<typename std::decay<SuccessType>::type, unexpect_t>::value,
                      "Expected<T,E>: T must not be unexpect_t");

        static_assert(!std::is_reference<ErrorType>::value, "Expected<T,E>: E must not be a reference");
        static_assert(!std::is_function<ErrorType>::value, "Expected<T,E>: E must not be a function type");

        // ====================== Aliases ====================== //
        using value_type      = SuccessType;
        using error_type      = ErrorType;
        using unexpected_type = Unexpected<ErrorType>;

        /**
         * @brief Алиас для создания нового Expected с другим типом успешного значения.
         * @tparam U Новый тип успешного значения.
         * @note Этот алиас упрощает типо-преобразования в пользовательском коде и тестах.
         */
        template <typename U> using rebind = Expected<U, ErrorType>;

        /// @brief Мета-признак для определения, является ли тип Expected, по умолчанию false, т.к. любой тип не
        /// является Expected.
        template <typename T> struct is_expected : std::false_type {};

        /// @brief Мета-признак для определения, является ли тип Expected. Тут true, т.к. Expected<S, E> и
        /// Expected<void, E> являются Expected.
        template <typename S, typename E> struct is_expected<Expected<S, E>> : std::true_type {};

        /// @brief Мета-признак для определения, является ли тип Expected. Тут true, т.к. специализация для void -
        /// Expected<void, E> является Expected.
        template <typename E> struct is_expected<Expected<void, E>> : std::true_type {};

        // ====================== Constructors ====================== //

        /**
         * @brief Конструктор по умолчанию.
         * @details Создает объект `Expected` в успешном состоянии, содержащий значение `SuccessType()`,
         *          инициализированное по умолчанию. Этот конструктор доступен, только если `SuccessType`
         *          может быть сконструирован по умолчанию.
         * @note Этот конструктор предполагает, что `SuccessType` является конструктороспособным по умолчанию.
         *       Если `SuccessType` не имеет конструктора по умолчанию, этот конструктор вызовет ошибку компиляции.
         * @throws Потенциально может выбросить исключение, если конструктор по умолчанию `SuccessType` выбрасывает.
         */
        LUMEX_CONSTEXPR_CTOR
        Expected() : m_storage(), m_has_value(true) { new(std::addressof(m_storage.m_value)) SuccessType(); }

        /**
         * @brief Конструктор копирования.
         * @details Создает новый объект `Expected`, копируя состояние и содержащиеся в `other` значение или ошибку.
         * @param[in] other Объект `Expected`, из которого будет выполнено копирование.
         * @note Гарантия отсутствия исключений зависит от гарантий конструкторов копирования `SuccessType` и
         * `ErrorType`.
         * @throws Может выбросить исключение, если конструктор копирования `SuccessType` или `ErrorType` выбрасывает.
         */
        LUMEX_CONSTEXPR_CTOR
        Expected(Expected const &other) : m_has_value(other.m_has_value)
        {
          if(m_has_value)
            new(std::addressof(m_storage.m_value)) SuccessType(other.m_storage.m_value);
          else
            new(std::addressof(m_storage.m_error)) ErrorType(other.m_storage.m_error);
        }

        /**
         * @brief Конструктор перемещения.
         * @details Создает новый объект `Expected`, перемещая состояние и содержащиеся в `other` значение или ошибку.
         *          После выполнения конструктора `other` остается в допустимом, но неопределенном состоянии.
         * @param[in] other Объект `Expected`, из которого будет выполнено перемещение.
         * @note Этот конструктор помечен как `noexcept` условно, если конструкторы перемещения `SuccessType` и
         * `ErrorType` не выбрасывают исключений.
         * @throws Потенциально может выбросить исключение, если конструктор перемещения `SuccessType` или `ErrorType`
         * выбрасывает.
         */
        LUMEX_CONSTEXPR_CTOR
        Expected(Expected &&other) LUMEX_NOEXCEPT_FUNCTION_CONDITIONAL(
          std::is_nothrow_move_constructible<SuccessType>::value &&std::is_nothrow_move_constructible<ErrorType>::value)
            : m_has_value(other.m_has_value)
        {
          if(m_has_value)
            new(std::addressof(m_storage.m_value)) SuccessType(std::move(other.m_storage.m_value));
          else
            new(std::addressof(m_storage.m_error)) ErrorType(std::move(other.m_storage.m_error));
        }

        /**
         * @brief Конструктор из объекта `Unexpected` (копирование).
         * @details Создает объект `Expected` в ошибочном состоянии, копируя значение ошибки из `unexp`.
         * @param[in] unexp Константная ссылка на объект `Unexpected`, содержащий ошибку.
         * @note Гарантия отсутствия исключений зависит от гарантий конструктора копирования `ErrorType`.
         * @throws Может выбросить исключение, если конструктор копирования `ErrorType` выбрасывает.
         */
        LUMEX_CONSTEXPR_CTOR explicit Expected(Unexpected<ErrorType> const &unexp) : m_storage(), m_has_value(false)
        {
          new(std::addressof(m_storage.m_error)) ErrorType(unexp.error());
        }

        /**
         * @brief Конструктор из объекта `Unexpected` (перемещение).
         * @details Создает объект `Expected` в ошибочном состоянии, перемещая значение ошибки из `unexp`.
         *          После выполнения конструктора `unexp` остается в допустимом, но неопределенном состоянии.
         * @param[in] unexp rvalue ссылка на объект `Unexpected`, содержащий ошибку.
         * @note Гарантия отсутствия исключений зависит от гарантий конструктора перемещения `ErrorType`.
         * @throws Может выбросить исключение, если конструктор перемещения `ErrorType` выбрасывает.
         */
        LUMEX_CONSTEXPR_CTOR explicit Expected(Unexpected<ErrorType> &&unexp) : m_storage(), m_has_value(false)
        {
          new(std::addressof(m_storage.m_error)) ErrorType(std::move(unexp).error());
        }

        /**
         * @brief Конструктор из успешного значения (неявное преобразование).
         * @details Создает объект `Expected` в успешном состоянии, содержащий значение `val`.
         *          Этот конструктор позволяет неявно преобразовывать `U` в `Expected<SuccessType, ErrorType>`.
         * @tparam U Тип входного значения, который должен быть конвертируем в `SuccessType`.
         * @param[in] val Значение для хранения в объекте `Expected`.
         * @note Участие в SFINAE для предотвращения конфликтов с другими конструкторами.
         * @throws Может выбросить исключение, если конструктор `SuccessType` из `U` выбрасывает.
         */
        template <typename U = SuccessType,
                  typename std::enable_if<!std::is_same<typename std::decay<U>::type, Expected>::value
                                            && !std::is_same<typename std::decay<U>::type, in_place_tag>::value
                                            && !std::is_same<typename std::decay<U>::type, unexpect_t>::value
                                            && !std::is_same<typename std::decay<U>::type, Unexpected<ErrorType>>::value
                                            && std::is_convertible<U &&, SuccessType>::value
                                            && std::is_constructible<SuccessType, U &&>::value,
                                          int>::type
                  = 0>
        LUMEX_CONSTEXPR_CTOR
        Expected(U &&val)
            : m_storage(), m_has_value(true)
        {
          new(std::addressof(m_storage.m_value)) SuccessType(std::forward<U>(val));
        }

        /**
         * @brief Конструктор для создания успешного значения на месте (in-place).
         * @details Создает объект `Expected` в успешном состоянии, конструируя `SuccessType` на месте
         *          с использованием переданных аргументов.
         * @tparam Args Типы аргументов, передаваемых конструктору `SuccessType`.
         * @param[in] unused Тег `in_place_tag` для выбора этого конструктора.
         * @param[in] args Аргументы, пересылаемые конструктору `SuccessType`.
         * @note Этот конструктор позволяет избежать ненужных копирований или перемещений при создании значения.
         * @throws Может выбросить исключение, если конструктор `SuccessType` выбрасывает.
         */
        template <typename... Args>
        LUMEX_CONSTEXPR_CTOR explicit Expected(in_place_tag /* unused */, Args &&...args)
            : m_storage(), m_has_value(true)
        {
          new(std::addressof(m_storage.m_value)) SuccessType(std::forward<Args>(args)...);
        }

        /**
         * @brief Конструирует ошибку in-place по тегу unexpect.
         * @details Переводит объект в состояние ошибки и конструирует E непосредственно в хранилище.
         */
        template <typename... Args>
        LUMEX_CONSTEXPR_CTOR explicit Expected(unexpect_t /*unused*/, Args &&...args) : m_storage(), m_has_value(false)
        {
          new(std::addressof(m_storage.m_error)) ErrorType(std::forward<Args>(args)...);
        }

        /**
         * @brief Конструктор из объекта `Unexpected` (копирование) для неявного создания ошибки.
         * @details Создает объект `Expected` в ошибочном состоянии, копируя значение ошибки из `unex`.
         *          Предоставляет альтернативный способ инициализации `Expected` с ошибкой.
         * @tparam Err Тип ошибки, который должен быть конвертируем в `ErrorType`.
         * @param[in] unex Константная ссылка на объект `Unexpected<Err>`, содержащий ошибку.
         * @note Гарантия отсутствия исключений зависит от гарантий конструктора копирования `ErrorType`.
         * @throws Может выбросить исключение, если конструктор копирования `ErrorType` выбрасывает.
         */
        template <typename Err = ErrorType,
                  typename
                  = typename std::enable_if<std::is_constructible<ErrorType, Err const &>::value
                                            && !std::is_same<typename std::decay<Err>::type, in_place_tag>::value
                                            && !std::is_same<typename std::decay<Err>::type, unexpect_t>::value>::type>
        LUMEX_CONSTEXPR_CTOR explicit Expected(Unexpected<Err> const &unex) : m_storage(), m_has_value(false)
        {
          new(std::addressof(m_storage.m_error)) ErrorType(unex.error());
        }

        /**
         * @brief Конструктор из объекта `Unexpected` (перемещение) для неявного создания ошибки.
         * @details Создает объект `Expected` в ошибочном состоянии, перемещая значение ошибки из `unex`.
         *          Предоставляет альтернативный способ инициализации `Expected` с ошибкой, избегая копирования.
         * @tparam Err Тип ошибки, который должен быть конвертируем в `ErrorType`.
         * @param[in] unex rvalue ссылка на объект `Unexpected<Err>`, содержащий ошибку.
         * @note Гарантия отсутствия исключений зависит от гарантий конструктора перемещения `ErrorType`.
         * @throws Может выбросить исключение, если конструктор перемещения `ErrorType` выбрасывает.
         */
        template <typename Err = ErrorType,
                  typename
                  = typename std::enable_if<std::is_constructible<ErrorType, Err &&>::value
                                            && !std::is_same<typename std::decay<Err>::type, in_place_tag>::value
                                            && !std::is_same<typename std::decay<Err>::type, unexpect_t>::value>::type>
        LUMEX_CONSTEXPR_CTOR explicit Expected(Unexpected<Err> &&unex) : m_storage(), m_has_value(false)
        {
          new(std::addressof(m_storage.m_error)) ErrorType(std::move(unex).error());
        }

        /**
         * @brief Деструктор.
         * @details Уничтожает хранимое значение `SuccessType` или ошибку `ErrorType` в зависимости от текущего
         * состояния. Обеспечивает корректное освобождение ресурсов, вызывая деструктор для активного члена объединения.
         * @note Гарантированно не выбрасывает исключений (`noexcept`), если деструкторы `SuccessType` и `ErrorType` не
         * выбрасывают.
         */
        LUMEX_CONSTEXPR_DTOR ~Expected() LUMEX_NOEXCEPT_FUNCTION { destroy_value(); }

        // ====================== Assignment Operators ====================== //

        /**
         * @brief Оператор присваивания копированием.
         * @details Присваивает содержимое другого объекта `Expected` текущему объекту.
         *          Использует идиому "copy-and-swap" для обеспечения строгой гарантии исключений (strong exception
         * guarantee).
         * @param[in] other Объект `Expected`, из которого будет выполнено присваивание.
         * @return Ссылка на текущий объект `Expected`.
         * @note Этот оператор помечен как `noexcept` условно, если конструкторы/операторы перемещения/присваивания
         *       `SuccessType` и `ErrorType` не выбрасывают исключений.
         * @throws Может выбросить исключение, если конструктор копирования `Expected` или `std::swap` выбрасывают.
         */
        LUMEX_CONSTEXPR_FUNCTION Expected &
        operator=(Expected const &other) LUMEX_NOEXCEPT_FUNCTION_CONDITIONAL(
          std::is_nothrow_move_constructible<SuccessType>::value &&std::is_nothrow_move_constructible<ErrorType>::value
            &&std::is_nothrow_move_assignable<SuccessType>::value &&std::is_nothrow_move_assignable<ErrorType>::value)
        {
          // 1. Создаем временную копию. Если здесь произойдет исключение,
          // наш объект (*this) останется в исходном, валидном состоянии.
          Expected temp(other);

          // 2. Обмениваем содержимое с временной копией. Эта операция не бросает исключений.
          swap(temp);
          return *this;

          // `temp` уничтожается при выходе из функции, освобождая старые ресурсы *this
        }

        /**
         * @brief Оператор присваивания перемещением.
         * @details Присваивает содержимое другого объекта `Expected` текущему объекту путем перемещения.
         *          Использует `swap` для эффективного обмена ресурсами без дополнительных аллокаций.
         * @param[in] other Объект `Expected`, из которого будет выполнено перемещение.
         * @return Ссылка на текущий объект `Expected`.
         * @note Гарантированно не выбрасывает исключений (`noexcept`).
         */
        LUMEX_CONSTEXPR_FUNCTION Expected &
        operator=(Expected &&other) LUMEX_NOEXCEPT_FUNCTION
        {
          // Просто обмениваемся ресурсами. Никаких new/delete.
          swap(other);
          return *this;
        }

        // ====================== Observers ======================

        /**
         * @brief Проверяет, содержит ли объект `Expected` успешное значение.
         * @return `true`, если объект содержит значение (успешное состояние), `false` в противном случае (состояние
         * ошибки).
         * @note Эта функция не выбрасывает исключений. Используйте `[[nodiscard]]` для обеспечения обработки
         * возвращаемого значения.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value indicates state; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION bool
        has_value() const LUMEX_NOEXCEPT_FUNCTION
        {
          return m_has_value;
        }

        /**
         * @brief Неявное преобразование к `bool`.
         * @details Позволяет использовать объект `Expected` в условных выражениях (например, `if (myExpected)`).
         * @return `true`, если объект содержит успешное значение, `false` в противном случае.
         * @note Эта функция не выбрасывает исключений. Используйте `[[nodiscard]]` для обеспечения обработки
         * возвращаемого значения.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value indicates state; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION explicit
        operator bool() const LUMEX_NOEXCEPT_FUNCTION
        {
          return has_value();
        }

        /**
         * @brief Возвращает изменяемую lvalue ссылку на успешное значение.
         * @warning Вызов этой функции, когда объект `Expected` находится в состоянии ошибки, приведет к выбросу
         * `BadExpectedAccess<ErrorType>`.
         * @return Ссылка на значение типа `SuccessType`.
         * @throws BadExpectedAccess<ErrorType> Если объект не содержит значения.
         * @note Используйте эту функцию, когда вы уверены, что `Expected` содержит значение, или готовы обработать
         * исключение. Используйте `[[nodiscard]]` для обеспечения обработки возвращаемого значения.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained value; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION SuccessType &
        value() &
        {
          if(!m_has_value) throw BadExpectedAccess<ErrorType>(m_storage.m_error);
          return m_storage.m_value;
        }

        /**
         * @brief Возвращает rvalue ссылку на успешное значение (для перемещения).
         * @warning Вызов этой функции, когда объект `Expected` находится в состоянии ошибки, приведет к выбросу
         * `BadExpectedAccess<ErrorType>`.
         * @return rvalue ссылка на значение типа `SuccessType`.
         * @throws BadExpectedAccess<ErrorType> Если объект не содержит значения.
         * @note Эта функция предназначена для перемещения значения из `Expected`. После вызова `Expected` остается в
         * допустимом, но неопределенном состоянии. Используйте `[[nodiscard]]`.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained value; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION SuccessType &&
        value() &&
        {
          if(!m_has_value)
            throw BadExpectedAccess<ErrorType>(std::move(m_storage.m_error)); // Перемещаем ошибку в исключение
          return std::move(m_storage.m_value);
        }

        /**
         * @brief Возвращает константную lvalue ссылку на успешное значение.
         * @warning Вызов этой функции, когда объект `Expected` находится в состоянии ошибки, приведет к выбросу
         * `BadExpectedAccess<ErrorType>`.
         * @return Константная ссылка на значение типа `SuccessType`.
         * @throws BadExpectedAccess<ErrorType> Если объект не содержит значения.
         * @note Используйте эту функцию для доступа к значению без его изменения. Используйте `[[nodiscard]]`.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained value; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION SuccessType const &
        value() const &
        {
          if(!m_has_value) throw BadExpectedAccess<ErrorType>(m_storage.m_error);
          return m_storage.m_value;
        }

        /**
         * @brief Возвращает константную rvalue ссылку на успешное значение.
         * @warning Вызов этой функции, когда объект `Expected` находится в состоянии ошибки, приведет к выбросу
         * `BadExpectedAccess<ErrorType>`.
         * @return rvalue константная ссылка на значение типа `SuccessType`.
         * @throws BadExpectedAccess<ErrorType> Если объект не содержит значения.
         * @note Эта функция предназначена для перемещения константного значения из `Expected`. Используйте
         * `[[nodiscard]]`.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained value; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION SuccessType const &&
        value() const &&
        {
          if(!m_has_value)
            throw BadExpectedAccess<ErrorType>(std::move(m_storage.m_error)); // Перемещаем ошибку в исключение
          return std::move(m_storage.m_value);
        }

        /**
         * @brief Возвращает изменяемую lvalue ссылку на хранимую ошибку.
         * @pre !has_value()
         * @warning Нарушение предусловия - неопределенное поведение (проверяется через assert в отладочных сборках).
         * @return Ссылка на ошибку типа `ErrorType`.
         * @note Используйте эту функцию, когда вы уверены, что `Expected` содержит ошибку. Используйте `[[nodiscard]]`.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained error; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION ErrorType &
        error() &
        {
          // Предусловие: !m_has_value.
          // Нарушение - UB (неопределенное поведение) по двум причинам:
          //
          // 1) Контракт уровня интерфейса (совместимость со стандартом C++23).
          //    В модели std::expected метод error() - непроверенный наблюдатель с предусловием,
          //    что объект находится в состоянии ошибки. При нарушении предусловия поведение не
          //    определено. Проверенный доступ предусмотрен через value(), который бросает
          //    bad_expected_access<E>. Мы сознательно сохраняем ту же модель: error() не делает
          //    дополнительных проверок/бросков, чтобы оставаться noexcept и без накладных расходов.
          //
          // 2) Правила языка для union (уровень реализации).
          //    Внутри Expected хранение реализовано через union: либо активен член со значением
          //    (или фиктивный маркер для void-успеха), либо активен член m_error. Если m_has_value == true,
          //    активен НЕ m_error. Любой доступ к неактивному члену объединения - это UB по стандарту C++.
          //    Следовательно, чтение m_storage.m_error при m_has_value == true уже само по себе
          //    приводит к неопределенному поведению на уровне языка.
          //
          // Почему не бросаем исключение здесь?
          //  - Метод должен оставаться пригодным для noexcept-контекстов (деструкторы, swap, аварийные пути).
          //  - Нулевые накладные расходы в релизе: assert удаляется под NDEBUG, не добавляя ветвлений/бросков.
          //  - Проверяемый, перехватываемый доступ уже предоставлен через value() (и предварительную
          //    проверку has_value()).
          //
          // Практическое следствие:
          //  - В отладке assert упадет сразу, показывая место нарушения контракта.
          //  - В релизе ответственность на вызывающей стороне: перед вызовом error() необходимо
          //    гарантировать !has_value() (например, через if(!has_value()) ...).
          assert(!m_has_value && "Calling error() while value is present is undefined behavior.");
          return m_storage.m_error;
        }

        /**
         * @brief Возвращает константную lvalue ссылку на хранимую ошибку.
         * @pre !has_value()
         * @warning Нарушение предусловия - неопределенное поведение (проверяется через assert в отладочных сборках).
         * @return Константная ссылка на ошибку типа `ErrorType`.
         * @note Используйте эту функцию для доступа к ошибке без ее изменения. Используйте `[[nodiscard]]`.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained error; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION ErrorType const &
        error() const &
        {
          assert(!m_has_value && "Calling error() while value is present is undefined behavior.");
          return m_storage.m_error;
        }

        /**
         * @brief Возвращает rvalue ссылку на хранимую ошибку.
         * @pre !has_value()
         * @warning Нарушение предусловия - неопределенное поведение (проверяется через assert в отладочных сборках).
         * @return rvalue ссылка на ошибку типа `ErrorType`.
         * @note Эта функция предназначена для перемещения ошибки из `Expected`. Используйте `[[nodiscard]]`.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained error; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION ErrorType &&
        error() &&
        {
          assert(!m_has_value && "Calling error() while value is present is undefined behavior.");
          return std::move(m_storage.m_error);
        }

        /**
         * @brief Возвращает константную rvalue ссылку на хранимую ошибку.
         * @pre !has_value()
         * @warning Нарушение предусловия - неопределенное поведение (проверяется через assert в отладочных сборках).
         * @return Константная rvalue ссылка на ошибку типа `ErrorType`.
         * @note Эта функция предназначена для перемещения константной ошибки из `Expected`. Используйте
         * `[[nodiscard]]`.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained error; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION ErrorType const &&
        error() const &&
        {
          assert(!m_has_value && "Calling error() while value is present is undefined behavior.");
          return std::move(m_storage.m_error);
        }

        /**
         * @brief Возвращает хранимое значение, если оно присутствует, иначе - значение по умолчанию.
         * @tparam U Тип значения по умолчанию, должен быть конвертируем в `SuccessType`.
         * @param[in] default_value Значение, которое будет возвращено, если объект не содержит успешного значения.
         * @return Хранимое значение или `default_value`.
         * @note Эта функция не выбрасывает исключений.
         * @note Используйте `[[nodiscard]]` для обеспечения обработки возвращаемого значения.
         */
        template <typename U = typename std::remove_cv<SuccessType>::type>
        LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained value or a default; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION SuccessType value_or(U &&default_value) const &
        {
          return m_has_value ? m_storage.m_value : static_cast<SuccessType>(std::forward<U>(default_value));
        }

        /**
         * @brief Возвращает хранимое значение путем перемещения, если оно присутствует, иначе - значение по умолчанию.
         * @tparam U Тип значения по умолчанию, должен быть конвертируем в `SuccessType`.
         * @param[in] default_value Значение, которое будет возвращено, если объект не содержит успешного значения.
         * @return Хранимое значение, перемещенное из `Expected`, или `default_value`.
         * @note Эта функция не выбрасывает исключений. Если `Expected` содержит значение, оно будет перемещено. После
         * этого `Expected` остается в допустимом, но неопределенном состоянии.
         * @note Используйте `[[nodiscard]]` для обеспечения обработки возвращаемого значения.
         */
        template <typename U = typename std::remove_cv<SuccessType>::type>
        LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained value or a default; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION SuccessType value_or(U &&default_value) &&
        {
          return m_has_value ? std::move(m_storage.m_value) : static_cast<SuccessType>(std::forward<U>(default_value));
        }

        /**
         * @brief Возвращает ошибку, если она есть, либо заданное значение ошибки.
         * @details Для rvalue-объекта: при наличии ошибки перемещает ее наружу; иначе
         *          возвращает сконструированную из `default_error` копию/перемещение.
         *
         * @tparam G Тип аргумента по умолчанию (по умолчанию равен ErrorType).
         * @param default_error Значение ошибки по умолчанию.
         * @return ErrorType
         *
         * @note Перегрузка для rvalue-объектов позволяет избежать лишнего копирования
         *       реальной ошибки за счет перемещения.
         * @par Гарантии исключений
         *      Может бросать, если копирование/перемещение `ErrorType` или `G -> ErrorType`
         *      может бросать.
         */
        template <typename G = ErrorType>
        LUMEX_ATTRIBUTE_NODISCARD("Return value is the error or a default-constructed substitute; should be used.")
        LUMEX_CONSTEXPR_FUNCTION ErrorType error_or(G &&default_error) &&
        {
          if(!m_has_value) return std::move(m_storage.m_error);
          return static_cast<ErrorType>(std::forward<G>(default_error));
        }

        /**
         * @brief Возвращает хранимую ошибку, если она присутствует, иначе - ошибку по умолчанию.
         * @tparam U Тип ошибки по умолчанию, должен быть конвертируем в `ErrorType`.
         * @param[in] default_error Ошибка, которая будет возвращена, если объект содержит успешное значение.
         * @return Хранимая ошибка или `default_error`.
         * @note Эта функция не выбрасывает исключений.
         * @note Используйте `[[nodiscard]]` для обеспечения обработки возвращаемого значения.
         */
        template <typename U = ErrorType>
        LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained error or a default; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION ErrorType error_or(U &&default_error) const &
        {
          return !m_has_value ? m_storage.m_error : static_cast<ErrorType>(std::forward<U>(default_error));
        }

        /**
         * @brief Оператор разыменования (lvalue).
         * @details Возвращает изменяемую lvalue ссылку на хранимое успешное значение.
         * @warning Предполагает, что объект `Expected` содержит значение. Если значение отсутствует, поведение
         * неопределено (сработает `assert`).
         * @return Ссылка на значение типа `SuccessType`.
         * @note Эта функция не выбрасывает исключений, но требует предварительной проверки `has_value()`.
         * @note Используйте `[[nodiscard]]` для обеспечения обработки возвращаемого значения.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained value; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION SuccessType &
          operator*()
          & LUMEX_NOEXCEPT_FUNCTION
        {
          assert(m_has_value && "Dereferencing Expected without a value.");
          return m_storage.m_value;
        }

        /**
         * @brief Оператор разыменования (rvalue).
         * @details Возвращает rvalue ссылку на хранимое успешное значение для перемещения.
         * @warning Предполагает, что объект `Expected` содержит значение. Если значение отсутствует, поведение
         * неопределено (сработает `assert`).
         * @return rvalue ссылка на значение типа `SuccessType`.
         * @note Эта функция не выбрасывает исключений, но требует предварительной проверки `has_value()`. После вызова
         * `Expected` остается в допустимом, но неопределенном состоянии.
         * @note Используйте `[[nodiscard]]` для обеспечения обработки возвращаемого значения.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained value; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION SuccessType &&
          operator*()
          && LUMEX_NOEXCEPT_FUNCTION
        {
          assert(m_has_value && "Dereferencing Expected without a value.");
          return std::move(m_storage.m_value);
        }

        /**
         * @brief Константный оператор разыменования (lvalue).
         * @details Возвращает константную lvalue ссылку на хранимое успешное значение.
         * @warning Предполагает, что объект `Expected` содержит значение. Если значение отсутствует, поведение
         * неопределено (сработает `assert`).
         * @return Константная ссылка на значение типа `SuccessType`.
         * @note Эта функция не выбрасывает исключений, но требует предварительной проверки `has_value()`.
         * @note Используйте `[[nodiscard]]` для обеспечения обработки возвращаемого значения.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained value; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION SuccessType const &
        operator*() const &LUMEX_NOEXCEPT_FUNCTION
        {
          assert(m_has_value && "Dereferencing Expected without a value.");
          return m_storage.m_value;
        }

        /**
         * @brief Константный оператор разыменования (rvalue).
         * @details Возвращает константную rvalue ссылку на хранимое успешное значение.
         * @warning Предполагает, что объект `Expected` содержит значение. Если значение отсутствует, поведение
         * неопределено (сработает `assert`).
         * @return Константная rvalue ссылка на значение типа `SuccessType`.
         * @note Эта функция не выбрасывает исключений, но требует предварительной проверки `has_value()`. После вызова
         * `Expected` остается в допустимом, но неопределенном состоянии.
         * @note Используйте `[[nodiscard]]` для обеспечения обработки возвращаемого значения.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained value; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION SuccessType const &&
        operator*() const &&LUMEX_NOEXCEPT_FUNCTION
        {
          assert(m_has_value && "Dereferencing Expected without a value.");
          return std::move(m_storage.m_value);
        }

        /**
         * @brief Оператор доступа к членам (lvalue).
         * @details Возвращает указатель на хранимое успешное значение.
         * @warning Предполагает, что объект `Expected` содержит значение. Если значение отсутствует, поведение
         * неопределено (сработает `assert`).
         * @return Указатель на значение типа `SuccessType`.
         * @note Эта функция не выбрасывает исключений, но требует предварительной проверки `has_value()`.
         * @note Используйте `[[nodiscard]]` для обеспечения обработки возвращаемого значения.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained value; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION SuccessType *
        operator->() LUMEX_NOEXCEPT_FUNCTION
        {
          assert(m_has_value && "Accessing Expected members without a value.");
          return std::addressof(m_storage.m_value);
        }

        /**
         * @brief Константный оператор доступа к членам (lvalue).
         * @details Возвращает константный указатель на хранимое успешное значение.
         * @warning Предполагает, что объект `Expected` содержит значение. Если значение отсутствует, поведение
         * неопределено (сработает `assert`).
         * @return Константный указатель на значение типа `SuccessType`.
         * @note Эта функция не выбрасывает исключений, но требует предварительной проверки `has_value()`.
         * @note Используйте `[[nodiscard]]` для обеспечения обработки возвращаемого значения.
         */
        LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained value; should always be used.")
        LUMEX_CONSTEXPR_FUNCTION SuccessType const *
        operator->() const LUMEX_NOEXCEPT_FUNCTION
        {
          assert(m_has_value && "Accessing Expected members without a value.");
          return std::addressof(m_storage.m_value);
        }

        // ====================== Modifiers ======================

        /**
         * @brief Конструирует значение `SuccessType` на месте, разрушая текущее содержимое `Expected`.
         * @details Эта функция сначала уничтожает текущее хранимое значение (или ошибку), а затем
         *          конструирует новое успешное значение `SuccessType` на месте, используя переданные аргументы.
         * @tparam Args Типы аргументов для конструктора `SuccessType`.
         * @param[in] args Аргументы, пересылаемые конструктору `SuccessType`.
         * @return Ссылка на только что сконструированное значение `SuccessType`.
         * @note Может выбросить исключение, если конструктор `SuccessType` выбрасывает. В случае исключения
         *       объект `Expected` может остаться в невалидном состоянии.
         */
        template <typename... Args>
        LUMEX_CONSTEXPR_FUNCTION SuccessType &
        emplace(Args &&...args)
        {
          *this = Expected(in_place, std::forward<Args>(args)...);
          return m_storage.m_value;
        }

        /**
         * @brief Конструирует значение `ErrorType` на месте, разрушая текущее содержимое `Expected`.
         * @details Эта функция сначала уничтожает текущее хранимое значение (или ошибку), а затем
         *          конструирует новую ошибку `ErrorType` на месте, используя переданные аргументы.
         * @tparam Args Типы аргументов для конструктора `ErrorType`.
         * @param[in] args Аргументы, пересылаемые конструктору `ErrorType`.
         * @return Ссылка на только что сконструированную ошибку `ErrorType`.
         * @note Может выбросить исключение, если конструктор `ErrorType` выбрасывает. В случае исключения
         *       объект `Expected` может остаться в невалидном состоянии.
         */
        template <typename... Args>
        LUMEX_CONSTEXPR_FUNCTION ErrorType &
        emplace_error(Args &&...args)
        {
          destroy_value(); // Уничтожаем текущий активный член
          new(std::addressof(m_storage.m_error)) ErrorType(std::forward<Args>(args)...); // Конструируем ошибку на месте
          m_has_value = false; // Устанавливаем состояние ошибки
          return m_storage.m_error;
        }

        /**
         * @brief Обменивает содержимое с другим объектом `Expected`.
         * @details Обменивает флаг `m_has_value` и, при необходимости, содержимое (значение или ошибку)
         *          с другим объектом `Expected`. Если оба объекта содержат значения (или ошибки),
         *          используется `std::swap` для их обмена. Если один содержит значение, а другой ошибку,
         *          выполняется перемещение содержимого для изменения состояния обоих объектов.
         * @param[in,out] other Другой объект `Expected` для обмена содержимым.
         * @note Гарантия отсутствия исключений зависит от `std::is_nothrow_move_constructible` и,
         *       начиная с C++17, `std::is_nothrow_swappable` для `SuccessType` и `ErrorType`.
         * @throws Потенциально может выбросить исключение, если конструкторы перемещения или `std::swap`
         *         `SuccessType` или `ErrorType` выбрасывают.
         */
        LUMEX_CONSTEXPR_FUNCTION void
        swap(Expected &other)
#if __cplusplus >= 201703L
          LUMEX_NOEXCEPT_FUNCTION_CONDITIONAL(
            std::is_nothrow_move_constructible_v<SuccessType> &&std::is_nothrow_move_constructible_v<ErrorType>
              &&std::is_nothrow_swappable_v<SuccessType> &&std::is_nothrow_swappable_v<ErrorType>)
#else
          LUMEX_NOEXCEPT_FUNCTION_CONDITIONAL(std::is_nothrow_move_constructible<SuccessType>::value
                                                &&std::is_nothrow_move_constructible<ErrorType>::value)
#endif
        {
          if(this == &other) return;

          if(m_has_value && other.m_has_value) { std::swap(m_storage.m_value, other.m_storage.m_value); }
          else if(!m_has_value && !other.m_has_value)
          { // Оба содержат ошибку
            std::swap(m_storage.m_error, other.m_storage.m_error);
          }
          else
          { // Один с void-значением, другой с ошибкой. Нужно перемещать.
            if(m_has_value)
            {
              ErrorType temp_error(std::move(other.m_storage.m_error));
              other.m_storage.m_error.~ErrorType();
              new(std::addressof(other.m_storage.m_value)) SuccessType(std::move(m_storage.m_value));
              m_storage.m_value.~SuccessType();
              new(std::addressof(m_storage.m_error)) ErrorType(std::move(temp_error));
            }
            else
            {
              SuccessType temp_value(std::move(other.m_storage.m_value));
              other.m_storage.m_value.~SuccessType();
              new(std::addressof(other.m_storage.m_error)) ErrorType(std::move(m_storage.m_error));
              m_storage.m_error.~ErrorType();
              new(std::addressof(m_storage.m_value)) SuccessType(std::move(temp_value));
            }
            std::swap(m_has_value, other.m_has_value);
          }
        }

        // ====================== Monadic Operations ======================

        /**
         * @brief Применяет функцию 'func' к содержащемуся значению, если оно присутствует.
         * @details Если Expected содержит значение, 'func' вызывается с этим значением,
         *          и возвращается результат 'func'. 'func' должна возвращать Expected<U, ErrorType>.
         *          Если Expected содержит ошибку, функция 'func' не вызывается,
         *          и возвращается текущая ошибка.
         * @tparam FunctionType Тип функции, принимающей SuccessType и возвращающей Expected<U, ErrorType>.
         * @param func Функция для применения.
         * @return Expected<U, ErrorType>, содержащий результат 'func' или текущую ошибку.
         */
        template <typename FunctionType,
                  typename ReturnType = typename std::result_of<FunctionType(SuccessType &)>::type,
                  typename            = typename std::enable_if<is_expected<ReturnType>::value>::type>
        LUMEX_CONSTEXPR_FUNCTION auto
        and_then(FunctionType func) & -> ReturnType
        {
          if(m_has_value) return func(m_storage.m_value);
          return ReturnType(Unexpected<ErrorType>(m_storage.m_error));
        }

        /**
         * @brief Применяет функцию `func` к содержащемуся значению (const lvalue), если оно присутствует, и возвращает
         * `Expected`.
         * @details Если `Expected` содержит значение, `func` вызывается с константной lvalue ссылкой на это значение,
         *          и возвращается результат `func`. `func` должна возвращать `Expected<U, ErrorType>`.
         *          Если `Expected` содержит ошибку, функция `func` не вызывается,
         *          и возвращается новый `Expected`, содержащий текущую ошибку.
         * @tparam FunctionType Тип функции, принимающей `const SuccessType &` и возвращающей `Expected<U, ErrorType>`.
         * @param[in] func Функция для применения.
         * @return `Expected<U, ErrorType>`, содержащий результат `func` или текущую ошибку.
         * @note Эта перегрузка позволяет использовать `and_then` на константных lvalue `Expected` объектах.
         * @throws Потенциально может выбросить исключение, если `func` выбрасывает исключение или конструктор
         * `Expected` из ошибки выбрасывает.
         */
        template <typename FunctionType,
                  typename ResultOfFunc = typename std::result_of<FunctionType(SuccessType const &)>::type,
                  typename              = typename std::enable_if<is_expected<ResultOfFunc>::value>::type>
        LUMEX_CONSTEXPR_FUNCTION auto
        and_then(FunctionType func) const & -> ResultOfFunc
        {
          if(m_has_value) return func(m_storage.m_value);
          return ResultOfFunc(Unexpected<ErrorType>(m_storage.m_error));
        }

        /**
         * @brief Применяет функцию `func` к содержащемуся значению (rvalue), если оно присутствует, и возвращает
         * `Expected`.
         * @details Если `Expected` содержит значение, `func` вызывается с rvalue ссылкой на это значение (для
         * перемещения), и возвращается результат `func`. `func` должна возвращать `Expected<U, ErrorType>`. Если
         * `Expected` содержит ошибку, функция `func` не вызывается, и возвращается новый `Expected`, содержащий текущую
         * ошибку, перемещенную из `Expected`.
         * @tparam FunctionType Тип функции, принимающей `SuccessType &&` и возвращающей `Expected<U, ErrorType>`.
         * @param[in] func Функция для применения.
         * @return `Expected<U, ErrorType>`, содержащий результат `func` или текущую ошибку.
         * @note Эта перегрузка позволяет использовать `and_then` на rvalue `Expected` объектах, обеспечивая семантику
         * перемещения.
         * @throws Потенциально может выбросить исключение, если `func` выбрасывает исключение или конструктор
         * `Expected` из ошибки выбрасывает.
         */
        template <typename FunctionType,
                  typename ResultOfFunc = typename std::result_of<FunctionType(SuccessType &&)>::type,
                  typename              = typename std::enable_if<is_expected<ResultOfFunc>::value>::type>
        LUMEX_CONSTEXPR_FUNCTION auto
        and_then(FunctionType func) && -> ResultOfFunc
        {
          if(m_has_value) return func(std::move(m_storage.m_value));
          return ResultOfFunc(Unexpected<ErrorType>(std::move(m_storage.m_error)));
        }

        /**
         * @brief Применяет функцию `func` к содержащемуся значению (const rvalue), если оно присутствует, и возвращает
         * `Expected`.
         * @details Если `Expected` содержит значение, `func` вызывается с константной rvalue ссылкой на это значение,
         *          и возвращается результат `func`. `func` должна возвращать `Expected<U, ErrorType>`.
         *          Если `Expected` содержит ошибку, функция `func` не вызывается,
         *          и возвращается новый `Expected`, содержащий текущую ошибку, перемещенную из `Expected`.
         * @tparam FunctionType Тип функции, принимающей `const SuccessType &&` и возвращающей `Expected<U, ErrorType>`.
         * @param[in] func Функция для применения.
         * @return `Expected<U, ErrorType>`, содержащий результат `func` или текущую ошибку.
         * @note Эта перегрузка позволяет использовать `and_then` на константных rvalue `Expected` объектах.
         * @throws Потенциально может выбросить исключение, если `func` выбрасывает исключение или конструктор
         * `Expected` из ошибки выбрасывает.
         */
        template <typename FunctionType,
                  typename ResultOfFunc = typename std::result_of<FunctionType(SuccessType const &&)>::type,
                  typename              = typename std::enable_if<is_expected<ResultOfFunc>::value>::type>
        LUMEX_CONSTEXPR_FUNCTION auto
        and_then(FunctionType func) const && -> ResultOfFunc
        {
          if(m_has_value) return func(std::move(m_storage.m_value));
          return ResultOfFunc(Unexpected<ErrorType>(std::move(m_storage.m_error)));
        }

        /**
         * @brief Применяет функцию 'func' к содержащемуся значению, если оно присутствует, и трансформирует его.
         * @details Если Expected содержит значение, 'func' вызывается с этим значением,
         *          и возвращается новый Expected, содержащий результат 'func'.
         *          Если Expected содержит ошибку, функция 'func' не вызывается,
         *          и возвращается Expected, содержащий текущую ошибку.
         * @tparam FunctionType Тип функции, принимающей SuccessType и возвращающей U.
         * @param func Функция для применения.
         * @return Expected<U, ErrorType>, содержащий трансформированное значение или текущую ошибку.
         */
        template <
          typename FunctionType, typename ResultOfFunc = typename std::result_of<FunctionType(SuccessType &)>::type,
          typename ReturnType = Expected<ResultOfFunc, ErrorType>,
          typename
          = typename std::enable_if<!std::is_void<ResultOfFunc>::value && !is_expected<ResultOfFunc>::value>::type>
        LUMEX_CONSTEXPR_FUNCTION auto
        transform(FunctionType func) & -> ReturnType
        {
          if(m_has_value) return ReturnType(in_place, func(m_storage.m_value));
          return ReturnType(Unexpected<ErrorType>(m_storage.m_error));
        }

        /**
         * @brief Применяет функцию `func` к содержащемуся значению (const lvalue), если оно присутствует, и
         * трансформирует его.
         * @details Если `Expected` содержит значение, `func` вызывается с константной lvalue ссылкой на это значение,
         *          и возвращается новый `Expected`, содержащий результат `func`. Если `Expected` содержит ошибку,
         *          функция `func` не вызывается, и возвращается `Expected`, содержащий текущую ошибку.
         * @tparam FunctionType Тип функции, принимающей `const SuccessType &` и возвращающей `U`.
         * @param[in] func Функция для применения.
         * @return `Expected<U, ErrorType>`, содержащий трансформированное значение или текущую ошибку.
         * @note Эта перегрузка позволяет использовать `transform` на константных lvalue `Expected` объектах.
         * @throws Потенциально может выбросить исключение, если `func` выбрасывает исключение или конструктор
         * `Expected` из ошибки выбрасывает.
         */
        template <typename FunctionType,
                  // Используем std::result_of для получения типа возвращаемого значения func
                  typename ResultOfFunc = typename std::result_of<FunctionType(SuccessType const &)>::type,
                  typename ReturnType   = Expected<ResultOfFunc, ErrorType>,
                  // SFINAE: эта перегрузка будет существовать, только если func можно вызвать
                  // с SuccessType const & и возвращаемое значение не void и не Expected.
                  typename = typename std::enable_if<!std::is_void<ResultOfFunc>::value
                                                     && !is_expected<ResultOfFunc>::value>::type>
        LUMEX_CONSTEXPR_FUNCTION auto
        transform(FunctionType func) const & -> ReturnType
        {
          if(m_has_value) return ReturnType(in_place, func(m_storage.m_value));
          // Ошибка НЕ обрабатывается, а просто перемещается в новый Expected типа unexpect
          return ReturnType(Unexpected<ErrorType>(m_storage.m_error));
        }

        /**
         * @brief Применяет функцию `func` к содержащемуся значению (rvalue), если оно присутствует, и трансформирует
         * его.
         * @details Если `Expected` содержит значение, `func` вызывается с rvalue ссылкой на это значение (для
         * перемещения), и возвращается новый `Expected`, содержащий результат `func`. Если `Expected` содержит ошибку,
         *          функция `func` не вызывается, и возвращается `Expected`, содержащий текущую ошибку, перемещенную из
         * `Expected`.
         * @tparam FunctionType Тип функции, принимающей `SuccessType &&` и возвращающей `U`.
         * @param[in] func Функция для применения.
         * @return `Expected<U, ErrorType>`, содержащий трансформированное значение или текущую ошибку.
         * @note Эта перегрузка позволяет использовать `transform` на rvalue `Expected` объектах, обеспечивая семантику
         * перемещения.
         * @throws Потенциально может выбросить исключение, если `func` выбрасывает исключение или конструктор
         * `Expected` из ошибки выбрасывает.
         */
        template <
          typename FunctionType, typename ResultOfFunc = typename std::result_of<FunctionType(SuccessType &&)>::type,
          typename ReturnType = Expected<ResultOfFunc, ErrorType>,
          typename
          = typename std::enable_if<!std::is_void<ResultOfFunc>::value && !is_expected<ResultOfFunc>::value>::type>
        LUMEX_CONSTEXPR_FUNCTION auto
        transform(FunctionType func) && -> ReturnType
        {
          if(m_has_value) return ReturnType(in_place, func(std::move(m_storage.m_value)));
          return ReturnType(Unexpected<ErrorType>(std::move(m_storage.m_error)));
        }

        /**
         * @brief Применяет функцию `func` к содержащемуся значению (const rvalue), если оно присутствует, и
         * трансформирует его.
         * @details Если `Expected` содержит значение, `func` вызывается с константной rvalue ссылкой на это значение,
         *          и возвращается новый `Expected`, содержащий результат `func`. Если `Expected` содержит ошибку,
         *          функция `func` не вызывается, и возвращается `Expected`, содержащий текущую ошибку, перемещенную из
         * `Expected`.
         * @tparam FunctionType Тип функции, принимающей `const SuccessType &&` и возвращающей `U`.
         * @param[in] func Функция для применения.
         * @return `Expected<U, ErrorType>`, содержащий трансформированное значение или текущую ошибку.
         * @note Эта перегрузка позволяет использовать `transform` на константных rvalue `Expected` объектах.
         * @throws Потенциально может выбросить исключение, если `func` выбрасывает исключение или конструктор
         * `Expected` из ошибки выбрасывает.
         */
        template <typename FunctionType,
                  typename ResultOfFunc = typename std::result_of<FunctionType(SuccessType const &&)>::type,
                  typename ReturnType   = Expected<ResultOfFunc, ErrorType>,
                  typename              = typename std::enable_if<!std::is_void<ResultOfFunc>::value
                                                                  && !is_expected<ResultOfFunc>::value>::type>
        LUMEX_CONSTEXPR_FUNCTION auto
        transform(FunctionType func) const && -> ReturnType
        {
          if(m_has_value) return ReturnType(in_place, func(std::move(m_storage.m_value)));
          return ReturnType(Unexpected<ErrorType>(std::move(m_storage.m_error)));
        }

        /**
         * @brief Применяет функцию 'func' к содержащейся ошибке, если она присутствует.
         * @details Если Expected содержит ошибку, 'func' вызывается с этой ошибкой,
         *          и возвращается результат 'func'. 'func' должна возвращать Expected<SuccessType, F_E>.
         *          Если Expected содержит значение, функция 'func' не вызывается,
         *          и возвращается текущее значение.
         * @tparam FunctionType Тип функции, принимающей ErrorType и возвращающей Expected<SuccessType, F_E>.
         * @param func Функция для применения.
         * @return Expected<SuccessType, F_E>, содержащий текущее значение или результат 'func'.
         */
        template <typename FunctionType,
                  typename ResultOfFunc = typename std::result_of<FunctionType(ErrorType &)>::type,
                  typename              = typename std::enable_if<is_expected<ResultOfFunc>::value>::type>
        LUMEX_CONSTEXPR_FUNCTION auto
        or_else(FunctionType func) & -> ResultOfFunc
        {
          if(m_has_value)
            return Expected<SuccessType, ErrorType>(in_place,
                                                    m_storage.m_value); // Возвращаем SuccessType без изменения
          return func(m_storage.m_error);
        }

        /**
         * @brief Применяет функцию `func` к содержащейся ошибке (const lvalue), если она присутствует, и возвращает
         * `Expected`.
         * @details Если `Expected` содержит ошибку, `func` вызывается с константной lvalue ссылкой на эту ошибку,
         *          и возвращается результат `func`. `func` должна возвращать `Expected<SuccessType, F_E>`.
         *          Если `Expected` содержит значение, функция `func` не вызывается,
         *          и возвращается новый `Expected`, содержащий текущее значение.
         * @tparam FunctionType Тип функции, принимающей `const ErrorType &` и возвращающей `Expected<SuccessType,
         * F_E>`.
         * @param[in] func Функция для применения.
         * @return `Expected<SuccessType, F_E>`, содержащий текущее значение или результат `func`.
         * @note Эта перегрузка позволяет использовать `or_else` на константных lvalue `Expected` объектах.
         * @throws Потенциально может выбросить исключение, если `func` выбрасывает исключение или конструктор
         * `Expected` из значения выбрасывает.
         */
        template <typename FunctionType,
                  typename ResultOfFunc = typename std::result_of<FunctionType(ErrorType const &)>::type,
                  typename              = typename std::enable_if<is_expected<ResultOfFunc>::value>::type>
        LUMEX_CONSTEXPR_FUNCTION auto
        or_else(FunctionType func) const & -> ResultOfFunc
        {
          if(m_has_value) return Expected<SuccessType, ErrorType>(in_place, m_storage.m_value);
          return func(m_storage.m_error);
        }

        /**
         * @brief Применяет функцию `func` к содержащейся ошибке (rvalue), если она присутствует, и возвращает
         * `Expected`.
         * @details Если `Expected` содержит ошибку, `func` вызывается с rvalue ссылкой на эту ошибку (для перемещения),
         *          и возвращается результат `func`. `func` должна возвращать `Expected<SuccessType, F_E>`.
         *          Если `Expected` содержит значение, функция `func` не вызывается,
         *          и возвращается новый `Expected`, содержащий текущее значение.
         * @tparam FunctionType Тип функции, принимающей `ErrorType &&` и возвращающей `Expected<SuccessType, F_E>`.
         * @param[in] func Функция для применения.
         * @return `Expected<SuccessType, F_E>`, содержащий текущее значение или результат `func`.
         * @note Эта перегрузка позволяет использовать `or_else` на rvalue `Expected` объектах, обеспечивая семантику
         * перемещения.
         * @throws Потенциально может выбросить исключение, если `func` выбрасывает исключение или конструктор
         * `Expected` из значения выбрасывает.
         */
        template <typename FunctionType,
                  typename ResultOfFunc = typename std::result_of<FunctionType(ErrorType &&)>::type,
                  typename              = typename std::enable_if<is_expected<ResultOfFunc>::value>::type>
        LUMEX_CONSTEXPR_FUNCTION auto
        or_else(FunctionType func) && -> ResultOfFunc
        {
          if(m_has_value) return Expected<SuccessType, ErrorType>(in_place, std::move(m_storage.m_value));
          return func(std::move(m_storage.m_error));
        }

        /**
         * @brief Применяет функцию `func` к содержащейся ошибке (const rvalue), если она присутствует, и возвращает
         * `Expected`.
         * @details Если `Expected` содержит ошибку, `func` вызывается с константной rvalue ссылкой на эту ошибку,
         *          и возвращается результат `func`. `func` должна возвращать `Expected<SuccessType, F_E>`.
         *          Если `Expected` содержит значение, функция `func` не вызывается,
         *          и возвращается новый `Expected`, содержащий текущее значение.
         * @tparam FunctionType Тип функции, принимающей `const ErrorType &&` и возвращающей `Expected<SuccessType,
         * F_E>`.
         * @param[in] func Функция для применения.
         * @return `Expected<SuccessType, F_E>`, содержащий текущее значение или результат `func`.
         * @note Эта перегрузка позволяет использовать `or_else` на константных rvalue `Expected` объектах.
         * @throws Потенциально может выбросить исключение, если `func` выбрасывает исключение или конструктор
         * `Expected` из значения выбрасывает.
         */
        template <typename FunctionType,
                  typename ResultOfFunc = typename std::result_of<FunctionType(ErrorType const &&)>::type,
                  typename              = typename std::enable_if<is_expected<ResultOfFunc>::value>::type>
        LUMEX_CONSTEXPR_FUNCTION auto
        or_else(FunctionType func) const && -> ResultOfFunc
        {
          if(m_has_value) return Expected<SuccessType, ErrorType>(in_place, std::move(m_storage.m_value));
          return func(std::move(m_storage.m_error));
        }

        /**
         * @brief Применяет функцию 'func' к содержащейся ошибке, если она присутствует, и трансформирует ее.
         * @details Если Expected содержит ошибку, 'func' вызывается с этой ошибкой,
         *          и возвращается новый Expected, содержащий трансформированную ошибку.
         *          Если Expected содержит значение, функция 'func' не вызывается,
         *          и возвращается Expected, содержащий текущую ошибку.
         * @tparam FunctionType Тип функции, принимающей ErrorType и возвращающей F_E.
         * @param func Функция для применения.
         * @return Expected<SuccessType, F_E>, содержащий текущее значение или трансформированную ошибку.
         */
        template <
          typename FunctionType, typename ResultOfFunc = typename std::result_of<FunctionType(ErrorType &)>::type,
          typename ReturnType = Expected<SuccessType, ResultOfFunc>,
          typename
          = typename std::enable_if<!std::is_void<ResultOfFunc>::value && !is_expected<ResultOfFunc>::value>::type>
        LUMEX_CONSTEXPR_FUNCTION auto
        transform_error(FunctionType func) & -> ReturnType
        {
          if(m_has_value) return ReturnType(in_place, m_storage.m_value);
          return ReturnType(Unexpected<ResultOfFunc>(func(m_storage.m_error)));
        }

        /**
         * @brief Применяет функцию `func` к содержащейся ошибке (const lvalue), если она присутствует, и трансформирует
         * ее.
         * @details Если `Expected` содержит ошибку, `func` вызывается с константной lvalue ссылкой на эту ошибку,
         *          и возвращается новый `Expected`, содержащий трансформированную ошибку.
         *          Если `Expected` содержит значение, функция `func` не вызывается,
         *          и возвращается `Expected`, содержащий текущее значение.
         * @tparam FunctionType Тип функции, принимающей `const ErrorType &` и возвращающей `F_E`.
         * @param[in] func Функция для применения.
         * @return `Expected<SuccessType, F_E>`, содержащий текущее значение или трансформированную ошибку.
         * @note Эта перегрузка позволяет использовать `transform_error` на константных lvalue `Expected` объектах.
         * @throws Потенциально может выбросить исключение, если `func` выбрасывает исключение или конструктор
         * `Expected` из значения выбрасывает.
         */
        template <
          typename FunctionType, typename ResultOfFunc = typename std::result_of<FunctionType(ErrorType const &)>::type,
          typename ReturnType = Expected<SuccessType, ResultOfFunc>,
          typename
          = typename std::enable_if<!std::is_void<ResultOfFunc>::value && !is_expected<ResultOfFunc>::value>::type>
        LUMEX_CONSTEXPR_FUNCTION auto
        transform_error(FunctionType func) const & -> ReturnType
        {
          if(m_has_value) return ReturnType(in_place, m_storage.m_value);
          return ReturnType(Unexpected<ResultOfFunc>(func(m_storage.m_error)));
        }

        /**
         * @brief Применяет функцию `func` к содержащейся ошибке (rvalue), если она присутствует, и трансформирует ее.
         * @details Если `Expected` содержит ошибку, `func` вызывается с rvalue ссылкой на эту ошибку (для перемещения),
         *          и возвращается новый `Expected`, содержащий трансформированную ошибку.
         *          Если `Expected` содержит значение, функция `func` не вызывается,
         *          и возвращается `Expected`, содержащий текущее значение.
         * @tparam FunctionType Тип функции, принимающей `ErrorType &&` и возвращающей `F_E`.
         * @param[in] func Функция для применения.
         * @return `Expected<SuccessType, F_E>`, содержащий текущее значение или трансформированную ошибку.
         * @note Эта перегрузка позволяет использовать `transform_error` на rvalue `Expected` объектах, обеспечивая
         * семантику перемещения.
         * @throws Потенциально может выбросить исключение, если `func` выбрасывает исключение или конструктор
         * `Expected` из значения выбрасывает.
         */
        template <
          typename FunctionType, typename ResultOfFunc = typename std::result_of<FunctionType(ErrorType &&)>::type,
          typename ReturnType = Expected<SuccessType, ResultOfFunc>,
          typename
          = typename std::enable_if<!std::is_void<ResultOfFunc>::value && !is_expected<ResultOfFunc>::value>::type>
        LUMEX_CONSTEXPR_FUNCTION auto
        transform_error(FunctionType func) && -> ReturnType
        {
          if(m_has_value) return ReturnType(in_place, std::move(m_storage.m_value));
          return ReturnType(Unexpected<ResultOfFunc>(func(std::move(m_storage.m_error))));
        }

        /**
         * @brief Применяет функцию `func` к содержащейся ошибке (const rvalue), если она присутствует, и трансформирует
         * ее.
         * @details Если `Expected` содержит ошибку, `func` вызывается с константной rvalue ссылкой на эту ошибку,
         *          и возвращается новый `Expected`, содержащий трансформированную ошибку.
         *          Если `Expected` содержит значение, функция `func` не вызывается,
         *          и возвращается `Expected`, содержащий текущее значение.
         * @tparam FunctionType Тип функции, принимающей `const ErrorType &&` и возвращающей `F_E`.
         * @param[in] func Функция для применения.
         * @return `Expected<SuccessType, F_E>`, содержащий текущее значение или трансформированную ошибку.
         * @note Эта перегрузка позволяет использовать `transform_error` на константных rvalue `Expected` объектах.
         * @throws Потенциально может выбросить исключение, если `func` выбрасывает исключение или конструктор
         * `Expected` из значения выбрасывает.
         */
        template <typename FunctionType,
                  typename ResultOfFunc = typename std::result_of<FunctionType(ErrorType const &&)>::type,
                  typename ReturnType   = Expected<SuccessType, ResultOfFunc>,
                  typename              = typename std::enable_if<!std::is_void<ResultOfFunc>::value
                                                                  && !is_expected<ResultOfFunc>::value>::type>
        LUMEX_CONSTEXPR_FUNCTION auto
        transform_error(FunctionType func) const && -> ReturnType
        {
          if(m_has_value) return ReturnType(in_place, std::move(m_storage.m_value));
          return ReturnType(Unexpected<ResultOfFunc>(func(std::move(m_storage.m_error))));
        }

      private:
        /**
         * @brief Объединение для хранения либо успешного значения, либо значения ошибки.
         * @details Используется для экономии памяти, так как объект `Expected` одновременно
         *          может содержать только одно из двух: либо `SuccessType`, либо `ErrorType`.
         *          Жизненный цикл членов объединения управляется вручную.
         * @note Конструкторы копирования и перемещения, а также операторы присваивания удалены,
         *       поскольку жизненный цикл членов управляется классом `Expected`.
         */
        union Storage {
          /**
           * @brief Хранимое успешное значение.
           * @details Активно, если `Expected` находится в успешном состоянии (`m_has_value == true`).
           */
          SuccessType m_value;

          /**
           * @brief Хранимое значение ошибки.
           * @details Активно, если `Expected` находится в состоянии ошибки (`m_has_value == false`).
           */
          ErrorType m_error;

          /**
           * @brief Конструктор по умолчанию для `Storage`.
           * @details Не выполняет никакой инициализации членов, так как их жизненный цикл
           *          управляется вручную извне.
           * @note Гарантированно не выбрасывает исключений (`noexcept`).
           */
          Storage() LUMEX_NOEXCEPT_FUNCTION {}
          /**
           * @brief Деструктор по умолчанию для `Storage`.
           * @details Не выполняет никакого уничтожения членов, так как их жизненный цикл
           *          управляется вручную извне.
           * @note Гарантированно не выбрасывает исключений (`noexcept`).
           */
          ~Storage() LUMEX_NOEXCEPT_FUNCTION {}

          /**
           * @brief Удаленный конструктор копирования.
           * @details Копирование `Storage` запрещено, чтобы предотвратить двойное управление
           *          временем жизни активного члена.
           */
          Storage(Storage const &) = delete;

          /**
           * @brief Удаленный оператор присваивания копированием.
           * @details Присваивание запрещено по тем же причинам, что и копирующий конструктор.
           */
          Storage &operator=(Storage const &) = delete;

          /**
           * @brief Удаленный конструктор перемещения.
           * @details Перемещение запрещено, поскольку перенос владения активным членом выполняется
           *          контролируемо внешним классом через явные действия.
           */
          Storage(Storage &&) = delete;

          /**
           * @brief Удаленный оператор присваивания перемещением.
           * @details Запрещен для исключения неконтролируемого переключения активного члена.
           */
          Storage &operator=(Storage &&) = delete;
        };

        /**
         * @brief Объединение `Storage`, фактически хранящее либо значение `SuccessType`, либо ошибку `ErrorType`.
         * @details Этот член является центральным для `Expected`, так как он содержит фактические данные.
         *          Доступ к `m_value` или `m_error` должен осуществляться только после проверки `m_has_value`.
         */
        Storage m_storage;
        /**
         * @brief Флаг, указывающий, содержит ли `Expected` успешное значение (`true`) или ошибку (`false`).
         * @details Этот член управляет тем, какой член объединения `m_storage` является активным и, следовательно,
         *          какой объект (`SuccessType` или `ErrorType`) необходимо конструировать или уничтожать.
         */
        bool m_has_value;

        /**
         * @brief Уничтожает активный член объединения `m_storage`.
         * @details В зависимости от значения `m_has_value`, вызывается деструктор либо для `m_value` (SuccessType),
         *          либо для `m_error` (ErrorType). Это обеспечивает корректное управление ресурсами при вызове
         *          деструктора `Expected` или при смене состояния объекта (например, при `emplace`).
         * @note Гарантированно не выбрасывает исключений (`noexcept`), если деструкторы `SuccessType` и `ErrorType` не
         * выбрасывают.
         */
        void
        destroy_value() LUMEX_NOEXCEPT_FUNCTION
        {
          if(m_has_value)
            m_storage.m_value.~SuccessType();
          else
            m_storage.m_error.~ErrorType();
        }
      };

      // ====================== Non-member functions ======================

      /**
       * @brief Сравнивает два объекта `Expected<SuccessType, ErrorType>` на равенство.
       * @details Два объекта считаются равными, если одновременно находятся в одном состоянии
       *          (оба успешны или оба содержат ошибку) и:
       *          - в случае успеха - их содержащиеся значения равны (`*lhs == *rhs`);
       *          - в случае ошибки - равны их ошибки (`lhs.error() == rhs.error()`).
       *
       * @tparam SuccessType Тип успешного значения.
       * @tparam ErrorType   Тип ошибки.
       * @param[in] lhs Левый операнд сравнения.
       * @param[in] rhs Правый операнд сравнения.
       * @return `true`, если объекты эквивалентны по состоянию и содержимому, иначе `false`.
       *
       * @note Операция требует, чтобы для `SuccessType` и `ErrorType` были определены операции сравнения `operator==`.
       * @par Потокобезопасность
       *      Не потокобезопасно при одновременном доступе к тем же экземплярам без синхронизации.
       * @par Гарантии исключений
       *      Может бросать исключения, если `operator==` у `SuccessType` или `ErrorType` бросает.
       */
      template <typename SuccessType, typename ErrorType>
      LUMEX_CONSTEXPR_FUNCTION bool
      operator==(Expected<SuccessType, ErrorType> const &lhs, Expected<SuccessType, ErrorType> const &rhs)
      {
        if(lhs.has_value() != rhs.has_value()) return false;
        if(lhs.has_value()) return *lhs == *rhs;
        return lhs.error() == rhs.error();
      }

      /**
       * @brief Сравнивает два объекта `Expected<void, ErrorType>` на равенство.
       * @details Два объекта считаются равными, если:
       *          - оба находятся в успешном состоянии (в этом случае они всегда равны);
       *          - или оба содержат ошибку и эти ошибки равны (`lhs.error() == rhs.error()`).
       *
       * @tparam ErrorType Тип ошибки.
       * @param[in] lhs Левый операнд сравнения.
       * @param[in] rhs Правый операнд сравнения.
       * @return `true`, если объекты эквивалентны по состоянию и (при ошибке) по значению ошибки, иначе `false`.
       *
       * @note Требует корректной реализации `operator==` для `ErrorType`.
       * @par Потокобезопасность
       *      Не потокобезопасно при параллельном доступе к одним и тем же экземплярам.
       * @par Гарантии исключений
       *      Может бросать исключения, если `operator==` у `ErrorType` бросает.
       */
      template <typename ErrorType>
      LUMEX_CONSTEXPR_FUNCTION bool
      operator==(Expected<void, ErrorType> const &lhs, Expected<void, ErrorType> const &rhs)
      {
        if(lhs.has_value() != rhs.has_value()) return false;
        if(lhs.has_value()) return true; // Оба void-успех
        return lhs.error() == rhs.error();
      }

      /**
       * @brief Обменивает содержимое двух объектов `Expected<SuccessType, ErrorType>`.
       * @details Вызывает `lhs.swap(rhs)`, делегируя обмен внутренней логике класса.
       *
       * @tparam SuccessType Тип успешного значения.
       * @tparam ErrorType   Тип ошибки.
       * @param[in,out] lhs Левый операнд обмена.
       * @param[in,out] rhs Правый операнд обмена.
       *
       * @note Функция помечена `noexcept` согласно макросу `LUMEX_NOEXCEPT_FUNCTION`; фактическая гарантия
       *       зависит от исключительных гарантий `swap` у `SuccessType`/`ErrorType` внутри метода класса.
       * @par Потокобезопасность
       *      Не потокобезопасно для одних и тех же объектов без внешней синхронизации.
       * @par Производительность
       *      Обмен, как правило, O(1) и не требует аллокаций памяти.
       */
      template <typename SuccessType, typename ErrorType>
      LUMEX_CONSTEXPR_FUNCTION void
      swap(Expected<SuccessType, ErrorType> &lhs, Expected<SuccessType, ErrorType> &rhs) LUMEX_NOEXCEPT_FUNCTION
      {
        lhs.swap(rhs);
      }

      /**
       * @brief Обменивает содержимое двух объектов `Expected<void, ErrorType>`.
       * @details Делегирует работу методу `swap` соответствующей специализации класса.
       *
       * @tparam ErrorType Тип ошибки.
       * @param[in,out] lhs Левый операнд обмена.
       * @param[in,out] rhs Правый операнд обмена.
       *
       * @note Фактическая `noexcept`-гарантия определяется реализацией `Expected<void, ErrorType>::swap`.
       * @par Потокобезопасность
       *      Не потокобезопасно без внешней синхронизации для тех же экземпляров.
       */
      template <typename ErrorType>
      LUMEX_CONSTEXPR_FUNCTION void
      swap(Expected<void, ErrorType> &lhs, Expected<void, ErrorType> &rhs) LUMEX_NOEXCEPT_FUNCTION
      {
        lhs.swap(rhs);
      }

      // ====================== Вспомогательные функции make_expected/make_unexpected ======================

      /**
       * @brief Создает успешный `Expected<T, ErrorType>` из значения.
       * @details Конструирует успешное состояние на месте (in-place), избегая лишних копирований/перемещений.
       *
       * @tparam ErrorType Тип ошибки.
       * @tparam U_val     Тип входного значения; после `std::decay` определяет `T = std::decay_t<U_val>`.
       * @param[in] val    Значение для инициализации успешного результата.
       * @return `Expected<std::decay_t<U_val>, ErrorType>` в успешном состоянии.
       *
       * @note Помечено `[[nodiscard]]` (через макрос), чтобы не терять результат.
       * @par Гарантии исключений
       *      Может бросать, если конструктор `T` из `U_val` бросает.
       * @par Потокобезопасность
       *      Потокобезопасность зависит от типов `T` и `ErrorType` и их конструкторов.
       */
      template <typename ErrorType, typename U_val>
      LUMEX_ATTRIBUTE_NODISCARD("Return value is an expected result; should always be used.")
      LUMEX_CONSTEXPR_FUNCTION Expected<typename std::decay<U_val>::type, ErrorType> make_expected(U_val &&val)
      {
        return Expected<typename std::decay<U_val>::type, ErrorType>(in_place, std::forward<U_val>(val));
      }

      /**
       * @brief Создает успешный `Expected<void, ErrorType>`.
       * @details Возвращает объект в успешном состоянии без значения.
       *
       * @tparam ErrorType Тип ошибки.
       * @return `Expected<void, ErrorType>` в успешном состоянии.
       *
       * @note Помечено `[[nodiscard]]` (через макрос). Полезно для API, где факт успеха важен.
       * @par Гарантии исключений
       *      Не бросает, если конструирование `Expected<void, ErrorType>` не бросает.
       */
      template <typename ErrorType>
      LUMEX_ATTRIBUTE_NODISCARD("Return value is an expected result; should always be used.")
      LUMEX_CONSTEXPR_FUNCTION Expected<void, ErrorType> make_expected()
      {
        return Expected<void, ErrorType>(in_place);
      }

      /**
       * @brief Создает ошибочный `Expected<SuccessType, E>` из значения ошибки.
       * @details Оборачивает переданный объект ошибки в `Unexpected<E>` и возвращает соответствующий `Expected`
       *          в состоянии ошибки.
       *
       * @tparam SuccessType Тип успешного значения (параметризует возвращаемый `Expected`).
       * @tparam U_err       Входной тип ошибки; итоговый тип ошибки `E = std::decay_t<U_err>`.
       * @param[in] err      Объект ошибки, который будет сохранен (копирован или перемещен) внутри `Expected`.
       * @return `Expected<SuccessType, std::decay_t<U_err>>` в состоянии ошибки.
       *
       * @note Помечено `[[nodiscard]]` (через макрос).
       * @par Гарантии исключений
       *      Может бросать, если копирование/перемещение `E` бросает.
       * @par Потокобезопасность
       *      Потокобезопасность зависит от свойств типа ошибки `E`.
       * @deprecated Используйте make_unexpected<E>(...) returning Unexpected<E> and Expected(unexpect_t, ...) вместо
       * этой функции.
       */
      template <typename SuccessType, typename U_err>
      LUMEX_ATTRIBUTE_DEPRECATED_MSG(
        "Use make_unexpected<E>(...) returning Unexpected<E> and Expected(unexpect_t, ...) instead.")
      LUMEX_CONSTEXPR_FUNCTION Expected<SuccessType, typename std::decay<U_err>::type> make_unexpected(U_err &&err)
      {
        return Expected<SuccessType, typename std::decay<U_err>::type>(
          Unexpected<typename std::decay<U_err>::type>(std::forward<U_err>(err)));
      }

      /**
       * @brief Создает ошибочный `Expected<void, ErrorType>` из значения ошибки.
       * @details Оборачивает переданный объект ошибки в `Unexpected<ErrorType>` и возвращает `Expected<void,
       * ErrorType>` в состоянии ошибки.
       *
       * @tparam ErrorType Тип ошибки.
       * @param[in] err Объект ошибки (копируется или перемещается).
       * @return `Expected<void, ErrorType>` в состоянии ошибки.
       *
       * @note Помечено `[[nodiscard]]` (через макрос).
       * @par Гарантии исключений
       *      Может бросать, если копирование/перемещение `ErrorType` бросает.
       * @par Потокобезопасность
       *      Не потокобезопасно при совместном доступе к возвращаемому объекту без синхронизации.
       * @deprecated Используйте make_unexpected<E>(...) returning Unexpected<E> and Expected(unexpect_t, ...) вместо
       * этой функции.
       */
      template <typename ErrorType>
      LUMEX_ATTRIBUTE_DEPRECATED_MSG(
        "Use make_unexpected<E>(...) returning Unexpected<E> and Expected(unexpect_t, ...) instead.")
      LUMEX_CONSTEXPR_FUNCTION Expected<void, ErrorType> make_unexpected(ErrorType &&err)
      {
        return Expected<void, ErrorType>(Unexpected<ErrorType>(std::forward<ErrorType>(err)));
      }

      /**
       * @brief Фабрика ошибки по стандарту: создает Unexpected<E> in-place.
       */
      template <typename E, typename... Args>
      LUMEX_ATTRIBUTE_NODISCARD("Return value is an unexpected value; should always be used.")
      LUMEX_CONSTEXPR_FUNCTION Unexpected<typename std::decay<E>::type> make_unexpected(Args &&...args)
      {
        using Err = typename std::decay<E>::type;
        return Unexpected<Err>(Err(std::forward<Args>(args)...));
      }
    } // namespace Expected
  } // namespace Core
} // namespace Lumex

#endif // !LUMEX_EXPECTED_HPP
