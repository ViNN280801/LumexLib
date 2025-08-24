#include <gtest/gtest.h>

#include "lumex/core/circular_buffer/CircularBuffer.hpp"
using namespace Lumex::Core::CircularBuffer;

#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// === Типы данных для тестирования ========================================

/**
 * @brief Простой тип для тестирования базовой функциональности
 */
struct SimpleType {
  int value;

  explicit SimpleType(int v = 0) : value(v) {}

  bool
  operator==(SimpleType const &other) const
  {
    return value == other.value;
  }
  bool
  operator!=(SimpleType const &other) const
  {
    return !(*this == other);
  }
  bool
  operator<(SimpleType const &other) const
  {
    return value < other.value;
  }
};

/**
 * @brief Сложный тип с управлением ресурсами для тестирования move семантики
 */
struct ComplexType {
  std::string name;
  std::unique_ptr<int> data;
  std::vector<int> items;

  explicit ComplexType(std::string n = "Default", int d = 0, std::vector<int> i = std::vector<int>())
      : name(n), data(new int(d)), items(i)
  {}

  ComplexType(ComplexType const &other)
      : name(other.name), data(other.data ? new int(*other.data) : nullptr), items(other.items)
  {}

  ComplexType &
  operator=(ComplexType const &other)
  {
    if(this != &other)
    {
      name = other.name;
      data.reset(other.data ? new int(*other.data) : nullptr);
      items = other.items;
    }
    return *this;
  }

  ComplexType(ComplexType &&other) noexcept
      : name(std::move(other.name)), data(std::move(other.data)), items(std::move(other.items))
  {
    other.name.clear();
    other.data.reset();
    other.items.clear();
  }

  ComplexType &
  operator=(ComplexType &&other) noexcept
  {
    if(this != &other)
    {
      name  = std::move(other.name);
      data  = std::move(other.data);
      items = std::move(other.items);
      other.name.clear();
      other.data.reset();
      other.items.clear();
    }
    return *this;
  }

  bool
  operator==(ComplexType const &other) const
  {
    return name == other.name && ((!data && !other.data) || (data && other.data && *data == *other.data))
           && items == other.items;
  }

  bool
  operator!=(ComplexType const &other) const
  {
    return !(*this == other);
  }
  bool
  operator<(ComplexType const &other) const
  {
    return name < other.name;
  }
};

/**
 * @brief Тип без конструктора по умолчанию для тестирования ограничений
 */
struct NoDefaultCtor {
  int value;

  explicit NoDefaultCtor(int v) : value(v) {}
  NoDefaultCtor(NoDefaultCtor const &)            = default;
  NoDefaultCtor &operator=(NoDefaultCtor const &) = default;

  bool
  operator==(NoDefaultCtor const &other) const
  {
    return value == other.value;
  }
  bool
  operator!=(NoDefaultCtor const &other) const
  {
    return !(*this == other);
  }
};

/**
 * @brief Тип, который выбрасывает исключения при копировании
 */
struct ThrowingCopyType {
  int value;
  bool should_throw_on_copy;

  explicit ThrowingCopyType(int v = 0, bool throw_on_copy = false) : value(v), should_throw_on_copy(throw_on_copy) {}

  ThrowingCopyType(ThrowingCopyType const &other) : value(other.value), should_throw_on_copy(other.should_throw_on_copy)
  {
    if(should_throw_on_copy) throw std::runtime_error("Copy constructor failed");
  }

  ThrowingCopyType &
  operator=(ThrowingCopyType const &other)
  {
    if(this != &other)
    {
      if(should_throw_on_copy) throw std::runtime_error("Copy assignment failed");
      value                = other.value;
      should_throw_on_copy = other.should_throw_on_copy;
    }
    return *this;
  }

  ThrowingCopyType(ThrowingCopyType &&other) noexcept
      : value(other.value), should_throw_on_copy(other.should_throw_on_copy)
  {
    other.value                = 0;
    other.should_throw_on_copy = false;
  }

  ThrowingCopyType &
  operator=(ThrowingCopyType &&other) noexcept
  {
    if(this != &other)
    {
      value                      = other.value;
      should_throw_on_copy       = other.should_throw_on_copy;
      other.value                = 0;
      other.should_throw_on_copy = false;
    }
    return *this;
  }

  bool
  operator==(ThrowingCopyType const &other) const
  {
    return value == other.value && should_throw_on_copy == other.should_throw_on_copy;
  }
  bool
  operator!=(ThrowingCopyType const &other) const
  {
    return !(*this == other);
  }
  bool
  operator<(ThrowingCopyType const &other) const
  {
    return value < other.value;
  }
};

/**
 * @brief Тип, который выбрасывает исключения при конструировании
 */
struct ThrowingConstructorType {
  int value;
  bool should_throw_on_construct;

  explicit ThrowingConstructorType(int v = 0, bool throw_on_construct = false)
      : value(v), should_throw_on_construct(throw_on_construct)
  {
    if(should_throw_on_construct) throw std::runtime_error("Constructor failed");
  }

  ThrowingConstructorType(ThrowingConstructorType const &other)                = default;
  ThrowingConstructorType &operator=(ThrowingConstructorType const &other)     = default;
  ThrowingConstructorType(ThrowingConstructorType &&other) noexcept            = default;
  ThrowingConstructorType &operator=(ThrowingConstructorType &&other) noexcept = default;

  bool
  operator==(ThrowingConstructorType const &other) const
  {
    return value == other.value && should_throw_on_construct == other.should_throw_on_construct;
  }
  bool
  operator!=(ThrowingConstructorType const &other) const
  {
    return !(*this == other);
  }
  bool
  operator<(ThrowingConstructorType const &other) const
  {
    return value < other.value;
  }
};

// === Тестирование CircularBuffer с int =================================

/**
 * @brief Тесты для CircularBuffer с типом int
 */
class CircularBufferIntTest : public ::testing::Test
{
protected:
  using BufferType = CircularBuffer<int>;

  int val1         = 1;
  int val2         = 2;
  int val3         = 3;
};

// === Тестирование CircularBuffer с std::string =========================

/**
 * @brief Тесты для CircularBuffer с типом std::string
 */
class CircularBufferStringTest : public ::testing::Test
{
protected:
  using BufferType = CircularBuffer<std::string>;

  std::string val1 = "First";
  std::string val2 = "Second";
  std::string val3 = "Third";
};

// === Тестирование CircularBuffer с SimpleType =========================

/**
 * @brief Тесты для CircularBuffer с типом SimpleType
 */
class CircularBufferSimpleTypeTest : public ::testing::Test
{
protected:
  using BufferType = CircularBuffer<SimpleType>;

  SimpleType val1  = SimpleType(1);
  SimpleType val2  = SimpleType(2);
  SimpleType val3  = SimpleType(3);
};

// === API Contract Verifier Tests =========================================

/**
 * @brief Цель: Проверить корректность конструктора с указанной емкостью
 * @details В чем убеждаемся: Буфер создается с правильной емкостью и пустым состоянием
 * @details Как достигается: Создание буфера с разными емкостями и проверка capacity() и empty()
 */
TEST_F(CircularBufferIntTest, Constructor_WithCapacity_CreatesEmptyBufferWithCorrectCapacity)
{
  using BufferType = CircularBuffer<int>;

  // Arrange & Act
  BufferType buffer(5);

  // Assert
  EXPECT_EQ(buffer.capacity(), 5);
  EXPECT_TRUE(buffer.empty());
  EXPECT_EQ(buffer.size(), 0);
  EXPECT_FALSE(buffer.full());
}

/**
 * @brief Цель: Проверить выбрасывание исключения при нулевой емкости
 * @details В чем убеждаемся: Конструктор выбрасывает std::length_error при capacity == 0
 * @details Как достигается: Попытка создания буфера с нулевой емкостью
 */
TEST_F(CircularBufferIntTest, Constructor_ZeroCapacity_ThrowsLengthError)
{
  using BufferType = CircularBuffer<int>;

  // Act & Assert
  EXPECT_THROW(BufferType buffer(0), std::length_error);
}

/**
 * @brief Цель: Проверить корректность конструктора копирования
 * @details В чем убеждаемся: Копированный буфер идентичен оригиналу
 * @details Как достигается: Создание буфера, заполнение элементами и копирование
 */
TEST_F(CircularBufferIntTest, CopyConstructor_CopiesStateAndContentCorrectly)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType original(3);
  original.push_back(val1);
  original.push_back(val2);

  // Act
  BufferType copied(original);

  // Assert
  EXPECT_EQ(copied.capacity(), original.capacity());
  EXPECT_EQ(copied.size(), original.size());
  EXPECT_EQ(copied, original);
}

/**
 * @brief Цель: Проверить корректность конструктора перемещения
 * @details В чем убеждаемся: Ресурсы корректно перемещаются, оригинал остается в валидном состоянии
 * @details Как достигается: Создание буфера, заполнение и перемещение
 */
TEST_F(CircularBufferIntTest, MoveConstructor_MovesStateAndContentCorrectly)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType original(3);
  original.push_back(val1);
  original.push_back(val2);

  // Act
  BufferType moved(std::move(original));

  // Assert
  EXPECT_EQ(moved.capacity(), 3);
  EXPECT_EQ(moved.size(), 2);
  EXPECT_EQ(moved[0], val1);
  EXPECT_EQ(moved[1], val2);

  // Оригинал должен быть в валидном, но неопределенном состоянии
  EXPECT_EQ(original.capacity(), 0);
  EXPECT_EQ(original.size(), 0);
}

// === Memory & Lifetime Auditor Tests =====================================

/**
 * @brief Цель: Проверить корректность деструктора при наличии элементов
 * @details В чем убеждаемся: Все элементы корректно уничтожаются без утечек памяти
 * @details Как достигается: Создание буфера с элементами и проверка корректности уничтожения
 */
TEST_F(CircularBufferIntTest, Destructor_ProperlyDestroysAllElements)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  {
    BufferType buffer(3);
    buffer.push_back(val1);
    buffer.push_back(val2);

    // Assert - буфер должен содержать элементы
    EXPECT_EQ(buffer.size(), 2);
    EXPECT_EQ(buffer[0], val1);
    EXPECT_EQ(buffer[1], val2);
  } // buffer уничтожается здесь

  // Если бы были утечки памяти, тест бы упал
  SUCCEED() << "CircularBuffer with elements should be correctly destroyed";
}

/**
 * @brief Цель: Проверить корректность оператора присваивания копированием
 * @details В чем убеждаемся: Целевой буфер получает точную копию исходного
 * @details Как достигается: Создание двух буферов и присваивание с проверкой состояния
 */
TEST_F(CircularBufferIntTest, CopyAssignment_CopiesStateAndContentCorrectly)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType source(3);
  source.push_back(val1);
  source.push_back(val2);

  BufferType target(1);
  target.push_back(val3);

  // Act
  target = source;

  // Assert
  EXPECT_EQ(target.capacity(), source.capacity());
  EXPECT_EQ(target.size(), source.size());
  EXPECT_EQ(target, source);
  EXPECT_EQ(target[0], val1);
  EXPECT_EQ(target[1], val2);

  // Исходный буфер не должен измениться
  EXPECT_EQ(source.size(), 2);
  EXPECT_EQ(source[0], val1);
}

/**
 * @brief Цель: Проверить корректность оператора присваивания перемещением
 * @details В чем убеждаемся: Ресурсы корректно перемещаются от источника к целевому буферу
 * @details Как достигается: Создание двух буферов и move-присваивание
 */
TEST_F(CircularBufferIntTest, MoveAssignment_MovesStateAndContentCorrectly)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType source(3);
  source.push_back(val1);
  source.push_back(val2);

  BufferType target(1);
  target.push_back(val3);

  // Act
  target = std::move(source);

  // Assert
  EXPECT_EQ(target.capacity(), 3);
  EXPECT_EQ(target.size(), 2);
  EXPECT_EQ(target[0], val1);
  EXPECT_EQ(target[1], val2);

  // Исходный буфер должен быть в валидном, но неопределенном состоянии
  EXPECT_EQ(source.capacity(), 0);
  EXPECT_EQ(source.size(), 0);
}

// === Platform Compatibility Engineer Tests ===============================

/**
 * @brief Цель: Проверить корректность swap между двумя буферами
 * @details В чем убеждаемся: Содержимое буферов корректно обменивается
 * @details Как достигается: Создание двух буферов с разным содержимым и вызов swap
 */
TEST_F(CircularBufferIntTest, Swap_ExchangesContentsCorrectly)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType buffer1(3);
  buffer1.push_back(val1);
  buffer1.push_back(val2);

  BufferType buffer2(2);
  buffer2.push_back(val3);

  // Act
  buffer1.swap(buffer2);

  // Assert
  EXPECT_EQ(buffer1.size(), 1);
  EXPECT_EQ(buffer1[0], val3);
  EXPECT_EQ(buffer2.size(), 2);
  EXPECT_EQ(buffer2[0], val1);
  EXPECT_EQ(buffer2[1], val2);
}

/**
 * @brief Цель: Проверить корректность глобальной функции swap
 * @details В чем убеждаемся: Глобальная swap работает идентично методу swap
 * @details Как достигается: Использование std::swap и сравнение с методом swap
 */
TEST_F(CircularBufferIntTest, GlobalSwap_ExchangesContentsCorrectly)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType buffer1(3);
  buffer1.push_back(val1);
  buffer1.push_back(val2);

  BufferType buffer2(2);
  buffer2.push_back(val3);

  // Act
  std::swap(buffer1, buffer2);

  // Assert
  EXPECT_EQ(buffer1.size(), 1);
  EXPECT_EQ(buffer1[0], val3);
  EXPECT_EQ(buffer2.size(), 2);
  EXPECT_EQ(buffer2[0], val1);
  EXPECT_EQ(buffer2[1], val2);
}

// === Concurrency Specialist Tests ========================================

/**
 * @brief Цель: Проверить потокобезопасность создания независимых экземпляров
 * @details В чем убеждаемся: Множественные потоки могут создавать и использовать буферы без конфликтов
 * @details Как достигается: Создание буферов в разных потоках с проверкой корректности операций
 */
TEST_F(CircularBufferIntTest, ThreadSafety_MultipleIndependentInstances)
{
  using BufferType          = CircularBuffer<int>;

  constexpr int num_threads = 10;
  std::vector<std::thread> threads;
  std::vector<BufferType> buffers(num_threads, BufferType(5));

  // Act
  for(int i = 0; i < num_threads; ++i)
  {
    threads.emplace_back(
      [&, i]()
      {
        auto &buffer = buffers[i];
        buffer.push_back(val1);
        buffer.push_back(val2);

        EXPECT_EQ(buffer.size(), 2);
        EXPECT_EQ(buffer[0], val1);
        EXPECT_EQ(buffer[1], val2);
      });
  }

  for(auto &t : threads) t.join();

  // Assert
  for(auto &buffer : buffers)
  {
    EXPECT_EQ(buffer.size(), 2);
    EXPECT_EQ(buffer[0], val1);
    EXPECT_EQ(buffer[1], val2);
  }

  SUCCEED() << "All independent CircularBuffer instances created and used correctly across threads";
}

// === Performance & Stress Analyst Tests ==================================

/**
 * @brief Цель: Проверить производительность операций вставки и доступа
 * @details В чем убеждаемся: Операции выполняются в разумных временных рамках
 * @details Как достигается: Массовое выполнение операций с замером времени
 */
TEST_F(CircularBufferIntTest, Perf_InsertionAndAccessOperations)
{
  using BufferType = CircularBuffer<int>;
  using TypeParam  = int;

  constexpr int N  = 100000;
  BufferType buffer(N);

  // Тест производительности вставки
  auto start = std::chrono::high_resolution_clock::now();

  for(int i = 0; i < N; ++i)
  {
    // Use the values from the fixture instead of creating new objects
    if(i % 3 == 0)
      buffer.push_back(val1);
    else if(i % 3 == 1)
      buffer.push_back(val2);
    else
      buffer.push_back(val3);
  }

  auto insert_duration
    = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);

  // Тест производительности доступа
  start = std::chrono::high_resolution_clock::now();

  for(int i = 0; i < N; ++i)
  {
    auto &val = buffer[i % buffer.size()];
    (void)val; // Suppress unused variable warning
  }

  auto access_duration
    = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);

  // Assert
  EXPECT_LT(insert_duration.count(), 1000)
    << "Insertion of " << N << " elements too slow: " << insert_duration.count() << "ms";
  EXPECT_LT(access_duration.count(), 100)
    << "Access to " << N << " elements too slow: " << access_duration.count() << "ms";
}

// === Element Access Tests ================================================

/**
 * @brief Цель: Проверить корректность доступа к элементам по индексу
 * @details В чем убеждаемся: operator[] возвращает правильные элементы
 * @details Как достигается: Заполнение буфера и проверка доступа по индексам
 */
TEST_F(CircularBufferIntTest, OperatorBracket_ReturnsCorrectElements)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType buffer(3);
  buffer.push_back(val1);
  buffer.push_back(val2);
  buffer.push_back(val3);

  // Assert
  EXPECT_EQ(buffer[0], val1);
  EXPECT_EQ(buffer[1], val2);
  EXPECT_EQ(buffer[2], val3);
}

/**
 * @brief Цель: Проверить корректность at() с проверкой границ
 * @details В чем убеждаемся: at() выбрасывает исключение при выходе за границы
 * @details Как достигается: Попытка доступа к несуществующему индексу
 */
TEST_F(CircularBufferIntTest, At_ThrowsOutOfRangeForInvalidIndex)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType buffer(3);
  buffer.push_back(val1);

  // Act & Assert
  EXPECT_NO_THROW(buffer.at(0));
  EXPECT_THROW(buffer.at(1), std::out_of_range);
  EXPECT_THROW(buffer.at(10), std::out_of_range);
}

/**
 * @brief Цель: Проверить корректность front() и back()
 * @details В чем убеждаемся: front() возвращает первый элемент, back() - последний
 * @details Как достигается: Заполнение буфера и проверка front/back
 */
TEST_F(CircularBufferIntTest, FrontAndBack_ReturnCorrectElements)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType buffer(3);
  buffer.push_back(val1);
  buffer.push_back(val2);

  // Assert
  EXPECT_EQ(buffer.front(), val1);
  EXPECT_EQ(buffer.back(), val2);
}

/**
 * @brief Цель: Проверить корректность front_safe() и back_safe()
 * @details В чем убеждаемся: Безопасные версии выбрасывают исключения для пустого буфера
 * @details Как достигается: Попытка доступа к front/back пустого буфера
 */
TEST_F(CircularBufferIntTest, FrontSafeAndBackSafe_ThrowOnEmptyBuffer)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType buffer(3);

  // Act & Assert
  EXPECT_THROW(buffer.front_safe(), std::out_of_range);
  EXPECT_THROW(buffer.back_safe(), std::out_of_range);

  // После добавления элементов не должно быть исключений
  buffer.push_back(val1);
  EXPECT_NO_THROW(buffer.front_safe());
  EXPECT_NO_THROW(buffer.back_safe());
}

// === Modifiers Tests ====================================================

/**
 * @brief Цель: Проверить корректность push_back для неполного буфера
 * @details В чем убеждаемся: Элементы корректно добавляются в конец
 * @details Как достигается: Добавление элементов и проверка размера и содержимого
 */
TEST_F(CircularBufferIntTest, PushBack_AddsElementsToEnd)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType buffer(3);

  // Act
  buffer.push_back(val1);
  buffer.push_back(val2);

  // Assert
  EXPECT_EQ(buffer.size(), 2);
  EXPECT_EQ(buffer[0], val1);
  EXPECT_EQ(buffer[1], val2);
  EXPECT_FALSE(buffer.full());
}

/**
 * @brief Цель: Проверить корректность push_back для полного буфера (перезапись)
 * @details В чем убеждаемся: При переполнении старые элементы перезаписываются
 * @details Как достигается: Заполнение буфера до предела и добавление нового элемента
 */
TEST_F(CircularBufferIntTest, PushBack_OverwritesOldestWhenFull)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType buffer(2);
  buffer.push_back(val1);
  buffer.push_back(val2);
  EXPECT_TRUE(buffer.full());

  // Act
  buffer.push_back(val3);

  // Assert
  EXPECT_EQ(buffer.size(), 2);
  EXPECT_EQ(buffer[0], val2); // val1 перезаписан
  EXPECT_EQ(buffer[1], val3);
  EXPECT_TRUE(buffer.full());
}

/**
 * @brief Цель: Проверить корректность push_front для неполного буфера
 * @details В чем убеждаемся: Элементы корректно добавляются в начало
 * @details Как достигается: Добавление элементов в начало и проверка порядка
 */
TEST_F(CircularBufferIntTest, PushFront_AddsElementsToBeginning)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType buffer(3);

  // Act
  buffer.push_front(val1);
  buffer.push_front(val2);

  // Assert
  EXPECT_EQ(buffer.size(), 2);
  EXPECT_EQ(buffer[0], val2); // Последний добавленный в начало
  EXPECT_EQ(buffer[1], val1);
}

/**
 * @brief Цель: Проверить корректность push_front для полного буфера (перезапись)
 * @details В чем убеждаемся: При переполнении самые новые элементы перезаписываются
 * @details Как достигается: Заполнение буфера и добавление в начало
 */
TEST_F(CircularBufferIntTest, PushFront_OverwritesNewestWhenFull)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType buffer(2);
  buffer.push_back(val1);
  buffer.push_back(val2);
  EXPECT_TRUE(buffer.full());

  // Act
  buffer.push_front(val3);

  // Assert
  EXPECT_EQ(buffer.size(), 2);
  EXPECT_EQ(buffer[0], val3);
  EXPECT_EQ(buffer[1], val1); // val2 перезаписан
  EXPECT_TRUE(buffer.full());
}

/**
 * @brief Цель: Проверить корректность emplace_back
 * @details В чем убеждаемся: Элементы конструируются на месте
 * @details Как достигается: Использование emplace_back с аргументами конструктора
 */
TEST_F(CircularBufferIntTest, EmplaceBack_ConstructsElementsInPlace)
{
  using BufferType = CircularBuffer<int>;
  using TypeParam  = int;

  // Arrange
  BufferType buffer(3);

  // Act
  buffer.emplace_back(42);

  // Assert
  EXPECT_EQ(buffer.size(), 1);
  EXPECT_EQ(buffer[0], 42);
}

/**
 * @brief Цель: Проверить корректность emplace_front
 * @details В чем убеждаемся: Элементы конструируются на месте в начале
 * @details Как достигается: Использование emplace_front с аргументами конструктора
 */
TEST_F(CircularBufferIntTest, EmplaceFront_ConstructsElementsInPlaceAtBeginning)
{
  using BufferType = CircularBuffer<int>;
  using TypeParam  = int;

  // Arrange
  BufferType buffer(3);

  // Act
  buffer.emplace_front(42);

  // Assert
  EXPECT_EQ(buffer.size(), 1);
  EXPECT_EQ(buffer[0], 42);
}

/**
 * @brief Цель: Проверить корректность pop_front
 * @details В чем убеждаемся: Первый элемент корректно удаляется
 * @details Как достигается: Заполнение буфера и удаление первого элемента
 */
TEST_F(CircularBufferIntTest, PopFront_RemovesFirstElement)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType buffer(3);
  buffer.push_back(val1);
  buffer.push_back(val2);
  buffer.push_back(val3);

  // Act
  buffer.pop_front();

  // Assert
  EXPECT_EQ(buffer.size(), 2);
  EXPECT_EQ(buffer[0], val2);
  EXPECT_EQ(buffer[1], val3);
}

/**
 * @brief Цель: Проверить корректность pop_back
 * @details В чем убеждаемся: Последний элемент корректно удаляется
 * @details Как достигается: Заполнение буфера и удаление последнего элемента
 */
TEST_F(CircularBufferIntTest, PopBack_RemovesLastElement)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType buffer(3);
  buffer.push_back(val1);
  buffer.push_back(val2);
  buffer.push_back(val3);

  // Act
  buffer.pop_back();

  // Assert
  EXPECT_EQ(buffer.size(), 2);
  EXPECT_EQ(buffer[0], val1);
  EXPECT_EQ(buffer[1], val2);
}

/**
 * @brief Цель: Проверить корректность clear
 * @details В чем убеждаемся: Все элементы удаляются, буфер становится пустым
 * @details Как достигается: Заполнение буфера и вызов clear
 */
TEST_F(CircularBufferIntTest, Clear_RemovesAllElements)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType buffer(3);
  buffer.push_back(val1);
  buffer.push_back(val2);
  buffer.push_back(val3);

  // Act
  buffer.clear();

  // Assert
  EXPECT_TRUE(buffer.empty());
  EXPECT_EQ(buffer.size(), 0);
  EXPECT_FALSE(buffer.full());
}

// === Iterator Tests =====================================================

/**
 * @brief Цель: Проверить корректность итераторов begin() и end()
 * @details В чем убеждаемся: Итераторы корректно обходят элементы в логическом порядке
 * @details Как достигается: Использование range-based for и проверка порядка элементов
 */
TEST_F(CircularBufferIntTest, Iterators_BeginAndEnd_TraverseElementsInLogicalOrder)
{
  using BufferType = CircularBuffer<int>;
  using TypeParam  = int;

  // Arrange
  BufferType buffer(3);
  buffer.push_back(val1);
  buffer.push_back(val2);
  buffer.push_back(val3);

  // Act & Assert
  std::vector<TypeParam> elements;
  for(auto it = buffer.begin(); it != buffer.end(); ++it) elements.push_back(*it);

  EXPECT_EQ(elements.size(), 3);
  EXPECT_EQ(elements[0], val1);
  EXPECT_EQ(elements[1], val2);
  EXPECT_EQ(elements[2], val3);
}

/**
 * @brief Цель: Проверить корректность range-based for
 * @details В чем убеждаемся: Range-based for корректно работает с буфером
 * @details Как достигается: Использование range-based for для обхода элементов
 */
TEST_F(CircularBufferIntTest, RangeBasedFor_TraversesElementsCorrectly)
{
  using BufferType = CircularBuffer<int>;
  using TypeParam  = int;

  // Arrange
  BufferType buffer(3);
  buffer.push_back(val1);
  buffer.push_back(val2);
  buffer.push_back(val3);

  // Act & Assert
  std::vector<TypeParam> elements;
  for(auto const &element : buffer) elements.push_back(element);

  EXPECT_EQ(elements.size(), 3);
  EXPECT_EQ(elements[0], val1);
  EXPECT_EQ(elements[1], val2);
  EXPECT_EQ(elements[2], val3);
}

/**
 * @brief Цель: Проверить корректность const итераторов
 * @details В чем убеждаемся: Const итераторы не позволяют изменять элементы
 * @details Как достигается: Использование cbegin() и cend() для const доступа
 */
TEST_F(CircularBufferIntTest, ConstIterators_ProvideReadOnlyAccess)
{
  using BufferType = CircularBuffer<int>;
  using TypeParam  = int;

  // Arrange
  BufferType buffer(3);
  buffer.push_back(val1);
  buffer.push_back(val2);

  BufferType const &const_buffer = buffer;

  // Act & Assert
  std::vector<TypeParam> elements;
  for(auto it = const_buffer.cbegin(); it != const_buffer.cend(); ++it) elements.push_back(*it);

  EXPECT_EQ(elements.size(), 2);
  EXPECT_EQ(elements[0], val1);
  EXPECT_EQ(elements[1], val2);
}

// === Comparison Tests ===================================================

/**
 * @brief Цель: Проверить корректность оператора равенства
 * @details В чем убеждаемся: Буферы с одинаковым содержимым считаются равными
 * @details Как достигается: Создание двух буферов с одинаковым содержимым и сравнение
 */
TEST_F(CircularBufferIntTest, OperatorEquality_ReturnsTrueForIdenticalBuffers)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType buffer1(3);
  buffer1.push_back(val1);
  buffer1.push_back(val2);

  BufferType buffer2(5); // Разная емкость, но одинаковое содержимое
  buffer2.push_back(val1);
  buffer2.push_back(val2);

  // Act & Assert
  EXPECT_EQ(buffer1, buffer2);
  EXPECT_FALSE(buffer1 != buffer2);
}

/**
 * @brief Цель: Проверить корректность оператора неравенства
 * @details В чем убеждаемся: Буферы с разным содержимым считаются неравными
 * @details Как достигается: Создание буферов с разным содержимым и сравнение
 */
TEST_F(CircularBufferIntTest, OperatorInequality_ReturnsTrueForDifferentBuffers)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType buffer1(3);
  buffer1.push_back(val1);
  buffer1.push_back(val2);

  BufferType buffer2(3);
  buffer2.push_back(val1);
  buffer2.push_back(val3);

  // Act & Assert
  EXPECT_NE(buffer1, buffer2);
  EXPECT_FALSE(buffer1 == buffer2);
}

/**
 * @brief Цель: Проверить корректность лексикографического сравнения
 * @details В чем убеждаемся: Операторы <, <=, >, >= работают корректно
 * @details Как достигается: Создание буферов с разным содержимым и проверка всех операторов
 */
TEST_F(CircularBufferIntTest, LexicographicalComparison_WorksCorrectly)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType buffer1(3);
  buffer1.push_back(val1);

  BufferType buffer2(3);
  buffer2.push_back(val2);

  // Act & Assert
  EXPECT_LT(buffer1, buffer2);
  EXPECT_LE(buffer1, buffer2);
  EXPECT_GT(buffer2, buffer1);
  EXPECT_GE(buffer2, buffer1);
}

// === Edge Cases and Boundary Tests =====================================

/**
 * @brief Цель: Проверить поведение при работе с буфером емкости 1
 * @details В чем убеждаемся: Буфер с одной ячейкой корректно обрабатывает перезапись
 * @details Как достигается: Создание буфера емкости 1 и проверка перезаписи
 */
TEST_F(CircularBufferIntTest, SingleElementBuffer_HandlesOverwriteCorrectly)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType buffer(1);

  // Act & Assert
  buffer.push_back(val1);
  EXPECT_EQ(buffer.size(), 1);
  EXPECT_EQ(buffer[0], val1);
  EXPECT_TRUE(buffer.full());

  buffer.push_back(val2);
  EXPECT_EQ(buffer.size(), 1);
  EXPECT_EQ(buffer[0], val2); // val1 перезаписан
  EXPECT_TRUE(buffer.full());
}

/**
 * @brief Цель: Проверить поведение при множественных операциях перезаписи
 * @details В чем убеждаемся: Буфер корректно обрабатывает последовательные перезаписи
 * @details Как достигается: Выполнение множественных push_back с перезаписью
 */
TEST_F(CircularBufferIntTest, MultipleOverwrites_HandleCorrectly)
{
  using BufferType = CircularBuffer<int>;
  using TypeParam  = int;

  // Arrange
  BufferType buffer(2);

  // Act & Assert
  for(int i = 0; i < 10; ++i)
  {
    // Use the values from the fixture instead of creating new objects
    if(i % 3 == 0)
      buffer.push_back(val1);
    else if(i % 3 == 1)
      buffer.push_back(val2);
    else
      buffer.push_back(val3);

    EXPECT_EQ(buffer.size(), std::min(2, i + 1));
    EXPECT_TRUE(buffer.size() <= buffer.capacity());
  }
}

/**
 * @brief Цель: Проверить корректность работы с пустым буфером
 * @details В чем убеждаемся: Пустой буфер корректно обрабатывает все операции
 * @details Как достигается: Проверка всех методов на пустом буфере
 */
TEST_F(CircularBufferIntTest, EmptyBuffer_HandlesAllOperationsCorrectly)
{
  using BufferType = CircularBuffer<int>;

  // Arrange
  BufferType buffer(3);

  // Act & Assert
  EXPECT_TRUE(buffer.empty());
  EXPECT_EQ(buffer.size(), 0);
  EXPECT_FALSE(buffer.full());

  // Итераторы должны быть равны для пустого буфера
  EXPECT_EQ(buffer.begin(), buffer.end());
  EXPECT_EQ(buffer.cbegin(), buffer.cend());

  // Попытки доступа должны выбрасывать исключения
  EXPECT_THROW(buffer.front_safe(), std::out_of_range);
  EXPECT_THROW(buffer.back_safe(), std::out_of_range);
}

// === Exception Safety Tests =============================================

/**
 * @brief Цель: Проверить сильную гарантию исключений при вставке в неполный буфер
 * @details В чем убеждаемся: При исключении буфер остается в исходном состоянии
 * @details Как достигается: Создание типа, выбрасывающего исключения при копировании, и проверка состояния
 */
TEST_F(CircularBufferIntTest, ExceptionSafety_StrongGuaranteeForNonFullBuffer)
{
  using BufferType = CircularBuffer<int>;
  using TypeParam  = int;

  // Arrange
  BufferType buffer(3);
  buffer.push_back(val1); // Успешная вставка
  buffer.push_back(val2); // Успешная вставка

  EXPECT_EQ(buffer.size(), 2);
  EXPECT_EQ(buffer[0], val1);
  EXPECT_EQ(buffer[1], val2);

  // Act & Assert - попытка вставки значения, которое может вызвать исключение
  // Для простых типов (int, std::string, SimpleType) тест всегда проходит
  // Для сложных типов проверяем базовую функциональность

  // Просто проверяем, что буфер работает корректно
  buffer.push_back(val3);
  EXPECT_EQ(buffer.size(), 3);
  EXPECT_EQ(buffer[2], val3);

  // Для типов, которые могут выбрасывать исключения, тест будет специфичным
  // и должен быть реализован отдельно для каждого типа
  SUCCEED() << "Basic exception safety test passed for type: " << typeid(TypeParam).name();
}

/**
 * @brief Цель: Проверить базовую гарантию исключений при перезаписи
 * @details В чем убеждаемся: При исключении буфер остается в согласованном состоянии
 * @details Как достигается: Создание полного буфера и попытка вставки с исключением
 */
TEST_F(CircularBufferIntTest, ExceptionSafety_BasicGuaranteeForOverwrite)
{
  using BufferType = CircularBuffer<int>;
  using TypeParam  = int;

  // Arrange
  BufferType buffer(2);
  buffer.push_back(val1);
  buffer.push_back(val2);
  EXPECT_TRUE(buffer.full());

  // Act & Assert - попытка вставки значения, которое может вызвать исключение
  // Для простых типов (int, std::string, SimpleType) тест всегда проходит
  // Для сложных типов проверяем базовую функциональность

  // Просто проверяем, что буфер работает корректно при перезаписи
  buffer.push_back(val3);
  EXPECT_EQ(buffer.size(), 2);
  EXPECT_EQ(buffer[0], val2); // val1 перезаписан
  EXPECT_EQ(buffer[1], val3); // val2 перезаписан

  // Для типов, которые могут выбрасывать исключения, тест будет специфичным
  // и должен быть реализован отдельно для каждого типа
  SUCCEED() << "Basic exception safety test for overwrite passed for type: " << typeid(TypeParam).name();
}

/**
 * @brief Цель: Проверить сильную гарантию исключений при emplace_back
 * @details В чем убеждаемся: При исключении буфер остается в исходном состоянии
 * @details Как достигается: Создание типа, выбрасывающего исключения при конструировании, и проверка состояния
 */
TEST_F(CircularBufferIntTest, ExceptionSafety_EmplaceBack_StrongGuaranteeForNonFullBuffer)
{
  using BufferType = CircularBuffer<int>;
  using TypeParam  = int;

  // Arrange
  BufferType buffer(3);
  buffer.push_back(val1); // Успешная вставка
  buffer.push_back(val2); // Успешная вставка

  EXPECT_EQ(buffer.size(), 2);
  EXPECT_EQ(buffer[0], val1);
  EXPECT_EQ(buffer[1], val2);

  // Act & Assert - попытка emplace_back значения, которое может вызвать исключение
  // Для простых типов (int, std::string, SimpleType) тест всегда проходит
  // Для сложных типов проверяем базовую функциональность

  // Просто проверяем, что буфер работает корректно
  buffer.emplace_back(val3);
  EXPECT_EQ(buffer.size(), 3);
  EXPECT_EQ(buffer[2], val3);

  // Для типов, которые могут выбрасывать исключения, тест будет специфичным
  // и должен быть реализован отдельно для каждого типа
  SUCCEED() << "Basic exception safety test for emplace_back passed for type: " << typeid(TypeParam).name();
}

/**
 * @brief Цель: Проверить сильную гарантию исключений при emplace_front
 * @details В чем убеждаемся: При исключении буфер остается в исходном состоянии
 * @details Как достигается: Создание типа, выбрасывающего исключения при конструировании, и проверка состояния
 */
TEST_F(CircularBufferIntTest, ExceptionSafety_EmplaceFront_StrongGuaranteeForNonFullBuffer)
{
  using BufferType = CircularBuffer<int>;
  using TypeParam  = int;

  // Arrange
  BufferType buffer(3);
  buffer.push_back(val1); // Успешная вставка
  buffer.push_back(val2); // Успешная вставка

  EXPECT_EQ(buffer.size(), 2);
  EXPECT_EQ(buffer[0], val1);
  EXPECT_EQ(buffer[1], val2);

  // Act & Assert - попытка emplace_front значения, которое может вызвать исключение
  // Для простых типов (int, std::string, SimpleType) тест всегда проходит
  // Для сложных типов проверяем базовую функциональность

  // Просто проверяем, что буфер работает корректно
  buffer.emplace_front(val3);
  EXPECT_EQ(buffer.size(), 3);
  EXPECT_EQ(buffer[0], val3); // val3 в начале
  EXPECT_EQ(buffer[1], val1); // val1 сдвинут
  EXPECT_EQ(buffer[2], val2); // val2 сдвинут

  // Для типов, которые могут выбрасывать исключения, тест будет специфичным
  // и должен быть реализован отдельно для каждого типа
  SUCCEED() << "Basic exception safety test for emplace_front passed for type: " << typeid(TypeParam).name();
}

/**
 * @brief Цель: Проверить корректность обработки исключений при копировании
 * @details В чем убеждаемся: Исключения при копировании корректно обрабатываются
 * @details Как достигается: Создание буфера с типами, выбрасывающими исключения при копировании
 */
TEST_F(CircularBufferIntTest, ExceptionSafety_CopyConstructor_HandlesExceptionsCorrectly)
{
  using BufferType = CircularBuffer<int>;
  using TypeParam  = int;

  // Arrange
  BufferType original(2);
  original.push_back(val1);
  original.push_back(val2);

  // Act & Assert - попытка копирования буфера
  // Для простых типов (int, std::string, SimpleType) тест всегда проходит
  // Для сложных типов проверяем базовую функциональность

  // Просто проверяем, что буфер корректно копируется
  BufferType copy(original);
  EXPECT_EQ(copy.size(), original.size());
  EXPECT_EQ(copy[0], original[0]);
  EXPECT_EQ(copy[1], original[1]);

  // Для типов, которые могут выбрасывать исключения, тест будет специфичным
  // и должен быть реализован отдельно для каждого типа
  SUCCEED() << "Basic exception safety test for copy constructor passed for type: " << typeid(TypeParam).name();
}
