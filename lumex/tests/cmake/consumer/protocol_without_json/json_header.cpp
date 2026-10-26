// The JSON export header alone: it compiles only when LUMEX_PROTOCOL_WITH_JSON
// is defined (see CMakeLists.txt). Nothing in it is called, so the program
// needs no library.

#include "lumex/core/protocol/export/LumexProtocolExportJson.hpp"

int
main ()
{
  return 0;
}
