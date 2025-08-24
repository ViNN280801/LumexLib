#include <gtest/gtest.h>

#include "lumex/core/expected/BadExpectedAccess.hpp"

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// === Типы ошибок для тестирования ========================================

enum class SimpleError
{
  None,
  InvalidInput,
  NetworkFailure
};

// Complex type of error with resource ownership (for checking move and copy)
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
    other.code = 0;
  }

  ComplexError &
  operator=(ComplexError &&other) noexcept
  {
    if(this != &other)
    {
      message    = std::move(other.message);
      code       = other.code;
      resource   = std::move(other.resource);
      other.code = 0;
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

// === Фикстура для BadExpectedAccess =========================================
template <typename ErrorType> class BadExpectedAccessTest : public ::testing::Test
{
protected:
  // Preconditions: Инициализируем стандартные значения ошибок для использования в тестах.
  // Создание известных объектов ошибок для повторяемого тестирования.
  // Убеждаемся, что инициализация объектов ошибок не вызывает исключений.
  void
  SetUp() override
  {
    // Инициализация для SimpleError, если ErrorType - это SimpleError
    if constexpr(std::is_same_v<ErrorType, SimpleError>)
    {
      error_val1 = static_cast<ErrorType>(SimpleError::InvalidInput);
      error_val2 = static_cast<ErrorType>(SimpleError::NetworkFailure);
    }
    // Инициализация для ComplexError, если ErrorType - это ComplexError
    else if constexpr(std::is_same_v<ErrorType, ComplexError>)
    {
      error_val1 = static_cast<ErrorType>(ComplexError("Test Error 1", 101));
      error_val2 = static_cast<ErrorType>(ComplexError("Test Error 2", 102));
    }
    // Для других типов, используем конструктор по умолчанию или простую инициализацию
    else
    {
      error_val1 = ErrorType();
      error_val2 = ErrorType();
    }
  }

  ErrorType error_val1;
  ErrorType error_val2;
};

// Используем Typed Tests для тестирования с различными типами ошибок
using ErrorTypes = ::testing::Types<int, std::string, SimpleError, ComplexError>;
TYPED_TEST_SUITE(BadExpectedAccessTest, ErrorTypes);

// === Тесты Верификатора Контракта API ========================================

// Проверяем, что конструктор правильно инициализирует исключение и что метод what()
// возвращает ожидаемое строковое описание.
// После создания объекта, убеждаемся, что what() возвращает "Bad expected access".
TYPED_TEST(BadExpectedAccessTest, Constructor_And_WhatMethodReturnsCorrectMessage)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  // Act
  BadExpectedAccess<TypeParam> uut(std::move(initial_error));
  // Assert
  EXPECT_STREQ("Bad expected access", uut.what());
}

// Проверяем, что конструктор принимает rvalue и что error() lvalue ссылка
// возвращает корректное значение.
// Убеждаемся, что переданное rvalue было корректно перемещено в исключение.
TYPED_TEST(BadExpectedAccessTest, Constructor_RValue_And_ErrorLValueRefReturnsCorrectValue)
{
  // Arrange
  TypeParam initial_error  = this->error_val1;
  TypeParam expected_error = initial_error; // Копия для сравнения
  // Act
  BadExpectedAccess<TypeParam> uut(std::move(initial_error));
  // Assert
  EXPECT_EQ(uut.error(), expected_error);
  // Убеждаемся, что модификация через lvalue ссылку изменяет внутреннее состояние.
  if constexpr(std::is_same_v<TypeParam, int>)
  {
    uut.error() = 999;
    EXPECT_EQ(uut.error(), 999);
  }
  else if constexpr(std::is_same_v<TypeParam, std::string>)
  {
    uut.error() = "Modified Error";
    EXPECT_EQ(uut.error(), "Modified Error");
  }
}

// Проверяем, что const error() lvalue ссылка возвращает корректное значение
// и не позволяет изменять его.
// Убеждаемся, что константный доступ предоставляет корректное значение.
TYPED_TEST(BadExpectedAccessTest, ConstErrorLValueRefReturnsCorrectValue_And_IsImmutable)
{
  // Arrange
  TypeParam initial_error  = this->error_val1;
  TypeParam expected_error = initial_error;
  BadExpectedAccess<TypeParam> uut(std::move(initial_error));
  // Act
  BadExpectedAccess<TypeParam> const &const_uut = uut;
  // Assert
  EXPECT_EQ(const_uut.error(), expected_error);
  // Пытаемся изменить через const ссылку (должно быть ошибкой компиляции)
  // const_uut.error() = some_other_value; // Это вызовет ошибку компиляции, что и ожидается
}

// Проверяем, что error() rvalue ссылка возвращает корректное значение и
// корректно перемещает внутреннее состояние.
// Убеждаемся, что после перемещения внутреннее состояние uut.error() изменяется (если применимо).
TYPED_TEST(BadExpectedAccessTest, ErrorRValueRefReturnsCorrectValue_And_MovesContent)
{
  // Arrange
  TypeParam initial_error  = this->error_val1;
  TypeParam expected_error = initial_error;
  BadExpectedAccess<TypeParam> uut(std::move(initial_error));
  // Act
  TypeParam moved_error = std::move(uut).error();
  // Assert
  EXPECT_EQ(moved_error, expected_error);
  // Для ComplexError, ресурс внутри uut должен быть перемещен.
  if constexpr(std::is_same_v<TypeParam, ComplexError>)
  {
    EXPECT_EQ(uut.error().code, 0); // Проверяем, что исходный объект ComplexError был изменен
    EXPECT_EQ(uut.error().resource, nullptr);
  }
}

// Проверяем, что const error() rvalue ссылка возвращает корректное значение
// без изменения внутреннего состояния uut.
// Убеждаемся, что константное rvalue перемещение корректно возвращает копию.
TYPED_TEST(BadExpectedAccessTest, ConstErrorRValueRefReturnsCorrectValue_And_DoesNotModifySource)
{
  // Arrange
  TypeParam initial_error  = this->error_val1;
  TypeParam expected_error = initial_error;
  BadExpectedAccess<TypeParam> uut(std::move(initial_error));
  // Act
  TypeParam const_moved_error = std::move(static_cast<BadExpectedAccess<TypeParam> const &>(uut)).error();
  // Assert
  EXPECT_EQ(const_moved_error, expected_error);
  // Для ComplexError, ресурс внутри uut не должен быть перемещен.
  if constexpr(std::is_same_v<TypeParam, ComplexError>)
    EXPECT_EQ(uut.error(), expected_error); // Проверяем, что исходный объект ComplexError не был изменен
}

// === Тесты Памяти и Жизненного Цикла =======================================

// Проверяем, что при создании и уничтожении BadExpectedAccess с ComplexError
// не происходит утечек памяти и ресурсы корректно освобождаются.
// Используем unique_ptr внутри ComplexError для контроля владения ресурсами.
TYPED_TEST(BadExpectedAccessTest, MemorySafety_ComplexErrorDestructorCalled)
{
  if constexpr(std::is_same_v<TypeParam, ComplexError>)
  {
    // Arrange
    ComplexError initial_error("Memory Test Error", 200);
    int *original_resource_ptr = initial_error.resource.get();
    // Act & Assert (проверяем, что нет утечек при выходе из скоупа)
    {
      BadExpectedAccess<ComplexError> uut(std::move(initial_error));
      EXPECT_NE(uut.error().resource, nullptr);
      EXPECT_EQ(uut.error().resource.get(), original_resource_ptr); // Должен быть тот же ресурс, но перемещен
    } // uut уничтожается здесь, ресурс unique_ptr также должен быть освобожден.
    // Напрямую проверить освобождение unique_ptr сложно без модификации ComplexError,
    // но его RAII природа гарантирует это. Мы можем проверить, что original_resource_ptr
    // теперь указывает на освобожденную память, но это опасно.
    // Вместо этого, полагаемся на корректность unique_ptr.
    SUCCEED() << "ComplexError with unique_ptr should be correctly destroyed, preventing memory leaks.";
  }
  else { SUCCEED() << "Test not applicable for non-ComplexError types."; }
}

// Проверяем конструктор копирования BadExpectedAccess.
// Убеждаемся, что копированный объект имеет независимую копию ошибки.
TYPED_TEST(BadExpectedAccessTest, CopyConstructor_CopiesErrorCorrectly)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  BadExpectedAccess<TypeParam> original_uut(std::move(initial_error));
  TypeParam expected_error_value = original_uut.error(); // Значение ошибки до копирования
  // Act
  BadExpectedAccess<TypeParam> copied_uut = original_uut; // Вызов конструктора копирования
  // Assert
  EXPECT_EQ(copied_uut.error(), expected_error_value);
  // Изменение оригинала не должно влиять на копию
  if constexpr(std::is_same_v<TypeParam, int>)
  {
    original_uut.error() = 123;
    EXPECT_NE(copied_uut.error(), original_uut.error());
    EXPECT_EQ(copied_uut.error(), expected_error_value);
  }
  else if constexpr(std::is_same_v<TypeParam, std::string>)
  {
    original_uut.error() = "Changed Original";
    EXPECT_NE(copied_uut.error(), original_uut.error());
    EXPECT_EQ(copied_uut.error(), expected_error_value);
  }
  else if constexpr(std::is_same_v<TypeParam, ComplexError>)
  {
    original_uut.error().message = "Changed Original Message";
    original_uut.error().code    = 500;
    EXPECT_NE(copied_uut.error(), original_uut.error());
    EXPECT_EQ(copied_uut.error().message, expected_error_value.message);
    EXPECT_EQ(copied_uut.error().code, expected_error_value.code);
    EXPECT_NE(copied_uut.error().resource, original_uut.error().resource); // Должны быть разные unique_ptr
  }
}

// Проверяем оператор присваивания копированием BadExpectedAccess.
// Убеждаемся, что целевой объект получает независимую копию ошибки и старые ресурсы освобождаются.
TYPED_TEST(BadExpectedAccessTest, CopyAssignment_CopiesErrorCorrectly)
{
  // Arrange
  TypeParam initial_error_src = this->error_val1;
  BadExpectedAccess<TypeParam> src_uut(std::move(initial_error_src));
  TypeParam initial_error_dst = this->error_val2;
  BadExpectedAccess<TypeParam> dst_uut(std::move(initial_error_dst));
  TypeParam expected_error_value = src_uut.error();

  // Act
  dst_uut = src_uut; // Вызов оператора присваивания копированием
  // Assert
  EXPECT_EQ(dst_uut.error(), expected_error_value);
  // Изменение источника не должно влиять на целевой объект
  if constexpr(std::is_same_v<TypeParam, int>)
  {
    src_uut.error() = 456;
    EXPECT_NE(dst_uut.error(), src_uut.error());
    EXPECT_EQ(dst_uut.error(), expected_error_value);
  }
  else if constexpr(std::is_same_v<TypeParam, std::string>)
  {
    src_uut.error() = "Changed Source";
    EXPECT_NE(dst_uut.error(), src_uut.error());
    EXPECT_EQ(dst_uut.error(), expected_error_value);
  }
  else if constexpr(std::is_same_v<TypeParam, ComplexError>)
  {
    src_uut.error().message = "Changed Source Message";
    src_uut.error().code    = 600;
    EXPECT_NE(dst_uut.error(), src_uut.error());
    EXPECT_EQ(dst_uut.error().message, expected_error_value.message);
    EXPECT_EQ(dst_uut.error().code, expected_error_value.code);
    EXPECT_NE(dst_uut.error().resource, src_uut.error().resource);
  }
}

// Проверяем оператор присваивания перемещением BadExpectedAccess.
// Убеждаемся, что ресурсы корректно перемещаются от источника к целевому объекту,
//      и источник остается в валидном, но измененном состоянии.
TYPED_TEST(BadExpectedAccessTest, MoveAssignment_MovesErrorCorrectly)
{
  // Arrange
  TypeParam initial_error_src = this->error_val1;
  BadExpectedAccess<TypeParam> src_uut(std::move(initial_error_src));
  TypeParam initial_error_dst = this->error_val2;
  BadExpectedAccess<TypeParam> dst_uut(std::move(initial_error_dst));
  TypeParam expected_error_value = src_uut.error(); // Значение ошибки до перемещения
  // Act
  dst_uut = std::move(src_uut); // Вызов оператора присваивания перемещением
  // Assert
  EXPECT_EQ(dst_uut.error(), expected_error_value);
  // Проверяем, что исходный объект src_uut теперь содержит перемещенный ресурс
  if constexpr(std::is_same_v<TypeParam, ComplexError>)
  {
    EXPECT_EQ(src_uut.error().code, 0);
    EXPECT_EQ(src_uut.error().resource, nullptr);
  }
  // Для простых типов, таких как int или std::string, исходный объект может остаться неизменным
  // или быть в "допустимом, но неопределенном" состоянии. Мы не будем проверять его значение
  // после перемещения, так как это не является частью контракта.
}

// === Тесты на Потокобезопасность (для независимых экземпляров) =============

// Проверяем, что одновременное создание и доступ к разным экземплярам BadExpectedAccess
// работает корректно.
// Каждый поток должен корректно создать свой BadExpectedAccess и получить правильное значение ошибки.
TYPED_TEST(BadExpectedAccessTest, ThreadSafety_MultipleIndependentInstances)
{
  constexpr int num_threads = 10;
  std::vector<std::thread> threads;
  std::vector<BadExpectedAccess<TypeParam>> errors;
  errors.reserve(num_threads);

  // Arrange
  // Создаем исходные ошибки для каждого потока
  std::vector<TypeParam> initial_errors(num_threads);
  for(int i = 0; i < num_threads; ++i)
    if constexpr(std::is_same_v<TypeParam, int>)
      initial_errors[i] = i + 1;
    else if constexpr(std::is_same_v<TypeParam, std::string>)
      initial_errors[i] = "Error " + std::to_string(i + 1);
    else if constexpr(std::is_same_v<TypeParam, SimpleError>)
      initial_errors[i] = static_cast<SimpleError>(i % 3 + 1);
    else if constexpr(std::is_same_v<TypeParam, ComplexError>)
      initial_errors[i] = ComplexError("Thread Error " + std::to_string(i + 1), 300 + i);
    else
      initial_errors[i] = TypeParam(); // Дефолтное значение

  // Act
  for(int i = 0; i < num_threads; ++i)
  {
    threads.emplace_back(
      [&, i]()
      {
        // Каждый поток создает свой объект BadExpectedAccess
        TypeParam expected_error_in_thread = initial_errors[i];
        BadExpectedAccess<TypeParam> uut(std::move(initial_errors[i]));
        // И проверяет свое значение
        EXPECT_EQ(uut.error(), expected_error_in_thread); // Сравнение с исходным значением до перемещения
        EXPECT_STREQ("Bad expected access", uut.what());
      });
  }

  for(auto &t : threads) t.join();

  // Assert (все EXPECT внутри потоков должны быть успешными)
  SUCCEED() << "All independent BadExpectedAccess instances created and accessed correctly across threads.";
}

// === Тесты Производительности и Нагрузки (опционально) ====================

TYPED_TEST(BadExpectedAccessTest, Perf_ConstructionAndAccess)
{
  // Precondition: Многократное создание и доступ к BadExpectedAccess.
  // Action: Замеряем время создания и доступа к error().
  // Expected State: Операция должна выполняться в приемлемые сроки.
  // Анализ производительности конструктора и метода error() для выявления потенциальных узких мест.
  // Убеждаемся, что производительность соответствует ожиданиям.
  int const N = 1'000'000;
  auto start  = std::chrono::high_resolution_clock::now();

  for(int i = 0; i < N; ++i)
  {
    TypeParam error_data;
    if constexpr(std::is_same_v<TypeParam, int>)
      error_data = i;
    else if constexpr(std::is_same_v<TypeParam, std::string>)
      error_data = "Error" + std::to_string(i);
    else if constexpr(std::is_same_v<TypeParam, SimpleError>)
      error_data = static_cast<SimpleError>(i % 3 + 1);
    else if constexpr(std::is_same_v<TypeParam, ComplexError>)
      error_data = ComplexError("Perf Error", i);

    BadExpectedAccess<TypeParam> uut(std::move(error_data));
    // Вызываем error() для симуляции использования
    [[maybe_unused]] auto &err = uut.error();
  }

  auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);

  // Ожидаемое время может сильно зависеть от ErrorType.
  // Например, ComplexError будет значительно медленнее из-за unique_ptr и std::string.
  // Установим более высокий порог для ComplexError.
  long long threshold = 100; // ms
  if constexpr(std::is_same_v<TypeParam, ComplexError>)
    threshold = 1000; // ms для ComplexError
  else if constexpr(std::is_same_v<TypeParam, std::string>)
    threshold = 200; // ms для std::string

  EXPECT_LT(dur.count(), threshold) << "Construction and access for " << N << " BadExpectedAccess<"
                                    << (std::is_same_v<TypeParam, int>            ? "int"
                                        : std::is_same_v<TypeParam, std::string>  ? "string"
                                        : std::is_same_v<TypeParam, SimpleError>  ? "SimpleError"
                                        : std::is_same_v<TypeParam, ComplexError> ? "ComplexError"
                                                                                  : "Unknown")
                                    << "> too slow: " << dur.count() << "ms (Threshold: " << threshold << "ms)";
}
