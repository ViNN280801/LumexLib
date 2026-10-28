// Two typedefs in the continuation lines of a macro: the checker counts the
// text of a #define like any other code.
#define LUMEX_FIXTURE_ROW(name)                                               \
  typedef int name##_type;                                                    \
  typedef std::vector<name##_type> name##_list;                               \
  struct name##_row                                                           \
  {                                                                           \
    name##_type value;                                                        \
  }
