#include <cstdint>
#include <iostream>
#include <vector>

#include "lumex/core/crc/LumexCrc"

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

using namespace lumex::core::crc::catalog;
using namespace lumex::core::crc::parametric;

int
main ()
{
  std::cout << "=== CRC catalog and parametric engines ===\n\n";

  std::uint8_t const check_ascii[]
      = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
  std::vector<std::uint8_t> const payload (check_ascii,
                                           check_ascii + sizeof (check_ascii));

  std::cout << "--- 1. Catalogue size and first few bit widths ---\n";
  std::uint32_t const count = GetCrcCatalogEntryCount ();
  std::cout << "catalog_entries=" << count << '\n';
  std::uint32_t const preview = count < 8U ? count : 8U;
  for (std::uint32_t i = 0; i < preview; ++i)
    {
      std::cout << "  [" << i << "] width=" << GetCrcCatalogBitWidth (i)
                << " crc=" << ComputeCrcCatalog (i, payload) << '\n';
    }

  std::cout << "\n--- 2. CRC-32/ISO-HDLC (Ethernet / ZIP / PNG) ---\n";
  std::uint32_t const crc32
      = Crc32IsoHdlc::calculate (check_ascii, sizeof (check_ascii));
  std::uint32_t const crc32_vec = Crc32IsoHdlc::calculate (payload);
  std::cout << "crc32=0x" << std::hex << crc32
            << " vector_match=" << (crc32 == crc32_vec ? "yes" : "no")
            << std::dec << '\n';

  std::cout << "\n--- 3. CRC-8/MAXIM-DOW (1-Wire) ---\n";
  std::uint8_t const crc8
      = Crc8MaximDow::calculate (check_ascii, sizeof (check_ascii));
  std::cout << "crc8=0x" << std::hex << static_cast<unsigned> (crc8)
            << std::dec << '\n';

  std::cout << "\n--- 4. Empty buffer is a defined check value ---\n";
  std::vector<std::uint8_t> const empty;
  std::cout << "crc32(empty)=0x" << std::hex << Crc32IsoHdlc::calculate (empty)
            << std::dec << '\n';

  std::cout << "\n--- 5. Out-of-range catalogue index ---\n";
  std::uint64_t const bogus
      = ComputeCrcCatalog (count + 100U, check_ascii, sizeof (check_ascii));
  std::cout << "crc[count+100]=" << bogus << '\n';

  std::cout << "\n=== CRC example finished ===\n";
  return 0;
}
