#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

#include "lumex/core/base64/LumexBase64"

using namespace lumex::core::base64::codec::Types;
using namespace lumex::core::base64::decode;
using namespace lumex::core::base64::encode;
using namespace lumex::core::base64::validate;

namespace
{
void
print_bytes (char const *label, std::vector<byte_type> const &bytes)
{
  std::cout << label << " size=" << bytes.size () << " hex=";
  for (std::size_t i = 0; i < bytes.size (); ++i)
    {
      unsigned int const value = static_cast<unsigned int> (bytes[i]);
      if (value < 16U)
        std::cout << '0';
      std::cout << std::hex << value << std::dec;
      if (i + 1U < bytes.size ())
        std::cout << ' ';
    }
  std::cout << '\n';
}
}

int
main ()
{
  std::cout << "=== Base64 encode / decode / validate ===\n\n";

  std::cout << "--- 1. Encode a C string (pointer + size) ---\n";
  char const *payload = "Hello, Lumex!";
  std::string const encoded_ptr
      = Encoder::encode (payload, std::char_traits<char>::length (payload));
  std::cout << "input=\"" << payload << "\" encoded=" << encoded_ptr << '\n';

  std::cout << "\n--- 2. Encode a vector of bytes ---\n";
  std::vector<byte_type> const raw = { 0x00, 0x01, 0xFE, 0xFF, 'A', 'B', 'C' };
  std::string const encoded_vec = Encoder::encode (raw);
  print_bytes ("raw", raw);
  std::cout << "encoded=" << encoded_vec << '\n';

  std::cout << "\n--- 3. Decode into an output vector (clears `out`) ---\n";
  std::vector<byte_type> decoded;
  bool const ok_out = Decoder::decode (encoded_ptr, decoded);
  std::cout << "decode(out) ok=" << (ok_out ? "yes" : "no") << '\n';
  print_bytes ("decoded", decoded);

  std::cout << "\n--- 4. Decode as a returned vector ---\n";
  std::vector<byte_type> const roundtrip = Decoder::decode (encoded_vec);
  print_bytes ("roundtrip", roundtrip);
  std::cout << "matches_raw=" << (roundtrip == raw ? "yes" : "no") << '\n';

  std::cout << "\n--- 5. Validator: well-formed vs broken ---\n";
  char const *good = "SGVsbG8=";
  char const *bad_len = "SGVsbG8";
  char const *bad_char = "SGVs$G8=";
  std::cout << "is_valid(\"" << good
            << "\")=" << (Validator::is_valid_base64 (good) ? "yes" : "no")
            << '\n';
  std::cout << "is_valid(\"" << bad_len
            << "\")=" << (Validator::is_valid_base64 (bad_len) ? "yes" : "no")
            << '\n';
  std::cout << "is_valid(\"" << bad_char
            << "\")=" << (Validator::is_valid_base64 (bad_char) ? "yes" : "no")
            << '\n';

  std::cout << "\n--- 6. Empty / null inputs ---\n";
  std::string const empty_enc = Encoder::encode (nullptr, 0);
  std::vector<byte_type> const empty_dec = Decoder::decode (empty_enc);
  std::cout << "encode(nullptr,0) empty="
            << (empty_enc.empty () ? "yes" : "no")
            << " decode(\"\") size=" << empty_dec.size () << '\n';

  std::cout << "\n--- 7. Invalid decode must fail cleanly ---\n";
  std::vector<byte_type> junk;
  bool const bad_ok = Decoder::decode (std::string ("@@@@"), junk);
  std::cout << "decode(\"@@@@\") ok=" << (bad_ok ? "yes" : "no")
            << " out_size=" << junk.size () << '\n';

  std::cout << "\n=== Base64 example finished ===\n";
  return 0;
}
