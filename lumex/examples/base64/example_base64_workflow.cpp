#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "lumex/core/base64/LumexBase64"

using namespace lumex::core::base64::codec::Types;
using namespace lumex::core::base64::decode;
using namespace lumex::core::base64::encode;
using namespace lumex::core::base64::validate;

int
main ()
{
  std::cout << "=== Workflow: pack a config blob for a text channel ===\n\n";

  std::ostringstream blob;
  blob << "user=lab\n"
       << "instrument=hplc-01\n"
       << "run=42\n";
  std::string const plaintext = blob.str ();

  std::vector<byte_type> bytes (plaintext.begin (), plaintext.end ());
  std::string const wire = Encoder::encode (bytes);
  std::cout << "plaintext_bytes=" << bytes.size ()
            << " wire_chars=" << wire.size () << '\n';
  std::cout << "wire=" << wire << '\n';

  if (!Validator::is_valid_base64 (wire))
    {
      std::cerr << "encoded payload failed Validator::is_valid_base64\n";
      return 1;
    }

  std::vector<byte_type> recovered;
  if (!Decoder::decode (wire, recovered))
    {
      std::cerr << "decode failed on a validator-accepted string\n";
      return 1;
    }

  std::string const restored (recovered.begin (), recovered.end ());
  std::cout << "restored:\n" << restored;
  std::cout << "roundtrip_ok=" << (restored == plaintext ? "yes" : "no")
            << '\n';
  return 0;
}
