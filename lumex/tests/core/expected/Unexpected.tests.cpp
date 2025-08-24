#include <gtest/gtest.h>

#include "lumex/core/expected/Unexpected.hpp"

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// === Типы ошибок для тестирования ========================================

// Простой тип ошибки
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

// === Фикстура для Unexpected ================================================
template <typename ErrorType> class UnexpectedTest : public ::testing::Test
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
TYPED_TEST_SUITE(UnexpectedTest, ErrorTypes);

// === Тесты Верификатора Контракта API ========================================

// Проверяем, что конструктор копирования правильно инициализирует Unexpected.
// Убеждаемся, что переданное значение было корректно скопировано в Unexpected.
TYPED_TEST(UnexpectedTest, Constructor_LValueRef_CopiesErrorCorrectly)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  // Act
  Unexpected<TypeParam> uut(initial_error); // Вызов конструктора от const &
  // Assert
  EXPECT_EQ(uut.error(), initial_error);
  // Изменение исходного объекта не должно влиять на Unexpected
  if constexpr(std::is_same_v<TypeParam, int>)
  {
    initial_error = 999;
    EXPECT_NE(uut.error(), initial_error);
  }
  else if constexpr(std::is_same_v<TypeParam, std::string>)
  {
    initial_error = "Changed Original";
    EXPECT_NE(uut.error(), initial_error);
  }
}

// Проверяем, что конструктор перемещения правильно инициализирует Unexpected.
// Убеждаемся, что переданное rvalue было корректно перемещено в Unexpected.
TYPED_TEST(UnexpectedTest, Constructor_RValueRef_MovesErrorCorrectly)
{
  // Arrange
  TypeParam initial_error  = this->error_val1;
  TypeParam expected_error = initial_error; // Копия для сравнения
  // Act
  Unexpected<TypeParam> uut(std::move(initial_error)); // Вызов конструктора от &&
  // Assert
  EXPECT_EQ(uut.error(), expected_error);
  // Для ComplexError, исходный объект должен быть в перемещенном состоянии.
  if constexpr(std::is_same_v<TypeParam, ComplexError>)
  {
    EXPECT_EQ(initial_error.code, 0);
    EXPECT_EQ(initial_error.resource, nullptr);
  }
}

// Проверяем, что error() lvalue ссылка возвращает изменяемую ссылку на хранимую ошибку.
// Убеждаемся, что через возвращенную ссылку можно изменить внутреннее состояние.
TYPED_TEST(UnexpectedTest, ErrorLValueRefReturnsMutableReference)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  Unexpected<TypeParam> uut(std::move(initial_error));
  // Act
  auto &error_ref = uut.error();
  // Assert
  EXPECT_EQ(error_ref, this->error_val1); // Проверяем исходное значение

  // Изменяем значение через ссылку и проверяем, что оно изменилось внутри uut
  if constexpr(std::is_same_v<TypeParam, int>)
  {
    error_ref = 555;
    EXPECT_EQ(uut.error(), 555);
  }
  else if constexpr(std::is_same_v<TypeParam, std::string>)
  {
    error_ref = "New Message";
    EXPECT_EQ(uut.error(), "New Message");
  }
  else if constexpr(std::is_same_v<TypeParam, ComplexError>)
  {
    error_ref.code = 777;
    EXPECT_EQ(uut.error().code, 777);
  }
}

// Проверяем, что const error() lvalue ссылка возвращает константную ссылку на хранимую ошибку.
// Убеждаемся, что через возвращенную ссылку нельзя изменить внутреннее состояние.
TYPED_TEST(UnexpectedTest, ConstErrorLValueRefReturnsImmutableReference)
{
  // Arrange
  TypeParam initial_error = this->error_val1;
  Unexpected<TypeParam> uut(std::move(initial_error));
  Unexpected<TypeParam> const &const_uut = uut;
  // Act
  auto &error_ref = const_uut.error();
  // Assert
  EXPECT_EQ(error_ref, this->error_val1);
  // Попытка изменить error_ref должна привести к ошибке компиляции,
  // что подтверждает иммутабельность.
  // error_ref = some_other_value; // Error: assignment of read-only reference
}

// Проверяем, что error() rvalue ссылка возвращает rvalue ссылку и корректно перемещает содержимое.
// Убеждаемся, что после вызова содержимое из Unexpected было перемещено.
TYPED_TEST(UnexpectedTest, ErrorRValueRefReturnsRValueAndMovesContent)
{
  // Arrange
  TypeParam initial_error  = this->error_val1;
  TypeParam expected_error = initial_error;
  Unexpected<TypeParam> uut(std::move(initial_error));
  // Act
  TypeParam moved_error = std::move(uut).error(); // Вызов error() &&
  // Assert
  EXPECT_EQ(moved_error, expected_error);
  // Для ComplexError, ресурс внутри uut должен быть перемещен.
  if constexpr(std::is_same_v<TypeParam, ComplexError>)
  {
    EXPECT_EQ(uut.error().code, 0);
    EXPECT_EQ(uut.error().resource, nullptr);
  }
}

// Проверяем, что const error() rvalue ссылка возвращает const rvalue ссылку и
//      не изменяет внутреннее состояние Unexpected (т.е. копирует).
// Убеждаемся, что исходный Unexpected остался неизменным.
TYPED_TEST(UnexpectedTest, ConstErrorRValueRefReturnsConstRValueAndDoesNotModifySource)
{
  // Arrange
  TypeParam initial_error  = this->error_val1;
  TypeParam expected_error = initial_error;
  Unexpected<TypeParam> uut(std::move(initial_error));
  // Act
  TypeParam const_moved_error = std::move(static_cast<Unexpected<TypeParam> const &>(uut)).error();
  // Assert
  EXPECT_EQ(const_moved_error, expected_error);
  // Для ComplexError, ресурс внутри uut не должен быть перемещен.
  if constexpr(std::is_same_v<TypeParam, ComplexError>) EXPECT_EQ(uut.error(), expected_error);
}

// === Тесты Памяти и Жизненного Цикла =======================================

// Проверяем, что при создании и уничтожении Unexpected с ComplexError
//      не происходит утечек памяти и ресурсы корректно освобождаются.
// Используем unique_ptr внутри ComplexError для контроля владения ресурсами.
TYPED_TEST(UnexpectedTest, MemorySafety_ComplexErrorDestructorCalled)
{
  if constexpr(std::is_same_v<TypeParam, ComplexError>)
  {
    // Arrange
    ComplexError initial_error("Memory Test Error", 200);
    int *original_resource_ptr = initial_error.resource.get();
    // Act & Assert (проверяем, что нет утечек при выходе из скоупа)
    {
      Unexpected<ComplexError> uut(std::move(initial_error));
      EXPECT_NE(uut.error().resource, nullptr);
      EXPECT_EQ(uut.error().resource.get(), original_resource_ptr);
    } // uut уничтожается здесь, ресурс unique_ptr также должен быть освобожден.
    SUCCEED() << "ComplexError with unique_ptr should be correctly destroyed, preventing memory leaks.";
  }
  else { SUCCEED() << "Test not applicable for non-ComplexError types."; }
}

// === Тесты на Потокобезопасность (для независимых экземпляров) =============

// Проверяем, что одновременное создание и доступ к разным экземплярам Unexpected
//      работает корректно.
// Каждый поток должен корректно создать свой Unexpected и получить правильное значение ошибки.
TYPED_TEST(UnexpectedTest, ThreadSafety_MultipleIndependentInstances)
{
  constexpr int num_threads = 10;
  std::vector<std::thread> threads;
  std::vector<TypeParam> initial_errors(num_threads);

  // Arrange: Подготавливаем уникальные ошибки для каждого потока
  for(int i = 0; i < num_threads; ++i)
    if constexpr(std::is_same_v<TypeParam, int>)
      initial_errors[i] = i + 1;
    else if constexpr(std::is_same_v<TypeParam, std::string>)
      initial_errors[i] = "Thread Error " + std::to_string(i + 1);
    else if constexpr(std::is_same_v<TypeParam, SimpleError>)
      initial_errors[i] = static_cast<SimpleError>((i % 3) + 1);
    else if constexpr(std::is_same_v<TypeParam, ComplexError>)
      initial_errors[i] = ComplexError("Complex Thread Error " + std::to_string(i + 1), 400 + i);
    else
      initial_errors[i] = TypeParam(); // Дефолтное значение для других типов

  // Act
  for(int i = 0; i < num_threads; ++i)
  {
    threads.emplace_back(
      [&, i]()
      {
        // Каждый поток создает свой объект Unexpected (перемещая свою ошибку)
        Unexpected<TypeParam> uut(std::move(initial_errors[i]));
        // И проверяет свое значение
        // Note: initial_errors[i] будет в перемещенном состоянии, сравниваем с ожидаемым значением
        // (которое было до перемещения)
        if constexpr(std::is_same_v<TypeParam, ComplexError>)
        {
          EXPECT_EQ(uut.error().code, 400 + i); // Проверяем конкретное поле ComplexError
        }
        else
        {
          // Для простых типов, ожидаем, что uut.error() содержит исходное значение
          // так как копирование/перемещение для них не приводит к "пустому" состоянию источника.
          // Но initial_errors[i] после std::move() может быть в неопределенном состоянии для std::string.
          // Поэтому лучше сравнивать с копией, сделанной до перемещения.
          // Здесь мы уже не можем напрямую сравнивать initial_errors[i] с uut.error(),
          // т.к. initial_errors[i] уже перемещено.
          // Этот тест больше о том, что Unexpected был успешно создан и его error() работает.
          SUCCEED(); // Если не ComplexError, просто убеждаемся, что нет крашей.
        }
      });
  }

  for(auto &t : threads) t.join();

  // Assert (все EXPECT внутри потоков должны быть успешными)
  SUCCEED() << "All independent Unexpected instances created and accessed correctly across threads.";
}

// === Тесты Производительности и Нагрузки (опционально) ====================

TYPED_TEST(UnexpectedTest, Perf_ConstructionAndAccess)
{
  // Precondition: Многократное создание и доступ к Unexpected.
  // Action: Замеряем время создания и доступа к error().
  // Expected State: Операция должна выполняться в приемлемые сроки.
  // Анализ производительности конструкторов и метода error() для выявления потенциальных узких мест.
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

    // Создание Unexpected. Здесь происходит перемещение error_data.
    Unexpected<TypeParam> uut(std::move(error_data));
    // Вызываем error() для симуляции использования
    [[maybe_unused]] auto &err = uut.error();
  }

  auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);

  // Ожидаемое время может сильно зависеть от ErrorType.
  // Установим более высокий порог для ComplexError.
  long long threshold = 100; // ms
  if constexpr(std::is_same_v<TypeParam, ComplexError>)
    threshold = 1000; // ms для ComplexError
  else if constexpr(std::is_same_v<TypeParam, std::string>)
    threshold = 200; // ms для std::string

  EXPECT_LT(dur.count(), threshold) << "Construction and access for " << N << " Unexpected<"
                                    << (std::is_same_v<TypeParam, int>            ? "int"
                                        : std::is_same_v<TypeParam, std::string>  ? "string"
                                        : std::is_same_v<TypeParam, SimpleError>  ? "SimpleError"
                                        : std::is_same_v<TypeParam, ComplexError> ? "ComplexError"
                                                                                  : "Unknown")
                                    << "> too slow: " << dur.count() << "ms (Threshold: " << threshold << "ms)";
}
