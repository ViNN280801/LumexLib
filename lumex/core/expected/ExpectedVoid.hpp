#ifndef EXPECTED_VOID_HPP
#define EXPECTED_VOID_HPP

#include "Expected.hpp"

// ====================== Специализация для Expected<void, ErrorType> ======================
/**
 * @brief Специализация класса `Expected` для случая, когда успешное значение отсутствует (т.е. `void`).
 * @details Эта специализация позволяет использовать `Expected` для функций, которые либо успешно
 *          выполняются, не возвращая конкретного значения, либо возвращают ошибку.
 *          Она является аналогом `std::expected<void, E>` из C++23.
 * @tparam ErrorType Тип ошибки.
 * @note В этой специализации `has_value()` означает успешное выполнение без возвращаемого значения,
 *       а `!has_value()` означает, что объект содержит ошибку.
 */
template <typename ErrorType> class Expected<void, ErrorType>
{
public:
  // ====================== Static assertions ====================== //
  static_assert(!std::is_reference<ErrorType>::value, "Expected<void,E>: E must not be a reference");
  static_assert(!std::is_function<ErrorType>::value, "Expected<void,E>: E must not be a function type");

  // ====================== Aliases ====================== //
  using value_type      = void;
  using error_type      = ErrorType;
  using unexpected_type = Unexpected<ErrorType>;

  /**
   * @brief Алиас для создания нового Expected с другим типом успешного значения.
   * @tparam U Новый тип успешного значения.
   * @note Этот алиас упрощает типо-преобразования в пользовательском коде и тестах.
   */
  template <typename U> using rebind = Expected<U, ErrorType>;

  // ====================== Constructors ====================== //

  /**
   * @brief Конструктор по умолчанию (создает успешное состояние "void")
   * @details Создает объект `Expected` в успешном состоянии (`m_has_value = true`),
   *          не содержащий конкретного значения (так как `SuccessType` это `void`).
   * @note Гарантированно не выбрасывает исключений (`noexcept`).
   */
  LUMEX_CONSTEXPR_CTOR
  Expected() LUMEX_NOEXCEPT_FUNCTION : m_has_value(true) {}

  /**
   * @brief Конструктор копирования для специализации `Expected<void, ErrorType>`.
   * @details Создает новый объект `Expected`, копируя состояние и, если `other` содержит ошибку,
   *          копирует это значение ошибки. Если `other` находится в успешном состоянии (`void`),
   *          новый объект также будет в успешном состоянии.
   * @param[in] other Объект `Expected<void, ErrorType>`, из которого будет выполнено копирование.
   * @note Гарантия отсутствия исключений зависит от гарантий конструктора копирования `ErrorType`.
   * @throws Может выбросить исключение, если конструктор копирования `ErrorType` выбрасывает.
   */
  LUMEX_CONSTEXPR_CTOR
  Expected(Expected const &other) : m_has_value(other.m_has_value)
  {
    if(!m_has_value)
    { // Если содержит ошибку, копируем ее
      new(std::addressof(m_storage.m_error)) ErrorType(other.m_storage.m_error);
    }
  }

  /**
   * @brief Конструктор перемещения для специализации `Expected<void, ErrorType>`.
   * @details Создает новый объект `Expected`, перемещая состояние и, если `other` содержит ошибку,
   *          перемещает это значение ошибки. Если `other` находится в успешном состоянии (`void`),
   *          новый объект также будет в успешном состоянии. После выполнения `other` остается в допустимом,
   *          но неопределенном состоянии.
   * @param[in] other Объект `Expected<void, ErrorType>`, из которого будет выполнено перемещение.
   * @note Этот конструктор помечен как `noexcept` условно, если конструктор перемещения `ErrorType` не
   * выбрасывает исключений.
   * @throws Потенциально может выбросить исключение, если конструктор перемещения `ErrorType` выбрасывает.
   */
  LUMEX_CONSTEXPR_CTOR
  Expected(Expected &&other) LUMEX_NOEXCEPT_FUNCTION_CONDITIONAL(std::is_nothrow_move_constructible<ErrorType>::value)
      : m_has_value(other.m_has_value)
  {
    if(!m_has_value)
    { // Если содержит ошибку, перемещаем ее
      new(std::addressof(m_storage.m_error)) ErrorType(std::move(other.m_storage.m_error));
    }
  }

  /**
   * @brief Конструктор для создания успешного состояния "void" на месте (in-place).
   * @details Создает объект `Expected` в успешном состоянии (`m_has_value = true`).
   *          Этот конструктор явно указывает, что объект должен быть создан в "успешном" состоянии
   *          без какого-либо значения, используя тег `in_place_tag`.
   * @param[in] unused Тег `in_place_tag` для выбора этого конструктора. Не используется в теле функции.
   * @note Гарантированно не выбрасывает исключений (`noexcept`).
   */
  LUMEX_CONSTEXPR_CTOR explicit Expected(in_place_tag /* unused */) LUMEX_NOEXCEPT_FUNCTION : m_has_value(true) {}

  /**
   * @brief Конструирует ошибку in-place по тегу unexpect (void-специализация).
   */
  template <typename... Args>
  LUMEX_CONSTEXPR_CTOR explicit Expected(unexpect_t /*unused*/, Args &&...args) : m_has_value(false)
  {
    new(std::addressof(m_storage.m_error)) ErrorType(std::forward<Args>(args)...);
  }

  /**
   * @brief Конструктор из объекта `Unexpected` (копирование) для специализации `Expected<void, ErrorType>`.
   * @details Создает объект `Expected` в ошибочном состоянии (`m_has_value = false`), копируя значение ошибки
   *          из `unexp`. Этот конструктор используется, когда `Expected` должен содержать ошибку, но не имеет
   *          успешного значения (так как `SuccessType` это `void`).
   * @tparam Err Тип ошибки, который должен быть конвертируем в `ErrorType`.
   * @param[in] unexp Константная ссылка на объект `Unexpected`, содержащий ошибку.
   * @note Участие в SFINAE для предотвращения конфликтов.
   * @throws Может выбросить исключение, если конструктор копирования `ErrorType` выбрасывает.
   */
  template <typename Err = ErrorType,
            // SFINAE: включаем, только если ErrorType можно сконструировать из Err
            typename
            = typename std::enable_if<std::is_constructible<ErrorType, Err const &>::value
                                      && !std::is_same<typename std::decay<Err>::type, in_place_tag>::value
                                      && !std::is_same<typename std::decay<Err>::type, unexpect_t>::value>::type>
  LUMEX_CONSTEXPR_CTOR explicit Expected(Unexpected<Err> const &unexp) : m_has_value(false)
  {
    new(std::addressof(m_storage.m_error)) ErrorType(unexp.error());
  }

  /**
   * @brief Конструктор перемещения для специализации `Expected<void, ErrorType>`.
   * @details Создает новый объект `Expected`, перемещая состояние и, если `other` содержит ошибку,
   *          перемещает это значение ошибки. Если `other` находится в успешном состоянии (`void`),
   *          новый объект также будет в успешном состоянии. После выполнения `other` остается в допустимом,
   *          но неопределенном состоянии.
   * @param[in] other Объект `Expected<void, ErrorType>`, из которого будет выполнено перемещение.
   * @note Этот конструктор помечен как `noexcept` условно, если конструктор перемещения `ErrorType` не
   * выбрасывает исключений.
   * @throws Потенциально может выбросить исключение, если конструктор перемещения `ErrorType` выбрасывает.
   */
  template <typename Err = ErrorType,
            typename
            = typename std::enable_if<std::is_constructible<ErrorType, Err &&>::value
                                      && !std::is_same<typename std::decay<Err>::type, in_place_tag>::value
                                      && !std::is_same<typename std::decay<Err>::type, unexpect_t>::value>::type>
  LUMEX_CONSTEXPR_CTOR explicit Expected(Unexpected<Err> &&unexp)
    LUMEX_NOEXCEPT_FUNCTION_CONDITIONAL(std::is_nothrow_move_constructible<ErrorType>::value)
      : m_has_value(false)
  {
    new(std::addressof(m_storage.m_error)) ErrorType(std::move(unexp).error());
  }

  /**
   * @brief Деструктор для специализации `Expected<void, ErrorType>`.
   * @details Уничтожает хранимую ошибку, если объект находится в состоянии ошибки.
   *          Если объект находится в успешном состоянии, никаких действий не предпринимается.
   * @note Гарантированно не выбрасывает исключений (`noexcept`), если деструктор `ErrorType` не выбрасывает.
   */
  LUMEX_CONSTEXPR_DTOR ~Expected() LUMEX_NOEXCEPT_FUNCTION
  {
    if(!m_has_value)
    { // Если содержит ошибку, уничтожаем ее
      m_storage.m_error.~ErrorType();
    }
  }

  // ====================== Assignment Operators ====================== //
  /**
   * @brief Оператор присваивания копированием для специализации `Expected<void, ErrorType>`.
   * @details Присваивает содержимое другого объекта `Expected` текущему объекту.
   *          Использует идиому "copy-and-swap" для обеспечения строгой гарантии исключений (strong exception
   *          guarantee).
   * @param[in] other Объект `Expected<void, ErrorType>`, из которого будет выполнено присваивание.
   * @return Ссылка на текущий объект `Expected<void, ErrorType>`.
   * @note Этот оператор помечен как `noexcept` условно, если конструкторы/операторы перемещения/присваивания
   *       `ErrorType` не выбрасывают исключений.
   * @throws Может выбросить исключение, если конструктор копирования `Expected` или `std::swap` выбрасывают.
   */
  LUMEX_CONSTEXPR_FUNCTION Expected &
  operator=(Expected const &other) LUMEX_NOEXCEPT_FUNCTION_CONDITIONAL(
    std::is_nothrow_move_constructible<ErrorType>::value &&std::is_nothrow_move_assignable<ErrorType>::value)
  {
    Expected temp(other);
    swap(temp);
    return *this;
  }

  /**
   * @brief Оператор присваивания перемещением для специализации `Expected<void, ErrorType>`.
   * @details Присваивает содержимое другого объекта `Expected` текущему объекту путем перемещения.
   *          Использует `swap` для эффективного обмена ресурсами без дополнительных аллокаций.
   * @param[in] other Объект `Expected<void, ErrorType>`, из которого будет выполнено перемещение.
   * @return Ссылка на текущий объект `Expected<void, ErrorType>`.
   * @note Гарантированно не выбрасывает исключений (`noexcept`).
   */
  LUMEX_CONSTEXPR_FUNCTION Expected &
  operator=(Expected &&other) LUMEX_NOEXCEPT_FUNCTION
  {
    swap(other);
    return *this;
  }

  // ====================== Observers ======================
  /**
   * @brief Проверяет, содержит ли объект `Expected<void, ErrorType>` успешное состояние (т.е. "void" значение).
   * @return `true`, если объект находится в успешном состоянии, `false` в противном случае (состояние ошибки).
   * @note Эта функция не выбрасывает исключений. Используйте `[[nodiscard]]` для обеспечения обработки
   * возвращаемого значения.
   */
  LUMEX_ATTRIBUTE_NODISCARD("Return value indicates state; should always be used.")
  bool
  has_value() const LUMEX_NOEXCEPT_FUNCTION
  {
    return m_has_value;
  }

  /**
   * @brief Неявное преобразование к `bool` для специализации `Expected<void, ErrorType>`.
   * @details Позволяет использовать объект `Expected` в условных выражениях (например, `if (myExpected)`).
   * @return `true`, если объект содержит успешное состояние (void), `false` в противном случае.
   * @note Эта функция не выбрасывает исключений. Используйте `[[nodiscard]]` для обеспечения обработки
   * возвращаемого значения.
   */
  LUMEX_ATTRIBUTE_NODISCARD("Return value indicates state; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION explicit
  operator bool() const LUMEX_NOEXCEPT_FUNCTION
  {
    return m_has_value;
  }

  /**
   * @brief Проверяет наличие успешного состояния (void) и выбрасывает исключение, если его нет (lvalue).
   * @details Эта функция не возвращает значения, поскольку специализация предназначена для `void`.
   *          Она используется для проверки того, находится ли объект `Expected` в успешном состоянии,
   *          и генерирует исключение `BadExpectedAccess<ErrorType>`, если он содержит ошибку.
   * @warning Вызов этой функции, когда объект содержит ошибку, приведет к выбросу `BadExpectedAccess<ErrorType>`.
   * @throws BadExpectedAccess<ErrorType> Если объект содержит ошибку.
   * @note Эта функция не выбрасывает исключений.
   */
  LUMEX_CONSTEXPR_FUNCTION void
  value() &
  {
    if(!m_has_value) throw BadExpectedAccess<ErrorType>(m_storage.m_error);
  }

  /**
   * @brief Проверяет наличие успешного состояния (void) и выбрасывает исключение, если его нет (rvalue).
   * @details Аналогична lvalue-версии, но для rvalue ссылок. Используется для объектов `Expected`, которые
   *          перемещаются. Если объект содержит ошибку, она будет перемещена в исключение.
   * @warning Вызов этой функции, когда объект содержит ошибку, приведет к выбросу `BadExpectedAccess<ErrorType>`.
   * @throws BadExpectedAccess<ErrorType> Если объект содержит ошибку.
   * @note Эта функция не выбрасывает исключений.
   */
  LUMEX_CONSTEXPR_FUNCTION void
  value() &&
  {
    if(!m_has_value) throw BadExpectedAccess<ErrorType>(std::move(m_storage.m_error));
  }

  /**
   * @brief Проверяет наличие успешного состояния (void) и выбрасывает исключение, если его нет (const lvalue).
   * @details Аналогична lvalue-версии, но для константных lvalue ссылок. Используется для объектов `Expected`,
   *          состояние которых не должно изменяться. Если объект содержит ошибку, она будет скопирована в
   * исключение.
   * @warning Вызов этой функции, когда объект содержит ошибку, приведет к выбросу `BadExpectedAccess<ErrorType>`.
   * @throws BadExpectedAccess<ErrorType> Если объект содержит ошибку.
   * @note Эта функция не выбрасывает исключений.
   */
  LUMEX_CONSTEXPR_FUNCTION void
  value() const &
  {
    if(!m_has_value) throw BadExpectedAccess<ErrorType>(m_storage.m_error);
  }

  /**
   * @brief Проверяет наличие успешного состояния (void) и выбрасывает исключение, если его нет (const rvalue).
   * @details Аналогична rvalue-версии, но для константных rvalue ссылок. Используется для объектов `Expected`,
   *          которые перемещаются и состояние которых не должно изменяться. Если объект содержит ошибку,
   *          она будет перемещена в исключение.
   * @warning Вызов этой функции, когда объект содержит ошибку, приведет к выбросу `BadExpectedAccess<ErrorType>`.
   * @throws BadExpectedAccess<ErrorType> Если объект содержит ошибку.
   * @note Эта функция не выбрасывает исключений.
   */
  LUMEX_CONSTEXPR_FUNCTION void
  value() const &&
  {
    if(!m_has_value) throw BadExpectedAccess<ErrorType>(std::move(m_storage.m_error));
  }

  /**
   * @brief Возвращает изменяемую lvalue ссылку на хранимую ошибку.
   * @pre !has_value()
   * @warning Нарушение предусловия - неопределенное поведение (проверяется через assert в отладочных сборках).
   * @details Эта функция предназначена для получения изменяемой ссылки на ошибку, когда `Expected`
   *          находится в ошибочном состоянии. Если объект находится в успешном состоянии, выбрасывается
   *          исключение `BadExpectedAccess<ErrorType>`.
   * @return Ссылка на ошибку типа `ErrorType`.
   * @note Используйте `[[nodiscard]]` для обеспечения обработки возвращаемого значения.
   */
  LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained error; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION ErrorType &
  error() &
  {
    assert(!m_has_value && "Calling error() while value is present is undefined behavior.");
    return m_storage.m_error;
  }

  /**
   * @brief Возвращает константную lvalue ссылку на хранимую ошибку.
   * @pre !has_value()
   * @warning Нарушение предусловия - неопределенное поведение (проверяется через assert в отладочных сборках).
   * @details Эта функция предназначена для получения константной ссылки на ошибку, когда `Expected`
   *          находится в ошибочном состоянии. Если объект находится в успешном состоянии, выбрасывается
   *          исключение `BadExpectedAccess<ErrorType>`.
   * @return Константная ссылка на ошибку типа `ErrorType`.
   * @note Используйте `[[nodiscard]]` для обеспечения обработки возвращаемого значения.
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
   * @details Эта функция предназначена для получения rvalue ссылки на ошибку, когда `Expected`
   *          находится в ошибочном состоянии. Если объект находится в успешном состоянии, выбрасывается
   *          исключение `BadExpectedAccess<ErrorType>`. Ошибка перемещается.
   * @return rvalue ссылка на ошибку типа `ErrorType`.
   * @note Используйте `[[nodiscard]]` для обеспечения обработки возвращаемого значения.
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
   * @details Эта функция предназначена для получения константной rvalue ссылки на ошибку, когда `Expected`
   *          находится в ошибочном состоянии. Если объект находится в успешном состоянии, выбрасывается
   *          исключение `BadExpectedAccess<ErrorType>`. Ошибка перемещается.
   * @return Константная rvalue ссылка на ошибку типа `ErrorType`.
   * @note Используйте `[[nodiscard]]` для обеспечения обработки возвращаемого значения.
   */
  LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained error; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION ErrorType const &&
  error() const &&
  {
    assert(!m_has_value && "Calling error() while value is present is undefined behavior.");
    return std::move(m_storage.m_error);
  }

  /**
   * @brief Возвращает хранимую ошибку, если она присутствует, иначе — ошибку по умолчанию.
   * @tparam U Тип значения по умолчанию; должен быть конвертируем в `ErrorType`.
   * @param[in] default_error Ошибка, которая будет возвращена, если объект находится в успешном состоянии.
   * @return Хранимая ошибка или `default_error`.
   * @note Метод не бросает сам по себе, но приведение/копирование/перемещение `U -> ErrorType` может бросать.
   * @note Используйте `[[nodiscard]]` для обеспечения обработки возвращаемого значения.
   */
  template <typename U>
  LUMEX_ATTRIBUTE_NODISCARD("Return value is the contained error or a default; should always be used.")
  LUMEX_CONSTEXPR_FUNCTION ErrorType error_or(U &&default_error) const &
  {
    return !m_has_value ? m_storage.m_error : static_cast<ErrorType>(std::forward<U>(default_error));
  }

  /**
   * @brief Возвращает ошибку, если она есть, либо заданное значение ошибки; rvalue‑перегрузка.
   * @details Для временных объектов: при наличии ошибки она перемещается наружу; иначе
   *          возвращается сконструированная из `default_error` копия/перемещение.
   *
   * @tparam G Тип аргумента по умолчанию (по умолчанию равен `ErrorType`).
   * @param default_error Значение ошибки по умолчанию.
   * @return ErrorType
   *
   * @note Эта перегрузка позволяет избежать лишнего копирования реальной ошибки за счёт перемещения.
   * @par Гарантии исключений
   *      Может бросать, если копирование/перемещение `ErrorType` или приведение `G -> ErrorType` может бросать.
   */
  template <typename G = ErrorType>
  LUMEX_ATTRIBUTE_NODISCARD("Return value is the error or a default-constructed substitute; should be used.")
  LUMEX_CONSTEXPR_FUNCTION ErrorType error_or(G &&default_error) &&
  {
    if(!m_has_value) return std::move(m_storage.m_error);
    return static_cast<ErrorType>(std::forward<G>(default_error));
  }

  /**
   * @brief Оператор разыменования (lvalue) для специализации `Expected<void, ErrorType>`.
   * @details Эта функция используется для подтверждения успешного состояния, но не возвращает значения.
   * @warning Предполагает, что объект `Expected` находится в успешном состоянии. Если это не так, поведение
   *          неопределено (сработает `assert`).
   * @note Эта функция не выбрасывает исключений, но требует предварительной проверки `has_value()`.
   */
  LUMEX_CONSTEXPR_FUNCTION void
    operator*()
    & LUMEX_NOEXCEPT_FUNCTION
  {
    assert(m_has_value && "Dereferencing Expected<void> without a value.");
  }

  /**
   * @brief Оператор разыменования (rvalue) для специализации `Expected<void, ErrorType>`.
   * @details Эта функция используется для подтверждения успешного состояния, но не возвращает значения.
   * @warning Предполагает, что объект `Expected` находится в успешном состоянии. Если это не так, поведение
   *          неопределено (сработает `assert`).
   * @note Эта функция не выбрасывает исключений, но требует предварительной проверки `has_value()`.
   */
  LUMEX_CONSTEXPR_FUNCTION void
    operator*()
    && LUMEX_NOEXCEPT_FUNCTION
  {
    assert(m_has_value && "Dereferencing Expected<void> without a value.");
  }

  /**
   * @brief Константный оператор разыменования (lvalue) для специализации `Expected<void, ErrorType>`.
   * @details Эта функция используется для подтверждения успешного состояния, но не возвращает значения.
   * @warning Предполагает, что объект `Expected` находится в успешном состоянии. Если это не так, поведение
   *          неопределено (сработает `assert`).
   * @note Эта функция не выбрасывает исключений, но требует предварительной проверки `has_value()`.
   */
  LUMEX_CONSTEXPR_FUNCTION void
  operator*() const &LUMEX_NOEXCEPT_FUNCTION
  {
    assert(m_has_value && "Dereferencing Expected<void> without a value.");
  }

  /**
   * @brief Константный оператор разыменования (rvalue) для специализации `Expected<void, ErrorType>`.
   * @details Эта функция используется для подтверждения успешного состояния, но не возвращает значения.
   * @warning Предполагает, что объект `Expected` находится в успешном состоянии. Если это не так, поведение
   *          неопределено (сработает `assert`).
   * @note Эта функция не выбрасывает исключений, но требует предварительной проверки `has_value()`.
   */
  LUMEX_CONSTEXPR_FUNCTION void
  operator*() const &&LUMEX_NOEXCEPT_FUNCTION
  {
    assert(m_has_value && "Dereferencing Expected<void> without a value.");
  }

  // operator->() не применим для Expected<void>

  // ====================== Modifiers ======================
  /**
   * @brief Конструирует успешное состояние "void" на месте, разрушая текущее содержимое `Expected<void,
   * ErrorType>`.
   * @details Эта функция сначала уничтожает текущее хранимое состояние (если это была ошибка), а затем
   *          переводит объект в успешное состояние без значения. Это аналог `emplace` для `void` специализации.
   * @note Гарантированно не выбрасывает исключений (`noexcept`).
   */
  LUMEX_CONSTEXPR_FUNCTION void
  emplace()
  {
    *this = Expected(in_place);
  }

  /**
   * @brief Конструирует значение `ErrorType` на месте, разрушая текущее содержимое `Expected<void, ErrorType>`.
   * @details Эта функция сначала уничтожает текущее хранимое состояние (если это была ошибка), а затем
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
    if(!m_has_value)
    {
      // Если уже была ошибка, уничтожаем ее перед пересозданием
      m_storage.m_error.~ErrorType();
    }
    new(std::addressof(m_storage.m_error))
      ErrorType(std::forward<Args>(args)...); // Конструируем ошибку на месте (placement new)
    m_has_value = false;                      // Теперь объект в ошибочном состоянии (нет значения)
    return m_storage.m_error;
  }

  /**
   * @brief Обменивает содержимое с другим объектом `Expected<void, ErrorType>`.
   * @details Обменивает флаг `m_has_value` и, при необходимости, содержимое (ошибку) с другим объектом
   * `Expected`. Если оба объекта находятся в успешном состоянии, ничего не происходит. Если оба содержат ошибку,
   *          используется `std::swap` для их обмена. Если один содержит успешное состояние, а другой ошибку,
   *          выполняется перемещение содержимого для изменения состояния обоих объектов.
   * @param[in,out] other Другой объект `Expected<void, ErrorType>` для обмена содержимым.
   * @note Гарантия отсутствия исключений зависит от `std::is_nothrow_move_constructible` и,
   *       начиная с C++17, `std::is_nothrow_swappable` для `ErrorType`.
   * @throws Потенциально может выбросить исключение, если конструкторы перемещения или `std::swap`
   *         `ErrorType` выбрасывают.
   */
  LUMEX_CONSTEXPR_FUNCTION void
  swap(Expected &other)
#if __cplusplus >= 201703L
    LUMEX_NOEXCEPT_FUNCTION_CONDITIONAL(
      std::is_nothrow_move_constructible<ErrorType>::value &&std::is_nothrow_swappable<ErrorType>::value)
#else
    LUMEX_NOEXCEPT_FUNCTION_CONDITIONAL(std::is_nothrow_move_constructible<ErrorType>::value)
#endif
  {
    if(this == &other) return;

    if(m_has_value && other.m_has_value)
    {
      // Оба содержат void-значение, ничего не делаем
    }
    else if(!m_has_value && !other.m_has_value)
    { // Оба содержат ошибку
      std::swap(m_storage.m_error, other.m_storage.m_error);
    }
    else
    { // Один с void-значением, другой с ошибкой. Нужно перемещать.
      if(m_has_value)
      { // this имеет void-значение, other имеет ошибку
        new(std::addressof(m_storage.m_error)) ErrorType(std::move(other.m_storage.m_error));
        other.m_storage.m_error.~ErrorType();
        // other теперь имеет void-значение
      }
      else
      { // this имеет ошибку, other имеет void-значение
        new(std::addressof(other.m_storage.m_error)) ErrorType(std::move(m_storage.m_error));
        m_storage.m_error.~ErrorType();
        // this теперь имеет void-значение
      }
      std::swap(m_has_value, other.m_has_value);
    }
  }

  // ====================== Monadic Operations ======================
  /**
   * @brief Применяет функцию `func` без аргументов к успешному состоянию, если оно присутствует (lvalue).
   * @details Если `Expected` содержит успешное состояние (void), `func` вызывается без аргументов,
   *          и возвращается результат `func`. `func` должна возвращать `Expected<U, ErrorType>`.
   *          Если `Expected` содержит ошибку, функция `func` не вызывается,
   *          и возвращается новый `Expected`, содержащий текущую ошибку.
   * @tparam FunctionType Тип функции, принимающей `void` (без аргументов) и возвращающей `Expected<U,
   * ErrorType>`.
   * @param[in] func Функция для применения.
   * @return `Expected<U, ErrorType>`, содержащий результат `func` или текущую ошибку.
   * @note Эта перегрузка позволяет использовать `and_then` на lvalue `Expected<void, ErrorType>` объектах.
   * @throws Потенциально может выбросить исключение, если `func` выбрасывает исключение или конструктор
   * `Expected` из ошибки выбрасывает.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType> && is_expected_concept<std::invoke_result_t<FunctionType>>
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then(FunctionType func) & -> std::invoke_result_t<FunctionType>
  {
    if(m_has_value) return func();
    return std::invoke_result_t<FunctionType>(Unexpected<ErrorType>(m_storage.m_error));
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc = typename std::result_of<FunctionType()>::type, // Func вызывается без аргументов
            typename              = typename std::enable_if<is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then(FunctionType func) & -> ResultOfFunc
  {
    if(m_has_value) return func();
    return ResultOfFunc(Unexpected<ErrorType>(m_storage.m_error));
  }
#endif

  /**
   * @brief Применяет функцию `func` без аргументов к успешному состоянию (const lvalue), если оно присутствует.
   * @details Если `Expected` содержит успешное состояние (void), `func` вызывается без аргументов,
   *          и возвращается результат `func`. `func` должна возвращать `Expected<U, ErrorType>`.
   *          Если `Expected` содержит ошибку, функция `func` не вызывается,
   *          и возвращается новый `Expected`, содержащий текущую ошибку.
   * @tparam FunctionType Тип функции, принимающей `void` (без аргументов) и возвращающей `Expected<U,
   * ErrorType>`.
   * @param[in] func Функция для применения.
   * @return `Expected<U, ErrorType>`, содержащий результат `func` или текущую ошибку.
   * @note Эта перегрузка позволяет использовать `and_then` на константных lvalue `Expected<void, ErrorType>`
   * объектах.
   * @throws Потенциально может выбросить исключение, если `func` выбрасывает исключение или конструктор
   * `Expected` из ошибки выбрасывает.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType> && is_expected_concept<std::invoke_result_t<FunctionType>>
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then(FunctionType func) const & -> std::invoke_result_t<FunctionType>
  {
    if(m_has_value) return func();
    return std::invoke_result_t<FunctionType>(Unexpected<ErrorType>(m_storage.m_error));
  }
#else
  template <typename FunctionType, typename ResultOfFunc = typename std::result_of<FunctionType()>::type,
            typename = typename std::enable_if<is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then(FunctionType func) const & -> ResultOfFunc
  {
    if(m_has_value) return func();
    return ResultOfFunc(Unexpected<ErrorType>(m_storage.m_error));
  }
#endif

  /**
   * @brief Применяет функцию `func` без аргументов к успешному состоянию (rvalue), если оно присутствует.
   * @details Если `Expected` содержит успешное состояние (void), `func` вызывается без аргументов,
   *          и возвращается результат `func`. `func` должна возвращать `Expected<U, ErrorType>`.
   *          Если `Expected` содержит ошибку, функция `func` не вызывается,
   *          и возвращается новый `Expected`, содержащий текущую ошибку.
   * @tparam FunctionType Тип функции, принимающей `void` (без аргументов) и возвращающей `Expected<U,
   * ErrorType>`.
   * @param[in] func Функция для применения.
   * @return `Expected<U, ErrorType>`, содержащий результат `func` или текущую ошибку.
   * @note Эта перегрузка позволяет использовать `and_then` на rvalue `Expected` объектах, обеспечивая семантику
   * перемещения.
   * @throws Потенциально может выбросить исключение, если `func` выбрасывает исключение или конструктор
   * `Expected` из ошибки выбрасывает.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType> && is_expected_concept<std::invoke_result_t<FunctionType>>
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then(FunctionType func) && -> std::invoke_result_t<FunctionType>
  {
    if(m_has_value) return func();
    return std::invoke_result_t<FunctionType>(Unexpected<ErrorType>(std::move(m_storage.m_error)));
  }
#else
  template <typename FunctionType, typename ResultOfFunc = typename std::result_of<FunctionType()>::type,
            typename = typename std::enable_if<is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then(FunctionType func) && -> ResultOfFunc
  {
    if(m_has_value) return func();
    return ResultOfFunc(Unexpected<ErrorType>(std::move(m_storage.m_error)));
  }
#endif

  /**
   * @brief Применяет функцию `func` без аргументов к успешному состоянию (const rvalue), если оно присутствует.
   * @details Если `Expected` содержит успешное состояние (void), `func` вызывается без аргументов,
   *          и возвращается результат `func`. `func` должна возвращать `Expected<U, ErrorType>`.
   *          Если `Expected` содержит ошибку, функция `func` не вызывается,
   *          и возвращается новый `Expected`, содержащий текущую ошибку.
   * @tparam FunctionType Тип функции, принимающей `void` (без аргументов) и возвращающей `Expected<U,
   * ErrorType>`.
   * @param[in] func Функция для применения.
   * @return `Expected<U, ErrorType>`, содержащий результат `func` или текущую ошибку.
   * @note Эта перегрузка позволяет использовать `and_then` на константных rvalue `Expected` объектах.
   * @throws Потенциально может выбросить исключение, если `func` выбрасывает исключение или конструктор
   * `Expected` из ошибки выбрасывает.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType> && is_expected_concept<std::invoke_result_t<FunctionType>>
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then(FunctionType func) const && -> std::invoke_result_t<FunctionType>
  {
    if(m_has_value) return func();
    return std::invoke_result_t<FunctionType>(Unexpected<ErrorType>(std::move(m_storage.m_error)));
  }
#else
  template <typename FunctionType, typename ResultOfFunc = typename std::result_of<FunctionType()>::type,
            typename = typename std::enable_if<is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  and_then(FunctionType func) const && -> ResultOfFunc
  {
    if(m_has_value) return func();
    return ResultOfFunc(Unexpected<ErrorType>(std::move(m_storage.m_error)));
  }
#endif

  /**
   * @brief Применяет функцию `func` без аргументов к успешному состоянию (lvalue), если оно присутствует,
   *          и трансформирует его в новый `Expected` с непустым результатом.
   * @details Если `Expected<void, ErrorType>` находится в успешном состоянии, `func` вызывается без аргументов,
   *          и ее непустой результат используется для создания нового `Expected<ResultOfFunc, ErrorType>`.
   *          Если `Expected` содержит ошибку, `func` не вызывается, и текущая ошибка
   *          передается в новый `Expected<ResultOfFunc, ErrorType>`.
   * @tparam FunctionType Тип функции, принимающей `void` (без аргументов) и возвращающей `ResultOfFunc` (не
   * `void`).
   * @tparam ResultOfFunc Тип возвращаемого значения `func`.
   * @param[in] func Функция для применения.
   * @return `Expected<ResultOfFunc, ErrorType>`, содержащий трансформированное значение или текущую ошибку.
   * @note Эта перегрузка позволяет использовать `transform` на lvalue `Expected<void, ErrorType>` объектах
   *       для получения `Expected` с конкретным значением.
   * @throws Потенциально может выбросить исключение, если `func` выбрасывает исключение или конструктор
   *         `Expected` из значения/ошибки выбрасывает.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType> && !std::is_void_v<std::invoke_result_t<FunctionType>>
             && !is_expected_concept<std::invoke_result_t<FunctionType>>
                LUMEX_CONSTEXPR_FUNCTION auto transform(
                  FunctionType func) & -> Expected<std::invoke_result_t<FunctionType>, ErrorType>
  {
    if(m_has_value) return Expected<std::invoke_result_t<FunctionType>, ErrorType>(in_place, func());
    return Expected<std::invoke_result_t<FunctionType>, ErrorType>(Unexpected<ErrorType>(m_storage.m_error));
  }
#else
  template <typename FunctionType, typename ResultOfFunc = typename std::result_of<FunctionType()>::type,
            typename ReturnType = Expected<ResultOfFunc, ErrorType>,
            typename
            = typename std::enable_if<!std::is_void<ResultOfFunc>::value && !is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform(FunctionType func) & -> ReturnType
  {
    if(m_has_value) return ReturnType(in_place, func());
    return ReturnType(Unexpected<ErrorType>(m_storage.m_error));
  }
#endif

  /**
   * @brief Применяет функцию `func` без аргументов к успешному состоянию (lvalue), если оно присутствует,
   *          и трансформирует его в новый `Expected` с пустым (`void`) результатом.
   * @details Если `Expected<void, ErrorType>` находится в успешном состоянии, `func` вызывается без аргументов.
   *          Ее `void` результат (отсутствие результата) используется для создания нового `Expected<void,
   * ErrorType>` в успешном состоянии. Если `Expected` содержит ошибку, `func` не вызывается, и текущая ошибка
   *          передается в новый `Expected<void, ErrorType>`.
   * @tparam FunctionType Тип функции, принимающей `void` (без аргументов) и возвращающей `void`.
   * @param[in] func Функция для применения.
   * @return `Expected<void, ErrorType>`, содержащий успешное состояние (void) или текущую ошибку.
   * @note Эта перегрузка позволяет использовать `transform` на lvalue `Expected<void, ErrorType>` объектах
   *       для сохранения `Expected` в `void` специализации.
   * @throws Потенциально может выбросить исключение, если `func` выбрасывает исключение или конструктор
   *         `Expected` из ошибки выбрасывает.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType> && std::is_void_v<std::invoke_result_t<FunctionType>>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform(FunctionType func) & -> Expected<void, ErrorType>
  {
    if(m_has_value)
    {
      func();
      return Expected<void, ErrorType>(in_place);
    }
    return Expected<void, ErrorType>(Unexpected<ErrorType>(m_storage.m_error));
  }
#else
  template <typename FunctionType, typename ReturnType = Expected<void, ErrorType>,
            typename = typename std::enable_if<
              std::is_void<typename std::result_of<FunctionType()>::type>::value>::type> // Если func возвращает void
  LUMEX_CONSTEXPR_FUNCTION auto
  transform(FunctionType func) & -> ReturnType // transform в Expected<void, E>
  {
    if(m_has_value)
    {
      func();
      return ReturnType(in_place);
    }
    return ReturnType(Unexpected<ErrorType>(m_storage.m_error));
  }
#endif

  /**
   * @brief Применяет функцию `func` без аргументов и трансформирует результат в новый `Expected`.
   * @details Используется в случае, когда функция `func` возвращает непустое значение.
   *          Если объект находится в состоянии успеха (`m_has_value == true`),
   *          вызывается `func()` и результат упаковывается в новый `Expected<ResultOfFunc, ErrorType>`.
   *          Если объект содержит ошибку, вызывается возврат `Unexpected` с текущей ошибкой.
   *
   * @tparam FunctionType Тип вызываемой функции (без аргументов).
   * @tparam ResultOfFunc Тип, возвращаемый функцией `func`.
   * @tparam ReturnType Конечный возвращаемый тип (`Expected<ResultOfFunc, ErrorType>`).
   * @return Новый `Expected`, содержащий результат выполнения `func` или текущую ошибку.
   *
   * @note Эта перегрузка предназначена для вызова на lvalue-ссылках.
   * @throws Может выбросить исключение, если `func` выбрасывает.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType> && !std::is_void_v<std::invoke_result_t<FunctionType>>
             && !is_expected_concept<std::invoke_result_t<FunctionType>>
                LUMEX_CONSTEXPR_FUNCTION auto transform(FunctionType func)
                  const & -> Expected<std::invoke_result_t<FunctionType>, ErrorType>
  {
    if(m_has_value) return Expected<std::invoke_result_t<FunctionType>, ErrorType>(in_place, func());
    return Expected<std::invoke_result_t<FunctionType>, ErrorType>(Unexpected<ErrorType>(m_storage.m_error));
  }
#else
  template <typename FunctionType, typename ResultOfFunc = typename std::result_of<FunctionType()>::type,
            typename ReturnType = Expected<ResultOfFunc, ErrorType>,
            typename
            = typename std::enable_if<!std::is_void<ResultOfFunc>::value && !is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform(FunctionType func) const & -> ReturnType
  {
    if(m_has_value) return ReturnType(in_place, func());
    return ReturnType(Unexpected<ErrorType>(m_storage.m_error));
  }
#endif

  /**
   * @brief Применяет функцию `func` без аргументов, возвращающую `void`, и трансформирует результат в
   * `Expected<void, ErrorType>`.
   * @details Если объект находится в состоянии успеха, вызывается `func()` и возвращается `Expected<void,
   * ErrorType>` в успешном состоянии. Если объект содержит ошибку, возвращается `Unexpected` с этой ошибкой.
   *
   * @tparam FunctionType Тип вызываемой функции (без аргументов), возвращающей `void`.
   * @tparam ReturnType Конечный возвращаемый тип (`Expected<void, ErrorType>`).
   * @return `Expected<void, ErrorType>`, указывающий на успех или ошибку.
   *
   * @note Эта перегрузка используется для функций, не возвращающих значений.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType> && std::is_void_v<std::invoke_result_t<FunctionType>>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform(FunctionType func) const & -> Expected<void, ErrorType>
  {
    if(m_has_value)
    {
      func();
      return Expected<void, ErrorType>(in_place);
    }
    return Expected<void, ErrorType>(Unexpected<ErrorType>(m_storage.m_error));
  }
#else
  template <typename FunctionType, typename ReturnType = Expected<void, ErrorType>,
            typename = typename std::enable_if<std::is_void<typename std::result_of<FunctionType()>::type>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform(FunctionType func) const & -> ReturnType
  {
    if(m_has_value)
    {
      func();
      return ReturnType(in_place);
    }
    return ReturnType(Unexpected<ErrorType>(m_storage.m_error));
  }
#endif

  /**
   * @brief Перегрузка `transform` для rvalue-ссылки и функций, возвращающих непустое значение.
   * @details Позволяет вызывать `func()` на временных объектах `Expected`. Если значение присутствует,
   *          результат оборачивается в `Expected<ResultOfFunc, ErrorType>`. Если объект содержит ошибку,
   *          возвращается `Unexpected`.
   *
   * @tparam FunctionType Тип вызываемой функции.
   * @tparam ResultOfFunc Тип, возвращаемый функцией `func`.
   * @tparam ReturnType Конечный возвращаемый тип.
   * @return Новый `Expected`, содержащий результат выполнения `func` или текущую ошибку.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType> && !std::is_void_v<std::invoke_result_t<FunctionType>>
             && !is_expected_concept<std::invoke_result_t<FunctionType>>
                LUMEX_CONSTEXPR_FUNCTION auto transform(
                  FunctionType func) && -> Expected<std::invoke_result_t<FunctionType>, ErrorType>
  {
    if(m_has_value) return Expected<std::invoke_result_t<FunctionType>, ErrorType>(in_place, func());
    return Expected<std::invoke_result_t<FunctionType>, ErrorType>(Unexpected<ErrorType>(std::move(m_storage.m_error)));
  }
#else
  template <typename FunctionType, typename ResultOfFunc = typename std::result_of<FunctionType()>::type,
            typename ReturnType = Expected<ResultOfFunc, ErrorType>,
            typename
            = typename std::enable_if<!std::is_void<ResultOfFunc>::value && !is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform(FunctionType func) && -> ReturnType
  {
    if(m_has_value) return ReturnType(in_place, func());
    return ReturnType(Unexpected<ErrorType>(std::move(m_storage.m_error)));
  }
#endif

  /**
   * @brief Перегрузка `transform` для rvalue-ссылки и функций, возвращающих `void`.
   * @details Если объект находится в состоянии успеха, вызывается `func()` и возвращается
   *          `Expected<void, ErrorType>` в успешном состоянии. В противном случае возвращается ошибка.
   *
   * @tparam FunctionType Тип вызываемой функции.
   * @tparam ReturnType Конечный возвращаемый тип.
   * @return Новый `Expected`, указывающий на успех или ошибку.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType> && std::is_void_v<std::invoke_result_t<FunctionType>>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform(FunctionType func) && -> Expected<void, ErrorType>
  {
    if(m_has_value)
    {
      func();
      return Expected<void, ErrorType>(in_place);
    }
    return Expected<void, ErrorType>(Unexpected<ErrorType>(std::move(m_storage.m_error)));
  }
#else
  template <typename FunctionType, typename ReturnType = Expected<void, ErrorType>,
            typename = typename std::enable_if<std::is_void<typename std::result_of<FunctionType()>::type>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform(FunctionType func) && -> ReturnType
  {
    if(m_has_value)
    {
      func();
      return ReturnType(in_place);
    }
    return ReturnType(Unexpected<ErrorType>(std::move(m_storage.m_error)));
  }
#endif

  /**
   * @brief Перегрузка `transform` для const rvalue-ссылки и функций, возвращающих непустое значение.
   * @details Позволяет трансформировать временные константные объекты. Если `m_has_value == true`,
   *          вызывается `func()` и результат оборачивается в `Expected`. В противном случае возвращается ошибка.
   *
   * @tparam FunctionType Тип вызываемой функции.
   * @tparam ResultOfFunc Тип результата `func`.
   * @tparam ReturnType Конечный возвращаемый тип.
   * @return Новый `Expected` с результатом выполнения `func` или ошибкой.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType> && !std::is_void_v<std::invoke_result_t<FunctionType>>
             && !is_expected_concept<std::invoke_result_t<FunctionType>>
                LUMEX_CONSTEXPR_FUNCTION auto transform(FunctionType func)
                  const && -> Expected<std::invoke_result_t<FunctionType>, ErrorType>
  {
    if(m_has_value) return Expected<std::invoke_result_t<FunctionType>, ErrorType>(in_place, func());
    return Expected<std::invoke_result_t<FunctionType>, ErrorType>(Unexpected<ErrorType>(std::move(m_storage.m_error)));
  }
#else
  template <typename FunctionType, typename ResultOfFunc = typename std::result_of<FunctionType()>::type,
            typename ReturnType = Expected<ResultOfFunc, ErrorType>,
            typename
            = typename std::enable_if<!std::is_void<ResultOfFunc>::value && !is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform(FunctionType func) const && -> ReturnType
  {
    if(m_has_value) return ReturnType(in_place, func());
    return ReturnType(Unexpected<ErrorType>(std::move(m_storage.m_error)));
  }
#endif

  /**
   * @brief Перегрузка `transform` для const rvalue-ссылки и функций, возвращающих `void`.
   * @details Если объект находится в состоянии успеха, вызывается `func()` и возвращается
   *          `Expected<void, ErrorType>` в успешном состоянии. Если объект содержит ошибку,
   *          возвращается `Unexpected` с этой ошибкой.
   *
   * @tparam FunctionType Тип вызываемой функции.
   * @tparam ReturnType Конечный возвращаемый тип.
   * @return Новый `Expected`, указывающий на успех или ошибку.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType> && std::is_void_v<std::invoke_result_t<FunctionType>>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform(FunctionType func) const && -> Expected<void, ErrorType>
  {
    if(m_has_value)
    {
      func();
      return Expected<void, ErrorType>(in_place);
    }
    return Expected<void, ErrorType>(Unexpected<ErrorType>(std::move(m_storage.m_error)));
  }
#else
  template <typename FunctionType, typename ReturnType = Expected<void, ErrorType>,
            typename = typename std::enable_if<std::is_void<typename std::result_of<FunctionType()>::type>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform(FunctionType func) const && -> ReturnType
  {
    if(m_has_value)
    {
      func();
      return ReturnType(in_place);
    }
    return ReturnType(Unexpected<ErrorType>(std::move(m_storage.m_error)));
  }
#endif

  /**
   * @brief Обрабатывает ошибку, если она присутствует, вызвав функцию `func`.
   * @details Если объект содержит ошибку, вызывается `func(error)` и возвращается его результат.
   *          Если объект находится в состоянии успеха, возвращается пустой `Expected` с успешным состоянием.
   *
   * @tparam FunctionType Тип функции, принимающей ссылку на `ErrorType`.
   * @tparam ResultOfFunc Тип результата функции.
   * @return Результат выполнения функции `func` или пустой `Expected`.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType, ErrorType &>
             && is_expected_concept<std::invoke_result_t<FunctionType, ErrorType &>>
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else(FunctionType func) & -> std::invoke_result_t<FunctionType, ErrorType &>
  {
    if(m_has_value) return std::invoke_result_t<FunctionType, ErrorType &>(in_place);
    return func(m_storage.m_error);
  }
#else
  template <typename FunctionType, typename ResultOfFunc = typename std::result_of<FunctionType(ErrorType &)>::type,
            typename = typename std::enable_if<is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else(FunctionType func) & -> ResultOfFunc
  {
    if(m_has_value) return ResultOfFunc(in_place);
    return func(m_storage.m_error);
  }
#endif

  /**
   * @brief Обрабатывает ошибку (const lvalue), если она присутствует.
   * @details Если объект содержит ошибку, вызывается `func(const ErrorType&)`.
   *          В случае успеха возвращается пустой `Expected`.
   *
   * @tparam FunctionType Тип функции.
   * @tparam ResultOfFunc Тип результата.
   * @return Результат выполнения `func` или пустой `Expected`.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType, const ErrorType &>
             && is_expected_concept<std::invoke_result_t<FunctionType, const ErrorType &>>
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else(FunctionType func) const & -> std::invoke_result_t<FunctionType, const ErrorType &>
  {
    if(m_has_value) return std::invoke_result_t<FunctionType, ErrorType const &>(in_place);
    return func(m_storage.m_error);
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc = typename std::result_of<FunctionType(ErrorType const &)>::type,
            typename              = typename std::enable_if<is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else(FunctionType func) const & -> ResultOfFunc
  {
    if(m_has_value) return ResultOfFunc(in_place);
    return func(m_storage.m_error);
  }
#endif

  /**
   * @brief Обрабатывает ошибку (rvalue), если она присутствует.
   * @details Если объект содержит ошибку, вызывается `func(ErrorType&&)`.
   *          В случае успеха возвращается пустой `Expected`.
   *
   * @tparam FunctionType Тип функции.
   * @tparam ResultOfFunc Тип результата.
   * @return Результат выполнения `func` или пустой `Expected`.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType, ErrorType &&>
             && is_expected_concept<std::invoke_result_t<FunctionType, ErrorType &&>>
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else(FunctionType func) && -> std::invoke_result_t<FunctionType, ErrorType &&>
  {
    if(m_has_value) return std::invoke_result_t<FunctionType, ErrorType &&>(in_place);
    return func(std::move(m_storage.m_error));
  }
#else
  template <typename FunctionType, typename ResultOfFunc = typename std::result_of<FunctionType(ErrorType &&)>::type,
            typename = typename std::enable_if<is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else(FunctionType func) && -> ResultOfFunc
  {
    if(m_has_value) return ResultOfFunc(in_place);
    return func(std::move(m_storage.m_error));
  }
#endif

  /**
   * @brief Обрабатывает ошибку (const rvalue), если она присутствует.
   * @details Если объект содержит ошибку, вызывается `func(const ErrorType&&)`.
   *          В случае успеха возвращается пустой `Expected`.
   *
   * @tparam FunctionType Тип функции.
   * @tparam ResultOfFunc Тип результата.
   * @return Результат выполнения `func` или пустой `Expected`.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType, const ErrorType &&>
             && is_expected_concept<std::invoke_result_t<FunctionType, const ErrorType &&>>
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else(FunctionType func) const && -> std::invoke_result_t<FunctionType, const ErrorType &&>
  {
    if(m_has_value) return std::invoke_result_t<FunctionType, ErrorType const &&>(in_place);
    return func(std::move(m_storage.m_error));
  }
#else
  template <typename FunctionType,
            typename ResultOfFunc = typename std::result_of<FunctionType(ErrorType const &&)>::type,
            typename              = typename std::enable_if<is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  or_else(FunctionType func) const && -> ResultOfFunc
  {
    if(m_has_value) return ResultOfFunc(in_place);
    return func(std::move(m_storage.m_error));
  }
#endif

  /**
   * @brief Трансформирует ошибку, если она присутствует, применяя функцию `func`.
   * @details Если объект содержит ошибку, вызывается `func(error)` и возвращается новый
   *          `Expected<void, ResultOfFunc>`. Если объект находится в успешном состоянии,
   *          возвращается `Expected<void, ResultOfFunc>` без ошибки.
   *
   * @tparam FunctionType Тип функции, принимающей `ErrorType`.
   * @tparam ResultOfFunc Тип результата функции.
   * @tparam ReturnType Итоговый тип `Expected<void, ResultOfFunc>`.
   * @return Новый `Expected` с трансформированной ошибкой или без нее.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType, ErrorType &>
             && !std::is_void_v<std::invoke_result_t<FunctionType, ErrorType &>>
             && !is_expected_concept<std::invoke_result_t<FunctionType, ErrorType &>>
                LUMEX_CONSTEXPR_FUNCTION auto transform_error(
                  FunctionType func) & -> Expected<void, std::invoke_result_t<FunctionType, ErrorType &>>
  {
    if(m_has_value) return Expected<void, std::invoke_result_t<FunctionType, ErrorType &>>(in_place);
    return Expected<void, std::invoke_result_t<FunctionType, ErrorType &>>(
      Unexpected<std::invoke_result_t<FunctionType, ErrorType &>>(func(m_storage.m_error)));
  }
#else
  template <typename FunctionType, typename ResultOfFunc = typename std::result_of<FunctionType(ErrorType &)>::type,
            typename ReturnType = Expected<void, ResultOfFunc>,
            typename
            = typename std::enable_if<!std::is_void<ResultOfFunc>::value && !is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform_error(FunctionType func) & -> ReturnType
  {
    if(m_has_value) return ReturnType(in_place);
    return ReturnType(Unexpected<ResultOfFunc>(func(m_storage.m_error)));
  }
#endif

  /**
   * @brief Трансформирует ошибку (const lvalue).
   * @details Если объект содержит ошибку, вызывается `func(const ErrorType&)`.
   *          Если объект находится в состоянии успеха, возвращается `Expected<void, ResultOfFunc>` без ошибки.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType, const ErrorType &>
             && !std::is_void_v<std::invoke_result_t<FunctionType, const ErrorType &>>
             && !is_expected_concept<std::invoke_result_t<FunctionType, const ErrorType &>>
                LUMEX_CONSTEXPR_FUNCTION auto transform_error(FunctionType func)
                  const & -> Expected<void, std::invoke_result_t<FunctionType, const ErrorType &>>
  {
    if(m_has_value) return Expected<void, std::invoke_result_t<FunctionType, ErrorType const &>>(in_place);
    return Expected<void, std::invoke_result_t<FunctionType, ErrorType const &>>(
      Unexpected<std::invoke_result_t<FunctionType, ErrorType const &>>(func(m_storage.m_error)));
  }
#else
  template <
    typename FunctionType, typename ResultOfFunc = typename std::result_of<FunctionType(ErrorType const &)>::type,
    typename ReturnType = Expected<void, ResultOfFunc>,
    typename = typename std::enable_if<!std::is_void<ResultOfFunc>::value && !is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform_error(FunctionType func) const & -> ReturnType
  {
    if(m_has_value) return ReturnType(in_place);
    return ReturnType(Unexpected<ResultOfFunc>(func(m_storage.m_error)));
  }
#endif

  /**
   * @brief Трансформирует ошибку (rvalue).
   * @details Если объект содержит ошибку, вызывается `func(ErrorType&&)`.
   *          Если объект находится в состоянии успеха, возвращается `Expected<void, ResultOfFunc>` без ошибки.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType, ErrorType &&>
             && !std::is_void_v<std::invoke_result_t<FunctionType, ErrorType &&>>
             && !is_expected_concept<std::invoke_result_t<FunctionType, ErrorType &&>>
                LUMEX_CONSTEXPR_FUNCTION auto transform_error(
                  FunctionType func) && -> Expected<void, std::invoke_result_t<FunctionType, ErrorType &&>>
  {
    if(m_has_value) return Expected<void, std::invoke_result_t<FunctionType, ErrorType &&>>(in_place);
    return Expected<void, std::invoke_result_t<FunctionType, ErrorType &&>>(
      Unexpected<std::invoke_result_t<FunctionType, ErrorType &&>>(func(std::move(m_storage.m_error))));
  }
#else
  template <typename FunctionType, typename ResultOfFunc = typename std::result_of<FunctionType(ErrorType &&)>::type,
            typename ReturnType = Expected<void, ResultOfFunc>,
            typename
            = typename std::enable_if<!std::is_void<ResultOfFunc>::value && !is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform_error(FunctionType func) && -> ReturnType
  {
    if(m_has_value)
      return ReturnType(std::move(
        m_storage.m_error)); // Corrected: return std::move(m_storage.m_error), not std::move(m_storage.m_error)
    return ReturnType(Unexpected<ResultOfFunc>(func(std::move(m_storage.m_error))));
  }
#endif

  /**
   * @brief Трансформирует ошибку (const rvalue).
   * @details Если объект содержит ошибку, вызывается `func(const ErrorType&&)`.
   *          Если объект находится в состоянии успеха, возвращается `Expected<void, ResultOfFunc>` без ошибки.
   */
#if __cplusplus >= 202002L
  template <typename FunctionType>
    requires std::invocable<FunctionType, const ErrorType &&>
             && !std::is_void_v<std::invoke_result_t<FunctionType, const ErrorType &&>>
             && !is_expected_concept<std::invoke_result_t<FunctionType, const ErrorType &&>>
                LUMEX_CONSTEXPR_FUNCTION auto transform_error(FunctionType func)
                  const && -> Expected<void, std::invoke_result_t<FunctionType, const ErrorType &&>>
  {
    if(m_has_value) return Expected<void, std::invoke_result_t<FunctionType, ErrorType const &&>>(in_place);
    return Expected<void, std::invoke_result_t<FunctionType, ErrorType const &&>>(
      Unexpected<std::invoke_result_t<FunctionType, ErrorType const &&>>(func(std::move(m_storage.m_error))));
  }
#else
  template <
    typename FunctionType, typename ResultOfFunc = typename std::result_of<FunctionType(ErrorType const &&)>::type,
    typename ReturnType = Expected<void, ResultOfFunc>,
    typename = typename std::enable_if<!std::is_void<ResultOfFunc>::value && !is_expected<ResultOfFunc>::value>::type>
  LUMEX_CONSTEXPR_FUNCTION auto
  transform_error(FunctionType func) const && -> ReturnType
  {
    if(m_has_value)
      return ReturnType(std::move(
        m_storage.m_error)); // Corrected: return std::move(m_storage.m_error), not std::move(m_storage.m_error)
    return ReturnType(Unexpected<ResultOfFunc>(func(std::move(m_storage.m_error))));
  }
#endif

private:
  /**
   * @brief Внутреннее объединение для хранения состояния `Expected<void, ErrorType>`.
   * @details Объединение экономит память, так как одновременно активен только один из членов:
   *          фиктивный объект `Unit` для представления успешного состояния без значения
   *          или объект ошибки `ErrorType`. Жизненным циклом активного члена управляет
   *          внешний класс вручную.
   *
   * @note Конструкторы копирования/перемещения и операторы присваивания удалены, поскольку
   *       владение и время жизни активного члена контролируются классом-оберткой.
   * @note Исключения не генерируются в деструкторе, а уничтожение активного члена выполняется
   *       снаружи (RAII обеспечивается внешним классом).
   * @warning Обращение к неактивному члену объединения приводит к неопределенному поведению.
   *          Перед доступом необходимо сверяться с флагом состояния (`m_has_value` у внешнего класса).
   * @par Потокобезопасность
   *       Не потокобезопасно без внешней синхронизации. Параллельный доступ к разным ветвям
   *       объединения недопустим.
   * @par Производительность
   *       Выбор `union` устраняет лишние аллокации и уменьшает накладные расходы хранения.
   * @par Гарантии исключений
   *       Само объединение не генерирует исключений; исключения возможны только при конструировании/
   *       уничтожении активного члена, выполняемых внешним классом.
   */
  union Storage {
    /**
     * @brief Пустышка для индикации успешного состояния без значения.
     * @details Экземпляр используется исключительно как маркер; содержательно не читается.
     * @note Инициализация поля не требуется и не влияет на поведение.
     */
    Unit m_dummy{}; // Пустышка для состояния "успех"

    /**
     * @brief Хранимое значение ошибки при неуспешном состоянии.
     * @details Активно только тогда, когда внешний объект находится в состоянии ошибки.
     * @warning Доступен к использованию исключительно при активном состоянии ошибки.
     */
    ErrorType m_error;

    /**
     * @brief Конструктор по умолчанию.
     * @details Не инициализирует ни один из членов (`m_dummy`/`m_error`), так как выбор активного
     *          члена и его конструирование осуществляются внешним классом.
     * @note Гарантировано не выбрасывает исключений.
     */
    Storage() {}

    /**
     * @brief Деструктор.
     * @details Намеренно пуст; уничтожение активного члена выполняется вручную внешним классом,
     *          исходя из текущего состояния.
     * @note Гарантировано не выбрасывает исключений.
     */
    ~Storage() {}

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
  } m_storage; ///< @brief Экземпляр внутреннего хранилища; активный член выбирается внешним классом по состоянию.

  /**
   * @brief Флаг, указывающий, содержит ли `Expected` успешное значение (`true`) или ошибку (`false`).
   * @details Этот член управляет тем, какой член объединения `m_storage` является активным и, следовательно,
   *          какой объект (`SuccessType` или `ErrorType`) необходимо конструировать или уничтожать.
   */
  bool m_has_value;
};

#endif // !EXPECTED_VOID_HPP
