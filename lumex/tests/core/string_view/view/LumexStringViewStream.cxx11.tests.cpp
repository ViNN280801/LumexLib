#include <atomic>
#include <cstddef>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>

#if defined(__linux__) && defined(__GLIBCXX__)
#include <pthread.h>
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "lumex/core/string_view/LumexStringView"

// operator<< of the views is a formatted output function, as the standard's:
// width, fill and adjustfield, the width reset to 0, the sentry, a failing
// buffer. Every case is run for a view and for the std::basic_string of the
// same characters, and the two streams must end up the same.

namespace
{
template <class CharT> struct kinds;

template <> struct kinds<char>
{
  typedef lumex_string_view view_t;
  typedef std::ostringstream stream_t;
  static std::string
  text (char const *narrow, std::size_t size)
  {
    return std::string (narrow, size);
  }
};

template <> struct kinds<wchar_t>
{
  typedef lumex_wstring_view view_t;
  typedef std::wostringstream stream_t;
  static std::wstring
  text (char const *narrow, std::size_t size)
  {
    std::wstring result;
    for (std::size_t i = 0; i < size; ++i)
      result.push_back (
          static_cast<wchar_t> (static_cast<unsigned char> (narrow[i])));
    return result;
  }
};

/// A buffer that accepts `limit` characters and then fails.
template <class CharT> class limited_buf : public std::basic_streambuf<CharT>
{
public:
  typedef typename std::basic_streambuf<CharT>::int_type int_type;
  explicit limited_buf (std::size_t limit, bool throws = false)
      : m_written (), m_limit (limit), m_throws (throws)
  {
  }
  std::basic_string<CharT> m_written;

protected:
  int_type
  overflow (int_type chr) override
  {
    if (m_written.size () >= m_limit)
      {
        if (m_throws)
          throw std::runtime_error ("buffer");
        return std::basic_streambuf<CharT>::traits_type::eof ();
      }
    m_written.push_back (
        std::basic_streambuf<CharT>::traits_type::to_char_type (chr));
    return chr;
  }
  std::streamsize
  xsputn (CharT const *str, std::streamsize count) override
  {
    std::streamsize done = 0;
    while (done < count && m_written.size () < m_limit)
      m_written.push_back (str[done++]);
    if (done < count && m_throws)
      throw std::runtime_error ("buffer");
    return done;
  }

private:
  std::size_t m_limit;
  bool m_throws;
};

template <class CharT>
void
compare_formatting (char const *narrow, std::size_t size)
{
  typedef typename kinds<CharT>::view_t view_t;
  typedef typename kinds<CharT>::stream_t stream_t;
  std::basic_string<CharT> const text = kinds<CharT>::text (narrow, size);
  int const widths[] = { 0, 1, 3, 8, 20 };
  CharT const fills[] = { static_cast<CharT> (' '), static_cast<CharT> ('*') };
  int mismatches = 0;
  for (std::size_t w = 0; w < 5; ++w)
    for (std::size_t f = 0; f < 2; ++f)
      for (int adjust = 0; adjust < 4; ++adjust)
        {
          stream_t viewed;
          stream_t reference;
          std::ios_base::fmtflags const adjusts[]
              = { std::ios_base::left, std::ios_base::right,
                  std::ios_base::internal, std::ios_base::fmtflags () };
          viewed << '[' << std::setw (widths[w]) << std::setfill (fills[f]);
          reference << '[' << std::setw (widths[w]) << std::setfill (fills[f]);
          viewed.setf (adjusts[adjust], std::ios_base::adjustfield);
          reference.setf (adjusts[adjust], std::ios_base::adjustfield);
          viewed << view_t (text.data (), text.size ());
          reference << text;
          viewed << ']' << view_t (text.data (), text.size ());
          reference << ']' << text;
          bool const ok = viewed.str () == reference.str ()
                          && viewed.width () == reference.width ()
                          && viewed.rdstate () == reference.rdstate ();
          if (!ok && ++mismatches < 10)
            ADD_FAILURE () << "width " << widths[w] << ", fill " << f
                           << ", adjust " << adjust;
        }
  EXPECT_EQ (mismatches, 0);
}
} // namespace

TEST (LumexStringViewStreamTest,
      GivenSetwSetfill_WhenInsertNarrow_ThenPaddedLikeAString)
{
  std::ostringstream out;
  out << '[' << std::setw (8) << std::setfill ('*')
      << lumex_string_view ("abc") << ']';
  EXPECT_EQ (out.str (), "[*****abc]");
}

TEST (LumexStringViewStreamTest,
      GivenSetwSetfill_WhenInsertWide_ThenPaddedLikeAString)
{
  std::wostringstream out;
  out << L'[' << std::setw (8) << std::setfill (L'*')
      << lumex_wstring_view (L"abc") << L']';
  EXPECT_EQ (out.str (), L"[*****abc]");
}

TEST (LumexStringViewStreamTest,
      GivenLeftAdjust_WhenInsert_ThenPaddedOnTheRight)
{
  std::ostringstream out;
  out << '[' << std::left << std::setw (6) << std::setfill ('.')
      << lumex_string_view ("ab") << ']';
  EXPECT_EQ (out.str (), "[ab....]");
}

TEST (LumexStringViewStreamTest,
      GivenRightAdjust_WhenInsert_ThenPaddedOnTheLeft)
{
  std::ostringstream out;
  out << '[' << std::right << std::setw (6) << std::setfill ('.')
      << lumex_string_view ("ab") << ']';
  EXPECT_EQ (out.str (), "[....ab]");
}

TEST (LumexStringViewStreamTest,
      GivenWidth_WhenInsertTwice_ThenWidthAppliesOnce)
{
  std::ostringstream out;
  out << std::setw (5) << lumex_string_view ("a") << lumex_string_view ("b");
  EXPECT_EQ (out.str (), "    ab");
  EXPECT_EQ (out.width (), 0);
}

TEST (LumexStringViewStreamTest,
      GivenWidthSmallerThanText_WhenInsert_ThenNoPadding)
{
  std::ostringstream out;
  out << std::setw (2) << lumex_string_view ("abcd") << std::setw (4)
      << lumex_string_view ("abcd") << std::setw (5)
      << lumex_string_view ("abcd");
  EXPECT_EQ (out.str (), "abcdabcd abcd");
  EXPECT_EQ (out.width (), 0);
}

TEST (LumexStringViewStreamTest, GivenFill_WhenInsert_ThenFillCharacterIsUsed)
{
  std::ostringstream out;
  out << std::setfill ('#') << std::setw (4) << lumex_string_view ("a");
  EXPECT_EQ (out.str (), "###a");
  EXPECT_EQ (out.fill (), '#');
}

TEST (LumexStringViewStreamTest,
      GivenEmbeddedNul_WhenInsert_ThenWholeTextWritten)
{
  std::ostringstream out;
  out << lumex_string_view ("a\0b", 3);
  EXPECT_EQ (out.str (), std::string ("a\0b", 3));
}

TEST (LumexStringViewStreamTest, GivenEmptyViews_WhenInsert_ThenOnlyPadding)
{
  std::ostringstream out;
  out << std::setw (3) << std::setfill ('-') << lumex_string_view ()
      << std::setw (2) << lumex_string_view ("");
  EXPECT_EQ (out.str (), "-----");
  std::ostringstream plain;
  plain << lumex_string_view () << lumex_string_view (nullptr, 0);
  EXPECT_EQ (plain.str (), "");
  EXPECT_TRUE (plain.good ());
}

TEST (LumexStringViewStreamTest,
      GivenChain_WhenInsert_ThenReturnsTheSameStream)
{
  std::ostringstream out;
  std::ostream &result = (out << lumex_string_view ("x"));
  EXPECT_EQ (&result, &out);
  std::wostringstream wide;
  std::wostream &wide_result = (wide << lumex_wstring_view (L"x"));
  EXPECT_EQ (&wide_result, &wide);
}

TEST (LumexStringViewStreamTest,
      GivenFormattingMatrix_WhenInsertNarrow_ThenSameAsString)
{
  compare_formatting<char> ("", 0);
  compare_formatting<char> ("ab", 2);
  compare_formatting<char> ("abcdefghij", 10);
  compare_formatting<char> ("a\0b", 3);
}

TEST (LumexStringViewStreamTest,
      GivenFormattingMatrix_WhenInsertWide_ThenSameAsString)
{
  compare_formatting<wchar_t> ("", 0);
  compare_formatting<wchar_t> ("ab", 2);
  compare_formatting<wchar_t> ("abcdefghij", 10);
  compare_formatting<wchar_t> ("a\0b", 3);
}

TEST (LumexStringViewStreamTest,
      GivenFailedStream_WhenInsert_ThenNothingWritten)
{
  std::ostringstream out;
  out.setstate (std::ios_base::badbit);
  out << std::setw (6) << lumex_string_view ("abc");
  EXPECT_EQ (out.str (), "");
  EXPECT_TRUE (out.bad ());
}

TEST (LumexStringViewStreamTest,
      GivenFailingBuffer_WhenInsert_ThenBadbitAndTheAcceptedPrefix)
{
  // The field is "***abc" (right adjust) or "abc***" (left adjust); a buffer
  // that accepts `limit` characters gets that prefix, and the stream the
  // badbit when the field does not fit. (Not compared with std::string: the
  // standard libraries differ in the other bits and the width after a
  // failure, and libc++ crashes on a left-adjusted insertion into a buffer
  // that fails.)
  for (int left = 0; left < 2; ++left)
    for (std::size_t limit = 0; limit <= 8; ++limit)
      {
        limited_buf<char> buffer (limit);
        std::ostream out (&buffer);
        out << (left != 0 ? std::left : std::right) << std::setw (6)
            << std::setfill ('*') << lumex_string_view ("abc");
        std::string const field = left != 0 ? "abc***" : "***abc";
        EXPECT_EQ (buffer.m_written, field.substr (0, limit < 6 ? limit : 6))
            << "limit " << limit << ", left " << left;
        EXPECT_EQ (out.bad (), limit < 6)
            << "limit " << limit << ", left " << left;
      }
}

TEST (LumexStringViewStreamTest,
      GivenThrowingBuffer_WhenInsert_ThenBadbitAndNoThrow)
{
  limited_buf<char> buffer (1, true);
  std::ostream out (&buffer);
  EXPECT_NO_THROW (out << lumex_string_view ("abc"));
  EXPECT_TRUE (out.bad ());
}

TEST (LumexStringViewStreamTest,
      GivenThrowingBufferAndBadbitMask_WhenInsert_ThenTheExceptionPropagates)
{
  limited_buf<char> buffer (1, true);
  std::ostream out (&buffer);
  out.exceptions (std::ios_base::badbit);
  EXPECT_THROW (out << lumex_string_view ("abc"), std::runtime_error);
  EXPECT_TRUE (out.bad ());
}

TEST (LumexStringViewStreamTest,
      GivenFailingBufferAndBadbitMask_WhenInsert_ThenIosFailure)
{
  limited_buf<char> buffer (1);
  std::ostream out (&buffer);
  out.exceptions (std::ios_base::badbit);
  EXPECT_THROW (out << lumex_string_view ("abc"), std::ios_base::failure);
}

#if defined(__linux__) && defined(__GLIBCXX__)
// pthread_cancel unwinds the cancelled thread with an exception that must not
// be swallowed (abi::__forced_unwind): an inserter that catches everything and
// does not rethrow makes glibc abort the thread's exit.
namespace
{
std::atomic<bool> g_in_buffer (false);
std::atomic<bool> g_unwound (false);
std::atomic<bool> g_continued (false);

class blocking_buf : public std::streambuf
{
protected:
  std::streamsize
  xsputn (char const *, std::streamsize) override
  {
    g_in_buffer = true;
    for (;;)
      ::sleep (1); // a cancellation point
  }
};

struct unwind_probe
{
  ~unwind_probe () { g_unwound = true; }
};

void *
insert_and_block (void *)
{
  unwind_probe probe;
  blocking_buf buffer;
  std::ostream out (&buffer);
  out << lumex_string_view ("abc");
  g_continued = true;
  return nullptr;
}
} // namespace

TEST (LumexStringViewStreamTest,
      GivenCancelledThread_WhenInsert_ThenTheUnwindIsNotSwallowed)
{
  pthread_t thread;
  ASSERT_EQ (pthread_create (&thread, nullptr, &insert_and_block, nullptr), 0);
  while (!g_in_buffer)
    ::usleep (1000);
  ASSERT_EQ (pthread_cancel (thread), 0);
  void *result = nullptr;
  ASSERT_EQ (pthread_join (thread, &result), 0);
  EXPECT_EQ (result, PTHREAD_CANCELED);
  EXPECT_TRUE (g_unwound) << "the stack was unwound";
  EXPECT_FALSE (g_continued) << "the insertion did not return normally";
}
#endif
