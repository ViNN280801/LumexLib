#ifndef LUMEX_TESTS_CORE_EXPECTED_EXPECTED_PROBE_HPP
#define LUMEX_TESTS_CORE_EXPECTED_EXPECTED_PROBE_HPP

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// A probe type that logs every constructor, assignment, swap and destructor
// into one log, and can be told to throw from the next operation of a name.
// The tests of the order of operations of expected (ExpectedReinit,
// ExpectedStdDifferential) read the log. Every file that includes it compiles
// from C++11.

namespace expected_probe
{
using log_t = std::vector<std::string>;

inline log_t &
the_log ()
{
  static log_t log;
  return log;
}

/// The operation that throws next, for example "V:copy-ctor"; empty for none.
inline std::string &
fail_on ()
{
  static std::string operation;
  return operation;
}

inline std::string
joined (log_t const &log)
{
  std::string text;
  for (std::string const &line : log)
    text += (text.empty () ? "" : " ") + line;
  return text;
}

struct injected_failure : std::runtime_error
{
  injected_failure () : std::runtime_error ("injected") {}
};

/// Logs what is done to it. `Name` prefixes the lines, `CopyNoexcept` and
/// `MoveNoexcept` decide whether the copy, the move and (both together) the
/// constructor from an `int` are declared `noexcept`; a declared `noexcept`
/// operation never fails, one that is not throws when `fail_on ()` names it.
template <char Name, bool CopyNoexcept, bool MoveNoexcept> struct probe_t
{
  int id;

  static void
  note (char const *what, int id_value, bool may_fail)
  {
    std::string const line = std::string (1, Name) + ":" + what;
    if (may_fail && fail_on () == line)
      {
        fail_on ().clear ();
        the_log ().push_back (line + "!");
        throw injected_failure ();
      }
    the_log ().push_back (line + "(" + std::to_string (id_value) + ")");
  }

  explicit probe_t (int v) noexcept (CopyNoexcept && MoveNoexcept) : id (v)
  {
    note ("ctor", id, !(CopyNoexcept && MoveNoexcept));
  }
  probe_t (probe_t const &other) noexcept (CopyNoexcept) : id (other.id)
  {
    note ("copy-ctor", id, !CopyNoexcept);
  }
  probe_t (probe_t &&other) noexcept (MoveNoexcept) : id (other.id)
  {
    note ("move-ctor", id, !MoveNoexcept);
    other.id = -1;
  }
  probe_t &
  operator= (probe_t const &other) noexcept (CopyNoexcept)
  {
    note ("copy-assign", other.id, !CopyNoexcept);
    id = other.id;
    return *this;
  }
  probe_t &
  operator= (probe_t &&other) noexcept (MoveNoexcept)
  {
    note ("move-assign", other.id, !MoveNoexcept);
    id = other.id;
    other.id = -1;
    return *this;
  }
  ~probe_t ()
  {
    the_log ().push_back (std::string (1, Name) + ":dtor("
                          + std::to_string (id) + ")");
  }
};

template <char Name, bool CopyNoexcept, bool MoveNoexcept>
void
swap (probe_t<Name, CopyNoexcept, MoveNoexcept> &lhs,
      probe_t<Name, CopyNoexcept, MoveNoexcept> &rhs)
{
  the_log ().push_back (std::string (1, Name) + ":swap("
                        + std::to_string (lhs.id) + ","
                        + std::to_string (rhs.id) + ")");
  std::swap (lhs.id, rhs.id);
}

// Nothing throws.
using v_nothrow_t = probe_t<'V', true, true>;
using e_nothrow_t = probe_t<'E', true, true>;
// The copy may throw, the move may not.
using v_copy_throws_t = probe_t<'V', false, true>;
using e_copy_throws_t = probe_t<'E', false, true>;
// Both the copy and the move may throw.
using v_all_throw_t = probe_t<'V', false, false>;
using e_all_throw_t = probe_t<'E', false, false>;

} // namespace expected_probe

#endif // !LUMEX_TESTS_CORE_EXPECTED_EXPECTED_PROBE_HPP
