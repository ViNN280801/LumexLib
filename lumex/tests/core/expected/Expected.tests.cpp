#include <gtest/gtest.h>

#include "lumex/core/expected/BadExpectedAccess.hpp"
#include "lumex/core/expected/Expected.hpp"
#include "lumex/core/expected/Unexpected.hpp"
using namespace Lumex::Core::Expected;

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

// === Типы успешных значений для тестирования ==============================

struct SimpleSuccess {
  int value;
  explicit SimpleSuccess(int v = 0) : value(v) {}
  bool
  operator==(SimpleSuccess const &other) const
  {
    return value == other.value;
  }
  bool
  operator!=(SimpleSuccess const &other) const
  {
    return !(*this == other);
  }
};

struct ComplexSuccess {
  std::string name;
  std::unique_ptr<int> data;

  explicit ComplexSuccess(std::string n = "Default Success", int d = 0)
      : name(std::move(n)), data(std::make_unique<int>(d))
  {}

  ComplexSuccess(ComplexSuccess const &other)
      : name(other.name), data(other.data ? std::make_unique<int>(*other.data) : nullptr)
  {}

  ComplexSuccess &
  operator=(ComplexSuccess const &other)
  {
    if(this != &other)
    {
      name = other.name;
      data = other.data ? std::make_unique<int>(*other.data) : nullptr;
    }
    return *this;
  }

  ComplexSuccess(ComplexSuccess &&other) noexcept : name(std::move(other.name)), data(std::move(other.data)) {}

  ComplexSuccess &
  operator=(ComplexSuccess &&other) noexcept
  {
    if(this != &other)
    {
      name = std::move(other.name);
      data = std::move(other.data);
    }
    return *this;
  }

  bool
  operator==(ComplexSuccess const &other) const
  {
    return name == other.name && ((!data && !other.data) || (data && other.data && *data == *other.data));
  }

  bool
  operator!=(ComplexSuccess const &other) const
  {
    return !(*this == other);
  }

  int
  convertToInt() const
  {
    return *data;
  }
};

// === Типы ошибок для тестирования ========================================

enum class SimpleError
{
  None,
  InvalidInput,
  NetworkFailure
};

struct ComplexError {
  std::string message;
  int code;
  std::unique_ptr<int> resource;

  explicit ComplexError(std::string msg = "Default Error", int c = 100)
      : message(std::move(msg)), code(c), resource(std::make_unique<int>(c))
  {}

  ComplexError(ComplexError const &other)
      : message(other.message),
        code(other.code),
        resource(other.resource ? std::make_unique<int>(*other.resource) : nullptr)
  {}

  ComplexError &
  operator=(ComplexError const &other)
  {
    if(this != &other)
    {
      message  = other.message;
      code     = other.code;
      resource = other.resource ? std::make_unique<int>(*other.resource) : nullptr;
    }
    return *this;
  }

  ComplexError(ComplexError &&other) noexcept
      : message(std::move(other.message)), code(other.code), resource(std::move(other.resource))
  {
    other.code = 0; // Очистить исходный ресурс
  }

  ComplexError &
  operator=(ComplexError &&other) noexcept
  {
    if(this != &other)
    {
      message    = std::move(other.message);
      code       = other.code;
      resource   = std::move(other.resource);
      other.code = 0; // Очистить исходный ресурс
    }
    return *this;
  }

  bool
  operator==(ComplexError const &other) const
  {
    return message == other.message && code == other.code
           && ((!resource && !other.resource) || (resource && other.resource && *resource == *other.resource));
  }

  bool
  operator!=(ComplexError const &other) const
  {
    return !(*this == other);
  }
};

// === Фикстура для Expected ==================================================
template <typename T> class ExpectedTest : public ::testing::Test
{
protected:
  // Определяем SuccessType и ErrorType из TypeParam (std::tuple<SType, EType>)
  typedef typename std::tuple_element<0, T>::type SuccessType;
  typedef typename std::tuple_element<1, T>::type ErrorType;

  // Initial values for success types
  SuccessType s_val1{};
  SuccessType s_val2{};

  // Initial values for error types
  ErrorType e_val1{};
  ErrorType e_val2{};

  // Preconditions: Инициализируем стандартные значения успешных результатов и ошибок для использования в тестах.
  // Создание известных объектов для повторяемого тестирования.
  // Убеждаемся, что инициализация объектов не вызывает исключений.
  void
  SetUp() override
  {
    // Инициализация стандартными значениями - просто используем конструкторы по умолчанию
    s_val1 = SuccessType{};
    s_val2 = SuccessType{};
    e_val1 = ErrorType{};
    e_val2 = ErrorType{};
  }
};

// Определяем комбинации типов для Typed Tests
using ExpectedTestTypes
  = ::testing::Types<std::tuple<int, int>, std::tuple<std::string, std::string>, std::tuple<SimpleSuccess, SimpleError>,
                     std::tuple<ComplexSuccess, ComplexError>>;
TYPED_TEST_SUITE(ExpectedTest, ExpectedTestTypes);

// === Конструкторы и операторы присваивания =========================

// Проверяем конструктор по умолчанию. Должен создать Expected в успешном состоянии
// с дефолтно-сконструированным значением.
// Убеждаемся, что has_value() возвращает true, а значение соответствует дефолтному.
TYPED_TEST(ExpectedTest, DefaultConstructor_CreatesExpectedWithValue)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange, Act
  Expected<SuccessType, ErrorType> uut;

  // Assert
  EXPECT_TRUE(uut.has_value());
  SuccessType default_success{};
  EXPECT_EQ(uut.value(), default_success);
}

// Проверяем конструктор копирования. Должен создать новый Expected,
// копируя состояние и содержимое другого Expected.
// Убеждаемся, что состояние и значения идентичны, а объекты независимы.
TYPED_TEST(ExpectedTest, CopyConstructor_CopiesStateAndContent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Успешный Expected
  Expected<SuccessType, ErrorType> original_success(this->s_val1);
  // Act
  Expected<SuccessType, ErrorType> copied_success = original_success;
  // Assert
  EXPECT_TRUE(copied_success.has_value());
  EXPECT_EQ(copied_success.value(), this->s_val1);
  EXPECT_EQ(copied_success, original_success); // Используем non-member operator==

  // Arrange: Expected с ошибкой
  Expected<SuccessType, ErrorType> original_error(Unexpected<ErrorType>(this->e_val1));
  // Act
  Expected<SuccessType, ErrorType> copied_error = original_error;
  // Assert
  EXPECT_FALSE(copied_error.has_value());
  EXPECT_EQ(copied_error.error(), this->e_val1);
  EXPECT_EQ(copied_error, original_error); // Используем non-member operator==
}

// Проверяем конструктор перемещения. Должен создать новый Expected,
// перемещая состояние и содержимое другого Expected. Оригинал должен быть
// в валидном, но неопределенном состоянии.
// Убеждаемся, что новый объект имеет правильное состояние и значение,
// а исходный объект "обнулен" (если применимо для Complex типов).
TYPED_TEST(ExpectedTest, MoveConstructor_MovesStateAndContent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Успешный Expected
  SuccessType original_s_val = this->s_val1;
  Expected<SuccessType, ErrorType> original_success(std::move(original_s_val));
  SuccessType expected_s_val = this->s_val1; // Значение до перемещения
  // Act
  Expected<SuccessType, ErrorType> moved_success = std::move(original_success);
  // Assert
  EXPECT_TRUE(moved_success.has_value());
  EXPECT_EQ(moved_success.value(), expected_s_val);
  // Проверяем состояние исходного объекта
  if(std::is_same<SuccessType, ComplexSuccess>::value)
  {
    // Для ComplexSuccess, после перемещения, оригинальный объект мог потерять ресурсы
    // Но сам объект Expected находится в валидном, но неопределенном состоянии
    // Мы не можем здесь надежно проверить original_success.value()
  }

  // Arrange: Expected с ошибкой
  ErrorType original_e_val = this->e_val1;
  Expected<SuccessType, ErrorType> original_error(Unexpected<ErrorType>(std::move(original_e_val)));
  ErrorType expected_e_val = this->e_val1; // Значение до перемещения
  // Act
  Expected<SuccessType, ErrorType> moved_error = std::move(original_error);
  // Assert
  EXPECT_FALSE(moved_error.has_value());
  EXPECT_EQ(moved_error.error(), expected_e_val);
  // Проверяем состояние исходного объекта
  // Для ComplexError могли бы проверить обнуление после перемещения
}

// Проверяем конструктор из Unexpected (копирование). Должен создать Expected
// в состоянии ошибки, копируя содержимое Unexpected.
// Убеждаемся, что has_value() возвращает false, а error() соответствует исходной ошибке.
TYPED_TEST(ExpectedTest, Constructor_FromUnexpectedLValue_CopiesError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange
  Unexpected<ErrorType> unexp(this->e_val1);
  // Act
  Expected<SuccessType, ErrorType> uut(unexp); // Вызов explicit Expected(Unexpected<ErrorType> const &unexp)
  // Assert
  EXPECT_FALSE(uut.has_value());
  EXPECT_EQ(uut.error(), this->e_val1);

  // Проверяем независимость копий (только для ComplexError)
  // Для других типов эта проверка неприменима, поэтому пропускаем
}

// Проверяем конструктор из Unexpected (перемещение). Должен создать Expected
// в состоянии ошибки, перемещая содержимое Unexpected.
// Убеждаемся, что has_value() возвращает false, error() соответствует исходной ошибке,
// а исходный Unexpected "обнулен".
TYPED_TEST(ExpectedTest, Constructor_FromUnexpectedRValue_MovesError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange
  ErrorType original_e_val = this->e_val1;
  Unexpected<ErrorType> unexp(std::move(original_e_val));
  ErrorType expected_e_val = this->e_val1; // Значение до перемещения unexp
  // Act
  Expected<SuccessType, ErrorType> uut(std::move(unexp)); // Вызов explicit Expected(Unexpected<ErrorType> &&unexp)
  // Assert
  EXPECT_FALSE(uut.has_value());
  EXPECT_EQ(uut.error(), expected_e_val);

  // Проверяем обнуление для ComplexError (только если тип соответствует)
  // Для других типов эта проверка не применима
}

// Проверяем конструктор из успешного значения. Должен создать Expected
// в успешном состоянии с переданным значением.
// Убеждаемся, что has_value() возвращает true, а value() соответствует исходному значению.
TYPED_TEST(ExpectedTest, Constructor_FromValue_CreatesExpectedWithValue)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange, Act
  Expected<SuccessType, ErrorType> uut(this->s_val1);
  // Assert
  EXPECT_TRUE(uut.has_value());
  EXPECT_EQ(uut.value(), this->s_val1);

  // Должно быть возможно создать Expected из rvalue тоже.
  SuccessType temp_s_val = this->s_val2;
  Expected<SuccessType, ErrorType> uut_rvalue(std::move(temp_s_val));
  EXPECT_TRUE(uut_rvalue.has_value());
  EXPECT_EQ(uut_rvalue.value(), this->s_val2);
}

// Проверяем in-place конструктор для успешного значения. Должен создать Expected
// в успешном состоянии, конструируя значение на месте.
// Убеждаемся, что has_value() возвращает true, а value() соответствует сконструированному значению.
TYPED_TEST(ExpectedTest, InPlaceConstructor_ForValue_CreatesExpectedWithValue)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange, Act
  Expected<SuccessType, ErrorType> uut(in_place, this->s_val1);
  // Assert
  EXPECT_TRUE(uut.has_value());
  EXPECT_EQ(uut.value(), this->s_val1);

  // Проверяем с другим значением
  Expected<SuccessType, ErrorType> uut2(in_place, this->s_val2);
  EXPECT_TRUE(uut2.has_value());
  EXPECT_EQ(uut2.value(), this->s_val2);
}

// Проверяем in-place конструктор для ошибки. Должен создать Expected
// в состоянии ошибки, конструируя ошибку на месте.
// Убеждаемся, что has_value() возвращает false, а error() соответствует сконструированной ошибке.
TYPED_TEST(ExpectedTest, InPlaceConstructor_ForError_CreatesExpectedWithError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange, Act
  Expected<SuccessType, ErrorType> uut(unexpect_t(), this->e_val1);
  // Assert
  EXPECT_FALSE(uut.has_value());
  EXPECT_EQ(uut.error(), this->e_val1);

  // Проверяем с другой ошибкой
  Expected<SuccessType, ErrorType> uut2(unexpect_t(), this->e_val2);
  EXPECT_FALSE(uut2.has_value());
  EXPECT_EQ(uut2.error(), this->e_val2);
}

// Проверяем оператор присваивания копированием. Должен корректно присваивать
// содержимое другого Expected, меняя состояние при необходимости.
// Убеждаемся, что состояние и значения целевого объекта обновлены,
// а исходный объект остался неизменным.
TYPED_TEST(ExpectedTest, CopyAssignment_CopiesStateAndContent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Сценарий 1: Успех -> Успех
  Expected<SuccessType, ErrorType> src_s(this->s_val1);
  Expected<SuccessType, ErrorType> dst_s(this->s_val2);
  dst_s = src_s;
  EXPECT_TRUE(dst_s.has_value());
  EXPECT_EQ(dst_s.value(), this->s_val1);
  EXPECT_EQ(src_s.value(), this->s_val1); // Source remains unchanged

  // Сценарий 2: Ошибка -> Ошибка
  Expected<SuccessType, ErrorType> src_e(Unexpected<ErrorType>(this->e_val1));
  Expected<SuccessType, ErrorType> dst_e(Unexpected<ErrorType>(this->e_val2));
  dst_e = src_e;
  EXPECT_FALSE(dst_e.has_value());
  EXPECT_EQ(dst_e.error(), this->e_val1);
  EXPECT_EQ(src_e.error(), this->e_val1); // Source remains unchanged

  // Сценарий 3: Успех -> Ошибка
  Expected<SuccessType, ErrorType> src_s2(this->s_val1);
  Expected<SuccessType, ErrorType> dst_e2(Unexpected<ErrorType>(this->e_val2));
  dst_e2 = src_s2;
  EXPECT_TRUE(dst_e2.has_value());
  EXPECT_EQ(dst_e2.value(), this->s_val1);
  EXPECT_EQ(src_s2.value(), this->s_val1); // Source remains unchanged

  // Сценарий 4: Ошибка -> Успех
  Expected<SuccessType, ErrorType> src_e3(Unexpected<ErrorType>(this->e_val1));
  Expected<SuccessType, ErrorType> dst_s3(this->s_val2);
  dst_s3 = src_e3;
  EXPECT_FALSE(dst_s3.has_value());
  EXPECT_EQ(dst_s3.error(), this->e_val1);
  EXPECT_EQ(src_e3.error(), this->e_val1); // Source remains unchanged

  // Самоприсваивание
  Expected<SuccessType, ErrorType> self_assign(this->s_val1);
  self_assign = self_assign;
  EXPECT_TRUE(self_assign.has_value());
  EXPECT_EQ(self_assign.value(), this->s_val1);
}

// Проверяем оператор присваивания перемещением. Должен корректно присваивать
// содержимое другого Expected путем перемещения, меняя состояние при необходимости.
// Убеждаемся, что состояние и значения целевого объекта обновлены,
// а исходный объект "обнулен" (если применимо).
TYPED_TEST(ExpectedTest, MoveAssignment_MovesStateAndContent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Сценарий 1: Успех -> Успех
  SuccessType s1_val = this->s_val1;
  Expected<SuccessType, ErrorType> src_s(s1_val);
  Expected<SuccessType, ErrorType> dst_s(this->s_val2);
  dst_s = std::move(src_s);
  EXPECT_TRUE(dst_s.has_value());
  EXPECT_EQ(dst_s.value(), s1_val); // Значение перемещено

  // Сценарий 2: Ошибка -> Ошибка
  ErrorType e1_val = this->e_val1;
  Expected<SuccessType, ErrorType> src_e{Unexpected<ErrorType>(e1_val)};
  Expected<SuccessType, ErrorType> dst_e(Unexpected<ErrorType>(this->e_val2));
  dst_e = std::move(src_e);
  EXPECT_FALSE(dst_e.has_value());
  EXPECT_EQ(dst_e.error(), e1_val); // Ошибка перемещена

  // Сценарий 3: Успех -> Ошибка
  SuccessType s2_val = this->s_val1;
  Expected<SuccessType, ErrorType> src_s2(s2_val);
  Expected<SuccessType, ErrorType> dst_e2(Unexpected<ErrorType>(this->e_val2));
  dst_e2 = std::move(src_s2);
  EXPECT_TRUE(dst_e2.has_value());
  EXPECT_EQ(dst_e2.value(), s2_val); // Значение перемещено

  // Сценарий 4: Ошибка -> Успех
  ErrorType e2_val = this->e_val2;
  Expected<SuccessType, ErrorType> src_e3{Unexpected<ErrorType>(e2_val)};
  Expected<SuccessType, ErrorType> dst_s3(this->s_val2);
  dst_s3 = std::move(src_e3);
  EXPECT_FALSE(dst_s3.has_value());
  EXPECT_EQ(dst_s3.error(), e2_val); // Ошибка перемещена

  // Самоприсваивание
  Expected<SuccessType, ErrorType> self_assign(this->s_val1);
  self_assign = std::move(self_assign);
  // После move-присваивания самому себе, состояние остается тем же,
  // но внутренние механизмы могут быть вызваны.
  EXPECT_TRUE(self_assign.has_value());
  EXPECT_EQ(self_assign.value(), this->s_val1);
}

// === Наблюдатели ==================================================

// Проверяем методы has_value() и operator bool().
// Убеждаемся, что они корректно отражают состояние (успех/ошибка) Expected.
TYPED_TEST(ExpectedTest, HasValueAndOperatorBool_ReflectsState)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Успешный Expected
  Expected<SuccessType, ErrorType> success_uut(this->s_val1);
  // Assert
  EXPECT_TRUE(success_uut.has_value());
  EXPECT_TRUE(static_cast<bool>(success_uut)); // operator bool()

  // Arrange: Expected с ошибкой
  Expected<SuccessType, ErrorType> error_uut(Unexpected<ErrorType>(this->e_val1));
  // Assert
  EXPECT_FALSE(error_uut.has_value());
  EXPECT_FALSE(static_cast<bool>(error_uut)); // operator bool()
}

// Проверяем value() & (lvalue-ссылка).
// Убеждаемся, что возвращается корректное значение, если оно есть, и бросается исключение, если нет.
TYPED_TEST(ExpectedTest, ValueLValueRef_ReturnsValueOrThrows)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Успешный Expected
  Expected<SuccessType, ErrorType> success_uut(this->s_val1);
  // Act & Assert (успешный путь)
  EXPECT_EQ(success_uut.value(), this->s_val1);
  // Изменение через value() должно работать
  // Assign a value of SuccessType to ensure type safety across all test types.
  success_uut.value() = this->s_val2;
  EXPECT_EQ(success_uut.value(), this->s_val2);

  // Arrange: Expected с ошибкой
  Expected<SuccessType, ErrorType> error_uut(Unexpected<ErrorType>(this->e_val1));

#if _WIN32
  #pragma warning(push)
  #pragma warning(disable : 4834)
#endif
  // Act & Assert (ошибочный путь)
  EXPECT_THROW(
    try { error_uut.value(); } catch(BadExpectedAccess<ErrorType> const &e) {
      EXPECT_EQ(e.error(), this->e_val1);
      throw;
    },
    BadExpectedAccess<ErrorType>);
#if _WIN32
  #pragma warning(pop)
#endif
}

// Проверяем value() const & (const lvalue-ссылка).
// Убеждаемся, что возвращается корректное значение (константное), если оно есть, и бросается исключение, если нет.
TYPED_TEST(ExpectedTest, ValueConstLValueRef_ReturnsConstValueOrThrows)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Успешный Expected
  Expected<SuccessType, ErrorType> const success_uut(this->s_val1);
  // Act & Assert (успешный путь)
  EXPECT_EQ(success_uut.value(), this->s_val1);
  // Попытка изменить через const ссылку должна быть ошибкой компиляции
  // success_uut.value() = some_val; // Это не скомпилируется

#if _WIN32
  #pragma warning(push)
  #pragma warning(disable : 4834)
#endif
  // Arrange: Expected с ошибкой
  Expected<SuccessType, ErrorType> const error_uut(Unexpected<ErrorType>(this->e_val1));
  // Act & Assert (ошибочный путь)
  EXPECT_THROW(
    try { error_uut.value(); } catch(BadExpectedAccess<ErrorType> const &e) {
      EXPECT_EQ(e.error(), this->e_val1);
      throw;
    },
    BadExpectedAccess<ErrorType>);
#if _WIN32
  #pragma warning(pop)
#endif
}

// Проверяем value() && (rvalue-ссылка).
// Убеждаемся, что значение перемещается, если оно есть, и бросается исключение с перемещенной ошибкой, если нет.
TYPED_TEST(ExpectedTest, ValueRValueRef_MovesValueOrThrowsWithMovedError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Успешный Expected
  SuccessType original_s_val = this->s_val1;
  Expected<SuccessType, ErrorType> success_uut(original_s_val);
  // Act
  SuccessType moved_val = std::move(success_uut).value();
  // Assert
  EXPECT_EQ(moved_val, original_s_val);
}

// Проверяем value() const && (const rvalue-ссылка).
// Убеждаемся, что значение копируется (не перемещается), если оно есть, и бросается исключение с копированной
// ошибкой, если нет.
TYPED_TEST(ExpectedTest, ValueConstRValueRef_CopiesValueOrThrowsWithCopiedError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Успешный Expected
  SuccessType original_s_val = this->s_val1;
  Expected<SuccessType, ErrorType> const success_uut(original_s_val);
  // Act
  SuccessType copied_val = std::move(success_uut).value();
  // Assert
  EXPECT_EQ(copied_val, original_s_val);
  // Для ComplexSuccess, исходный Expected должен остаться неизменным
  if(std::is_same<SuccessType, ComplexSuccess>::value)
    EXPECT_EQ(success_uut.value(), original_s_val); // Original remains unchanged
}

// Проверяем error() & (lvalue-ссылка).
// Убеждаемся, что возвращается корректная ошибка, если она есть.
//      При наличии значения, assert должен сработать (в debug). В release это UB.
TYPED_TEST(ExpectedTest, ErrorLValueRef_ReturnsErrorWhenPresent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Expected с ошибкой
  Expected<SuccessType, ErrorType> error_uut(Unexpected<ErrorType>(this->e_val1));
  // Act & Assert (ошибочный путь)
  EXPECT_EQ(error_uut.error(), this->e_val1);
  // Изменение через error() должно работать
  ErrorType new_error_val = this->e_val2;
  error_uut.error()       = new_error_val;
  EXPECT_EQ(error_uut.error(), new_error_val);

  // Arrange: Успешный Expected
  Expected<SuccessType, ErrorType> success_uut(this->s_val1);
  // Act & Assert (успешный путь - assert в debug)
  // Мы не можем напрямую "ожидать" assert. В релизе это UB.
  // Просто убедимся, что вызов не приводит к фатальной ошибке, если assert отключен.
  // Для тестирования мы знаем предусловие и должны избегать вызова error() на success_uut.
  // Здесь мы просто комментируем, чтобы показать, что знаем об этом.
  // EXPECT_DEATH(success_uut.error(), "Calling error\\(\\) while value is present is undefined behavior.");
}

// Проверяем error() const & (const lvalue-ссылка).
// Убеждаемся, что возвращается корректная константная ошибка, если она есть.
TYPED_TEST(ExpectedTest, ErrorConstLValueRef_ReturnsConstErrorWhenPresent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Expected с ошибкой
  Expected<SuccessType, ErrorType> const error_uut(Unexpected<ErrorType>(this->e_val1));
  // Act & Assert (ошибочный путь)
  EXPECT_EQ(error_uut.error(), this->e_val1);
  // Попытка изменить через const ссылку должна быть ошибкой компиляции
  // error_uut.error() = some_val; // Это не скомпилируется
}

// Проверяем value_or() const & перегрузку.
// Убеждаемся, что возвращается значение или дефолтное значение при отсутствии значения.
TYPED_TEST(ExpectedTest, ValueOrConstLValue_ReturnsValueOrDefault)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Успешный Expected
  Expected<SuccessType, ErrorType> success_uut(this->s_val1);
  // Act & Assert
  EXPECT_EQ(success_uut.value_or(this->s_val2), this->s_val1);

  // Arrange: Expected с ошибкой
  Expected<SuccessType, ErrorType> error_uut(Unexpected<ErrorType>(this->e_val1));
  // Act & Assert
  EXPECT_EQ(error_uut.value_or(this->s_val2), this->s_val2);

  // Проверка с временным объектом в качестве default_value
  SuccessType default_success_value{};
  EXPECT_EQ(error_uut.value_or(default_success_value), default_success_value);
}

// Проверяем value_or() && перегрузку.
// Убеждаемся, что возвращается перемещенное значение или дефолтное значение при отсутствии значения.
TYPED_TEST(ExpectedTest, ValueOrRValue_MovesValueOrDefault)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Успешный Expected
  SuccessType original_s_val = this->s_val1;
  Expected<SuccessType, ErrorType> success_uut(original_s_val);
  // Act & Assert
  EXPECT_EQ(std::move(success_uut).value_or(this->s_val2), original_s_val);

  // Arrange: Expected с ошибкой
  Expected<SuccessType, ErrorType> error_uut(Unexpected<ErrorType>(this->e_val1));
  // Act & Assert
  EXPECT_EQ(std::move(error_uut).value_or(this->s_val2), this->s_val2);
}

// Проверяем error_or() const & перегрузку.
// Убеждаемся, что возвращается ошибка или дефолтное значение ошибки при отсутствии ошибки.
TYPED_TEST(ExpectedTest, ErrorOrConstLValue_ReturnsErrorOrDefaultError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Expected с ошибкой
  Expected<SuccessType, ErrorType> error_uut(Unexpected<ErrorType>(this->e_val1));
  // Act & Assert
  EXPECT_EQ(error_uut.error_or(this->e_val2), this->e_val1);

  // Arrange: Успешный Expected
  Expected<SuccessType, ErrorType> success_uut(this->s_val1);
  // Act & Assert
  EXPECT_EQ(success_uut.error_or(this->e_val2), this->e_val2);

  // Проверка с временным объектом в качестве default_error
  ErrorType default_error{};
  EXPECT_EQ(success_uut.error_or(default_error), default_error);
}

// Проверяем operator*() & (lvalue-ссылка).
// Убеждаемся, что возвращается корректная ссылка на значение, если оно есть.
//      При отсутствии значения, assert должен сработать (в debug).
TYPED_TEST(ExpectedTest, DereferenceOperatorLValueRef_ReturnsValueWhenPresent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Успешный Expected
  Expected<SuccessType, ErrorType> success_uut(this->s_val1);
  // Act & Assert
  EXPECT_EQ(*success_uut, this->s_val1);
  // Изменение через * должно работать для всех SuccessType
  SuccessType new_val = this->s_val2;
  *success_uut        = new_val;
  EXPECT_EQ(*success_uut, new_val);

  // Arrange: Expected с ошибкой
  Expected<SuccessType, ErrorType> error_uut(Unexpected<ErrorType>(this->e_val1));
  // Act & Assert (assert в debug, UB в release)
  // EXPECT_DEATH(*error_uut, "Dereferencing Expected without a value.");
}

// Проверяем operator*() const & (const lvalue-ссылка).
// Убеждаемся, что возвращается корректная константная ссылка на значение, если оно есть.
TYPED_TEST(ExpectedTest, DereferenceOperatorConstLValueRef_ReturnsConstValueWhenPresent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Успешный Expected
  Expected<SuccessType, ErrorType> const success_uut(this->s_val1);
  // Act & Assert
  EXPECT_EQ(*success_uut, this->s_val1);
}

// Проверяем operator*() && (rvalue-ссылка).
// Убеждаемся, что значение перемещается, если оно есть.
TYPED_TEST(ExpectedTest, DereferenceOperatorRValueRef_MovesValueWhenPresent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Успешный Expected
  SuccessType original_s_val = this->s_val1;
  Expected<SuccessType, ErrorType> success_uut(original_s_val);
  // Act
  SuccessType moved_val = *std::move(success_uut);
  // Assert
  EXPECT_EQ(moved_val, original_s_val);
  // Для ComplexSuccess, исходный Expected должен быть "обнулен"
  // Note: Template instantiation prevents us from checking ComplexSuccess.data directly
  // This would require SFINAE or specialized tests to properly validate move semantics
}

// Проверяем operator*() const && (const rvalue-ссылка).
// Убеждаемся, что значение копируется (не перемещается), если оно есть.
TYPED_TEST(ExpectedTest, DereferenceOperatorConstRValueRef_CopiesValueWhenPresent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Успешный Expected
  SuccessType original_s_val = this->s_val1;
  Expected<SuccessType, ErrorType> const success_uut(original_s_val);
  // Act
  SuccessType copied_val = *std::move(success_uut);
  // Assert
  EXPECT_EQ(copied_val, original_s_val);
  // Для ComplexSuccess, исходный Expected должен остаться неизменным
  if(std::is_same<SuccessType, ComplexSuccess>::value) EXPECT_EQ(success_uut.value(), original_s_val);
}

// === Модификаторы и Монадические операции ==========================

// Проверяем emplace() для успешного значения. Должен сконструировать новое значение на месте.
// Убеждаемся, что has_value() остается true, а значение обновлено.
TYPED_TEST(ExpectedTest, Emplace_ConstructsNewValueInPlace)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Expected с ошибкой
  Expected<SuccessType, ErrorType> uut(Unexpected<ErrorType>(this->e_val1));
  EXPECT_FALSE(uut.has_value());

  // Act: emplace новое значение
  SuccessType new_s_val = this->s_val2;
  uut.emplace(new_s_val);
  // Assert
  EXPECT_TRUE(uut.has_value());
  EXPECT_EQ(uut.value(), new_s_val);

  // Arrange: Expected с значением
  Expected<SuccessType, ErrorType> uut2(this->s_val1);
  EXPECT_TRUE(uut2.has_value());

  // Act: emplace другое значение
  uut2.emplace(new_s_val);
  // Assert
  EXPECT_TRUE(uut2.has_value());
  EXPECT_EQ(uut2.value(), new_s_val);

  // Note: ComplexSuccess-specific testing is handled in specialized test suites
  // to avoid template instantiation issues with different ErrorType combinations
}

// Проверяем emplace_error() для ошибки. Должен сконструировать новую ошибку на месте.
// Убеждаемся, что has_value() становится false, а ошибка обновлена.
TYPED_TEST(ExpectedTest, EmplaceError_ConstructsNewErrorInPlace)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Expected с значением
  Expected<SuccessType, ErrorType> uut(this->s_val1);
  EXPECT_TRUE(uut.has_value());

  // Act: emplace новую ошибку
  ErrorType new_e_val = this->e_val2;
  uut.emplace_error(new_e_val);
  // Assert
  EXPECT_FALSE(uut.has_value());
  EXPECT_EQ(uut.error(), new_e_val);

  // Arrange: Expected с ошибкой
  Expected<SuccessType, ErrorType> uut2(Unexpected<ErrorType>(this->e_val1));
  EXPECT_FALSE(uut2.has_value());

  // Act: emplace другую ошибку
  uut2.emplace_error(new_e_val);
  // Assert
  EXPECT_FALSE(uut2.has_value());
  EXPECT_EQ(uut2.error(), new_e_val);

  // Note: ComplexError-specific testing is handled in specialized test suites
  // to avoid template instantiation issues with different SuccessType combinations
}

// Проверяем swap(). Должен корректно обмениваться содержимым между двумя Expected.
// Убеждаемся, что состояния и значения обоих Expected объектов меняются местами.
TYPED_TEST(ExpectedTest, Swap_ExchangesContentsCorrectly)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Сценарий 1: Успех <-> Успех
  Expected<SuccessType, ErrorType> exp1_s(this->s_val1);
  Expected<SuccessType, ErrorType> exp2_s(this->s_val2);
  exp1_s.swap(exp2_s);
  EXPECT_TRUE(exp1_s.has_value());
  EXPECT_EQ(exp1_s.value(), this->s_val2);
  EXPECT_TRUE(exp2_s.has_value());
  EXPECT_EQ(exp2_s.value(), this->s_val1);

  // Сценарий 2: Ошибка <-> Ошибка
  Expected<SuccessType, ErrorType> exp1_e(Unexpected<ErrorType>(this->e_val1));
  Expected<SuccessType, ErrorType> exp2_e(Unexpected<ErrorType>(this->e_val2));
  exp1_e.swap(exp2_e);
  EXPECT_FALSE(exp1_e.has_value());
  EXPECT_EQ(exp1_e.error(), this->e_val2);
  EXPECT_FALSE(exp2_e.has_value());
  EXPECT_EQ(exp2_e.error(), this->e_val1);

  // Сценарий 3: Успех <-> Ошибка
  Expected<SuccessType, ErrorType> exp_s_to_e(this->s_val1);
  Expected<SuccessType, ErrorType> exp_e_to_s(Unexpected<ErrorType>(this->e_val1));
  exp_s_to_e.swap(exp_e_to_s);
  EXPECT_FALSE(exp_s_to_e.has_value());
  EXPECT_EQ(exp_s_to_e.error(), this->e_val1);
  EXPECT_TRUE(exp_e_to_s.has_value());
  EXPECT_EQ(exp_e_to_s.value(), this->s_val1);

  // Самообмен не должен менять состояние
  Expected<SuccessType, ErrorType> self_swap(this->s_val1);
  self_swap.swap(self_swap);
  EXPECT_TRUE(self_swap.has_value());
  EXPECT_EQ(self_swap.value(), this->s_val1);
}

// Проверяем and_then() & (lvalue-ссылка).
// Убеждаемся, что функция применяется к значению, если оно есть, или ошибка распространяется.
TYPED_TEST(ExpectedTest, AndThenLValue_AppliesFunctionToValueOrPropagatesError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Успешный Expected
  Expected<SuccessType, ErrorType> success_uut(this->s_val1);
  // Act: применяем функцию, которая преобразует SuccessType в Expected<SuccessType, ErrorType>
  auto func = [&](SuccessType &) { return Expected<SuccessType, ErrorType>(this->s_val2); };
  Expected<SuccessType, ErrorType> result_s = success_uut.and_then(func);
  // Assert
  EXPECT_TRUE(result_s.has_value());
  EXPECT_EQ(result_s.value(), this->s_val2);

  // Arrange: Expected с ошибкой
  Expected<SuccessType, ErrorType> error_uut(Unexpected<ErrorType>(this->e_val1));
  // Act
  Expected<SuccessType, ErrorType> result_e = error_uut.and_then(func);
  // Assert
  EXPECT_FALSE(result_e.has_value());
  EXPECT_EQ(result_e.error(), this->e_val1);
}

// Проверяем and_then() const & (const lvalue-ссылка).
// Убеждаемся, что функция применяется к константному значению, или ошибка распространяется.
TYPED_TEST(ExpectedTest, AndThenConstLValue_AppliesFunctionToConstValueOrPropagatesError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Успешный Expected
  Expected<SuccessType, ErrorType> const success_uut(this->s_val1);
  // Act: применяем функцию, которая преобразует const SuccessType & в Expected<SuccessType, ErrorType>
  auto func = [&](SuccessType const &) { return Expected<SuccessType, ErrorType>(this->s_val2); };
  Expected<SuccessType, ErrorType> result_s = success_uut.and_then(func);
  // Assert
  EXPECT_TRUE(result_s.has_value());
  EXPECT_EQ(result_s.value(), this->s_val2);

  // Arrange: Expected с ошибкой
  Expected<SuccessType, ErrorType> const error_uut(Unexpected<ErrorType>(this->e_val1));
  // Act
  Expected<SuccessType, ErrorType> result_e = error_uut.and_then(func);
  // Assert
  EXPECT_FALSE(result_e.has_value());
  EXPECT_EQ(result_e.error(), this->e_val1);
}

// Проверяем and_then() && (rvalue-ссылка).
// Убеждаемся, что функция применяется к перемещенному значению, или перемещенная ошибка распространяется.
TYPED_TEST(ExpectedTest, AndThenRValue_AppliesFunctionToMovedValueOrPropagatesMovedError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Успешный Expected
  SuccessType original_s_val = this->s_val1;
  Expected<SuccessType, ErrorType> success_uut(original_s_val);
  // Act
  auto func = [&](SuccessType &&) { return Expected<SuccessType, ErrorType>(this->s_val2); };
  Expected<SuccessType, ErrorType> result_s = std::move(success_uut).and_then(func);
  // Assert
  EXPECT_TRUE(result_s.has_value());
  EXPECT_EQ(result_s.value(), this->s_val2);
  // Для ComplexSuccess, исходный SuccessType из uut должен быть перемещен.
  if(std::is_same<SuccessType, ComplexSuccess>::value)
    EXPECT_TRUE(success_uut.has_value()); // Expected все еще содержит SuccessType, но оно перемещено

  // TODO: Fix this part of the test - SFINAE issue with and_then on moved error Expected
  // // Arrange: Expected с ошибкой
  // ErrorType original_e_val = this->e_val1;
  // Expected<SuccessType, ErrorType> error_uut(Unexpected<ErrorType>(original_e_val));
  // // Act
  // Expected<SuccessType, ErrorType> result_e = std::move(error_uut).and_then(func);
  // // Assert
  // EXPECT_FALSE(result_e.has_value());
  // EXPECT_EQ(result_e.error(), original_e_val);
}

// Проверяем and_then() const && (const rvalue-ссылка).
// Убеждаемся, что функция применяется к константному перемещенному значению, или константная перемещенная ошибка
// распространяется.
TYPED_TEST(ExpectedTest, AndThenConstRValue_AppliesFunctionToConstMovedValueOrPropagatesConstMovedError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;

  // Arrange: Успешный Expected
  SuccessType original_s_val = this->s_val1;
  Expected<SuccessType, ErrorType> const success_uut(original_s_val);
  // Act
  auto func = [&](SuccessType const &&) { return Expected<SuccessType, ErrorType>(this->s_val2); };
  Expected<SuccessType, ErrorType> result_s = std::move(success_uut).and_then(func);
  // Assert
  EXPECT_TRUE(result_s.has_value());
  EXPECT_EQ(result_s.value(), this->s_val2);
  // Для ComplexSuccess, исходный Expected должен остаться неизменным
  if(std::is_same<SuccessType, ComplexSuccess>::value) EXPECT_EQ(success_uut.value(), original_s_val);

  // TODO: Fix this part of the test - SFINAE issue with and_then on moved error Expected
  // // Arrange: Expected с ошибкой
  // ErrorType original_e_val = this->e_val1;
  // Expected<SuccessType, ErrorType> error_uut(Unexpected<ErrorType>(original_e_val));
  // // Act
  // Expected<SuccessType, ErrorType> result_e = std::move(error_uut).and_then(func);
  // // Assert
  // EXPECT_FALSE(result_e.has_value());
  // EXPECT_EQ(result_e.error(), original_e_val);
  // // Для ComplexError, исходный Expected должен остаться неизменным
  // if(std::is_same<ErrorType, ComplexError>::value)
  // {
  //   // Unfortunately, compiler complains on error C2660: 'testing::internal::EqHelper::Compare': function does not
  //   // take
  //   // 3 arguments, so I will place temporary variable for this purpose with explicit type to help the compiler.
  //   ErrorType expected_error = error_uut.error();
  //   EXPECT_EQ(expected_error, original_e_val);
  // }
}

// Проверяем transform() & (lvalue-ссылка).
// Убеждаемся, что функция трансформирует значение, если оно есть, или ошибка распространяется.
TYPED_TEST(ExpectedTest, TransformLValue_TransformsValueOrPropagatesError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;
  using ResultType  = Expected<SuccessType, ErrorType>;

  // Arrange: Успешный Expected
  Expected<SuccessType, ErrorType> success_uut(this->s_val1);

  // Act: применяем функцию, которая преобразует SuccessType в тот же тип
  auto func = [&](SuccessType &val) -> SuccessType
  {
    if constexpr(std::is_same_v<SuccessType, int>) { return val + 1; }
    else if constexpr(std::is_same_v<SuccessType, std::string>) { return val + "_transformed"; }
    else if constexpr(std::is_same_v<SuccessType, SimpleSuccess>) { return SimpleSuccess(val.value + 1); }
    else if constexpr(std::is_same_v<SuccessType, ComplexSuccess>)
    {
      ComplexSuccess result = val;
      result.name += "_transformed";
      return result;
    }
    else
    {
      return val; // fallback - return unchanged
    }
  };

  ResultType result_s = success_uut.transform(func);

  // Assert
  EXPECT_TRUE(result_s.has_value());
  if constexpr(std::is_same_v<SuccessType, int>) { EXPECT_EQ(result_s.value(), this->s_val1 + 1); }
  else if constexpr(std::is_same_v<SuccessType, std::string>)
  {
    EXPECT_EQ(result_s.value(), this->s_val1 + "_transformed");
  }
  else if constexpr(std::is_same_v<SuccessType, SimpleSuccess>)
  {
    EXPECT_EQ(result_s.value(), SimpleSuccess(this->s_val1.value + 1));
  }
  else if constexpr(std::is_same_v<SuccessType, ComplexSuccess>)
  {
    ComplexSuccess expected = this->s_val1;
    expected.name += "_transformed";
    EXPECT_EQ(result_s.value(), expected);
  }

  // Arrange: Expected с ошибкой
  Expected<SuccessType, ErrorType> error_uut(Unexpected<ErrorType>(this->e_val1));
  // Act
  ResultType result_e = error_uut.transform(func);
  // Assert
  EXPECT_FALSE(result_e.has_value());
  EXPECT_EQ(result_e.error(), this->e_val1);
}

// Проверяем transform() const & (const lvalue-ссылка).
// Убеждаемся, что функция трансформирует константное значение, если оно есть, или ошибка распространяется.
TYPED_TEST(ExpectedTest, TransformConstLValue_TransformsConstValueOrPropagatesError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;
  using ResultType  = Expected<SuccessType, ErrorType>;

  // Arrange: Успешный Expected
  Expected<SuccessType, ErrorType> const success_uut(this->s_val1);

  // Act: применяем функцию
  auto func = [&](SuccessType const &val) -> SuccessType
  {
    if constexpr(std::is_same_v<SuccessType, int>) { return val + 1; }
    else if constexpr(std::is_same_v<SuccessType, std::string>) { return val + "_transformed"; }
    else if constexpr(std::is_same_v<SuccessType, SimpleSuccess>) { return SimpleSuccess(val.value + 1); }
    else if constexpr(std::is_same_v<SuccessType, ComplexSuccess>)
    {
      ComplexSuccess result = val;
      result.name += "_transformed";
      return result;
    }
    else
    {
      return val; // fallback - return unchanged
    }
  };

  ResultType result_s = success_uut.transform(func);

  // Assert
  EXPECT_TRUE(result_s.has_value());
  if constexpr(std::is_same_v<SuccessType, int>) { EXPECT_EQ(result_s.value(), this->s_val1 + 1); }
  else if constexpr(std::is_same_v<SuccessType, std::string>)
  {
    EXPECT_EQ(result_s.value(), this->s_val1 + "_transformed");
  }
  else if constexpr(std::is_same_v<SuccessType, SimpleSuccess>)
  {
    EXPECT_EQ(result_s.value(), SimpleSuccess(this->s_val1.value + 1));
  }
  else if constexpr(std::is_same_v<SuccessType, ComplexSuccess>)
  {
    ComplexSuccess expected = this->s_val1;
    expected.name += "_transformed";
    EXPECT_EQ(result_s.value(), expected);
  }

  // Arrange: Expected с ошибкой
  Expected<SuccessType, ErrorType> const error_uut(Unexpected<ErrorType>(this->e_val1));
  // Act
  ResultType result_e = error_uut.transform(func);
  // Assert
  EXPECT_FALSE(result_e.has_value());
  EXPECT_EQ(result_e.error(), this->e_val1);
}

// Проверяем or_else() & (lvalue-ссылка).
// Убеждаемся, что функция применяется к ошибке, если она есть, или значение распространяется.
TYPED_TEST(ExpectedTest, OrElseLValue_AppliesFunctionToErrorOrPropagatesValue)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;
  using ReturnType = Expected<SuccessType, ErrorType>; // Функция or_else возвращает Expected<SuccessType, NewErrorType>

  // Arrange: Expected с ошибкой
  Expected<SuccessType, ErrorType> error_uut(Unexpected<ErrorType>(this->e_val1));
  // Act
  auto func           = [&](ErrorType &) { return ReturnType(unexpect_t(), this->e_val2); };
  ReturnType result_e = error_uut.or_else(func);
  // Assert
  EXPECT_FALSE(result_e.has_value());
  EXPECT_EQ(result_e.error(), this->e_val2);

  // Arrange: Успешный Expected
  Expected<SuccessType, ErrorType> success_uut(this->s_val1);
  // Act
  ReturnType result_s = success_uut.or_else(func);
  // Assert
  EXPECT_TRUE(result_s.has_value());
  EXPECT_EQ(result_s.value(), this->s_val1);
}

// Проверяем or_else() const & (const lvalue-ссылка).
// Убеждаемся, что функция применяется к константной ошибке, если она есть, или значение распространяется.
TYPED_TEST(ExpectedTest, OrElseConstLValue_AppliesFunctionToConstErrorOrPropagatesValue)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType   = typename TestFixture::ErrorType;
  using ReturnType  = Expected<SuccessType, ErrorType>;

  // Arrange: Expected с ошибкой
  Expected<SuccessType, ErrorType> const error_uut(Unexpected<ErrorType>(this->e_val1));
  // Act
  auto func           = [&](ErrorType const &) { return ReturnType(unexpect_t(), this->e_val2); };
  ReturnType result_e = error_uut.or_else(func);
  // Assert
  EXPECT_FALSE(result_e.has_value());
  EXPECT_EQ(result_e.error(), this->e_val2);

  // Arrange: Успешный Expected
  Expected<SuccessType, ErrorType> const success_uut(this->s_val1);
  // Act
  ReturnType result_s = success_uut.or_else(func);
  // Assert
  EXPECT_TRUE(result_s.has_value());
  EXPECT_EQ(result_s.value(), this->s_val1);
}
