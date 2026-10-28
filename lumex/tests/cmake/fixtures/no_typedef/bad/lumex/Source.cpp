// A function pointer, a class without a name and a list of declarators.
typedef void (*callback_t) (int code, char const *text);

typedef struct
{
  int first;
  int second;
} pair_t;

typedef int value_t, *pointer_t;
