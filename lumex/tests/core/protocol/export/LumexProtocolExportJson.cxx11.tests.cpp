// export_json (lumex/core/protocol/export/LumexProtocolExportJson.hpp): the
// document of a layer as a JSON value, checked against a schema-like
// description of its structure and against the values of the tables. The
// file is empty when the module is built without LUMEX_PROTOCOL_WITH_JSON.

#ifdef LUMEX_PROTOCOL_WITH_JSON

#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/core/protocol/export/LumexProtocolExportJson.hpp"
#include "lumex/tests/core/protocol/LumexProtocolIndexTables.hpp"

namespace
{
namespace tables = lumex_tests::index_tables;
namespace index = lumex::core::protocol::index;
using nlohmann::json;

// A small structural validator: "type" (object, array, string, integer,
// number, boolean), "required" members, "properties" and "items". It returns
// the first violation as text, empty when the value fits.
std::string
violation (json const &value, json const &schema, std::string const &path)
{
  std::string const type = schema.at ("type").get<std::string> ();
  if (type == "object"    ? !value.is_object ()
      : type == "array"   ? !value.is_array ()
      : type == "string"  ? !value.is_string ()
      : type == "integer" ? !value.is_number_integer ()
      : type == "number"  ? !value.is_number ()
                          : !value.is_boolean ())
    return path + ": not a " + type;
  if (type == "object")
    {
      if (schema.contains ("required"))
        for (json const &name : schema.at ("required"))
          if (!value.contains (name.get<std::string> ()))
            return path + ": no member " + name.get<std::string> ();
      if (schema.contains ("properties"))
        for (json::const_iterator member = schema.at ("properties").begin ();
             member != schema.at ("properties").end (); ++member)
          if (value.contains (member.key ()))
            {
              std::string const inner
                  = violation (value.at (member.key ()), member.value (),
                               path + "." + member.key ());
              if (!inner.empty ())
                return inner;
            }
    }
  if (type == "array" && schema.contains ("items"))
    for (std::size_t i = 0; i < value.size (); ++i)
      {
        std::string const inner
            = violation (value[i], schema.at ("items"),
                         path + "[" + std::to_string (i) + "]");
        if (!inner.empty ())
          return inner;
      }
  return std::string ();
}

json
field_schema ()
{
  return json::parse (R"({
    "type": "object",
    "required": ["name", "kind", "size", "order", "signed", "scale", "unit",
                 "count"],
    "properties": {
      "name": {"type": "string"}, "kind": {"type": "string"},
      "size": {"type": "integer"}, "order": {"type": "string"},
      "signed": {"type": "boolean"}, "unit": {"type": "string"},
      "count": {"type": "integer"},
      "scale": {"type": "object", "required": ["num", "den"],
                "properties": {"num": {"type": "integer"},
                               "den": {"type": "integer"}}}
    }})");
}

json
document_schema ()
{
  json const field = field_schema ();
  json schema = json::parse (R"({
    "type": "object",
    "required": ["protocol", "base", "key_width", "commands", "registries",
                 "units"],
    "properties": {
      "protocol": {"type": "string"}, "base": {"type": "string"},
      "key_width": {"type": "integer"},
      "units": {"type": "array", "items": {"type": "string"}}
    }})");
  json command = json::parse (R"({
    "type": "object",
    "required": ["identifier", "name", "description", "event", "key",
                 "request", "reply", "options"],
    "properties": {
      "identifier": {"type": "string"}, "name": {"type": "string"},
      "description": {"type": "string"}, "event": {"type": "boolean"},
      "key": {"type": "object", "required": ["kind"]},
      "options": {"type": "object"},
      "reply": {"type": "object", "required": ["form", "fields"],
                "properties": {"form": {"type": "string"}}}
    }})");
  command["properties"]["request"] = json::parse (R"({"type": "array"})");
  command["properties"]["request"]["items"] = field;
  command["properties"]["reply"]["properties"]["fields"]
      = json::parse (R"({"type": "array"})");
  command["properties"]["reply"]["properties"]["fields"]["items"] = field;
  json registry = json::parse (R"({
    "type": "object",
    "required": ["name", "entries"],
    "properties": {"name": {"type": "string"}}
  })");
  json entry = json::parse (R"({
    "type": "object",
    "required": ["identifier", "name", "description", "key", "type",
                 "access"],
    "properties": {
      "access": {"type": "string"},
      "key": {"type": "object", "required": ["kind", "text"]}
    }})");
  entry["properties"]["type"] = field;
  registry["properties"]["entries"] = json::parse (R"({"type": "array"})");
  registry["properties"]["entries"]["items"] = entry;
  schema["properties"]["commands"] = json::parse (R"({"type": "array"})");
  schema["properties"]["commands"]["items"] = command;
  schema["properties"]["registries"] = json::parse (R"({"type": "array"})");
  schema["properties"]["registries"]["items"] = registry;
  return schema;
}

json
gadget_document ()
{
  index::index_result_t const result
      = index::build_index<tables::gadget, tables::lamp> ();
  EXPECT_TRUE (result.has_value ());
  return index::export_json (*result);
}
} // namespace

TEST (LumexProtocolExportJsonTest,
      GivenALayerWithARegistry_WhenExported_ThenTheDocumentFitsTheSchema)
{
  EXPECT_EQ (violation (gadget_document (), document_schema (), "$"), "");
}

TEST (
    LumexProtocolExportJsonTest,
    GivenADamagedDocument_WhenCheckedAgainstTheSchema_ThenTheViolationIsFound)
{
  json const schema = document_schema ();
  json missing = gadget_document ();
  missing["commands"][1]["request"][0].erase ("scale");
  EXPECT_EQ (violation (missing, schema, "$"),
             "$.commands[1].request[0]: no member scale");
  json wrong = gadget_document ();
  wrong["commands"][1]["request"][0]["size"] = "two";
  EXPECT_EQ (violation (wrong, schema, "$"),
             "$.commands[1].request[0].size: not a integer");
  json no_registry = gadget_document ();
  no_registry["registries"][0]["entries"][0].erase ("access");
  EXPECT_EQ (violation (no_registry, schema, "$"),
             "$.registries[0].entries[0]: no member access");
}

TEST (LumexProtocolExportJsonTest,
      GivenTheTables_WhenExported_ThenTheNamesKeysScalesAndUnitsAreTheirs)
{
  json const document = gadget_document ();
  EXPECT_EQ (document["protocol"], "gadget");
  EXPECT_EQ (document["base"], "");
  EXPECT_EQ (document["key_width"], 1);
  ASSERT_EQ (document["commands"].size (), 5u);
  json const &speed = document["commands"][1];
  EXPECT_EQ (speed["identifier"], "SET_SPEED");
  EXPECT_EQ (speed["description"], "Set the speed");
  EXPECT_EQ (speed["event"], false);
  EXPECT_EQ (speed["key"]["kind"], "integer");
  EXPECT_EQ (speed["key"]["value"], 2);
  EXPECT_EQ (speed["key"]["width"], 1);
  EXPECT_EQ (speed["key"]["hex"], "0x02");
  EXPECT_EQ (speed["request"][0]["name"], "speed");
  EXPECT_EQ (speed["request"][0]["kind"], "integer");
  EXPECT_EQ (speed["request"][0]["size"], 2);
  EXPECT_EQ (speed["request"][0]["order"], "little");
  EXPECT_EQ (speed["request"][0]["scale"]["num"], 1);
  EXPECT_EQ (speed["request"][0]["scale"]["den"], 10);
  EXPECT_EQ (speed["request"][0]["unit"], "rpm");
  EXPECT_EQ (speed["options"]["timeout_ms"], 500);
  EXPECT_EQ (speed["options"]["retries"], 2);
  EXPECT_EQ (speed["reply"]["form"], "none");
}

TEST (LumexProtocolExportJsonTest,
      GivenRepliesOptionsAndComposites_WhenExported_ThenEachHasItsMembers)
{
  json const document = gadget_document ();
  json const &ping = document["commands"][0];
  ASSERT_EQ (ping["options"]["flags"].size (), 1u);
  EXPECT_EQ (ping["options"]["flags"][0], "idempotent");
  json const &status = document["commands"][2];
  EXPECT_EQ (status["description"], "Read the state | now");
  EXPECT_EQ (status["reply"]["form"], "layout");
  EXPECT_EQ (status["reply"]["layout"], "state_t");
  EXPECT_EQ (status["reply"]["fields"][0]["name"], "mode");
  EXPECT_EQ (status["reply"]["fields"][0]["kind"], "enumeration");
  EXPECT_EQ (status["options"]["log_level"], "debug");
  EXPECT_EQ (status["options"]["accepted_in"][0], 0);
  json const &points = document["commands"][3];
  EXPECT_EQ (points["event"], true);
  EXPECT_EQ (points["options"]["stream_interval_ms"], 250);
  json const &array = points["reply"]["fields"][0];
  EXPECT_EQ (array["kind"], "array");
  EXPECT_EQ (array["children"][0]["role"], "element");
  json const &tail = points["reply"]["fields"][1];
  EXPECT_EQ (tail["kind"], "optional_tail");
  EXPECT_EQ (tail["children"].size (), 2u);
  EXPECT_EQ (tail["children"][1]["unit"], "%");
  json const &erase = document["commands"][4];
  EXPECT_EQ (erase["name"], "DELETE");
  EXPECT_EQ (erase["identifier"], "ERASE");
  EXPECT_EQ (erase["options"]["firmware"]["to"], 9);
  EXPECT_EQ (erase["options"]["models"][1], "B");
  EXPECT_EQ (erase["options"]["flags"][0], "dangerous");
}

TEST (LumexProtocolExportJsonTest,
      GivenARegistry_WhenExported_ThenEveryEntryHasKeyTypeAccessAndRanges)
{
  json const document = gadget_document ();
  ASSERT_EQ (document["registries"].size (), 1u);
  json const &lamp = document["registries"][0];
  EXPECT_EQ (lamp["name"], "lamp");
  ASSERT_EQ (lamp["entries"].size (), 5u);
  json const &frequency = lamp["entries"][0];
  EXPECT_EQ (frequency["identifier"], "FREQUENCY");
  EXPECT_EQ (frequency["key"]["kind"], "index");
  EXPECT_EQ (frequency["key"]["first"], 16);
  EXPECT_EQ (frequency["type"]["unit"], "Hz");
  EXPECT_EQ (frequency["type"]["scale"]["den"], 10);
  EXPECT_EQ (frequency["access"], "RW");
  EXPECT_EQ (frequency["limits"]["max"], 100.0);
  EXPECT_EQ (frequency["firmware"]["from"], 2);
  EXPECT_EQ (lamp["entries"][2]["models"][0], "A");
  EXPECT_EQ (lamp["entries"][3]["name"], "SN");
  EXPECT_FALSE (lamp["entries"][1].contains ("limits"));
}

TEST (LumexProtocolExportJsonTest,
      GivenTheUnitsInUse_WhenExported_ThenTheyAreListedOnceAndSorted)
{
  json const document = gadget_document ();
  json const expected = json::parse (R"(["%", "Hz", "rpm"])");
  EXPECT_EQ (document["units"], expected);
}

TEST (LumexProtocolExportJsonTest,
      GivenTheTextOfTheDocument_WhenParsedAgain_ThenItIsTheSameDocument)
{
  index::index_result_t const result
      = index::build_index<tables::gadget, tables::lamp> ();
  ASSERT_TRUE (result.has_value ());
  json const document = index::export_json (*result);
  EXPECT_EQ (json::parse (index::export_json_text (*result)), document);
  EXPECT_EQ (json::parse (index::export_json_text (*result, -1)), document);
  EXPECT_NE (index::export_json_text (*result).find ("\n  \"commands\""),
             std::string::npos);
}

TEST (LumexProtocolExportJsonTest,
      GivenATextKeyAndACompositeKey_WhenExported_ThenTheKeyHasItsKind)
{
  index::index_result_t const washer = index::build_index<tables::washer> ();
  ASSERT_TRUE (washer.has_value ());
  json const text = index::export_json (*washer);
  EXPECT_EQ (text["commands"][0]["key"]["kind"], "text");
  EXPECT_EQ (text["commands"][0]["key"]["text"], "AZ");
  index::index_result_t const pairs = index::build_index<tables::pairs> ();
  ASSERT_TRUE (pairs.has_value ());
  json const composite = index::export_json (*pairs);
  EXPECT_EQ (composite["commands"][0]["key"]["kind"], "composite");
  EXPECT_EQ (composite["commands"][0]["key"]["value"], 0x0305);
  EXPECT_EQ (composite["commands"][0]["key"]["hex"], "0x0305");
}

TEST (LumexProtocolExportJsonTest,
      GivenAMnemonicRegistry_WhenExported_ThenTheKeyHasTheLetters)
{
  index::index_result_t const result
      = index::build_index<tables::widget, tables::tuning, tables::mixed> ();
  ASSERT_TRUE (result.has_value ());
  json const document = index::export_json (*result);
  EXPECT_EQ (document["registries"][0]["entries"][0]["key"]["mnemonic"], "GN");
  EXPECT_EQ (document["registries"][1]["entries"][3]["key"]["text"],
             "mnemonic \"AZ\"");
  EXPECT_EQ (document["registries"][1]["entries"][1]["key"]["second"], 5);
}

TEST (LumexProtocolExportJsonTest,
      GivenAReplyKindAndAnOptionOfAnotherLayer_WhenExported_ThenBothAreMembers)
{
  index::index_result_t const result
      = index::build_index<tables::dialogue::dialogue_protocol> ();
  ASSERT_TRUE (result.has_value ());
  json const document = index::export_json (*result);
  EXPECT_EQ (document["commands"][0]["reply"]["form"], "dialogue");
  EXPECT_EQ (document["commands"][0]["options"]["rate_limit"], 50);
  EXPECT_EQ (document["commands"][0]["options"]["timeout_ms"], 200);
  EXPECT_EQ (document["commands"][1]["reply"]["form"], "single_field");
  EXPECT_EQ (document["commands"][1]["reply"]["fields"][0]["name"], "reply");
}

TEST (
    LumexProtocolExportJsonTest,
    GivenAReplyKindWithItsTextAndOptionsWithTexts_WhenExported_ThenTheTextsAreMembers)
{
  index::index_result_t const result
      = index::build_index<tables::hooked::hooked_protocol> ();
  ASSERT_TRUE (result.has_value ());
  json const document = index::export_json (*result);
  EXPECT_EQ (document["commands"][0]["reply"]["form"], "dialogue");
  EXPECT_EQ (document["commands"][0]["reply"]["kind"], "yes_no (one field)");
  EXPECT_EQ (document["commands"][0]["options"]["mode"], "fast");
  ASSERT_TRUE (document["commands"][0]["options"]["step"].is_array ());
  EXPECT_EQ (document["commands"][0]["options"]["step"][0], 1);
  EXPECT_EQ (document["commands"][0]["options"]["step"][1], 2);
  EXPECT_EQ (document["commands"][1]["options"]["mode"], "slow");
  EXPECT_FALSE (document["commands"][1]["reply"].contains ("kind"));
}

// A description in a code page of 8 bits (not UTF-8).
// clang-format off
#define LEGACY_ROWS(X, X_AS) \
  X (OLD, 0x01, "\xD1\xEA\xEE\xF0", (), (), (), ())
// clang-format on
LUMEX_PROTOCOL_DEFINE (legacy, tables::codec, tables::widget_header,
                       LEGACY_ROWS);

TEST (
    LumexProtocolExportJsonTest,
    GivenADescriptionThatIsNotUtf8_WhenExportedAsText_ThenTheBadBytesAreReplaced)
{
  index::index_result_t const result = index::build_index<legacy> ();
  ASSERT_TRUE (result.has_value ());
  std::string text;
  ASSERT_NO_THROW (text = index::export_json_text (*result));
  json const back = json::parse (text);
  EXPECT_EQ (back["commands"][0]["description"].get<std::string> (),
             "\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD");
}

#endif // LUMEX_PROTOCOL_WITH_JSON
