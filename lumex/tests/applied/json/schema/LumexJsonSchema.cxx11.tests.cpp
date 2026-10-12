// Tests of lumex/applied/json/schema: LumexJsonSchemaTraverser (the
// normalization walk, the violations and the parse) and
// LumexJsonSchemaException. The validator and the normalizer are tested in
// ../validation and ../normalization with the schema of
// LumexJsonSchemaTestFixtures.hpp.

#include <stdexcept>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "lumex/applied/json/LumexJson"
#include "lumex/core/string_view/view/LumexStringView.hpp"
#include "lumex/core/utility/attr/LumexAttributes.hpp"

#include "lumex/tests/applied/json/LumexJsonSchemaTestFixtures.hpp"

using lumex::applied::json::schema::LumexJsonSchemaCheckMode;
using lumex::applied::json::schema::LumexJsonSchemaException;
using lumex::applied::json::schema::LumexJsonSchemaFailure;
using lumex::applied::json::schema::LumexJsonSchemaTraverser;
using lumex::core::string_view::view::lumex_string_view;
using lumex_json_schema_test::person_schema;
using lumex_json_schema_test::valid_person;

namespace
{
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
  EXPECT_EQ (LumexJsonSchemaTraverser::parse (lumex_string_view (raw)),
             nlohmann::json::parse (raw));
}

TEST (LumexJsonSchemaTraverserTest,
      GivenViewOfPrefix_WhenParse_ThenOnlyViewedBytesAreParsed)
{
  std::string const raw ("[1] trailing garbage");
  EXPECT_EQ (
      LumexJsonSchemaTraverser::parse (lumex_string_view (raw.data (), 3)),
      nlohmann::json::array ({ 1 }));
}

TEST (LumexJsonSchemaTraverserTest,
      GivenInvalidOrEmptyText_WhenParse_ThenParseErrorAtRoot)
{
  try
    {
      LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (
          LumexJsonSchemaTraverser::parse (lumex_string_view ("{", 1)));
      FAIL () << "expected an exception";
    }
  catch (LumexJsonSchemaException const &exc)
    {
      EXPECT_EQ (exc.reason (), LumexJsonSchemaFailure::parse_error);
      EXPECT_EQ (exc.path (), "$");
    }
  EXPECT_THROW (LUMEX_ATTRIBUTE_MAYBE_UNUSED_VAR (
                    LumexJsonSchemaTraverser::parse (lumex_string_view ())),
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
  EXPECT_STREQ (to_string (LumexJsonSchemaFailure::parse_error),
                "parse_error");
  EXPECT_STREQ (to_string (LumexJsonSchemaFailure::missing_required),
                "missing_required");
  EXPECT_STREQ (to_string (LumexJsonSchemaFailure::type_mismatch),
                "type_mismatch");
  EXPECT_STREQ (to_string (LumexJsonSchemaFailure::const_mismatch),
                "const_mismatch");
  EXPECT_STREQ (to_string (LumexJsonSchemaFailure::enum_mismatch),
                "enum_mismatch");
  EXPECT_EQ (lumex::applied::json::schema::LumexJsonSchemaFailureSize, 5u);
}
