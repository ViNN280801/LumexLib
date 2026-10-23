// A protocol table after the platform headers that define macros with names a
// command table uses (<windows.h> on Windows: DELETE, ERROR, IN, OUT,
// OPTIONAL, TRUE, FALSE, min, max, interface, small; the POSIX headers
// elsewhere: ECHO, MIN, MAX, and major, minor on glibc). The rows with those
// specification names are written with X_AS; the macros stay defined while
// the module headers, the table, its layer, the entries and the client
// methods (named by the bare identifiers) compile, and a row encodes and
// decodes. The table is synthetic: an invented device, no real protocol.
// Exit code 0 when the names and the values read back.

#if defined(_WIN32)
#include <windows.h>
#if !defined(DELETE) || !defined(ERROR) || !defined(IN) || !defined(OUT)      \
    || !defined(OPTIONAL) || !defined(TRUE) || !defined(FALSE)                \
    || !defined(interface)
#error "<windows.h> did not define the macros this fixture is about"
#endif
// The Windows SDK of MSVC also defines min, max and small (MinGW does not
// for C++).
#if defined(_MSC_VER) && (!defined(min) || !defined(max) || !defined(small))
#error "<windows.h> did not define min, max and small"
#endif
#else
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/sysmacros.h>
#if !defined(major) || !defined(minor)
#error "<sys/sysmacros.h> did not define major and minor"
#endif
#endif
#if !defined(ECHO) || !defined(MIN) || !defined(MAX)
#error "the POSIX headers did not define the macros this fixture is about"
#endif
#endif

#include <cstdio>
#include <cstring>
#include <ratio>

#include "lumex/core/protocol/LumexProtocol"

namespace fixture
{
namespace dsl = lumex::core::protocol::dsl;

struct codec
{
};

struct header
{
};

enum class phase
{
  ready,
  running
};

LUMEX_PROTOCOL_LAYOUT (reply_t, (u8_<>, u16_le_<scale<std::ratio<1, 10>>>),
                       ("IN", "OUT"));

// clang-format off
#define PLATFORM_ROWS(X, X_AS)                                                \
  X (PING, 0x01, "Liveness", (), (), (), (idempotent))                        \
  X_AS (DELETE_ITEM, "DELETE", 0x02, "Delete an item", (u8_<>), ("item"),     \
        (reply_t), (dangerous, timeout (100)))                                \
  X_AS (LAST_ERROR, "ERROR", 0x03, "Last error", (), (), (reply_t),           \
        (log_level (level::error)))                                           \
  X_AS (READ_IN, "IN", 0x04, "Read an input", (u8_<>), ("IN"), (reply_t),     \
        (accepted_in (phase::ready, phase::running)))                         \
  X_AS (WRITE_OUT, "OUT", 0x05, "Write an output", (u8_<>, u8_<>),            \
        ("OUT", "OPTIONAL"), (), ())                                          \
  X_AS (OPTIONAL_PART, "OPTIONAL", 0x06, "Optional part", (), (), (), ())     \
  X_AS (MIN_LEVEL, "min", 0x07, "Lowest level", (), (), (reply_t), ())        \
  X_AS (MAX_LEVEL, "max", 0x08, "Highest level", (), (), (reply_t), ())       \
  X_AS (SET_TRUE, "TRUE", 0x09, "Set", (), (), (), ())                        \
  X_AS (SET_FALSE, "FALSE", 0x0A, "Clear", (), (), (), ())                    \
  X_AS (ECHO_TEXT, "ECHO", 0x0B, "Echo", (), (), (), (stream (10)))           \
  X_AS (MAJOR_VERSION, "major", 0x0C, "Major version", (), (), (), ())        \
  X_AS (MINOR_VERSION, "minor", 0x0D, "Minor version", (), (), (), ())        \
  X_AS (INTERFACE_TYPE, "interface", 0x0E, "Interface", (), (), (), ())       \
  X_AS (SMALL_BLOCK, "small", 0x0F, "Small block", (), (), (), ())

#define PLATFORM_LAYER_ROWS(X, X_AS)                                          \
  X_AS (MIN_LEVEL, "min", 0x17, "Lowest level (layer)", (), (), (reply_t),    \
        (firmware (2, 3), model ("M")))
// clang-format on

LUMEX_PROTOCOL_DEFINE (platform_protocol, codec, header, PLATFORM_ROWS);
LUMEX_PROTOCOL_EXTEND (platform_layer, platform_protocol, PLATFORM_LAYER_ROWS);
LUMEX_PROTOCOL_CHECK_IDENTIFIERS (PLATFORM_ROWS);

struct client : platform_layer::lumex_methods<client>
{
  template <typename Row, typename... Args>
  char const *
  lumex_invoke (Args...)
  {
    return Row::specification_name ();
  }
};

int
check ()
{
  int failures = 0;
  client user;
  char const *const expected[] = { "DELETE",    "ERROR", "IN",    "OUT",
                                   "OPTIONAL",  "min",   "max",   "TRUE",
                                   "FALSE",     "ECHO",  "major", "minor",
                                   "interface", "small" };
  char const *const called[]
      = { user.DELETE_ITEM (1),   user.LAST_ERROR (),    user.READ_IN (2),
          user.WRITE_OUT (3, 4),  user.OPTIONAL_PART (), user.MIN_LEVEL (),
          user.MAX_LEVEL (),      user.SET_TRUE (),      user.SET_FALSE (),
          user.ECHO_TEXT (),      user.MAJOR_VERSION (), user.MINOR_VERSION (),
          user.INTERFACE_TYPE (), user.SMALL_BLOCK () };
  for (unsigned index = 0; index < sizeof (expected) / sizeof (expected[0]);
       ++index)
    if (std::strcmp (expected[index], called[index]) != 0)
      ++failures;

  lumex::core::span::view::span<dsl::command_entry_t const> const entries
      = dsl::command_entries<platform_layer> ();
  if (entries.size () != 15 || entries[6].key.integer != 0x17
      || !entries[6].options.has_firmware
      || std::strcmp (entries[1].specification_name, "DELETE") != 0
      || std::strcmp (entries[4].field_names[1], "OPTIONAL") != 0)
    ++failures;
  if (!dsl::is_platform_macro_name ("DELETE")
      || dsl::is_platform_macro_name ("DELETE_ITEM"))
    ++failures;

  // The field and layout code works with the macros defined too.
  namespace layout = lumex::core::protocol::layout;
  layout::bytes_t bytes;
  if (!layout::encode_request<platform_layer, platform_layer::WRITE_OUT_t> (
          bytes, 3, 4)
      || !layout::decode_request<platform_layer, platform_layer::WRITE_OUT_t> (
          layout::bytes_view_t (bytes.data (), bytes.size ())))
    ++failures;
  bytes.clear ();
  layout::reply_value_of<platform_layer::MIN_LEVEL_t>::type reply
      = layout::reply_value_of<platform_layer::MIN_LEVEL_t>::type ();
  if (!layout::encode_reply<platform_layer, platform_layer::MIN_LEVEL_t> (
          bytes, reply)
      || bytes.size () != 3)
    ++failures;
  return failures;
}
} // namespace fixture

int
main ()
{
  int const failures = fixture::check ();
  std::printf ("protocol_platform_macros: %d failures\n", failures);
  return failures == 0 ? 0 : 1;
}
