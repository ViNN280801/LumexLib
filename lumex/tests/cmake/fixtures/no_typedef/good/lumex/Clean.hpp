// typedef int in_a_comment_t;
/* typedef int in_a_block_comment_t; */
#include <cstddef>

using alias_t = int;
using function_t = void (*) (int);
template <typename T> using list_t = T *;

char const *const text = "typedef int in_a_string_t;";
char const *const raw = R"x(" typedef int in_a_raw_string_t; ")x";
int const separated = 1'000;
int const typedef_count = 3;
int const my_typedef = 4;

#define LUMEX_FIXTURE_ALIAS(name) using name##_type = int
