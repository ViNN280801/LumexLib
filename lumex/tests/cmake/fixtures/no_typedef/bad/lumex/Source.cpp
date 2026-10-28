// A function pointer, a class without a name and a list of declarators.
typedef void (*callback_t) (int code, char const *text);

typedef struct
{
  int first;
  int second;
} pair_t;

typedef int value_t, *pointer_t;

typedef A const const_alias_t;

typedef unsigned __int128 wide_t __attribute__((may_alias));

constexpr int digits = 1'000; typedef long after_digits_t;
