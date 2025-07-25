#include "lumex/core/string_view/LumexStringView"
#include <gtest/gtest.h>

#include <algorithm>
#include <cwchar>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Helper struct to track lifetimes for testing
struct WideLifetimeTracker {
  static int creations;
  static int destructions;

  int id;

  static void
  reset()
  {
    creations    = 0;
    destructions = 0;
  }

  WideLifetimeTracker() : id(creations) { creations++; }
  ~WideLifetimeTracker() { destructions++; }

  WideLifetimeTracker(WideLifetimeTracker const &) : id(creations) { creations++; }

  WideLifetimeTracker &operator=(WideLifetimeTracker const &) = default;
};

int WideLifetimeTracker::creations    = 0;
int WideLifetimeTracker::destructions = 0;

// Test fixture for basic tests
class LumexWStringViewTest : public ::testing::Test
{
protected:
  void
  SetUp() override
  {
    test_wstring    = L"Hello, World!";
    empty_wstring   = L"";
    long_wstring    = std::wstring(1000, L'A');
    unicode_wstring = L"Héllo Wörld 🌍";
    null_terminated = std::wstring(L"test\0hidden", 11);
  }

  std::wstring test_wstring;
  std::wstring empty_wstring;
  std::wstring long_wstring;
  std::wstring unicode_wstring;
  std::wstring null_terminated;
};

// Test fixture for lifetime tests
class LumexWStringViewLifetimeTest : public ::testing::Test
{
protected:
  void
  SetUp() override
  {
    WideLifetimeTracker::reset();
  }

  void
  TearDown() override
  {
    ASSERT_EQ(WideLifetimeTracker::creations, WideLifetimeTracker::destructions)
      << "Memory leak detected! Creations do not match destructions.";
  }
};

// --- Constructor Tests

TEST_F(LumexWStringViewTest, DefaultConstruction)
{
  LumexWStringView wsv;

  EXPECT_EQ(wsv.size(), 0);
  EXPECT_EQ(wsv.length(), 0);
  EXPECT_TRUE(wsv.empty());
  EXPECT_EQ(wsv.data(), nullptr);
  EXPECT_EQ(wsv.begin(), wsv.end());
}

TEST_F(LumexWStringViewTest, ValidCWStringConstruction)
{
  wchar_t const *cwstr = L"Hello, World!";
  LumexWStringView wsv(cwstr);

  EXPECT_EQ(wsv.size(), std::wcslen(cwstr));
  EXPECT_EQ(wsv.data(), cwstr);
  EXPECT_FALSE(wsv.empty());
  EXPECT_EQ(std::wstring(wsv.data(), wsv.size()), cwstr);
}

TEST_F(LumexWStringViewTest, NullptrConstruction)
{
  LumexWStringView wsv(nullptr);

  EXPECT_EQ(wsv.size(), 0);
  EXPECT_TRUE(wsv.empty());
  EXPECT_EQ(wsv.data(), nullptr);
}

TEST_F(LumexWStringViewTest, PointerLengthConstruction)
{
  wchar_t const *wstr = L"Hello, World!";
  std::size_t len     = 5; // Only "Hello"
  LumexWStringView wsv(wstr, len);

  EXPECT_EQ(wsv.size(), len);
  EXPECT_EQ(wsv.data(), wstr);
  EXPECT_EQ(std::wstring(wsv.data(), wsv.size()), L"Hello");
}

TEST_F(LumexWStringViewTest, ZeroLengthWithValidPointer)
{
  wchar_t const *wstr = L"Not empty";
  LumexWStringView wsv(wstr, 0);

  EXPECT_EQ(wsv.size(), 0);
  EXPECT_TRUE(wsv.empty());
  EXPECT_EQ(wsv.data(), wstr);
}

TEST_F(LumexWStringViewTest, StdWStringConstruction)
{
  LumexWStringView wsv(test_wstring);

  EXPECT_EQ(wsv.size(), test_wstring.size());
  EXPECT_EQ(wsv.data(), test_wstring.data());
  EXPECT_EQ(std::wstring(wsv.data(), wsv.size()), test_wstring);
}

TEST_F(LumexWStringViewTest, EmptyStdWStringConstruction)
{
  LumexWStringView wsv(empty_wstring);

  EXPECT_TRUE(wsv.empty());
  EXPECT_EQ(wsv.size(), 0);
}

// --- Copy and Assignment Tests

TEST_F(LumexWStringViewTest, CopyConstruction)
{
  LumexWStringView original(test_wstring);
  LumexWStringView copy(original);

  EXPECT_EQ(copy.size(), original.size());
  EXPECT_EQ(copy.data(), original.data());
  EXPECT_EQ(copy, original);
}

TEST_F(LumexWStringViewTest, Assignment)
{
  LumexWStringView wsv1(test_wstring);
  LumexWStringView wsv2(L"Different");

  wsv2 = wsv1;

  EXPECT_EQ(wsv2.size(), wsv1.size());
  EXPECT_EQ(wsv2.data(), wsv1.data());
  EXPECT_EQ(wsv2, wsv1);
}

// --- Iterator Tests

TEST_F(LumexWStringViewTest, ForwardIteration)
{
  LumexWStringView wsv(test_wstring);

  std::wstring reconstructed;
  for(auto it = wsv.begin(); it != wsv.end(); ++it) reconstructed += *it;

  EXPECT_EQ(reconstructed, test_wstring);
  EXPECT_EQ(wsv.cbegin(), wsv.begin());
  EXPECT_EQ(wsv.cend(), wsv.end());
}

TEST_F(LumexWStringViewTest, ReverseIteration)
{
  LumexWStringView wsv(L"abc");

  std::wstring reversed;
  for(auto it = wsv.rbegin(); it != wsv.rend(); ++it) reversed += *it;

  EXPECT_EQ(reversed, L"cba");
  EXPECT_EQ(wsv.crbegin(), wsv.rbegin());
  EXPECT_EQ(wsv.crend(), wsv.rend());
}

TEST_F(LumexWStringViewTest, EmptyWStringIteration)
{
  LumexWStringView wsv;

  EXPECT_EQ(wsv.begin(), wsv.end());
  EXPECT_EQ(wsv.rbegin(), wsv.rend());

  int count = 0;
  for(auto it = wsv.begin(); it != wsv.end(); ++it) ++count;
  EXPECT_EQ(count, 0);
}

// --- Capacity Tests

TEST_F(LumexWStringViewTest, CapacityMethods)
{
  LumexWStringView wsv(test_wstring);

  EXPECT_EQ(wsv.size(), test_wstring.size());
  EXPECT_EQ(wsv.length(), test_wstring.size());
  EXPECT_FALSE(wsv.empty());
  EXPECT_GT(wsv.max_size(), 0);
  EXPECT_GE(wsv.max_size(), wsv.size());
}

TEST_F(LumexWStringViewTest, EmptyCapacity)
{
  LumexWStringView wsv;

  EXPECT_EQ(wsv.size(), 0);
  EXPECT_EQ(wsv.length(), 0);
  EXPECT_TRUE(wsv.empty());
}

TEST_F(LumexWStringViewTest, LargeWStringCapacity)
{
  LumexWStringView wsv(long_wstring);

  EXPECT_EQ(wsv.size(), long_wstring.size());
  EXPECT_FALSE(wsv.empty());
  EXPECT_LE(wsv.size(), wsv.max_size());
}

// --- Element Access Tests

TEST_F(LumexWStringViewTest, ElementAccess)
{
  LumexWStringView wsv(L"Hello");

  EXPECT_EQ(wsv[0], L'H');
  EXPECT_EQ(wsv[1], L'e');
  EXPECT_EQ(wsv[4], L'o');
  EXPECT_EQ(wsv.front(), L'H');
  EXPECT_EQ(wsv.back(), L'o');
}

TEST_F(LumexWStringViewTest, AtMethodValid)
{
  LumexWStringView wsv(L"Test");

  EXPECT_EQ(wsv.at(0), L'T');
  EXPECT_EQ(wsv.at(3), L't');
}

TEST_F(LumexWStringViewTest, AtMethodInvalid)
{
  LumexWStringView wsv(L"Test");

  EXPECT_THROW(wsv.at(4), std::out_of_range);
  EXPECT_THROW(wsv.at(100), std::out_of_range);
}

TEST_F(LumexWStringViewTest, EmptyWStringElementAccess)
{
  LumexWStringView wsv;

  EXPECT_THROW(wsv.at(0), std::out_of_range);
}

TEST_F(LumexWStringViewTest, DataMethod)
{
  wchar_t const *wstr = L"DataTest";
  LumexWStringView wsv(wstr);

  EXPECT_EQ(wsv.data(), wstr);
  EXPECT_NE(wsv.data(), nullptr);
}

// --- Modifier Tests

TEST_F(LumexWStringViewTest, Clear)
{
  LumexWStringView wsv(test_wstring);
  ASSERT_FALSE(wsv.empty());

  wsv.clear();

  EXPECT_TRUE(wsv.empty());
  EXPECT_EQ(wsv.size(), 0);
  EXPECT_EQ(wsv.data(), nullptr);
}

TEST_F(LumexWStringViewTest, RemovePrefix)
{
  LumexWStringView wsv(L"Hello, World!");
  wchar_t const *original_data = wsv.data();

  wsv.remove_prefix(7); // Remove "Hello, "

  EXPECT_EQ(wsv.size(), 6);
  EXPECT_EQ(wsv.data(), original_data + 7);
  EXPECT_EQ(std::wstring(wsv.data(), wsv.size()), L"World!");
}

TEST_F(LumexWStringViewTest, RemovePrefixTooMuch)
{
  LumexWStringView wsv(L"Short");

  wsv.remove_prefix(100);

  EXPECT_TRUE(wsv.empty());
  EXPECT_EQ(wsv.size(), 0);
}

TEST_F(LumexWStringViewTest, RemoveSuffix)
{
  LumexWStringView wsv(L"Hello, World!");
  wchar_t const *original_data = wsv.data();

  wsv.remove_suffix(8); // Remove ", World!"

  EXPECT_EQ(wsv.size(), 5);
  EXPECT_EQ(wsv.data(), original_data);
  EXPECT_EQ(std::wstring(wsv.data(), wsv.size()), L"Hello");
}

TEST_F(LumexWStringViewTest, RemoveSuffixTooMuch)
{
  LumexWStringView wsv(L"Short");

  wsv.remove_suffix(100);

  EXPECT_TRUE(wsv.empty());
  EXPECT_EQ(wsv.size(), 0);
}

TEST_F(LumexWStringViewTest, Swap)
{
  LumexWStringView wsv1(L"First");
  LumexWStringView wsv2(L"Second");
  wchar_t const *data1 = wsv1.data();
  wchar_t const *data2 = wsv2.data();

  wsv1.swap(wsv2);

  EXPECT_EQ(wsv1.data(), data2);
  EXPECT_EQ(wsv2.data(), data1);
  EXPECT_EQ(std::wstring(wsv1.data(), wsv1.size()), L"Second");
  EXPECT_EQ(std::wstring(wsv2.data(), wsv2.size()), L"First");
}

// --- Copy Method Tests

TEST_F(LumexWStringViewTest, CopyMethod)
{
  LumexWStringView wsv(L"Hello, World!");
  wchar_t buffer[20] = {0};

  std::size_t copied = wsv.copy(buffer, 5, 7); // Copy "World" starting at pos 7

  EXPECT_EQ(copied, 5);
  EXPECT_EQ(std::wstring(buffer, copied), L"World");
}

TEST_F(LumexWStringViewTest, CopyBeyondEnd)
{
  LumexWStringView wsv(L"Test");
  wchar_t buffer[10] = {0};

  std::size_t copied = wsv.copy(buffer, 10, 2); // Request 10 chars from pos 2

  EXPECT_EQ(copied, 2); // Only "st" available
  EXPECT_EQ(std::wstring(buffer, copied), L"st");
}

TEST_F(LumexWStringViewTest, CopyInvalidPos)
{
  LumexWStringView wsv(L"Test");
  wchar_t buffer[10];

  EXPECT_THROW(wsv.copy(buffer, 5, 10), std::out_of_range);
}

// --- Substring Tests

TEST_F(LumexWStringViewTest, Substr)
{
  LumexWStringView wsv(L"Hello, World!");

  LumexWStringView sub = wsv.substr(7, 5); // "World"

  EXPECT_EQ(sub.size(), 5);
  EXPECT_EQ(std::wstring(sub.data(), sub.size()), L"World");
  EXPECT_EQ(sub.data(), wsv.data() + 7);
}

TEST_F(LumexWStringViewTest, SubstrToEnd)
{
  LumexWStringView wsv(L"Hello, World!");

  LumexWStringView sub = wsv.substr(7); // From pos 7 to end

  EXPECT_EQ(std::wstring(sub.data(), sub.size()), L"World!");
}

TEST_F(LumexWStringViewTest, SubstrBeyondLength)
{
  LumexWStringView wsv(L"Short");

  LumexWStringView sub = wsv.substr(2, 100); // Request more than available

  EXPECT_EQ(std::wstring(sub.data(), sub.size()), L"ort");
}

TEST_F(LumexWStringViewTest, SubstrInvalidPos)
{
  LumexWStringView wsv(L"Test");

  EXPECT_THROW(wsv.substr(10), std::out_of_range);
}

// --- Comparison Tests

TEST_F(LumexWStringViewTest, CompareEqual)
{
  LumexWStringView wsv1(L"Hello");
  LumexWStringView wsv2(L"Hello");

  EXPECT_EQ(wsv1.compare(wsv2), 0);
  EXPECT_TRUE(wsv1 == wsv2);
  EXPECT_FALSE(wsv1 != wsv2);
  EXPECT_FALSE(wsv1 < wsv2);
  EXPECT_FALSE(wsv1 > wsv2);
  EXPECT_TRUE(wsv1 <= wsv2);
  EXPECT_TRUE(wsv1 >= wsv2);
}

TEST_F(LumexWStringViewTest, CompareDifferent)
{
  LumexWStringView wsv1(L"abc");
  LumexWStringView wsv2(L"def");

  EXPECT_LT(wsv1.compare(wsv2), 0);
  EXPECT_GT(wsv2.compare(wsv1), 0);
  EXPECT_TRUE(wsv1 < wsv2);
  EXPECT_TRUE(wsv2 > wsv1);
  EXPECT_TRUE(wsv1 != wsv2);
}

TEST_F(LumexWStringViewTest, CompareDifferentLengths)
{
  LumexWStringView wsv1(L"abc");
  LumexWStringView wsv2(L"abcd");

  EXPECT_LT(wsv1.compare(wsv2), 0);
  EXPECT_TRUE(wsv1 < wsv2);
}

TEST_F(LumexWStringViewTest, CompareWithCWString)
{
  LumexWStringView wsv(L"Hello");

  EXPECT_EQ(wsv.compare(L"Hello"), 0);
  EXPECT_LT(wsv.compare(L"World"), 0);
  EXPECT_GT(wsv.compare(L"ABC"), 0);
}

TEST_F(LumexWStringViewTest, PartialCompare)
{
  LumexWStringView wsv(L"Hello, World!");
  LumexWStringView target(L"World");

  EXPECT_EQ(wsv.compare(7, 5, target), 0); // Compare "World" portion
}

// --- Starts/Ends With Tests

TEST_F(LumexWStringViewTest, StartsWith)
{
  LumexWStringView wsv(L"Hello, World!");

  EXPECT_TRUE(wsv.starts_with(L'H'));
  EXPECT_FALSE(wsv.starts_with(L'W'));
  EXPECT_TRUE(wsv.starts_with(LumexWStringView(L"Hello")));
  EXPECT_FALSE(wsv.starts_with(LumexWStringView(L"World")));
}

TEST_F(LumexWStringViewTest, EndsWith)
{
  LumexWStringView wsv(L"Hello, World!");

  EXPECT_TRUE(wsv.ends_with(L'!'));
  EXPECT_FALSE(wsv.ends_with(L'H'));
  EXPECT_TRUE(wsv.ends_with(LumexWStringView(L"World!")));
  EXPECT_FALSE(wsv.ends_with(LumexWStringView(L"Hello")));
}

TEST_F(LumexWStringViewTest, EmptyWStringStartsEndsWith)
{
  LumexWStringView wsv;

  EXPECT_FALSE(wsv.starts_with(L'A'));
  EXPECT_FALSE(wsv.ends_with(L'A'));
  EXPECT_FALSE(wsv.starts_with(LumexWStringView(L"test")));
  EXPECT_FALSE(wsv.ends_with(LumexWStringView(L"test")));
}

TEST_F(LumexWStringViewTest, StartsEndsWithLongerWString)
{
  LumexWStringView wsv(L"Hi");
  LumexWStringView longer(L"Hello");

  EXPECT_FALSE(wsv.starts_with(longer));
  EXPECT_FALSE(wsv.ends_with(longer));
}

// --- Find Character Tests

TEST_F(LumexWStringViewTest, FindWChar)
{
  LumexWStringView wsv(L"Hello, World!");

  EXPECT_EQ(wsv.find(L'H'), 0);
  EXPECT_EQ(wsv.find(L'o'), 4); // First 'o' in "Hello"
  EXPECT_EQ(wsv.find(L'!'), 12);
  EXPECT_EQ(wsv.find(L'X'), LumexWStringView::npos);
}

TEST_F(LumexWStringViewTest, FindWCharFromPosition)
{
  LumexWStringView wsv(L"Hello, World!");

  EXPECT_EQ(wsv.find(L'o', 5), 8); // Second 'o' in "World"
  EXPECT_EQ(wsv.find(L'H', 1), LumexWStringView::npos);
}

// --- Find WString Tests

TEST_F(LumexWStringViewTest, FindSubwstring)
{
  LumexWStringView wsv(L"Hello, World!");

  EXPECT_EQ(wsv.find(LumexWStringView(L"Hello")), 0);
  EXPECT_EQ(wsv.find(LumexWStringView(L"World")), 7);
  EXPECT_EQ(wsv.find(LumexWStringView(L"xyz")), LumexWStringView::npos);
}

TEST_F(LumexWStringViewTest, FindEmptyWString)
{
  LumexWStringView wsv(L"Hello");
  LumexWStringView empty;

  EXPECT_EQ(wsv.find(empty), 0); // Empty string found at any valid position
  EXPECT_EQ(wsv.find(empty, 3), 3);
  EXPECT_EQ(wsv.find(empty, 10), LumexWStringView::npos); // Beyond string
}

TEST_F(LumexWStringViewTest, FindCWString)
{
  LumexWStringView wsv(L"Hello, World!");

  EXPECT_EQ(wsv.find(L"World"), 7);
  EXPECT_EQ(wsv.find(L"xyz"), LumexWStringView::npos);
  EXPECT_EQ(wsv.find(L"Hello", 0, 2), 0); // Find "He" with count=2
}

// --- Reverse Find Tests

TEST_F(LumexWStringViewTest, RFind)
{
  LumexWStringView wsv(L"Hello, World!");
  LumexWStringView target(L"o");

  EXPECT_EQ(wsv.rfind(target), 8); // Last 'o' in "World"
  EXPECT_EQ(wsv.rfind(L'o'), 8);
}

TEST_F(LumexWStringViewTest, RFindNotFound)
{
  LumexWStringView wsv(L"Hello");

  EXPECT_EQ(wsv.rfind(L'X'), LumexWStringView::npos);
  EXPECT_EQ(wsv.rfind(LumexWStringView(L"xyz")), LumexWStringView::npos);
}

// --- Unicode and Special Character Tests

TEST_F(LumexWStringViewTest, UnicodeHandling)
{
  wchar_t const *unicode_wstr = L"Héllo 🌍"; // Multi-byte Unicode characters
  LumexWStringView wsv(unicode_wstr);

  EXPECT_EQ(wsv.size(), std::wcslen(unicode_wstr));
  EXPECT_EQ(wsv.data(), unicode_wstr);

  // Test Unicode character access
  EXPECT_EQ(wsv[0], L'H');
  EXPECT_EQ(wsv[1], L'é'); // Unicode character
}

TEST_F(LumexWStringViewTest, WStringWithNullBytes)
{
  wchar_t const data[] = {L'H', L'i', L'\0', L'B', L'y', L'e'};
  LumexWStringView wsv(data, sizeof(data) / sizeof(wchar_t));

  EXPECT_EQ(wsv.size(), 6);
  EXPECT_EQ(wsv[2], L'\0');
  EXPECT_EQ(wsv[3], L'B');
}

// --- Conversion Tests

TEST_F(LumexWStringViewTest, ToWStringConversion)
{
  LumexWStringView wsv(L"Hello");

  std::wstring converted = wsv.to_string();
  EXPECT_EQ(converted, L"Hello");

  // Test explicit conversion
  std::wstring explicit_conv = static_cast<std::wstring>(wsv);
  EXPECT_EQ(explicit_conv, L"Hello");
}

// --- Edge Case Tests

TEST_F(LumexWStringViewTest, SelfSwap)
{
  LumexWStringView wsv(L"test");
  wsv.swap(wsv);

  EXPECT_EQ(std::wstring(wsv.data(), wsv.size()), L"test");
}

TEST_F(LumexWStringViewTest, LargeWStringOperations)
{
  LumexWStringView wsv(long_wstring);

  // Test operations on large string
  EXPECT_EQ(wsv.find(L'A'), 0);
  EXPECT_EQ(wsv.find(L'A', 500), 500);
  EXPECT_TRUE(wsv.starts_with(L'A'));
  EXPECT_TRUE(wsv.ends_with(L'A'));
}

// --- Performance Tests

TEST_F(LumexWStringViewTest, PerformanceOperations)
{
  int const iterations = 1000;
  LumexWStringView wsv(test_wstring);

  // Test performance of common operations
  for(int i = 0; i < iterations; ++i)
  {
    EXPECT_NO_FATAL_FAILURE({
      wsv.find(L'o');
      wsv.substr(1, 5);
      wsv.starts_with(L'H');
      wsv.ends_with(L'!');
    });
  }
}

// --- Thread Safety Tests

TEST_F(LumexWStringViewTest, ThreadSafeReads)
{
  LumexWStringView wsv(test_wstring);
  std::vector<std::thread> threads;
  std::vector<bool> results(10, false);

  for(int i = 0; i < 10; ++i)
  {
    threads.emplace_back(
      [&wsv, &results, i]()
      {
        // Multiple threads reading simultaneously
        results[i] = (wsv.size() == 13 && wsv.find(L'H') == 0 && wsv.ends_with(L'!'));
      });
  }

  for(auto &thread : threads) thread.join();

  for(bool result : results) EXPECT_TRUE(result);
}

// --- Platform-Specific Tests

TEST_F(LumexWStringViewTest, PlatformSpecificWideChars)
{
  // Test platform-specific wide character behavior
  LumexWStringView wsv(L"Test");

  // wchar_t size varies by platform (2 bytes on Windows, 4 bytes on Linux/Mac)
  EXPECT_GT(sizeof(wchar_t), 0);
  EXPECT_EQ(wsv.size(), 4);

#ifdef _WIN32
  // Windows-specific wide character tests
  EXPECT_EQ(sizeof(wchar_t), 2);
#else
  // Unix/Linux-specific tests
  EXPECT_EQ(sizeof(wchar_t), 4);
#endif
}

// --- Find First/Last Of Tests

TEST_F(LumexWStringViewTest, FindFirstOf)
{
  LumexWStringView wsv(L"Hello, World!");
  LumexWStringView chars(L"aeiou");

  EXPECT_EQ(wsv.find_first_of(chars), 1); // 'e' in "Hello"
  EXPECT_EQ(wsv.find_first_of(L'o'), 4);
  EXPECT_EQ(wsv.find_first_of(L"xyz"), LumexWStringView::npos);
}

TEST_F(LumexWStringViewTest, FindLastOf)
{
  LumexWStringView wsv(L"Hello, World!");
  LumexWStringView chars(L"aeiou");

  EXPECT_EQ(wsv.find_last_of(chars), 8); // 'o' in "World"
  EXPECT_EQ(wsv.find_last_of(L'o'), 8);
}

TEST_F(LumexWStringViewTest, FindFirstNotOf)
{
  LumexWStringView wsv(L"aaeHello");
  LumexWStringView chars(L"ae");

  EXPECT_EQ(wsv.find_first_not_of(chars), 3); // 'H' at position 3
  EXPECT_EQ(wsv.find_first_not_of(L'a'), 2);  // 'e' at position 2
}

TEST_F(LumexWStringViewTest, FindLastNotOf)
{
  LumexWStringView wsv(L"Helloaaa");
  LumexWStringView chars(L"a");

  EXPECT_EQ(wsv.find_last_not_of(chars), 4); // 'o' at position 4
  EXPECT_EQ(wsv.find_last_not_of(L'a'), 4);
}
