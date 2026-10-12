// Tests of lumex/applied/json/validation: LumexJsonSchemaValidator and
// ILumexJsonSchemaValidator, over the schema of
// LumexJsonSchemaTestFixtures.hpp.

#include <exception>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "lumex/applied/json/LumexJson"
#include "lumex/core/string_view/view/LumexStringView.hpp"

#include "lumex/tests/applied/json/LumexJsonSchemaTestFixtures.hpp"

using lumex::applied::json::normalization::LumexJsonSchemaNormalizer;
using lumex::applied::json::schema::LumexJsonSchemaException;
using lumex::applied::json::schema::LumexJsonSchemaFailure;
using lumex::applied::json::validation::ILumexJsonSchemaValidator;
using lumex::applied::json::validation::LumexJsonSchemaValidator;
using lumex::core::string_view::view::lumex_string_view;
using lumex_json_schema_test::person_schema;
using lumex_json_schema_test::text;
using lumex_json_schema_test::valid_person;

// --- validator ---

TEST (LumexJsonSchemaValidatorTest, GivenValidText_WhenValidate_ThenNoThrow)
{
  LumexJsonSchemaValidator const validator (person_schema ());
  std::string const raw = text (valid_person ());
  EXPECT_NO_THROW (validator.validate (lumex_string_view (raw)));
  EXPECT_TRUE (validator.is_strict ());
  EXPECT_EQ (validator.schema (), person_schema ());
}

TEST (LumexJsonSchemaValidatorTest,
      GivenStrictAndViolation_WhenValidateWithError_ThenThrowsAndErrorNull)
{
  LumexJsonSchemaValidator const validator (person_schema (), true);
  nlohmann::json input = valid_person ();
  input["age"] = "old";
  std::string const raw = text (input);
  std::exception_ptr error = std::make_exception_ptr (std::logic_error ("x"));
  EXPECT_THROW (validator.validate (lumex_string_view (raw), error),
                LumexJsonSchemaException);
  EXPECT_FALSE (error);
}

TEST (LumexJsonSchemaValidatorTest,
      GivenNonStrictAndViolation_WhenValidateWithError_ThenErrorSet)
{
  LumexJsonSchemaValidator const validator (person_schema (), false);
  nlohmann::json input = valid_person ();
  input["age"] = "old";
  std::string const raw = text (input);
  std::exception_ptr error;
  EXPECT_NO_THROW (validator.validate (lumex_string_view (raw), error));
  ASSERT_TRUE (error);
  EXPECT_NE (LumexJsonSchemaNormalizer::get_exception_message (error).find (
                 "type_mismatch at '$.age'"),
             std::string::npos);
}

TEST (LumexJsonSchemaValidatorTest,
      GivenNonStrictAndValidText_WhenValidateWithError_ThenErrorCleared)
{
  LumexJsonSchemaValidator const validator (person_schema (), false);
  std::string const raw = text (valid_person ());
  std::exception_ptr error = std::make_exception_ptr (std::logic_error ("x"));
  validator.validate (lumex_string_view (raw), error);
  EXPECT_FALSE (error);
}

TEST (LumexJsonSchemaValidatorTest,
      GivenNonStrictAndViolation_WhenValidateWithoutError_ThenDoesNotThrow)
{
  LumexJsonSchemaValidator const validator (person_schema (), false);
  EXPECT_NO_THROW (validator.validate (lumex_string_view ("[]", 2)));
}

TEST (LumexJsonSchemaValidatorTest,
      GivenNonStrictAndUnparsable_WhenValidateWithoutError_ThenDoesNotThrow)
{
  LumexJsonSchemaValidator const validator (person_schema (), false);
  EXPECT_NO_THROW (validator.validate (lumex_string_view ("{", 1)));
  EXPECT_NO_THROW (validator.validate (lumex_string_view ()));
}

TEST (LumexJsonSchemaValidatorTest,
      GivenStrictAndViolation_WhenValidateWithoutError_ThenThrows)
{
  LumexJsonSchemaValidator const validator (person_schema (), true);
  EXPECT_THROW (validator.validate (lumex_string_view ("[]", 2)),
                LumexJsonSchemaException);
}

TEST (LumexJsonSchemaValidatorTest,
      GivenNonStrictThroughInterface_WhenValidateWithoutError_ThenNoThrow)
{
  LumexJsonSchemaValidator const validator (person_schema (), false);
  ILumexJsonSchemaValidator const &base = validator;
  EXPECT_NO_THROW (base.validate (lumex_string_view ("{}", 2)));
}

TEST (LumexJsonSchemaValidatorTest,
      GivenUnparsableText_WhenValidate_ThenParseError)
{
  LumexJsonSchemaValidator const validator (person_schema ());
  try
    {
      validator.validate (lumex_string_view ("not json", 8));
      FAIL () << "expected an exception";
    }
  catch (LumexJsonSchemaException const &exc)
    {
      EXPECT_EQ (exc.reason (), LumexJsonSchemaFailure::parse_error);
    }
}

TEST (LumexJsonSchemaValidatorTest,
      GivenValidatorThroughInterface_WhenValidate_ThenDispatches)
{
  LumexJsonSchemaValidator const validator (person_schema (), false);
  ILumexJsonSchemaValidator const &base = validator;
  std::exception_ptr error;
  base.validate (lumex_string_view ("{}", 2), error);
  EXPECT_TRUE (error);
  EXPECT_TRUE ((std::is_polymorphic<ILumexJsonSchemaValidator>::value));
  EXPECT_TRUE (
      (std::has_virtual_destructor<ILumexJsonSchemaValidator>::value));
}
