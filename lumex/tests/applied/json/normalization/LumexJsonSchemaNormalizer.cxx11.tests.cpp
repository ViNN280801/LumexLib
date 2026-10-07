// Tests of lumex/applied/json/normalization: LumexJsonSchemaNormalizer and
// ILumexJsonNormalizer, over the schema of LumexJsonSchemaTestFixtures.hpp.
// PersonNormalizer is the concrete normalizer of these tests.

#include <exception>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "lumex/applied/json/LumexJson"
#include "lumex/core/string_view/view/LumexStringView.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"

#include "lumex/tests/applied/json/LumexJsonSchemaTestFixtures.hpp"

using lumex::applied::json::normalization::ILumexJsonNormalizer;
using lumex::applied::json::normalization::LumexJsonSchemaNormalizer;
using lumex::applied::json::schema::LumexJsonSchemaCheckMode;
using lumex::applied::json::schema::LumexJsonSchemaException;
using lumex::applied::json::schema::LumexJsonSchemaTraverser;
using lumex::core::string_view::view::lumex_string_view;
using lumex_json_schema_test::person_schema;
using lumex_json_schema_test::text;
using lumex_json_schema_test::valid_person;

namespace
{
// A concrete normalizer: renames the legacy "fullName" to "name" before the
// schema walk, the typical job of a per-operation subclass.
class PersonNormalizer final : public LumexJsonSchemaNormalizer
{
public:
  explicit PersonNormalizer (bool strict) : LumexJsonSchemaNormalizer (strict)
  {
  }

  nlohmann::json
  normalize (lumex_string_view raw) const override
  {
    std::exception_ptr ignored;
    return normalize (raw, ignored);
  }

  nlohmann::json
  normalize (lumex_string_view raw, std::exception_ptr &error) const override
  {
    error = nullptr;
    try
      {
        return normalize_against_schema (person_schema (),
                                         _upgrade (parse (raw)));
      }
    catch (std::exception const &)
      {
        report_or_rethrow (error);
      }
    return nlohmann::json ();
  }

  void
  validate (lumex_string_view raw) const override
  {
    std::exception_ptr ignored;
    validate (raw, ignored);
  }

  void
  validate (lumex_string_view raw, std::exception_ptr &error) const override
  {
    error = nullptr;
    try
      {
        validate_against_schema (person_schema (), _upgrade (parse (raw)));
      }
    catch (std::exception const &)
      {
        report_or_rethrow (error);
      }
  }

  nlohmann::json
  process (nlohmann::json const &input, LumexJsonSchemaCheckMode mode) const
  {
    return process_against_schema (person_schema (), input, mode, "$.root");
  }

private:
  static nlohmann::json
  _upgrade (nlohmann::json document)
  {
    if (document.is_object () && document.contains ("fullName")
        && !document.contains ("name"))
      {
        document["name"] = document["fullName"];
        document.erase ("fullName");
      }
    return document;
  }
};
} // namespace

// --- normalizer ---

TEST (LumexJsonSchemaNormalizerTest,
      GivenLegacyField_WhenNormalize_ThenUpgradedAndShaped)
{
  PersonNormalizer const normalizer (true);
  nlohmann::json input = valid_person ();
  input.erase ("name");
  input["fullName"] = "Bob";
  std::string const raw = text (input);
  nlohmann::json const result = normalizer.normalize (lumex_string_view (raw));
  EXPECT_EQ (result["name"], "Bob");
  EXPECT_FALSE (result.contains ("fullName"));
  EXPECT_FALSE (result.contains ("ignored"));
}

TEST (LumexJsonSchemaNormalizerTest,
      GivenStrictAndViolation_WhenNormalizeWithError_ThenThrows)
{
  PersonNormalizer const normalizer (true);
  std::exception_ptr error;
  EXPECT_THROW (LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (
                    normalizer.normalize (lumex_string_view ("{}", 2), error)),
                LumexJsonSchemaException);
  EXPECT_FALSE (error);
  EXPECT_TRUE (normalizer.is_strict ());
}

TEST (LumexJsonSchemaNormalizerTest,
      GivenNonStrictAndViolation_WhenNormalizeWithError_ThenNullAndError)
{
  PersonNormalizer const normalizer (false);
  std::exception_ptr error;
  nlohmann::json const result
      = normalizer.normalize (lumex_string_view ("{}", 2), error);
  EXPECT_TRUE (result.is_null ());
  ASSERT_TRUE (error);
  EXPECT_NE (LumexJsonSchemaNormalizer::get_exception_message (error).find (
                 "missing_required"),
             std::string::npos);
}

TEST (LumexJsonSchemaNormalizerTest,
      GivenNonStrictAndUnparsable_WhenValidateWithError_ThenParseError)
{
  PersonNormalizer const normalizer (false);
  std::exception_ptr error;
  normalizer.validate (lumex_string_view ("{", 1), error);
  ASSERT_TRUE (error);
  EXPECT_NE (LumexJsonSchemaNormalizer::get_exception_message (error).find (
                 "parse_error"),
             std::string::npos);
}

TEST (LumexJsonSchemaNormalizerTest,
      GivenValidText_WhenValidate_ThenNoThrowAndErrorCleared)
{
  PersonNormalizer const normalizer (false);
  std::string const raw = text (valid_person ());
  std::exception_ptr error = std::make_exception_ptr (std::logic_error ("x"));
  normalizer.validate (lumex_string_view (raw), error);
  EXPECT_FALSE (error);
  EXPECT_NO_THROW (normalizer.validate (lumex_string_view (raw)));
}

TEST (LumexJsonSchemaNormalizerTest,
      GivenProcessAgainstSchema_WhenBothModes_ThenMatchesTraverser)
{
  PersonNormalizer const normalizer (true);
  EXPECT_TRUE (
      normalizer
          .process (valid_person (), LumexJsonSchemaCheckMode::validate_only)
          .is_null ());
  EXPECT_EQ (
      normalizer.process (valid_person (),
                          LumexJsonSchemaCheckMode::validate_and_normalize),
      LumexJsonSchemaTraverser::run (person_schema (), valid_person ()));
  nlohmann::json input = valid_person ();
  input["age"] = "x";
  try
    {
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (
          normalizer.process (input, LumexJsonSchemaCheckMode::validate_only));
      FAIL () << "expected an exception";
    }
  catch (LumexJsonSchemaException const &exc)
    {
      EXPECT_EQ (exc.path (), "$.root.age");
    }
}

TEST (LumexJsonSchemaNormalizerTest,
      GivenNonStrictAndViolation_WhenOneArgOverloads_ThenNoThrowAndNull)
{
  PersonNormalizer const normalizer (false);
  EXPECT_NO_THROW (normalizer.validate (lumex_string_view ("{}", 2)));
  EXPECT_NO_THROW (normalizer.validate (lumex_string_view ("{", 1)));
  nlohmann::json result;
  EXPECT_NO_THROW (result
                   = normalizer.normalize (lumex_string_view ("{}", 2)));
  EXPECT_TRUE (result.is_null ());
}

TEST (LumexJsonSchemaNormalizerTest,
      GivenStrictAndViolation_WhenOneArgOverloads_ThenThrow)
{
  PersonNormalizer const normalizer (true);
  EXPECT_THROW (normalizer.validate (lumex_string_view ("{}", 2)),
                LumexJsonSchemaException);
  EXPECT_THROW (LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (
                    normalizer.normalize (lumex_string_view ("{", 1))),
                LumexJsonSchemaException);
}

TEST (LumexJsonSchemaNormalizerTest,
      GivenExceptionPtrKinds_WhenGetExceptionMessage_ThenTextOrPlaceholder)
{
  EXPECT_EQ (LumexJsonSchemaNormalizer::get_exception_message (
                 std::make_exception_ptr (std::runtime_error ("boom"))),
             "boom");
  EXPECT_EQ (LumexJsonSchemaNormalizer::get_exception_message (
                 std::make_exception_ptr (42)),
             "<unknown exception>");
  EXPECT_EQ (
      LumexJsonSchemaNormalizer::get_exception_message (std::exception_ptr ()),
      "");
}

TEST (LumexJsonSchemaNormalizerTest,
      GivenNormalizerThroughInterface_WhenNormalize_ThenDispatches)
{
  PersonNormalizer const normalizer (true);
  ILumexJsonNormalizer const &base = normalizer;
  std::string const raw = text (valid_person ());
  EXPECT_EQ (base.normalize (lumex_string_view (raw))["name"], "Ann");
  EXPECT_TRUE ((std::is_abstract<LumexJsonSchemaNormalizer>::value));
  EXPECT_TRUE ((std::has_virtual_destructor<ILumexJsonNormalizer>::value));
}
