#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "lumex/core/crc/LumexCrc"

using namespace lumex::core::crc::parametric;

int
main ()
{
  std::cout << "=== Workflow: checksum a method file before transfer ===\n\n";

  std::string const method
      = "method=isocratic\nflow_ml_min=1.0\nwavelength_nm=254\n";
  std::vector<std::uint8_t> const bytes (method.begin (), method.end ());

  std::uint32_t const crc = Crc32IsoHdlc::calculate_crc32 (bytes);
  std::cout << "bytes=" << bytes.size () << " crc32=0x" << std::hex << crc
            << std::dec << '\n';

  std::vector<std::uint8_t> mutated = bytes;
  if (!mutated.empty ())
    mutated[mutated.size () / 2U] ^= 0x01U;
  std::uint32_t const mutated_crc = Crc32IsoHdlc::calculate_crc32 (mutated);
  std::cout << "mutated_crc=0x" << std::hex << mutated_crc << std::dec
            << " differs=" << (mutated_crc != crc ? "yes" : "no") << '\n';
  return 0;
}
