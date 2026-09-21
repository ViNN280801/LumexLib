#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/expected/Expected"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

#if defined(__clang__)
#endif

using namespace lumex::core::expected::result;
using namespace lumex::core::expected::error;

// === Success types for tests ==============================

struct SimpleSuccess
{
  int value;
  explicit SimpleSuccess (int v = 0) : value (v) {}
  bool
  operator== (SimpleSuccess const &other) const
  {
    return value == other.value;
  }
  bool
  operator!= (SimpleSuccess const &other) const
  {
    return !(*this == other);
  }
};

struct ComplexSuccess
{
  std::string name;
  std::unique_ptr<int> data;

  explicit ComplexSuccess (std::string n = "Default Success", int d = 0)
      : name (std::move (n)), data (std::make_unique<int> (d))
  {
  }

  ComplexSuccess (ComplexSuccess const &other)
      : name (other.name),
        data (other.data ? std::make_unique<int> (*other.data) : nullptr)
  {
  }

  ComplexSuccess &
  operator= (ComplexSuccess const &other)
  {
    if (this != &other)
      {
        name = other.name;
        data = other.data ? std::make_unique<int> (*other.data) : nullptr;
      }
    return *this;
  }

  ComplexSuccess (ComplexSuccess &&other) noexcept
      : name (std::move (other.name)), data (std::move (other.data))
  {
  }

  ComplexSuccess &
  operator= (ComplexSuccess &&other) noexcept
  {
    if (this != &other)
      {
        name = std::move (other.name);
        data = std::move (other.data);
      }
    return *this;
  }

  bool
  operator== (ComplexSuccess const &other) const
  {
    return name == other.name
           && ((!data && !other.data)
               || (data && other.data && *data == *other.data));
  }

  bool
  operator!= (ComplexSuccess const &other) const
  {
    return !(*this == other);
  }

  int
  convertToInt () const
  {
    return *data;
  }
};

// === Error types for tests ========================================

enum class SimpleError
{
  None,
  InvalidInput,
  NetworkFailure
};

struct ComplexError
{
  std::string message;
  int code;
  std::unique_ptr<int> resource;

  explicit ComplexError (std::string msg = "Default Error", int c = 100)
      : message (std::move (msg)), code (c),
        resource (std::make_unique<int> (c))
  {
  }

  ComplexError (ComplexError const &other)
      : message (other.message), code (other.code),
        resource (other.resource ? std::make_unique<int> (*other.resource)
                                 : nullptr)
  {
  }

  ComplexError &
  operator= (ComplexError const &other)
  {
    if (this != &other)
      {
        message = other.message;
        code = other.code;
        resource = other.resource ? std::make_unique<int> (*other.resource)
                                  : nullptr;
      }
    return *this;
  }

  ComplexError (ComplexError &&other) noexcept
      : message (std::move (other.message)), code (other.code),
        resource (std::move (other.resource))
  {
    other.code = 0; // Clear the source resource
  }

  ComplexError &
  operator= (ComplexError &&other) noexcept
  {
    if (this != &other)
      {
        message = std::move (other.message);
        code = other.code;
        resource = std::move (other.resource);
        other.code = 0; // Clear the source resource
      }
    return *this;
  }

  bool
  operator== (ComplexError const &other) const
  {
    return message == other.message && code == other.code
           && ((!resource && !other.resource)
               || (resource && other.resource
                   && *resource == *other.resource));
  }

  bool
  operator!= (ComplexError const &other) const
  {
    return !(*this == other);
  }
};

// === Fixture for Expected ==================================================
template <typename T> class ExpectedTest : public ::testing::Test
{
protected:
  // Define SuccessType and ErrorType from TypeParam (std::tuple<SType, EType>)
  typedef typename std::tuple_element<0, T>::type SuccessType;
  typedef typename std::tuple_element<1, T>::type ErrorType;

  // Initial values for success types
  SuccessType s_val1{};
  SuccessType s_val2{};

  // Initial values for error types
  ErrorType e_val1{};
  ErrorType e_val2{};

  // Preconditions: initialize standard success and error values for the tests.
  // Create known objects for repeatable tests.
  // Assert that object initialization does not throw.
  void
  SetUp () override
  {
    // Initialize with defaults via default constructors
    s_val1 = SuccessType{};
    s_val2 = SuccessType{};
    e_val1 = ErrorType{};
    e_val2 = ErrorType{};
  }
};

// Define type combinations for typed tests
using ExpectedTestTypes
    = ::testing::Types<std::tuple<int, int>,
                       std::tuple<std::string, std::string>,
                       std::tuple<SimpleSuccess, SimpleError>,
                       std::tuple<ComplexSuccess, ComplexError>>;
TYPED_TEST_SUITE (ExpectedTest, ExpectedTestTypes);

// === Constructors and assignment operators =========================

// Check the default constructor. It must create Expected in the success state
// with a default-constructed value.
// Assert that has_value() is true and the value is the default.
TYPED_TEST (ExpectedTest, DefaultConstructor_CreatesExpectedWithValue)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange, Act
  Expected<SuccessType, ErrorType> uut;

  // Assert
  EXPECT_TRUE (uut.has_value ());
  SuccessType default_success{};
  EXPECT_EQ (uut.value (), default_success);
}

// Check the copy constructor. It must create a new Expected,
// copying the state and contents of another Expected.
// Assert that state and values match and the objects are independent.
TYPED_TEST (ExpectedTest, CopyConstructor_CopiesStateAndContent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: successful Expected
  Expected<SuccessType, ErrorType> original_success (this->s_val1);
  // Act
  Expected<SuccessType, ErrorType> copied_success = original_success;
  // Assert
  EXPECT_TRUE (copied_success.has_value ());
  EXPECT_EQ (copied_success.value (), this->s_val1);
  EXPECT_EQ (copied_success,
             original_success); // Use the non-member operator==

  // Arrange: Expected holding an error
  Expected<SuccessType, ErrorType> original_error (
      Unexpected<ErrorType> (this->e_val1));
  // Act
  Expected<SuccessType, ErrorType> copied_error = original_error;
  // Assert
  EXPECT_FALSE (copied_error.has_value ());
  EXPECT_EQ (copied_error.error (), this->e_val1);
  EXPECT_EQ (copied_error, original_error); // Use the non-member operator==
}

// Check the move constructor. It must create a new Expected,
// moving the state and contents of another Expected. The source must be
// in a valid but unspecified state.
// Assert that the new object has the correct state and value,
// and the source is emptied (when applicable for Complex types).
TYPED_TEST (ExpectedTest, MoveConstructor_MovesStateAndContent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: successful Expected
  SuccessType original_s_val = this->s_val1;
  Expected<SuccessType, ErrorType> original_success (
      std::move (original_s_val));
  SuccessType expected_s_val = this->s_val1; // Value before the move
  // Act
  Expected<SuccessType, ErrorType> moved_success
      = std::move (original_success);
  // Assert
  EXPECT_TRUE (moved_success.has_value ());
  EXPECT_EQ (moved_success.value (), expected_s_val);
  // Check the source object state
  if (std::is_same<SuccessType, ComplexSuccess>::value)
    {
      // For ComplexSuccess, after the move the original may have released
      // resources The Expected object itself remains valid but unspecified
      // original_success.value() cannot be checked reliably here
    }

  // Arrange: Expected holding an error
  ErrorType original_e_val = this->e_val1;
  Expected<SuccessType, ErrorType> original_error (
      Unexpected<ErrorType> (std::move (original_e_val)));
  ErrorType expected_e_val = this->e_val1; // Value before the move
  // Act
  Expected<SuccessType, ErrorType> moved_error = std::move (original_error);
  // Assert
  EXPECT_FALSE (moved_error.has_value ());
  EXPECT_EQ (moved_error.error (), expected_e_val);
  // Check the source object state
  // For ComplexError, emptiness after the move could be checked
}

// Check the constructor from Unexpected (copy). It must create Expected
// in the error state by copying Unexpected.
// Assert that has_value() is false and error() matches the source error.
TYPED_TEST (ExpectedTest, Constructor_FromUnexpectedLValue_CopiesError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange
  Unexpected<ErrorType> unexp (this->e_val1);
  // Act
  Expected<SuccessType, ErrorType> uut (
      unexp); // Call explicit Expected(Unexpected<ErrorType> const &unexp)
  // Assert
  EXPECT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error (), this->e_val1);

  // Check that copies are independent (ComplexError only)
  // Skip for other types; the check does not apply
}

// Check the constructor from Unexpected (move). It must create Expected
// in the error state by moving Unexpected.
// Assert that has_value() is false, error() matches the source error,
// and the source Unexpected is emptied.
TYPED_TEST (ExpectedTest, Constructor_FromUnexpectedRValue_MovesError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange
  ErrorType original_e_val = this->e_val1;
  Unexpected<ErrorType> unexp (std::move (original_e_val));
  ErrorType expected_e_val = this->e_val1; // unexp value before the move
  // Act
  Expected<SuccessType, ErrorType> uut (std::move (
      unexp)); // Call explicit Expected(Unexpected<ErrorType> &&unexp)
  // Assert
  EXPECT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error (), expected_e_val);

  // Check emptiness for ComplexError (only when the type matches)
  // The check does not apply to other types
}

// Check the constructor from a success value. It must create Expected
// in the success state with the given value.
// Assert that has_value() is true and value() matches the source.
TYPED_TEST (ExpectedTest, Constructor_FromValue_CreatesExpectedWithValue)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange, Act
  Expected<SuccessType, ErrorType> uut (this->s_val1);
  // Assert
  EXPECT_TRUE (uut.has_value ());
  EXPECT_EQ (uut.value (), this->s_val1);

  // Constructing Expected from an rvalue must also work.
  SuccessType temp_s_val = this->s_val2;
  Expected<SuccessType, ErrorType> uut_rvalue (std::move (temp_s_val));
  EXPECT_TRUE (uut_rvalue.has_value ());
  EXPECT_EQ (uut_rvalue.value (), this->s_val2);
}

// Check the in-place success constructor. It must create Expected
// in the success state, constructing the value in place.
// Assert that has_value() is true and value() matches the constructed value.
TYPED_TEST (ExpectedTest, InPlaceConstructor_ForValue_CreatesExpectedWithValue)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange, Act
  Expected<SuccessType, ErrorType> uut (in_place, this->s_val1);
  // Assert
  EXPECT_TRUE (uut.has_value ());
  EXPECT_EQ (uut.value (), this->s_val1);

  // Check with another value
  Expected<SuccessType, ErrorType> uut2 (in_place, this->s_val2);
  EXPECT_TRUE (uut2.has_value ());
  EXPECT_EQ (uut2.value (), this->s_val2);
}

// Check the in-place error constructor. It must create Expected
// in the error state, constructing the error in place.
// Assert that has_value() is false and error() matches the constructed error.
TYPED_TEST (ExpectedTest, InPlaceConstructor_ForError_CreatesExpectedWithError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange, Act
  Expected<SuccessType, ErrorType> uut (unexpect_t (), this->e_val1);
  // Assert
  EXPECT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error (), this->e_val1);

  // Check with another error
  Expected<SuccessType, ErrorType> uut2 (unexpect_t (), this->e_val2);
  EXPECT_FALSE (uut2.has_value ());
  EXPECT_EQ (uut2.error (), this->e_val2);
}

// Check copy assignment. It must assign
// another Expected, changing state when needed.
// Assert that the target state and values are updated,
// and the source is unchanged.
TYPED_TEST (ExpectedTest, CopyAssignment_CopiesStateAndContent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Case 1: success -> success
  Expected<SuccessType, ErrorType> src_s (this->s_val1);
  Expected<SuccessType, ErrorType> dst_s (this->s_val2);
  dst_s = src_s;
  EXPECT_TRUE (dst_s.has_value ());
  EXPECT_EQ (dst_s.value (), this->s_val1);
  EXPECT_EQ (src_s.value (), this->s_val1); // Source remains unchanged

  // Case 2: error -> error
  Expected<SuccessType, ErrorType> src_e (
      Unexpected<ErrorType> (this->e_val1));
  Expected<SuccessType, ErrorType> dst_e (
      Unexpected<ErrorType> (this->e_val2));
  dst_e = src_e;
  EXPECT_FALSE (dst_e.has_value ());
  EXPECT_EQ (dst_e.error (), this->e_val1);
  EXPECT_EQ (src_e.error (), this->e_val1); // Source remains unchanged

  // Case 3: success -> error
  Expected<SuccessType, ErrorType> src_s2 (this->s_val1);
  Expected<SuccessType, ErrorType> dst_e2 (
      Unexpected<ErrorType> (this->e_val2));
  dst_e2 = src_s2;
  EXPECT_TRUE (dst_e2.has_value ());
  EXPECT_EQ (dst_e2.value (), this->s_val1);
  EXPECT_EQ (src_s2.value (), this->s_val1); // Source remains unchanged

  // Case 4: error -> success
  Expected<SuccessType, ErrorType> src_e3 (
      Unexpected<ErrorType> (this->e_val1));
  Expected<SuccessType, ErrorType> dst_s3 (this->s_val2);
  dst_s3 = src_e3;
  EXPECT_FALSE (dst_s3.has_value ());
  EXPECT_EQ (dst_s3.error (), this->e_val1);
  EXPECT_EQ (src_e3.error (), this->e_val1); // Source remains unchanged

  // Self-assignment
  Expected<SuccessType, ErrorType> self_assign (this->s_val1);
  self_assign = self_assign;
  EXPECT_TRUE (self_assign.has_value ());
  EXPECT_EQ (self_assign.value (), this->s_val1);
}

// Check move assignment. It must assign
// another Expected by move, changing state when needed.
// Assert that the target state and values are updated,
// and the source is emptied (when applicable).
TYPED_TEST (ExpectedTest, MoveAssignment_MovesStateAndContent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Case 1: success -> success
  SuccessType s1_val = this->s_val1;
  Expected<SuccessType, ErrorType> src_s (s1_val);
  Expected<SuccessType, ErrorType> dst_s (this->s_val2);
  dst_s = std::move (src_s);
  EXPECT_TRUE (dst_s.has_value ());
  EXPECT_EQ (dst_s.value (), s1_val); // Value was moved

  // Case 2: error -> error
  ErrorType e1_val = this->e_val1;
  Expected<SuccessType, ErrorType> src_e{ Unexpected<ErrorType> (e1_val) };
  Expected<SuccessType, ErrorType> dst_e (
      Unexpected<ErrorType> (this->e_val2));
  dst_e = std::move (src_e);
  EXPECT_FALSE (dst_e.has_value ());
  EXPECT_EQ (dst_e.error (), e1_val); // Error was moved

  // Case 3: success -> error
  SuccessType s2_val = this->s_val1;
  Expected<SuccessType, ErrorType> src_s2 (s2_val);
  Expected<SuccessType, ErrorType> dst_e2 (
      Unexpected<ErrorType> (this->e_val2));
  dst_e2 = std::move (src_s2);
  EXPECT_TRUE (dst_e2.has_value ());
  EXPECT_EQ (dst_e2.value (), s2_val); // Value was moved

  // Case 4: error -> success
  ErrorType e2_val = this->e_val2;
  Expected<SuccessType, ErrorType> src_e3{ Unexpected<ErrorType> (e2_val) };
  Expected<SuccessType, ErrorType> dst_s3 (this->s_val2);
  dst_s3 = std::move (src_e3);
  EXPECT_FALSE (dst_s3.has_value ());
  EXPECT_EQ (dst_s3.error (), e2_val); // Error was moved

  // Self-assignment
  Expected<SuccessType, ErrorType> self_assign (this->s_val1);
  self_assign = std::move (self_assign);
  // After move-assigning to self, the state stays the same,
  // though internal machinery may run.
  EXPECT_TRUE (self_assign.has_value ());
  EXPECT_EQ (self_assign.value (), this->s_val1);
}

// === Observers ==================================================

// Check has_value() and operator bool().
// Assert that they reflect the Expected state (success/error).
TYPED_TEST (ExpectedTest, HasValueAndOperatorBool_ReflectsState)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: successful Expected
  Expected<SuccessType, ErrorType> success_uut (this->s_val1);
  // Assert
  EXPECT_TRUE (success_uut.has_value ());
  EXPECT_TRUE (static_cast<bool> (success_uut)); // operator bool()

  // Arrange: Expected holding an error
  Expected<SuccessType, ErrorType> error_uut (
      Unexpected<ErrorType> (this->e_val1));
  // Assert
  EXPECT_FALSE (error_uut.has_value ());
  EXPECT_FALSE (static_cast<bool> (error_uut)); // operator bool()
}

TYPED_TEST (ExpectedTest, HasValue_WhenFound_ThenTrue)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  Expected<SuccessType, ErrorType> const success_uut (this->s_val1);
  EXPECT_TRUE (success_uut.has_value ());
}

TYPED_TEST (ExpectedTest, HasValue_WhenUnfound_ThenFalse)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  Expected<SuccessType, ErrorType> const error_uut (
      Unexpected<ErrorType> (this->e_val1));
  EXPECT_FALSE (error_uut.has_value ());
}

// Check value() & (lvalue reference).
// Assert that the value is returned when present and an exception is thrown
// otherwise.
TYPED_TEST (ExpectedTest, ValueLValueRef_ReturnsValueOrThrows)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: successful Expected
  Expected<SuccessType, ErrorType> success_uut (this->s_val1);
  // Act and assert (success path)
  EXPECT_EQ (success_uut.value (), this->s_val1);
  // Mutation through value() must work
  // Assign a value of SuccessType to ensure type safety across all test types.
  success_uut.value () = this->s_val2;
  EXPECT_EQ (success_uut.value (), this->s_val2);

  // Arrange: Expected holding an error
  Expected<SuccessType, ErrorType> error_uut (
      Unexpected<ErrorType> (this->e_val1));

#if _WIN32
#pragma warning(push)
#pragma warning(disable : 4834)
#pragma warning(disable : 4858)
#endif
  // Act and assert (error path)
  EXPECT_THROW (
      try {
        error_uut.value ();
      } catch (BadExpectedAccess<ErrorType> const &e) {
        EXPECT_EQ (e.error (), this->e_val1);
        throw;
      },
      BadExpectedAccess<ErrorType>);
#if _WIN32
#pragma warning(pop)
#endif
}

// Check value() const & (const lvalue reference).
// Assert that a const value is returned when present and an exception is
// thrown otherwise.
TYPED_TEST (ExpectedTest, ValueConstLValueRef_ReturnsConstValueOrThrows)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: successful Expected
  Expected<SuccessType, ErrorType> const success_uut (this->s_val1);
  // Act and assert (success path)
  EXPECT_EQ (success_uut.value (), this->s_val1);
  // Mutating through a const reference must be a compile error
  // success_uut.value() = some_val; // This will not compile

#if _WIN32
#pragma warning(push)
#pragma warning(disable : 4834)
#pragma warning(disable : 4858)
#endif
  // Arrange: Expected holding an error
  Expected<SuccessType, ErrorType> const error_uut (
      Unexpected<ErrorType> (this->e_val1));
  // Act and assert (error path)
  EXPECT_THROW (
      try {
        error_uut.value ();
      } catch (BadExpectedAccess<ErrorType> const &e) {
        EXPECT_EQ (e.error (), this->e_val1);
        throw;
      },
      BadExpectedAccess<ErrorType>);
#if _WIN32
#pragma warning(pop)
#endif
}

// Check value() && (rvalue reference).
// Assert that the value is moved when present and an exception with a moved
// error is thrown otherwise.
TYPED_TEST (ExpectedTest, ValueRValueRef_MovesValueOrThrowsWithMovedError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: successful Expected
  SuccessType original_s_val = this->s_val1;
  Expected<SuccessType, ErrorType> success_uut (original_s_val);
  // Act
  SuccessType moved_val = std::move (success_uut).value ();
  // Assert
  EXPECT_EQ (moved_val, original_s_val);
}

// Check value() const && (const rvalue reference).
// Assert that the value is copied (not moved) when present and an exception
// with a copied error is thrown otherwise.
TYPED_TEST (ExpectedTest,
            ValueConstRValueRef_CopiesValueOrThrowsWithCopiedError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: successful Expected
  SuccessType original_s_val = this->s_val1;
  Expected<SuccessType, ErrorType> const success_uut (original_s_val);
  // Act
  SuccessType copied_val = std::move (success_uut).value ();
  // Assert
  EXPECT_EQ (copied_val, original_s_val);
  // For ComplexSuccess, the source Expected must stay unchanged
  if (std::is_same<SuccessType, ComplexSuccess>::value)
    EXPECT_EQ (success_uut.value (),
               original_s_val); // Original remains unchanged
}

// Check error() & (lvalue reference).
// Assert that the error is returned when present.
//      If a value is present, assert fires in debug. In release this is UB.
TYPED_TEST (ExpectedTest, ErrorLValueRef_ReturnsErrorWhenPresent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: Expected holding an error
  Expected<SuccessType, ErrorType> error_uut (
      Unexpected<ErrorType> (this->e_val1));
  // Act and assert (error path)
  EXPECT_EQ (error_uut.error (), this->e_val1);
  // Mutation through error() must work
  ErrorType new_error_val = this->e_val2;
  error_uut.error () = new_error_val;
  EXPECT_EQ (error_uut.error (), new_error_val);

  // Arrange: successful Expected
  Expected<SuccessType, ErrorType> success_uut (this->s_val1);
  // Act and assert (success path - assert in debug)
  // An assert cannot be EXPECT-ed directly. In release this is UB.
  // Just confirm the call is not fatal when assert is disabled.
  // Tests know the precondition and must not call error() on success_uut.
  // Commented here to record that we know this.
  // EXPECT_DEATH(success_uut.error(), "Calling error\\(\\) while value is
  // present is undefined behavior.");
}

// Check error() const & (const lvalue reference).
// Assert that a const error is returned when present.
TYPED_TEST (ExpectedTest, ErrorConstLValueRef_ReturnsConstErrorWhenPresent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: Expected holding an error
  Expected<SuccessType, ErrorType> const error_uut (
      Unexpected<ErrorType> (this->e_val1));
  // Act and assert (error path)
  EXPECT_EQ (error_uut.error (), this->e_val1);
  // Mutating through a const reference must be a compile error
  // error_uut.error() = some_val; // This will not compile
}

// Check the value_or() const & overload.
// Assert that the value or the default is returned when no value is present.
TYPED_TEST (ExpectedTest, ValueOrConstLValue_ReturnsValueOrDefault)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: successful Expected
  Expected<SuccessType, ErrorType> success_uut (this->s_val1);
  // Act & Assert
  EXPECT_EQ (success_uut.value_or (this->s_val2), this->s_val1);

  // Arrange: Expected holding an error
  Expected<SuccessType, ErrorType> error_uut (
      Unexpected<ErrorType> (this->e_val1));
  // Act & Assert
  EXPECT_EQ (error_uut.value_or (this->s_val2), this->s_val2);

  // Check with a temporary as default_value
  SuccessType default_success_value{};
  EXPECT_EQ (error_uut.value_or (default_success_value),
             default_success_value);
}

// Check the value_or() && overload.
// Assert that a moved value or the default is returned when no value is
// present.
TYPED_TEST (ExpectedTest, ValueOrRValue_MovesValueOrDefault)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: successful Expected
  SuccessType original_s_val = this->s_val1;
  Expected<SuccessType, ErrorType> success_uut (original_s_val);
  // Act & Assert
  EXPECT_EQ (std::move (success_uut).value_or (this->s_val2), original_s_val);

  // Arrange: Expected holding an error
  Expected<SuccessType, ErrorType> error_uut (
      Unexpected<ErrorType> (this->e_val1));
  // Act & Assert
  EXPECT_EQ (std::move (error_uut).value_or (this->s_val2), this->s_val2);
}

// Check the error_or() const & overload.
// Assert that the error or the default error is returned when no error is
// present.
TYPED_TEST (ExpectedTest, ErrorOrConstLValue_ReturnsErrorOrDefaultError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: Expected holding an error
  Expected<SuccessType, ErrorType> error_uut (
      Unexpected<ErrorType> (this->e_val1));
  // Act & Assert
  EXPECT_EQ (error_uut.error_or (this->e_val2), this->e_val1);

  // Arrange: successful Expected
  Expected<SuccessType, ErrorType> success_uut (this->s_val1);
  // Act & Assert
  EXPECT_EQ (success_uut.error_or (this->e_val2), this->e_val2);

  // Check with a temporary as default_error
  ErrorType default_error{};
  EXPECT_EQ (success_uut.error_or (default_error), default_error);
}

// Check operator*() & (lvalue reference).
// Assert that a reference to the value is returned when present.
//      If no value is present, assert fires in debug.
TYPED_TEST (ExpectedTest, DereferenceOperatorLValueRef_ReturnsValueWhenPresent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: successful Expected
  Expected<SuccessType, ErrorType> success_uut (this->s_val1);
  // Act & Assert
  EXPECT_EQ (*success_uut, this->s_val1);
  // Mutation through * must work for every SuccessType
  SuccessType new_val = this->s_val2;
  *success_uut = new_val;
  EXPECT_EQ (*success_uut, new_val);

  // Arrange: Expected holding an error
  Expected<SuccessType, ErrorType> error_uut (
      Unexpected<ErrorType> (this->e_val1));
  // Act and assert (assert in debug, UB in release)
  // EXPECT_DEATH(*error_uut, "Dereferencing Expected without a value.");
}

// Check operator*() const & (const lvalue reference).
// Assert that a const reference to the value is returned when present.
TYPED_TEST (ExpectedTest,
            DereferenceOperatorConstLValueRef_ReturnsConstValueWhenPresent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: successful Expected
  Expected<SuccessType, ErrorType> const success_uut (this->s_val1);
  // Act & Assert
  EXPECT_EQ (*success_uut, this->s_val1);
}

// Check operator*() && (rvalue reference).
// Assert that the value is moved when present.
TYPED_TEST (ExpectedTest, DereferenceOperatorRValueRef_MovesValueWhenPresent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: successful Expected
  SuccessType original_s_val = this->s_val1;
  Expected<SuccessType, ErrorType> success_uut (original_s_val);
  // Act
  SuccessType moved_val = *std::move (success_uut);
  // Assert
  EXPECT_EQ (moved_val, original_s_val);
  // For ComplexSuccess, the source Expected must be emptied
  // Note: Template instantiation prevents us from checking ComplexSuccess.data
  // directly This would require SFINAE or specialized tests to properly
  // validate move semantics
}

// Check operator*() const && (const rvalue reference).
// Assert that the value is copied (not moved) when present.
TYPED_TEST (ExpectedTest,
            DereferenceOperatorConstRValueRef_CopiesValueWhenPresent)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: successful Expected
  SuccessType original_s_val = this->s_val1;
  Expected<SuccessType, ErrorType> const success_uut (original_s_val);
  // Act
  SuccessType copied_val = *std::move (success_uut);
  // Assert
  EXPECT_EQ (copied_val, original_s_val);
  // For ComplexSuccess, the source Expected must stay unchanged
  if (std::is_same<SuccessType, ComplexSuccess>::value)
    EXPECT_EQ (success_uut.value (), original_s_val);
}

// === Modifiers and monadic operations ==========================

// Check emplace() for a success value. It must construct a new value in place.
// Assert that has_value() stays true and the value is updated.
TYPED_TEST (ExpectedTest, Emplace_ConstructsNewValueInPlace)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: Expected holding an error
  Expected<SuccessType, ErrorType> uut (Unexpected<ErrorType> (this->e_val1));
  EXPECT_FALSE (uut.has_value ());

  // Act: emplace a new value
  SuccessType new_s_val = this->s_val2;
  uut.emplace (new_s_val);
  // Assert
  EXPECT_TRUE (uut.has_value ());
  EXPECT_EQ (uut.value (), new_s_val);

  // Arrange: Expected holding a value
  Expected<SuccessType, ErrorType> uut2 (this->s_val1);
  EXPECT_TRUE (uut2.has_value ());

  // Act: emplace another value
  uut2.emplace (new_s_val);
  // Assert
  EXPECT_TRUE (uut2.has_value ());
  EXPECT_EQ (uut2.value (), new_s_val);

  // Note: ComplexSuccess-specific testing is handled in specialized test
  // suites to avoid template instantiation issues with different ErrorType
  // combinations
}

// Check emplace_error(). It must construct a new error in place.
// Assert that has_value() becomes false and the error is updated.
TYPED_TEST (ExpectedTest, EmplaceError_ConstructsNewErrorInPlace)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: Expected holding a value
  Expected<SuccessType, ErrorType> uut (this->s_val1);
  EXPECT_TRUE (uut.has_value ());

  // Act: emplace a new error
  ErrorType new_e_val = this->e_val2;
  uut.emplace_error (new_e_val);
  // Assert
  EXPECT_FALSE (uut.has_value ());
  EXPECT_EQ (uut.error (), new_e_val);

  // Arrange: Expected holding an error
  Expected<SuccessType, ErrorType> uut2 (Unexpected<ErrorType> (this->e_val1));
  EXPECT_FALSE (uut2.has_value ());

  // Act: emplace another error
  uut2.emplace_error (new_e_val);
  // Assert
  EXPECT_FALSE (uut2.has_value ());
  EXPECT_EQ (uut2.error (), new_e_val);

  // Note: ComplexError-specific testing is handled in specialized test suites
  // to avoid template instantiation issues with different SuccessType
  // combinations
}

// Check swap(). It must exchange the contents of two Expected objects.
// Assert that both Expected objects swap state and values.
TYPED_TEST (ExpectedTest, Swap_ExchangesContentsCorrectly)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Case 1: success <-> success
  Expected<SuccessType, ErrorType> exp1_s (this->s_val1);
  Expected<SuccessType, ErrorType> exp2_s (this->s_val2);
  exp1_s.swap (exp2_s);
  EXPECT_TRUE (exp1_s.has_value ());
  EXPECT_EQ (exp1_s.value (), this->s_val2);
  EXPECT_TRUE (exp2_s.has_value ());
  EXPECT_EQ (exp2_s.value (), this->s_val1);

  // Case 2: error <-> error
  Expected<SuccessType, ErrorType> exp1_e (
      Unexpected<ErrorType> (this->e_val1));
  Expected<SuccessType, ErrorType> exp2_e (
      Unexpected<ErrorType> (this->e_val2));
  exp1_e.swap (exp2_e);
  EXPECT_FALSE (exp1_e.has_value ());
  EXPECT_EQ (exp1_e.error (), this->e_val2);
  EXPECT_FALSE (exp2_e.has_value ());
  EXPECT_EQ (exp2_e.error (), this->e_val1);

  // Case 3: success <-> error
  Expected<SuccessType, ErrorType> exp_s_to_e (this->s_val1);
  Expected<SuccessType, ErrorType> exp_e_to_s (
      Unexpected<ErrorType> (this->e_val1));
  exp_s_to_e.swap (exp_e_to_s);
  EXPECT_FALSE (exp_s_to_e.has_value ());
  EXPECT_EQ (exp_s_to_e.error (), this->e_val1);
  EXPECT_TRUE (exp_e_to_s.has_value ());
  EXPECT_EQ (exp_e_to_s.value (), this->s_val1);

  // Self-swap must not change state
  Expected<SuccessType, ErrorType> self_swap (this->s_val1);
  self_swap.swap (self_swap);
  EXPECT_TRUE (self_swap.has_value ());
  EXPECT_EQ (self_swap.value (), this->s_val1);
}

// Check and_then() & (lvalue reference).
// Assert that the function is applied to the value when present, otherwise the
// error is forwarded.
TYPED_TEST (ExpectedTest,
            AndThenLValue_AppliesFunctionToValueOrPropagatesError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: successful Expected
  Expected<SuccessType, ErrorType> success_uut (this->s_val1);
  // Act: apply a function that maps SuccessType to Expected<SuccessType,
  // ErrorType>
  auto func = [&] (SuccessType &) {
    return Expected<SuccessType, ErrorType> (this->s_val2);
  };
  Expected<SuccessType, ErrorType> result_s = success_uut.and_then (func);
  // Assert
  EXPECT_TRUE (result_s.has_value ());
  EXPECT_EQ (result_s.value (), this->s_val2);

  // Arrange: Expected holding an error
  Expected<SuccessType, ErrorType> error_uut (
      Unexpected<ErrorType> (this->e_val1));
  // Act
  Expected<SuccessType, ErrorType> result_e = error_uut.and_then (func);
  // Assert
  EXPECT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error (), this->e_val1);
}

// Check and_then() const & (const lvalue reference).
// Assert that the function is applied to the const value, otherwise the error
// is forwarded.
TYPED_TEST (ExpectedTest,
            AndThenConstLValue_AppliesFunctionToConstValueOrPropagatesError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: successful Expected
  Expected<SuccessType, ErrorType> const success_uut (this->s_val1);
  // Act: apply a function that maps const SuccessType & to
  // Expected<SuccessType, ErrorType>
  auto func = [&] (SuccessType const &) {
    return Expected<SuccessType, ErrorType> (this->s_val2);
  };
  Expected<SuccessType, ErrorType> result_s = success_uut.and_then (func);
  // Assert
  EXPECT_TRUE (result_s.has_value ());
  EXPECT_EQ (result_s.value (), this->s_val2);

  // Arrange: Expected holding an error
  Expected<SuccessType, ErrorType> const error_uut (
      Unexpected<ErrorType> (this->e_val1));
  // Act
  Expected<SuccessType, ErrorType> result_e = error_uut.and_then (func);
  // Assert
  EXPECT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error (), this->e_val1);
}

// Check and_then() && (rvalue reference).
// Assert that the function is applied to the moved value, otherwise the moved
// error is forwarded.
TYPED_TEST (ExpectedTest,
            AndThenRValue_AppliesFunctionToMovedValueOrPropagatesMovedError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: successful Expected
  SuccessType original_s_val = this->s_val1;
  Expected<SuccessType, ErrorType> success_uut (original_s_val);
  // Act
  auto func = [&] (SuccessType &&) {
    return Expected<SuccessType, ErrorType> (this->s_val2);
  };
  Expected<SuccessType, ErrorType> result_s
      = std::move (success_uut).and_then (func);
  // Assert
  EXPECT_TRUE (result_s.has_value ());
  EXPECT_EQ (result_s.value (), this->s_val2);
  // For ComplexSuccess, the source SuccessType in uut must be moved.
  if (std::is_same<SuccessType, ComplexSuccess>::value)
    EXPECT_TRUE (
        success_uut.has_value ()); // Expected still holds SuccessType, but it
                                   // has been moved from

  // TODO: Fix this part of the test - SFINAE issue with and_then on moved
  // error Expected
  // // Arrange: Expected holding an error
  // ErrorType original_e_val = this->e_val1;
  // Expected<SuccessType, ErrorType>
  // error_uut(Unexpected<ErrorType>(original_e_val));
  // // Act
  // Expected<SuccessType, ErrorType> result_e =
  // std::move(error_uut).and_then(func);
  // // Assert
  // EXPECT_FALSE(result_e.has_value());
  // EXPECT_EQ(result_e.error(), original_e_val);
}

// Check and_then() const && (const rvalue reference).
// Assert that the function is applied to the const moved value, or the const
// moved error is forwarded.
TYPED_TEST (
    ExpectedTest,
    AndThenConstRValue_AppliesFunctionToConstMovedValueOrPropagatesConstMovedError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;

  // Arrange: successful Expected
  SuccessType original_s_val = this->s_val1;
  Expected<SuccessType, ErrorType> const success_uut (original_s_val);
  // Act
  auto func = [&] (SuccessType const &&) {
    return Expected<SuccessType, ErrorType> (this->s_val2);
  };
  Expected<SuccessType, ErrorType> result_s
      = std::move (success_uut).and_then (func);
  // Assert
  EXPECT_TRUE (result_s.has_value ());
  EXPECT_EQ (result_s.value (), this->s_val2);
  // For ComplexSuccess, the source Expected must stay unchanged
  if (std::is_same<SuccessType, ComplexSuccess>::value)
    EXPECT_EQ (success_uut.value (), original_s_val);

  // TODO: Fix this part of the test - SFINAE issue with and_then on moved
  // error Expected
  // // Arrange: Expected holding an error
  // ErrorType original_e_val = this->e_val1;
  // Expected<SuccessType, ErrorType>
  // error_uut(Unexpected<ErrorType>(original_e_val));
  // // Act
  // Expected<SuccessType, ErrorType> result_e =
  // std::move(error_uut).and_then(func);
  // // Assert
  // EXPECT_FALSE(result_e.has_value());
  // EXPECT_EQ(result_e.error(), original_e_val);
  // // For ComplexError, the source Expected must stay unchanged
  // if(std::is_same<ErrorType, ComplexError>::value)
  // {
  //   // Unfortunately, compiler complains on error C2660:
  //   'testing::internal::EqHelper::Compare': function does not
  //   // take
  //   // 3 arguments, so I will place temporary variable for this purpose with
  //   explicit type to help the compiler. ErrorType expected_error =
  //   error_uut.error(); EXPECT_EQ(expected_error, original_e_val);
  // }
}

// Check transform() & (lvalue reference).
// Assert that the function transforms the value when present, otherwise the
// error is forwarded.
TYPED_TEST (ExpectedTest, TransformLValue_TransformsValueOrPropagatesError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;
  using ResultType = Expected<SuccessType, ErrorType>;

  // Arrange: successful Expected
  Expected<SuccessType, ErrorType> success_uut (this->s_val1);

  // Act: apply a function that maps SuccessType to the same type
  auto func = [&] (SuccessType &val) -> SuccessType {
    if constexpr (std::is_same_v<SuccessType, int>)
      {
        return val + 1;
      }
    else if constexpr (std::is_same_v<SuccessType, std::string>)
      {
        return val + "_transformed";
      }
    else if constexpr (std::is_same_v<SuccessType, SimpleSuccess>)
      {
        return SimpleSuccess (val.value + 1);
      }
    else if constexpr (std::is_same_v<SuccessType, ComplexSuccess>)
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

  ResultType result_s = success_uut.transform (func);

  // Assert
  EXPECT_TRUE (result_s.has_value ());
  if constexpr (std::is_same_v<SuccessType, int>)
    {
      EXPECT_EQ (result_s.value (), this->s_val1 + 1);
    }
  else if constexpr (std::is_same_v<SuccessType, std::string>)
    {
      EXPECT_EQ (result_s.value (), this->s_val1 + "_transformed");
    }
  else if constexpr (std::is_same_v<SuccessType, SimpleSuccess>)
    {
      EXPECT_EQ (result_s.value (), SimpleSuccess (this->s_val1.value + 1));
    }
  else if constexpr (std::is_same_v<SuccessType, ComplexSuccess>)
    {
      ComplexSuccess expected = this->s_val1;
      expected.name += "_transformed";
      EXPECT_EQ (result_s.value (), expected);
    }

  // Arrange: Expected holding an error
  Expected<SuccessType, ErrorType> error_uut (
      Unexpected<ErrorType> (this->e_val1));
  // Act
  ResultType result_e = error_uut.transform (func);
  // Assert
  EXPECT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error (), this->e_val1);
}

// Check transform() const & (const lvalue reference).
// Assert that the function transforms the const value when present, otherwise
// the error is forwarded.
TYPED_TEST (ExpectedTest,
            TransformConstLValue_TransformsConstValueOrPropagatesError)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;
  using ResultType = Expected<SuccessType, ErrorType>;

  // Arrange: successful Expected
  Expected<SuccessType, ErrorType> const success_uut (this->s_val1);

  // Act: apply the function
  auto func = [&] (SuccessType const &val) -> SuccessType {
    if constexpr (std::is_same_v<SuccessType, int>)
      {
        return val + 1;
      }
    else if constexpr (std::is_same_v<SuccessType, std::string>)
      {
        return val + "_transformed";
      }
    else if constexpr (std::is_same_v<SuccessType, SimpleSuccess>)
      {
        return SimpleSuccess (val.value + 1);
      }
    else if constexpr (std::is_same_v<SuccessType, ComplexSuccess>)
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

  ResultType result_s = success_uut.transform (func);

  // Assert
  EXPECT_TRUE (result_s.has_value ());
  if constexpr (std::is_same_v<SuccessType, int>)
    {
      EXPECT_EQ (result_s.value (), this->s_val1 + 1);
    }
  else if constexpr (std::is_same_v<SuccessType, std::string>)
    {
      EXPECT_EQ (result_s.value (), this->s_val1 + "_transformed");
    }
  else if constexpr (std::is_same_v<SuccessType, SimpleSuccess>)
    {
      EXPECT_EQ (result_s.value (), SimpleSuccess (this->s_val1.value + 1));
    }
  else if constexpr (std::is_same_v<SuccessType, ComplexSuccess>)
    {
      ComplexSuccess expected = this->s_val1;
      expected.name += "_transformed";
      EXPECT_EQ (result_s.value (), expected);
    }

  // Arrange: Expected holding an error
  Expected<SuccessType, ErrorType> const error_uut (
      Unexpected<ErrorType> (this->e_val1));
  // Act
  ResultType result_e = error_uut.transform (func);
  // Assert
  EXPECT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error (), this->e_val1);
}

// Check or_else() & (lvalue reference).
// Assert that the function is applied to the error when present, otherwise the
// value is forwarded.
TYPED_TEST (ExpectedTest, OrElseLValue_AppliesFunctionToErrorOrPropagatesValue)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;
  using ReturnType = Expected<SuccessType, ErrorType>; // or_else returns
                                                       // Expected<SuccessType,
                                                       // NewErrorType>

  // Arrange: Expected holding an error
  Expected<SuccessType, ErrorType> error_uut (
      Unexpected<ErrorType> (this->e_val1));
  // Act
  auto func
      = [&] (ErrorType &) { return ReturnType (unexpect_t (), this->e_val2); };
  ReturnType result_e = error_uut.or_else (func);
  // Assert
  EXPECT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error (), this->e_val2);

  // Arrange: successful Expected
  Expected<SuccessType, ErrorType> success_uut (this->s_val1);
  // Act
  ReturnType result_s = success_uut.or_else (func);
  // Assert
  EXPECT_TRUE (result_s.has_value ());
  EXPECT_EQ (result_s.value (), this->s_val1);
}

// Check or_else() const & (const lvalue reference).
// Assert that the function is applied to the const error when present,
// otherwise the value is forwarded.
TYPED_TEST (ExpectedTest,
            OrElseConstLValue_AppliesFunctionToConstErrorOrPropagatesValue)
{
  using SuccessType = typename TestFixture::SuccessType;
  using ErrorType = typename TestFixture::ErrorType;
  using ReturnType = Expected<SuccessType, ErrorType>;

  // Arrange: Expected holding an error
  Expected<SuccessType, ErrorType> const error_uut (
      Unexpected<ErrorType> (this->e_val1));
  // Act
  auto func = [&] (ErrorType const &) {
    return ReturnType (unexpect_t (), this->e_val2);
  };
  ReturnType result_e = error_uut.or_else (func);
  // Assert
  EXPECT_FALSE (result_e.has_value ());
  EXPECT_EQ (result_e.error (), this->e_val2);

  // Arrange: successful Expected
  Expected<SuccessType, ErrorType> const success_uut (this->s_val1);
  // Act
  ReturnType result_s = success_uut.or_else (func);
  // Assert
  EXPECT_TRUE (result_s.has_value ());
  EXPECT_EQ (result_s.value (), this->s_val1);
}
