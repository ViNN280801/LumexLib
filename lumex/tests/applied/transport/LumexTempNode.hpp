// A file in the temporary directory with a chosen mode, removed by the
// destructor even when an ASSERT_* leaves the test early. It stands in for a
// device node in the permission tests (POSIX only).

#ifndef LUMEX_TESTS_APPLIED_TRANSPORT_TEMP_NODE_HPP
#define LUMEX_TESTS_APPLIED_TRANSPORT_TEMP_NODE_HPP

#if !defined(_WIN32)

#include <cstdlib>
#include <string>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace lumex_transport_tests
{
class temp_node
{
public:
  /// Creates the file and sets `mode`; `path ()` is empty when that failed.
  explicit temp_node (mode_t mode)
  {
    char const *const dir = std::getenv ("TMPDIR");
    path_ = std::string (dir != nullptr && dir[0] != '\0' ? dir : "/tmp")
            + "/lumex_perm_XXXXXX";
    int const fd = ::mkstemp (&path_[0]);
    if (fd < 0)
      {
        path_.clear ();
        return;
      }
    ::close (fd);
    if (::chmod (path_.c_str (), mode) != 0)
      {
        ::unlink (path_.c_str ());
        path_.clear ();
      }
  }

  ~temp_node ()
  {
    if (!path_.empty ())
      ::unlink (path_.c_str ());
  }

  temp_node (temp_node const &) = delete;
  temp_node &operator= (temp_node const &) = delete;

  std::string const &
  path () const
  {
    return path_;
  }

private:
  std::string path_;
};
} // namespace lumex_transport_tests

#endif // !_WIN32

#endif // !LUMEX_TESTS_APPLIED_TRANSPORT_TEMP_NODE_HPP
