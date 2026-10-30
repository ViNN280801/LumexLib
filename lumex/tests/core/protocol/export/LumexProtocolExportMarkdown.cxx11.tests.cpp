// export_markdown (lumex/core/protocol/export): the document of a layer with
// its commands, fields, registry, options and units. The snapshot is the
// text a reader gets; a change of a name, a scale or a unit in the table
// changes it.

#include <string>

#include <gtest/gtest.h>

#include "lumex/core/protocol/LumexProtocol"
#include "lumex/tests/core/protocol/LumexProtocolIndexTables.hpp"

namespace
{
namespace tables = lumex_tests::index_tables;
namespace index = lumex::core::protocol::index;

// clang-format off
#define PIPED_PARAMS(P, P_AS) P (SPLIT, (1), "Left | right", (u8_), RW, ())
// clang-format on
LUMEX_PROTOCOL_REGISTRY (piped, PIPED_PARAMS);

index::protocol_index_t
gadget_index ()
{
  index::index_result_t result
      = index::build_index<tables::gadget, tables::lamp> ();
  EXPECT_TRUE (result.has_value ());
  return *result;
}

std::string const kExpected = R"MD(# gadget

Key width on the wire: 1 byte.

## Commands

### PING

Command `PING`, key `0x01`.

> Liveness check

Reply: none.

Options: idempotent.

Request fields:

none

### SET_SPEED

Command `SET_SPEED`, key `0x02`.

> Set the speed

Reply: none.

Options: timeout 500 ms; retries 2.

Request fields:

| Field | Kind | Size | Order | Signed | Scale | Unit | Details |
| --- | --- | --- | --- | --- | --- | --- | --- |
| speed | integer | 2 | little | no | 1/10 | rpm |  |

### STATUS

Command `STATUS`, key `0x03`.

> Read the state | now

Reply: layout `state_t`.

Options: log level debug; accepted in 0.

Request fields:

none

Reply fields:

| Field | Kind | Size | Order | Signed | Scale | Unit | Details |
| --- | --- | --- | --- | --- | --- | --- | --- |
| mode | enumeration | 1 | - | no | 1 |  |  |
| level | integer | 1 | - | no | 1 |  |  |

### POINTS

Event `POINTS`, key `0x04`.

> Read the points

Reply: layout `points_t`.

Options: stream every 250 ms.

Request fields:

none

Reply fields:

| Field | Kind | Size | Order | Signed | Scale | Unit | Details |
| --- | --- | --- | --- | --- | --- | --- | --- |
| pair | array | 4 | little | no | 1 |  | count 2 |
| pair[element] | integer | 2 | little | no | 1 |  |  |
| tail | optional_tail | var | - | no | 1 |  | count 2 |
| tail[tail 1] | integer | 1 | - | no | 1 |  |  |
| tail[tail 2] | integer | 1 | - | no | 1 | % |  |

### DELETE

Command `ERASE`, key `0x05`.

> Erase the log

Reply: none.

Options: firmware 2-9; models A, B; dangerous.

Request fields:

none

## Registry lamp

| Entry | Key | Kind | Size | Signed | Scale | Unit | Access | Limits | Firmware | Models | Description |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| FREQUENCY | index 16 | integer | 2 | no | 1/10 | Hz | RW | 1 .. 100 | 2-7 | - | Flash frequency |
| LEVEL | index 17 | integer | 1 | no | 1 | % | R | - | - | - | Lamp level |
| OFFSET | index 18 | integer | 2 | yes | 1/100 |  | RW | -5 .. 5 | - | A, B | Level offset |
| SERIAL (SN) | index 19 | integer | 4 | no | 1 |  | R | - | - | - | Serial number |
| REMOVE_ALL (DELETE) | index 20 | integer | 1 | no | 1 |  | W | - | - | - | Clear all parameters |

## Units

- %
- Hz
- rpm
)MD";
} // namespace

TEST (LumexProtocolExportMarkdownTest,
      GivenALayerWithARegistry_WhenExported_ThenTheDocumentIsTheSnapshot)
{
  EXPECT_EQ (index::export_markdown (gadget_index ()), kExpected);
}

TEST (LumexProtocolExportMarkdownTest,
      GivenALayerWithoutRegistries_WhenExported_ThenThereIsNoRegistrySection)
{
  index::index_result_t const &result
      = index::protocol_index<tables::widget_v2> ();
  ASSERT_TRUE (result.has_value ());
  std::string const text = index::export_markdown (*result);
  EXPECT_NE (text.find ("# widget_v2\n\nExtends `widget`.\n"),
             std::string::npos);
  EXPECT_EQ (text.find ("## Registry"), std::string::npos);
  EXPECT_NE (text.find ("### FLASH\n"), std::string::npos);
  EXPECT_NE (text.find ("firmware 2-9; models A"), std::string::npos);
}

TEST (LumexProtocolExportMarkdownTest,
      GivenARowWhoseScaleChanges_WhenExported_ThenTheDocumentChanges)
{
  index::index_result_t const &base = index::protocol_index<tables::widget> ();
  index::index_result_t const &layer
      = index::protocol_index<tables::widget_v2> ();
  ASSERT_TRUE (base.has_value ());
  ASSERT_TRUE (layer.has_value ());
  EXPECT_NE (index::export_markdown (*base).find (
                 "| speed | integer | 2 | little | no | 1/10 | rpm |"),
             std::string::npos);
  EXPECT_NE (index::export_markdown (*layer).find (
                 "| speed | integer | 2 | little | no | 1/100 | rpm |"),
             std::string::npos);
}

TEST (LumexProtocolExportMarkdownTest,
      GivenADescriptionWithAPipe_WhenExportedInATable_ThenThePipeIsEscaped)
{
  index::index_result_t const result
      = index::build_index<tables::widget, piped> ();
  ASSERT_TRUE (result.has_value ());
  std::string const text = index::export_markdown (*result);
  EXPECT_NE (text.find ("| Left \\| right |"), std::string::npos);
  EXPECT_EQ (text.find ("| Left | right |"), std::string::npos);
}

TEST (LumexProtocolExportMarkdownTest,
      GivenAReplyKindAndAnOptionOfAnotherLayer_WhenExported_ThenBothAreNamed)
{
  index::index_result_t const result
      = index::build_index<tables::dialogue::dialogue_protocol> ();
  ASSERT_TRUE (result.has_value ());
  std::string const text = index::export_markdown (*result);
  EXPECT_NE (text.find ("Reply: a reply kind of the dialogue."),
             std::string::npos);
  EXPECT_NE (text.find ("Options: timeout 200 ms; rate_limit 50."),
             std::string::npos);
  EXPECT_NE (text.find ("Reply: one field."), std::string::npos);
}

TEST (
    LumexProtocolExportMarkdownTest,
    GivenAReplyKindWithItsTextAndOptionsWithTexts_WhenExported_ThenTheTextsAreShown)
{
  index::index_result_t const result
      = index::build_index<tables::hooked::hooked_protocol> ();
  ASSERT_TRUE (result.has_value ());
  std::string const text = index::export_markdown (*result);
  EXPECT_NE (text.find ("Reply: a reply kind of the dialogue, yes_no (one "
                        "field)."),
             std::string::npos);
  EXPECT_NE (text.find ("Options: mode fast; step 1; step 2; step 3."),
             std::string::npos);
  EXPECT_NE (text.find ("Options: mode slow; step 4."), std::string::npos);
}

TEST (LumexProtocolExportMarkdownTest,
      GivenADescriptionWithLines_WhenExported_ThenItIsABlockQuoteAndNoHeading)
{
  index::index_result_t const result
      = index::build_index<tables::notes, tables::triple> ();
  ASSERT_TRUE (result.has_value ());
  std::string const text = index::export_markdown (*result);
  EXPECT_NE (text.find ("> Title\n> # Heading\n> - item\n\n"),
             std::string::npos);
  EXPECT_EQ (text.find ("\n# Heading"), std::string::npos);
}

TEST (
    LumexProtocolExportMarkdownTest,
    GivenALineBreakInARegistryDescription_WhenExported_ThenTheCellStaysOnOneRow)
{
  index::index_result_t const result
      = index::build_index<tables::notes, tables::triple> ();
  ASSERT_TRUE (result.has_value ());
  std::string const text = index::export_markdown (*result);
  EXPECT_NE (text.find ("| Line one Line two |"), std::string::npos);
  EXPECT_EQ (text.find ("Line one\nLine two"), std::string::npos);
  EXPECT_NE (text.find ("| mnemonic \"ABC\" |"), std::string::npos);
}
