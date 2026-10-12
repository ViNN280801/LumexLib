#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "lumex/core/contracts/LumexContracts"

namespace contracts = lumex::core::contracts;

namespace
{
// A fixed-capacity stack whose conditions are contract assertions: the class
// states what it requires of its callers and what it keeps true.
template <typename T> class bounded_stack
{
public:
  explicit bounded_stack (std::size_t capacity) : m_capacity (capacity)
  {
    LUMEX_CONTRACT_ASSERT_ENFORCE (capacity > 0);
    m_items.reserve (capacity);
  }

  void
  push (T value)
  {
    // Precondition: there is room.
    LUMEX_CONTRACT_ASSERT_ENFORCE (m_items.size () < m_capacity);
    m_items.push_back (value);
    // The invariant is cheap here, but checking it is a choice of the build.
    LUMEX_CONTRACT_ASSERT (m_items.size () <= m_capacity);
  }

  T
  pop ()
  {
    LUMEX_CONTRACT_ASSERT_ENFORCE (!m_items.empty ());
    T value = m_items.back ();
    m_items.pop_back ();
    return value;
  }

  std::size_t
  size () const
  {
    return m_items.size ();
  }

private:
  std::size_t m_capacity;
  std::vector<T> m_items;
};

struct report_t
{
  std::vector<std::string> comments;
};

report_t g_report;

// Production style: record the violation and let the program go on.
void
log_violation (contracts::contract_violation const &violation)
{
  g_report.comments.push_back (std::string (violation.comment ()) + " @ "
                               + violation.location ().function_name ());
}

// Test style: a violation becomes an exception the test can expect.
void
throw_violation (contracts::contract_violation const &violation)
{
  throw std::logic_error (std::string ("contract violated: ")
                          + violation.comment ());
}

// Applies the same operations to a stack under whatever handler is installed.
std::size_t
overfill (bounded_stack<int> &stack, int count)
{
  std::size_t pushed = 0;
  for (int index = 0; index < count; ++index)
    {
      stack.push (index);
      ++pushed;
    }
  return pushed;
}
} // namespace

int
main ()
{
  std::cout << "=== contracts: preconditions of a container ===\n\n";

  std::cout << "--- 1. Correct use is silent ---\n";
  {
    bounded_stack<int> stack (3);
    overfill (stack, 3);
    std::cout << "size after 3 pushes: " << stack.size () << '\n';
    std::cout << "top: " << stack.pop () << '\n';
  }

  std::cout << "\n--- 2. In a test, a violation is an exception ---\n";
  {
    contracts::scoped_violation_handler const scope (&throw_violation);
    bounded_stack<int> stack (2);
    try
      {
        overfill (stack, 3);
        std::cout << "no violation?\n";
        return 1;
      }
    catch (std::logic_error const &error)
      {
        std::cout << error.what () << '\n';
      }
    try
      {
        bounded_stack<int> empty (1);
        empty.pop ();
        return 1;
      }
    catch (std::logic_error const &error)
      {
        std::cout << error.what () << '\n';
      }
  }

  std::cout << "\n--- 3. A report of the handler of the program ---\n";
  {
    // observe keeps going after the report; enforce would end the program once
    // the handler returned. The explicit-semantic macro shows the difference
    // without ending this example.
    contracts::scoped_violation_handler const scope (&log_violation);
    int level = 7;
    LUMEX_CONTRACT_ASSERT_OBSERVE (level < 5);
    LUMEX_CONTRACT_ASSERT_OBSERVE (level > 0);
    for (std::string const &entry : g_report.comments)
      std::cout << "logged: " << entry << '\n';
    std::cout << "entries: " << g_report.comments.size () << '\n';
  }

  std::cout << "\nOK\n";
  return 0;
}
