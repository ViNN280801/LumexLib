// The JSON umbrella under the strict flags of a consumer, built with
// -Wall -Wextra -Wpedantic -Werror by cmake.hygiene_compile_checks:
// LumexJsonSchemaValidator and LumexJsonSchemaNormalizer discard a nodiscard
// result on purpose (-Wunused-result on GCC below C++17), and the reflected
// enums of the JSON headers must not pass an empty variadic argument to a
// macro (-Wpedantic below C++20).

#include "lumex/applied/json/LumexJson"
