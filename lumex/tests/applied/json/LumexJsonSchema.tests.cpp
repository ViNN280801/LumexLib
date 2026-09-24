// LumexJsonSchema.tests.cpp
#include <exception>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "lumex/applied/json/LumexJson"
#include "lumex/core/string_view/view/LumexStringView.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"

using lumex::applied::json::normalization::ILumexJsonNormalizer;
using lumex::applied::json::normalization::LumexJsonSchemaNormalizer;
using lumex::applied::json::schema::LumexJsonSchemaCheckMode;
using lumex::applied::json::schema::LumexJsonSchemaException;
using lumex::applied::json::schema::LumexJsonSchemaFailure;
using lumex::applied::json::schema::LumexJsonSchemaTraverser;
using lumex::applied::json::validation::ILumexJsonSchemaValidator;
using lumex::applied::json::validation::LumexJsonSchemaValidator;
using lumex::core::string_view::view::LumexStringView;

namespace
{
nlohmann::json
person_schema ()
{
  return nlohmann::json::parse (R"({
    "type": "object",
    "required": ["name", "age", "address", "tags"],
    "properties": {
      "name":    { "type": "string" },
      "age":     { "type": "integer" },
      "nick":    { "type": ["string", "null"] },
      "role":    { "type": "string", "enum": ["admin", "user"] },
      "kind":    { "const": "person" },
      "address": {
        "type": "object",
        "required": ["city"],
        "properties": { "city": { "type": "string" }, "zip": { "type": "string" } }
      },
      "tags":    { "type": "array", "items": { "type": "string" } },
      "extra":   { "type": ["object", "null"], "properties": {} },
      "any":     { "type": "array" }
    }
  })");
}

nlohmann::json
valid_person ()
{
  return nlohmann::json::parse (R"({
    "name": "Ann", "age": 30, "role": "admin", "kind": "person",
    "address": { "city": "Oslo", "zip": "0150", "unknown": 1 },
    "tags": ["a", "b"], "ignored": true
  })");
}

LumexJsonSchemaFailure
failure_of (nlohmann::json const &schema, nlohmann::json const &input,
            std::string *path = nullptr)
{
  try
    {
      LumexJsonSchemaTraverser::run (schema, input,
                                     LumexJsonSchemaCheckMode::validate_only);
    }
  catch (LumexJsonSchemaException const &exc)
    {
      if (path != nullptr)
        *path = exc.path ();
      return exc.reason ();
    }
  ADD_FAILURE () << "expected LumexJsonSchemaException";
  return LumexJsonSchemaFailure::parse_error;
}

std::string
text (nlohmann::json const &document)
{
  return document.dump ();
}

// A concrete normalizer: renames the legacy "fullName" to "name" before the
// schema walk, the typical job of a per-operation subclass.
class PersonNormalizer final : public LumexJsonSchemaNormalizer
{
public:
  explicit PersonNormalizer (bool strict) : LumexJsonSchemaNormalizer (strict)
  {
  }

  nlohmann::json
  normalize (LumexStringView raw) const override
  {
    std::exception_ptr ignored;
    return normalize (raw, ignored);
  }

  nlohmann::json
  normalize (LumexStringView raw, std::exception_ptr &error) const override
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
  validate (LumexStringView raw) const override
  {
    std::exception_ptr ignored;
    validate (raw, ignored);
  }

  void
  validate (LumexStringView raw, std::exception_ptr &error) const override
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

// --- traverser: normalization ---

TEST (LumexJsonSchemaTraverserTest,
      GivenValidDocument_WhenNormalize_ThenUnknownKeysDroppedAndShapeKept)
{
  nlohmann::json const result
      = LumexJsonSchemaTraverser::run (person_schema (), valid_person ());
  EXPECT_EQ (result["name"], "Ann");
  EXPECT_EQ (result["age"], 30);
  EXPECT_EQ (result["role"], "admin");
  EXPECT_EQ (result["kind"], "person");
  EXPECT_EQ (result["address"]["city"], "Oslo");
  EXPECT_FALSE (result["address"].contains ("unknown"));
  EXPECT_FALSE (result.contains ("ignored"));
  EXPECT_EQ (result["tags"], nlohmann::json::array ({ "a", "b" }));
}

TEST (LumexJsonSchemaTraverserTest,
      GivenMissingOptionalFields_WhenNormalize_ThenOmitted)
{
  nlohmann::json const result
      = LumexJsonSchemaTraverser::run (person_schema (), valid_person ());
  EXPECT_FALSE (result.contains ("nick"));
  EXPECT_FALSE (result.contains ("extra"));
  EXPECT_FALSE (result.contains ("any"));
}

TEST (LumexJsonSchemaTraverserTest,
      GivenMissingRequiredLeaf_WhenNormalize_ThenBecomesNull)
{
  nlohmann::json input = valid_person ();
  input.erase ("age");
  nlohmann::json const result
      = LumexJsonSchemaTraverser::run (person_schema (), input);
  ASSERT_TRUE (result.contains ("age"));
  EXPECT_TRUE (result["age"].is_null ());
}

TEST (LumexJsonSchemaTraverserTest,
      GivenNullRequiredLeaf_WhenNormalize_ThenStaysNull)
{
  nlohmann::json input = valid_person ();
  input["name"] = nullptr;
  EXPECT_TRUE (LumexJsonSchemaTraverser::run (person_schema (), input)["name"]
                   .is_null ());
}

TEST (LumexJsonSchemaTraverserTest,
      GivenValidDocument_WhenValidateOnly_ThenReturnsNull)
{
  EXPECT_TRUE (
      LumexJsonSchemaTraverser::run (person_schema (), valid_person (),
                                     LumexJsonSchemaCheckMode::validate_only)
          .is_null ());
}

TEST (LumexJsonSchemaTraverserTest,
      GivenObjectWithoutProperties_WhenNormalize_ThenEmptyObject)
{
  nlohmann::json const schema = { { "type", "object" } };
  EXPECT_EQ (LumexJsonSchemaTraverser::run (schema, { { "a", 1 } }),
             nlohmann::json::object ());
  EXPECT_TRUE (
      LumexJsonSchemaTraverser::run (schema, { { "a", 1 } },
                                     LumexJsonSchemaCheckMode::validate_only)
          .is_null ());
}

TEST (LumexJsonSchemaTraverserTest,
      GivenArrayWithoutItems_WhenNormalize_ThenPassesThroughUnchanged)
{
  nlohmann::json const schema = { { "type", "array" } };
  nlohmann::json const input = nlohmann::json::array ({ 1, "x", nullptr });
  EXPECT_EQ (LumexJsonSchemaTraverser::run (schema, input), input);
}

TEST (LumexJsonSchemaTraverserTest,
      GivenArrayOfObjects_WhenNormalize_ThenEachElementNormalized)
{
  nlohmann::json const schema = nlohmann::json::parse (R"({
    "type": "array",
    "items": { "type": "object", "required": ["id"],
               "properties": { "id": { "type": "integer" } } } })");
  nlohmann::json const input
      = nlohmann::json::parse (R"([{ "id": 1, "x": 0 }, {}])");
  nlohmann::json const result = LumexJsonSchemaTraverser::run (schema, input);
  ASSERT_EQ (result.size (), 2u);
  EXPECT_EQ (result[0], nlohmann::json ({ { "id", 1 } }));
  EXPECT_TRUE (result[1]["id"].is_null ());
}

TEST (LumexJsonSchemaTraverserTest,
      GivenNullableCompositeMissingOrNull_WhenNormalize_ThenNull)
{
  nlohmann::json const schema = nlohmann::json::parse (R"({
    "type": "object", "required": ["child"],
    "properties": { "child": { "type": ["object", "null"], "properties": {} } } })");
  nlohmann::json const absent
      = LumexJsonSchemaTraverser::run (schema, nlohmann::json::object ());
  nlohmann::json const explicit_null
      = LumexJsonSchemaTraverser::run (schema, { { "child", nullptr } });
  ASSERT_TRUE (absent.contains ("child"));
  EXPECT_TRUE (absent["child"].is_null ());
  EXPECT_EQ (absent, explicit_null);
}

TEST (LumexJsonSchemaTraverserTest,
      GivenWholeFloatForInteger_WhenValidate_ThenAccepted)
{
  nlohmann::json const schema = { { "type", "integer" } };
  EXPECT_EQ (LumexJsonSchemaTraverser::run (schema, 4.0), 4.0);
}

TEST (LumexJsonSchemaTraverserTest,
      GivenTypeList_WhenValueMatchesAnyEntry_ThenAccepted)
{
  nlohmann::json const schema
      = { { "type", nlohmann::json::array ({ "string", "boolean" }) } };
  EXPECT_EQ (LumexJsonSchemaTraverser::run (schema, true), true);
  EXPECT_EQ (LumexJsonSchemaTraverser::run (schema, "s"), "s");
}

TEST (LumexJsonSchemaTraverserTest,
      GivenNumberType_WhenIntegerOrFloat_ThenAccepted)
{
  nlohmann::json const schema = { { "type", "number" } };
  EXPECT_EQ (LumexJsonSchemaTraverser::run (schema, 1), 1);
  EXPECT_EQ (LumexJsonSchemaTraverser::run (schema, 1.5), 1.5);
}

TEST (LumexJsonSchemaTraverserTest, GivenConstNode_WhenNullInput_ThenPasses)
{
  nlohmann::json const schema = { { "const", 5 } };
  EXPECT_TRUE (LumexJsonSchemaTraverser::run (schema, nullptr).is_null ());
  EXPECT_EQ (LumexJsonSchemaTraverser::run (schema, 5), 5);
}

// --- traverser: violations ---

TEST (LumexJsonSchemaTraverserTest,
      GivenWrongLeafType_WhenValidate_ThenTypeMismatchWithPath)
{
  nlohmann::json input = valid_person ();
  input["address"]["city"] = 7;
  std::string path;
  EXPECT_EQ (failure_of (person_schema (), input, &path),
             LumexJsonSchemaFailure::type_mismatch);
  EXPECT_EQ (path, "$.address.city");
}

TEST (LumexJsonSchemaTraverserTest,
      GivenFractionalFloatForInteger_WhenValidate_ThenTypeMismatch)
{
  nlohmann::json input = valid_person ();
  input["age"] = 30.5;
  EXPECT_EQ (failure_of (person_schema (), input),
             LumexJsonSchemaFailure::type_mismatch);
}

TEST (LumexJsonSchemaTraverserTest,
      GivenValueOutsideEnum_WhenValidate_ThenEnumMismatch)
{
  nlohmann::json input = valid_person ();
  input["role"] = "root";
  std::string path;
  EXPECT_EQ (failure_of (person_schema (), input, &path),
             LumexJsonSchemaFailure::enum_mismatch);
  EXPECT_EQ (path, "$.role");
}

TEST (LumexJsonSchemaTraverserTest,
      GivenDifferentConst_WhenValidate_ThenConstMismatch)
{
  nlohmann::json input = valid_person ();
  input["kind"] = "robot";
  EXPECT_EQ (failure_of (person_schema (), input),
             LumexJsonSchemaFailure::const_mismatch);
}

TEST (LumexJsonSchemaTraverserTest,
      GivenMissingRequiredObject_WhenValidate_ThenMissingRequired)
{
  nlohmann::json input = valid_person ();
  input.erase ("address");
  std::string path;
  EXPECT_EQ (failure_of (person_schema (), input, &path),
             LumexJsonSchemaFailure::missing_required);
  EXPECT_EQ (path, "$.address");
}

TEST (LumexJsonSchemaTraverserTest,
      GivenNullRequiredArray_WhenValidate_ThenMissingRequired)
{
  nlohmann::json input = valid_person ();
  input["tags"] = nullptr;
  EXPECT_EQ (failure_of (person_schema (), input),
             LumexJsonSchemaFailure::missing_required);
}

TEST (LumexJsonSchemaTraverserTest,
      GivenScalarWhereObjectExpected_WhenValidate_ThenMissingRequired)
{
  nlohmann::json input = valid_person ();
  input["address"] = "Oslo";
  EXPECT_EQ (failure_of (person_schema (), input),
             LumexJsonSchemaFailure::missing_required);
}

TEST (LumexJsonSchemaTraverserTest,
      GivenObjectWhereArrayExpected_WhenValidate_ThenMissingRequired)
{
  nlohmann::json input = valid_person ();
  input["tags"] = nlohmann::json::object ();
  EXPECT_EQ (failure_of (person_schema (), input),
             LumexJsonSchemaFailure::missing_required);
}

TEST (LumexJsonSchemaTraverserTest,
      GivenBadArrayElement_WhenValidate_ThenPathHasIndex)
{
  nlohmann::json input = valid_person ();
  input["tags"] = nlohmann::json::array ({ "a", "b", 3 });
  std::string path;
  EXPECT_EQ (failure_of (person_schema (), input, &path),
             LumexJsonSchemaFailure::type_mismatch);
  EXPECT_EQ (path, "$.tags[2]");
}

TEST (LumexJsonSchemaTraverserTest,
      GivenCustomRootPath_WhenViolation_ThenPathStartsWithIt)
{
  nlohmann::json const schema = { { "type", "string" } };
  try
    {
      LumexJsonSchemaTraverser::run (
          schema, 1, LumexJsonSchemaCheckMode::validate_and_normalize,
          "$.payload");
      FAIL () << "expected an exception";
    }
  catch (LumexJsonSchemaException const &exc)
    {
      EXPECT_EQ (exc.path (), "$.payload");
    }
}

TEST (LumexJsonSchemaTraverserTest,
      GivenSchemaWithoutTypeOrConst_WhenRun_ThenOutOfRange)
{
  EXPECT_THROW (LumexJsonSchemaTraverser::run (nlohmann::json::object (), 1),
                nlohmann::json::out_of_range);
}

TEST (LumexJsonSchemaTraverserTest,
      GivenViolation_WhenNormalizeMode_ThenSameFailureAsValidateOnly)
{
  nlohmann::json input = valid_person ();
  input["role"] = "root";
  LumexJsonSchemaFailure normalized = LumexJsonSchemaFailure::parse_error;
  try
    {
      LumexJsonSchemaTraverser::run (person_schema (), input);
    }
  catch (LumexJsonSchemaException const &exc)
    {
      normalized = exc.reason ();
    }
  EXPECT_EQ (normalized, failure_of (person_schema (), input));
}

// --- traverser: parse ---

TEST (LumexJsonSchemaTraverserTest, GivenValidText_WhenParse_ThenDocument)
{
  std::string const raw ("{\"a\":[1,2]}");
  EXPECT_EQ (LumexJsonSchemaTraverser::parse (LumexStringView (raw)),
             nlohmann::json::parse (raw));
}

TEST (LumexJsonSchemaTraverserTest,
      GivenViewOfPrefix_WhenParse_ThenOnlyViewedBytesAreParsed)
{
  std::string const raw ("[1] trailing garbage");
  EXPECT_EQ (
      LumexJsonSchemaTraverser::parse (LumexStringView (raw.data (), 3)),
      nlohmann::json::array ({ 1 }));
}

TEST (LumexJsonSchemaTraverserTest,
      GivenInvalidOrEmptyText_WhenParse_ThenParseErrorAtRoot)
{
  try
    {
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (
          LumexJsonSchemaTraverser::parse (LumexStringView ("{", 1)));
      FAIL () << "expected an exception";
    }
  catch (LumexJsonSchemaException const &exc)
    {
      EXPECT_EQ (exc.reason (), LumexJsonSchemaFailure::parse_error);
      EXPECT_EQ (exc.path (), "$");
    }
  EXPECT_THROW (LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (
                    LumexJsonSchemaTraverser::parse (LumexStringView ())),
                LumexJsonSchemaException);
}

// --- exception ---

TEST (LumexJsonSchemaExceptionTest,
      GivenFields_WhenConstructed_ThenAccessorsAndWhatMatch)
{
  LumexJsonSchemaException const exc (LumexJsonSchemaFailure::enum_mismatch,
                                      "$.a", "bad");
  EXPECT_EQ (exc.reason (), LumexJsonSchemaFailure::enum_mismatch);
  EXPECT_EQ (exc.path (), "$.a");
  EXPECT_EQ (exc.detail (), "bad");
  EXPECT_STREQ (exc.what (),
                "[LumexJsonSchemaException] enum_mismatch at '$.a': bad");
  EXPECT_TRUE (
      (std::is_base_of<std::runtime_error, LumexJsonSchemaException>::value));
}

TEST (LumexJsonSchemaExceptionTest, GivenEveryFailure_WhenToString_ThenName)
{
  EXPECT_STREQ (toString (LumexJsonSchemaFailure::parse_error), "parse_error");
  EXPECT_STREQ (toString (LumexJsonSchemaFailure::missing_required),
                "missing_required");
  EXPECT_STREQ (toString (LumexJsonSchemaFailure::type_mismatch),
                "type_mismatch");
  EXPECT_STREQ (toString (LumexJsonSchemaFailure::const_mismatch),
                "const_mismatch");
  EXPECT_STREQ (toString (LumexJsonSchemaFailure::enum_mismatch),
                "enum_mismatch");
  EXPECT_EQ (lumex::applied::json::schema::LumexJsonSchemaFailureSize, 5u);
}

// --- validator ---

TEST (LumexJsonSchemaValidatorTest, GivenValidText_WhenValidate_ThenNoThrow)
{
  LumexJsonSchemaValidator const validator (person_schema ());
  std::string const raw = text (valid_person ());
  EXPECT_NO_THROW (validator.validate (LumexStringView (raw)));
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
  EXPECT_THROW (validator.validate (LumexStringView (raw), error),
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
  EXPECT_NO_THROW (validator.validate (LumexStringView (raw), error));
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
  validator.validate (LumexStringView (raw), error);
  EXPECT_FALSE (error);
}

TEST (LumexJsonSchemaValidatorTest,
      GivenNonStrictAndViolation_WhenValidateWithoutError_ThenDoesNotThrow)
{
  LumexJsonSchemaValidator const validator (person_schema (), false);
  EXPECT_NO_THROW (validator.validate (LumexStringView ("[]", 2)));
}

TEST (LumexJsonSchemaValidatorTest,
      GivenNonStrictAndUnparsable_WhenValidateWithoutError_ThenDoesNotThrow)
{
  LumexJsonSchemaValidator const validator (person_schema (), false);
  EXPECT_NO_THROW (validator.validate (LumexStringView ("{", 1)));
  EXPECT_NO_THROW (validator.validate (LumexStringView ()));
}

TEST (LumexJsonSchemaValidatorTest,
      GivenStrictAndViolation_WhenValidateWithoutError_ThenThrows)
{
  LumexJsonSchemaValidator const validator (person_schema (), true);
  EXPECT_THROW (validator.validate (LumexStringView ("[]", 2)),
                LumexJsonSchemaException);
}

TEST (LumexJsonSchemaValidatorTest,
      GivenNonStrictThroughInterface_WhenValidateWithoutError_ThenNoThrow)
{
  LumexJsonSchemaValidator const validator (person_schema (), false);
  ILumexJsonSchemaValidator const &base = validator;
  EXPECT_NO_THROW (base.validate (LumexStringView ("{}", 2)));
}

TEST (LumexJsonSchemaValidatorTest,
      GivenUnparsableText_WhenValidate_ThenParseError)
{
  LumexJsonSchemaValidator const validator (person_schema ());
  try
    {
      validator.validate (LumexStringView ("not json", 8));
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
  base.validate (LumexStringView ("{}", 2), error);
  EXPECT_TRUE (error);
  EXPECT_TRUE ((std::is_polymorphic<ILumexJsonSchemaValidator>::value));
  EXPECT_TRUE (
      (std::has_virtual_destructor<ILumexJsonSchemaValidator>::value));
}

// --- normalizer ---

TEST (LumexJsonSchemaNormalizerTest,
      GivenLegacyField_WhenNormalize_ThenUpgradedAndShaped)
{
  PersonNormalizer const normalizer (true);
  nlohmann::json input = valid_person ();
  input.erase ("name");
  input["fullName"] = "Bob";
  std::string const raw = text (input);
  nlohmann::json const result = normalizer.normalize (LumexStringView (raw));
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
                    normalizer.normalize (LumexStringView ("{}", 2), error)),
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
      = normalizer.normalize (LumexStringView ("{}", 2), error);
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
  normalizer.validate (LumexStringView ("{", 1), error);
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
  normalizer.validate (LumexStringView (raw), error);
  EXPECT_FALSE (error);
  EXPECT_NO_THROW (normalizer.validate (LumexStringView (raw)));
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
  EXPECT_NO_THROW (normalizer.validate (LumexStringView ("{}", 2)));
  EXPECT_NO_THROW (normalizer.validate (LumexStringView ("{", 1)));
  nlohmann::json result;
  EXPECT_NO_THROW (result = normalizer.normalize (LumexStringView ("{}", 2)));
  EXPECT_TRUE (result.is_null ());
}

TEST (LumexJsonSchemaNormalizerTest,
      GivenStrictAndViolation_WhenOneArgOverloads_ThenThrow)
{
  PersonNormalizer const normalizer (true);
  EXPECT_THROW (normalizer.validate (LumexStringView ("{}", 2)),
                LumexJsonSchemaException);
  EXPECT_THROW (LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (
                    normalizer.normalize (LumexStringView ("{", 1))),
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
  EXPECT_EQ (base.normalize (LumexStringView (raw))["name"], "Ann");
  EXPECT_TRUE ((std::is_abstract<LumexJsonSchemaNormalizer>::value));
  EXPECT_TRUE ((std::has_virtual_destructor<ILumexJsonNormalizer>::value));
}
