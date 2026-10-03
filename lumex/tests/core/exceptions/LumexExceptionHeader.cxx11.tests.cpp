// LumexExceptionHeader.cxx11.tests.cpp
// LumexException.hpp on its own: this translation unit includes no other
// Lumex header, so the macros below compile only while the header includes
// what they expand to (SET_SEH_TRANSLATOR of WindowsSEHTranslator.hpp in
// LUMEX_EXCEPTION_HANDLE_BEGIN, std::cerr in LUMEX_EXCEPTION_HANDLE_END,
// lumDemangle in LUMEX_THROW_EXCEPTION).
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>

#include <gtest/gtest.h>

#include "lumex/core/exceptions/exception/LumexException.hpp"

namespace
{
LUMEX_DEFINE_EXCEPTION (HeaderOnlyException, LumexBaseException);

// Redirects std::cerr for the lifetime of the object.
class HeaderCerrCapture
{
public:
  HeaderCerrCapture () : m_old (std::cerr.rdbuf (m_buffer.rdbuf ())) {}

  ~HeaderCerrCapture () { std::cerr.rdbuf (m_old); }

  HeaderCerrCapture (HeaderCerrCapture const &) = delete;
  HeaderCerrCapture &operator= (HeaderCerrCapture const &) = delete;

  std::string
  text () const
  {
    return m_buffer.str ();
  }

private:
  std::ostringstream m_buffer;
  std::streambuf *m_old;
};
} // namespace

TEST (LumexExceptionHeaderTest,
      GivenOnlyTheHeader_WhenTheHandledBlockDoesNotThrow_ThenItRunsOnce)
{
  // Arrange
  int runs = 0;

  // Act
  LUMEX_EXCEPTION_HANDLE_BEGIN
  ++runs;
  LUMEX_EXCEPTION_HANDLE_END

  // Assert
  EXPECT_EQ (1, runs);
}

TEST (
    LumexExceptionHeaderTest,
    GivenOnlyTheHeader_WhenTheHandledBlockThrowsAnInt_ThenTheHandlerReportsIt)
{
  // Arrange
  HeaderCerrCapture capture;

  // Act
  LUMEX_EXCEPTION_HANDLE_BEGIN
  throw 42;
  LUMEX_EXCEPTION_HANDLE_END

  // Assert
  EXPECT_NE (std::string::npos, capture.text ().find ("[Unknown exception]"))
      << capture.text ();
}

TEST (LumexExceptionHeaderTest,
      GivenOnlyTheHeader_WhenThrowExceptionRuns_ThenTheMessageNamesTheType)
{
  try
    {
      LUMEX_THROW_EXCEPTION (HeaderOnlyException, "header only")
      FAIL () << "LUMEX_THROW_EXCEPTION did not throw";
    }
  catch (HeaderOnlyException const &ex)
    {
      std::string const what = ex.what ();
      EXPECT_NE (std::string::npos, what.find ("HeaderOnlyException")) << what;
      EXPECT_NE (std::string::npos, what.find (": header only")) << what;
    }
}
