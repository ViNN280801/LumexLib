#include "lumex/core/string/LumexString"
#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <codecvt>
#include <cstdint>
#include <iostream>
#include <limits>
#include <locale>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

// Platform-specific includes
#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <winnls.h>
  #ifdef min
    #undef min
  #endif
  #ifdef max
    #undef max
  #endif
#else
  #include <iconv.h>
  #include <langinfo.h>
#endif

// Test fixture for StringifyTests
class LumexStringifyTest : public ::testing::Test
{};

// === Helper Types and Structures ===

// Custom type with operator<<
struct CustomStreamable {
  int value;
  CustomStreamable(int v) : value(v) {}
  friend std::ostream &
  operator<<(std::ostream &os, CustomStreamable const &cs)
  {
    return os << "CustomStreamable(" << cs.value << ")";
  }
};

// Custom type WITHOUT operator<< (not streamable)
struct NonStreamable {
  int value;
  NonStreamable(int v) : value(v) {}
  // No operator<< defined
};

// Type with alignment requirements
struct alignas(32) AlignedType {
  int data[4];
  AlignedType() : data{1, 2, 3, 4} {}
  friend std::ostream &
  operator<<(std::ostream &os, AlignedType const &at)
  {
    return os << "AlignedType[" << at.data[0] << "," << at.data[1] << "," << at.data[2] << "," << at.data[3] << "]";
  }
};

// Type with padding
struct PaddedType {
  char c;
  int i;
  char c2;
  PaddedType(char c1, int i1, char c2_) : c(c1), i(i1), c2(c2_) {}
  friend std::ostream &
  operator<<(std::ostream &os, PaddedType const &pt)
  {
    return os << "PaddedType{" << pt.c << "," << pt.i << "," << pt.c2 << "}";
  }
};

// Type with volatile members
struct VolatileType {
  int volatile value;
  VolatileType(int v) : value(v) {}
  friend std::ostream &
  operator<<(std::ostream &os, VolatileType const &vt)
  {
    return os << "VolatileType(" << vt.value << ")";
  }
};

// Type with const members
struct ConstType {
  int const value;
  ConstType(int v) : value(v) {}
  friend std::ostream &
  operator<<(std::ostream &os, ConstType const &ct)
  {
    return os << "ConstType(" << ct.value << ")";
  }
};

// Complex type with multiple members
struct ComplexType {
  std::string name;
  std::vector<int> values;
  ComplexType(std::string const &n, std::vector<int> const &v) : name(n), values(v) {}
  friend std::ostream &
  operator<<(std::ostream &os, ComplexType const &ct)
  {
    os << "ComplexType{name:'" << ct.name << "',values:[";
    for(size_t i = 0; i < ct.values.size(); ++i)
    {
      if(i > 0) os << ",";
      os << ct.values[i];
    }
    return os << "]}";
  }
};

// Type that throws in operator<<
struct ThrowingStreamable {
  int value;
  ThrowingStreamable(int v) : value(v) {}
  friend std::ostream &
  operator<<(std::ostream &os, ThrowingStreamable const &ts)
  {
    if(ts.value == 666) throw std::runtime_error("Evil number detected!");
    return os << "ThrowingStreamable(" << ts.value << ")";
  }
};

// === Basic Type Tests ===

TEST_F(LumexStringifyTest, BasicTypes_Clean)
{
  // Integer types
  EXPECT_EQ(stringify(42), "42");
  EXPECT_EQ(stringify(-42), "-42");
  EXPECT_EQ(stringify(0), "0");

  // Unsigned types
  EXPECT_EQ(stringify(42u), "42");
  EXPECT_EQ(stringify(0u), "0");

  // Long types
  EXPECT_EQ(stringify(42L), "42");
  EXPECT_EQ(stringify(42LL), "42");
  EXPECT_EQ(stringify(42UL), "42");
  EXPECT_EQ(stringify(42ULL), "42");

  // Floating point
  EXPECT_EQ(stringify(3.14f), "3.14");
  EXPECT_EQ(stringify(3.14), "3.14");
  EXPECT_EQ(stringify(3.14L), "3.14");

  // Character types
  EXPECT_EQ(stringify('A'), "A");
  EXPECT_EQ(stringify('\n'), "\n");
  EXPECT_EQ(stringify('\0'), std::string(1, '\0'));

  // Boolean
  EXPECT_EQ(stringify(true), "1");
  EXPECT_EQ(stringify(false), "0");
}

TEST_F(LumexStringifyTest, StringTypes_Clean)
{
  // C-style strings
  EXPECT_EQ(stringify("hello"), "hello");
  EXPECT_EQ(stringify(""), "");

  // std::string
  std::string s = "test string";
  EXPECT_EQ(stringify(s), "test string");

  // Empty string
  std::string empty;
  EXPECT_EQ(stringify(empty), "");

  // String with special characters
  std::string special = "Hello\nWorld\t!";
  EXPECT_EQ(stringify(special), "Hello\nWorld\t!");
}

TEST_F(LumexStringifyTest, EmptyArguments_Clean)
{
  // No arguments
  EXPECT_EQ(stringify(), "");
}

TEST_F(LumexStringifyTest, MultipleArguments_Clean)
{
  // Multiple basic types
  EXPECT_EQ(stringify(1, 2, 3), "123");
  EXPECT_EQ(stringify("Hello", " ", "World"), "Hello World");
  EXPECT_EQ(stringify(42, " is the answer"), "42 is the answer");

  // Mixed types
  EXPECT_EQ(stringify(1, 2.5, 'A', "test"), "12.5Atest");

  // Many arguments
  EXPECT_EQ(stringify(1, 2, 3, 4, 5, 6, 7, 8, 9, 10), "12345678910");
}

// === Extreme Value Tests ===

TEST_F(LumexStringifyTest, ExtremeValues_Dirty)
{
  // Integer limits
  EXPECT_EQ(stringify((std::numeric_limits<int>::max)()), std::to_string((std::numeric_limits<int>::max)()));
  EXPECT_EQ(stringify((std::numeric_limits<int>::min)()), std::to_string((std::numeric_limits<int>::min)()));

  // Long long limits
  EXPECT_EQ(stringify((std::numeric_limits<long long>::max)()),
            std::to_string((std::numeric_limits<long long>::max)()));
  EXPECT_EQ(stringify((std::numeric_limits<long long>::min)()),
            std::to_string((std::numeric_limits<long long>::min)()));

  // Unsigned limits
  EXPECT_EQ(stringify((std::numeric_limits<unsigned>::max)()), std::to_string((std::numeric_limits<unsigned>::max)()));
  EXPECT_EQ(stringify((std::numeric_limits<unsigned long long>::max)()),
            std::to_string((std::numeric_limits<unsigned long long>::max)()));

  // Floating point special values
  EXPECT_EQ(stringify((std::numeric_limits<double>::infinity)()), "inf");
  EXPECT_EQ(stringify(-(std::numeric_limits<double>::infinity)()), "-inf");
  EXPECT_EQ(stringify((std::numeric_limits<double>::quiet_NaN)()), "nan");

  // Very small and large numbers
  EXPECT_EQ(stringify(1e-100), "1e-100");
  EXPECT_EQ(stringify(1e100), "1e+100");
}

// === Fixed-Width Integer Tests ===

TEST_F(LumexStringifyTest, FixedWidthIntegers_Dirty)
{
  // 8-bit integers (cast to int for numeric output)
  EXPECT_EQ(stringify(static_cast<int>(std::int8_t{127})), "127");
  EXPECT_EQ(stringify(static_cast<int>(std::int8_t{-128})), "-128");
  EXPECT_EQ(stringify(static_cast<int>(std::uint8_t{255})), "255");

  // 16-bit integers
  EXPECT_EQ(stringify(std::int16_t{32767}), "32767");
  EXPECT_EQ(stringify(std::int16_t{-32768}), "-32768");
  EXPECT_EQ(stringify(std::uint16_t{65535}), "65535");

  // 32-bit integers
  EXPECT_EQ(stringify(std::int32_t{2147483647}), "2147483647");
  EXPECT_EQ(stringify(std::int32_t{-2147483648}), "-2147483648");
  EXPECT_EQ(stringify(std::uint32_t{4294967295}), "4294967295");

  // 64-bit integers
  EXPECT_EQ(stringify(std::int64_t{9223372036854775807LL}), "9223372036854775807");
  EXPECT_EQ(stringify(static_cast<std::int64_t>(-9223372036854775807LL - 1)), "-9223372036854775808");
  EXPECT_EQ(stringify(std::uint64_t{18446744073709551615ULL}), "18446744073709551615");
}

// === Custom Type Tests ===

TEST_F(LumexStringifyTest, CustomTypes_Clean)
{
  CustomStreamable cs(42);
  EXPECT_EQ(stringify(cs), "CustomStreamable(42)");

  // Multiple custom types
  CustomStreamable cs1(1), cs2(2);
  EXPECT_EQ(stringify(cs1, cs2), "CustomStreamable(1)CustomStreamable(2)");

  // Mixed with basic types
  EXPECT_EQ(stringify("Value: ", cs, " end"), "Value: CustomStreamable(42) end");
}

TEST_F(LumexStringifyTest, AlignedTypes_Dirty)
{
  AlignedType at;
  EXPECT_EQ(stringify(at), "AlignedType[1,2,3,4]");

  // Verify alignment is preserved
  EXPECT_EQ(alignof(AlignedType), 32);
  EXPECT_EQ(stringify("Aligned: ", at), "Aligned: AlignedType[1,2,3,4]");
}

TEST_F(LumexStringifyTest, PaddedTypes_Dirty)
{
  PaddedType pt('A', 42, 'Z');
  EXPECT_EQ(stringify(pt), "PaddedType{A,42,Z}");

  // Verify padding is handled correctly
  EXPECT_GT(sizeof(PaddedType), sizeof(char) + sizeof(int) + sizeof(char));
  EXPECT_EQ(stringify("Padded: ", pt), "Padded: PaddedType{A,42,Z}");
}

TEST_F(LumexStringifyTest, VolatileTypes_Dirty)
{
  VolatileType vt(100);
  EXPECT_EQ(stringify(vt), "VolatileType(100)");

  // Volatile member access
  EXPECT_EQ(stringify("Volatile: ", vt), "Volatile: VolatileType(100)");
}

TEST_F(LumexStringifyTest, ConstTypes_Dirty)
{
  ConstType ct(200);
  EXPECT_EQ(stringify(ct), "ConstType(200)");

  // Const member access
  EXPECT_EQ(stringify("Const: ", ct), "Const: ConstType(200)");
}

TEST_F(LumexStringifyTest, ComplexTypes_Dirty)
{
  ComplexType complex("test", {1, 2, 3});
  EXPECT_EQ(stringify(complex), "ComplexType{name:'test',values:[1,2,3]}");

  // Empty vector
  ComplexType empty_complex("empty", {});
  EXPECT_EQ(stringify(empty_complex), "ComplexType{name:'empty',values:[]}");
}

// === Pointer Tests ===

TEST_F(LumexStringifyTest, Pointers_Dirty)
{
  int value = 42;
  int *ptr  = &value;

  // Pointer address (exact value unpredictable, but should be non-empty)
  std::string ptr_str = stringify(ptr);
  EXPECT_FALSE(ptr_str.empty());
  // On Windows, pointer may not have "0x" prefix, just check it's not empty

  // Null pointer (Windows shows "0000000000000000", Unix shows "0")
  int *null_ptr        = nullptr;
  std::string null_str = stringify(null_ptr);
  EXPECT_TRUE(null_str == "0" || null_str == "0000000000000000");

  // Pointer to custom type
  CustomStreamable cs(99);
  CustomStreamable *cs_ptr = &cs;
  std::string cs_ptr_str   = stringify(cs_ptr);
  EXPECT_FALSE(cs_ptr_str.empty());
}

TEST_F(LumexStringifyTest, SmartPointers_Dirty)
{
  // unique_ptr
  auto uptr            = std::make_unique<int>(42);
  std::string uptr_str = stringify(uptr);
  EXPECT_FALSE(uptr_str.empty());

  // shared_ptr
  auto sptr            = std::make_shared<int>(42);
  std::string sptr_str = stringify(sptr);
  EXPECT_FALSE(sptr_str.empty());

  // Empty smart pointers (Windows shows "0000000000000000", Unix shows "0")
  std::unique_ptr<int> empty_uptr;
  std::string empty_uptr_str = stringify(empty_uptr);
  EXPECT_TRUE(empty_uptr_str == "0" || empty_uptr_str == "0000000000000000");

  std::shared_ptr<int> empty_sptr;
  std::string empty_sptr_str = stringify(empty_sptr);
  EXPECT_TRUE(empty_sptr_str == "0" || empty_sptr_str == "0000000000000000");
}

// === Array Tests ===

TEST_F(LumexStringifyTest, Arrays_Dirty)
{
  // C-style array (decays to pointer)
  int arr[3]          = {1, 2, 3};
  std::string arr_str = stringify(arr);
  EXPECT_FALSE(arr_str.empty());
  // On Windows, pointer may not have "0x" prefix, just check it's not empty

  // std::array (address only since it doesn't have operator<<)
  std::array<int, 3> std_arr = {1, 2, 3};
  std::string std_arr_str    = stringify("std::array at: ", static_cast<void const *>(std_arr.data()));
  EXPECT_FALSE(std_arr_str.empty());

  // Character array (C-string)
  char c_str[] = "hello";
  EXPECT_EQ(stringify(c_str), "hello");

  // Empty character array
  char empty_str[] = "";
  EXPECT_EQ(stringify(empty_str), "");
}

// === Container Tests ===

TEST_F(LumexStringifyTest, Containers_Dirty)
{
  // std::vector (address only since it doesn't have operator<<)
  std::vector<int> vec = {1, 2, 3};
  std::string vec_str  = stringify("vector size: ", vec.size());
  EXPECT_FALSE(vec_str.empty());

  // Empty vector
  std::vector<int> empty_vec;
  std::string empty_vec_str = stringify("empty vector size: ", empty_vec.size());
  EXPECT_FALSE(empty_vec_str.empty());

  // Vector with custom types (test individual elements)
  std::vector<CustomStreamable> custom_vec = {CustomStreamable(1), CustomStreamable(2)};
  std::string custom_vec_str               = stringify("first element: ", custom_vec[0], ", size: ", custom_vec.size());
  EXPECT_FALSE(custom_vec_str.empty());
}

// === Exception Tests ===

TEST_F(LumexStringifyTest, ThrowingTypes_Dirty)
{
  // Normal case
  ThrowingStreamable ts(42);
  EXPECT_EQ(stringify(ts), "ThrowingStreamable(42)");

  // Throwing case
  ThrowingStreamable evil(666);
  EXPECT_THROW(stringify(evil), std::runtime_error);

  // Mixed with non-throwing
  ThrowingStreamable normal(1);
  EXPECT_NO_THROW(stringify(normal, " is safe"));
}

// === Performance Tests ===

TEST_F(LumexStringifyTest, Performance_Dirty)
{
  // Large string concatenation
  std::string large_result;
  EXPECT_NO_THROW({
    for(int i = 0; i < 1000; ++i) large_result += stringify(i, " ");
  });
  EXPECT_FALSE(large_result.empty());

  // Many arguments
  EXPECT_NO_THROW({
    std::string result = stringify(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20);
    EXPECT_FALSE(result.empty());
  });
}

// === Move Semantics Tests ===

TEST_F(LumexStringifyTest, MoveSemantics_Dirty)
{
  // Temporary objects
  EXPECT_EQ(stringify(std::string("temporary")), "temporary");
  EXPECT_EQ(stringify(CustomStreamable(999)), "CustomStreamable(999)");

  // Moving large objects
  std::string large_string(10000, 'x');
  std::string moved_result = stringify(std::move(large_string));
  EXPECT_EQ(moved_result.size(), 10000);
  EXPECT_EQ(moved_result[0], 'x');
  EXPECT_EQ(moved_result[9999], 'x');
}

// === Type Trait Tests ===

TEST_F(LumexStringifyTest, TypeTraits_Dirty)
{
  // Test is_streamable trait
  EXPECT_TRUE(Lumex::String::Utility::is_streamable<int>::value);
  EXPECT_TRUE(Lumex::String::Utility::is_streamable<std::string>::value);
  EXPECT_TRUE(Lumex::String::Utility::is_streamable<CustomStreamable>::value);
  EXPECT_FALSE(Lumex::String::Utility::is_streamable<NonStreamable>::value);

  // Test all_streamable trait
  EXPECT_TRUE((Lumex::String::Utility::all_streamable<int, std::string>::value));
  EXPECT_TRUE((Lumex::String::Utility::all_streamable<CustomStreamable, int>::value));
  EXPECT_FALSE((Lumex::String::Utility::all_streamable<NonStreamable, int>::value));
  EXPECT_FALSE((Lumex::String::Utility::all_streamable<int, NonStreamable>::value));

  // Empty all_streamable
  EXPECT_TRUE(Lumex::String::Utility::all_streamable<>::value);
}

// === Compilation Tests ===

// These tests verify that certain code patterns compile or don't compile
// They are commented out because they would cause compilation errors

/*
TEST_F(LumexStringifyTest, CompilationTests_Dirty) {
    // This should NOT compile (NonStreamable doesn't have operator<<)
    // NonStreamable ns(42);
    // auto result = stringify(ns);  // Should cause static_assert failure

    // This should NOT compile (mixing streamable and non-streamable)
    // auto result2 = stringify(42, NonStreamable(1));  // Should cause static_assert failure
}
*/

// === Memory Layout Tests ===

TEST_F(LumexStringifyTest, MemoryLayout_Dirty)
{
  // Test with different sized types
  std::uint8_t u8   = 255;
  std::uint16_t u16 = 65535;
  std::uint32_t u32 = 4294967295;
  std::uint64_t u64 = 18446744073709551615ULL;

  EXPECT_EQ(stringify(static_cast<int>(u8)), "255");
  EXPECT_EQ(stringify(u16), "65535");
  EXPECT_EQ(stringify(u32), "4294967295");
  EXPECT_EQ(stringify(u64), "18446744073709551615");

  // All together (cast u8 to int for numeric output)
  EXPECT_EQ(stringify(static_cast<int>(u8), u16, u32, u64), "25565535429496729518446744073709551615");
}

// === Unicode and Special Characters Tests ===

TEST_F(LumexStringifyTest, SpecialCharacters_Dirty)
{
  // Unicode characters
  std::string unicode = "Hello 世界 🌍";
  EXPECT_EQ(stringify(unicode), "Hello 世界 🌍");

  // Control characters
  std::string control = "\x01\x02\x03";
  EXPECT_EQ(stringify(control), control);

  // High ASCII characters
  std::string high_ascii = "\x80\x81\x82";
  EXPECT_EQ(stringify(high_ascii), high_ascii);

  // Mixed content
  EXPECT_EQ(stringify("Text: ", unicode, " Control: ", control), "Text: Hello 世界 🌍 Control: \x01\x02\x03");
}

// === Stress Tests ===

TEST_F(LumexStringifyTest, StressTests_Dirty)
{
  // Very long argument list (testing variadic templates)
  auto result
    = stringify(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28,
                29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50);
  EXPECT_FALSE(result.empty());

  // Deeply nested calls
  auto nested_result = stringify(stringify(stringify(stringify(42))));
  EXPECT_EQ(nested_result, "42");

  // Large string generation
  std::string large_input(1000, 'A');
  auto large_result = stringify(large_input);
  EXPECT_EQ(large_result.size(), 1000);
  EXPECT_EQ(large_result, large_input);
}

// === Thread Safety Tests ===

TEST_F(LumexStringifyTest, ThreadSafety_Dirty)
{
  // Multiple threads calling stringify simultaneously
  std::vector<std::thread> threads;
  std::vector<std::string> results(100);

  for(int i = 0; i < 100; ++i)
    threads.emplace_back([&results, i]() { results[i] = stringify("Thread ", i, " result"); });

  for(auto &thread : threads)
    if(thread.joinable()) thread.join();

  // Verify all results are correct
  for(int i = 0; i < 100; ++i) EXPECT_EQ(results[i], "Thread " + std::to_string(i) + " result");
}

// === Platform-Specific Tests ===

// Helper for testing platform-specific behavior
struct PlatformHelper {
#ifdef _WIN32
  static std::string
  getPlatformName()
  {
    return "Windows";
  }
  static bool
  isWindows()
  {
    return true;
  }
  static bool
  isUnix()
  {
    return false;
  }
#else
  static std::string
  getPlatformName()
  {
    return "Unix/Linux";
  }
  static bool
  isWindows()
  {
    return false;
  }
  static bool
  isUnix()
  {
    return true;
  }
#endif

  friend std::ostream &
  operator<<(std::ostream &os, const PlatformHelper &)
  {
    return os << getPlatformName();
  }
};

TEST_F(LumexStringifyTest, PlatformSpecific_Dirty)
{
  PlatformHelper ph;
  std::string platform_str = stringify("Platform: ", ph);

#ifdef _WIN32
  EXPECT_EQ(platform_str, "Platform: Windows");
  EXPECT_TRUE(PlatformHelper::isWindows());
#else
  EXPECT_EQ(platform_str, "Platform: Unix/Linux");
  EXPECT_TRUE(PlatformHelper::isUnix());
#endif
}

// === Encoding Tests ===

// Test 1: ASCII (7-bit clean)
TEST_F(LumexStringifyTest, Encoding_ASCII_Dirty)
{
  std::string ascii = "Hello World 123!@#$%^&*()";
  EXPECT_EQ(stringify("ASCII: ", ascii), "ASCII: Hello World 123!@#$%^&*()");

  // Pure ASCII characters (0-127)
  std::string pure_ascii;
  for(int i = 32; i <= 126; ++i) pure_ascii += static_cast<char>(i);
  std::string result = stringify("Pure ASCII: ", pure_ascii);
  EXPECT_FALSE(result.empty());
  EXPECT_NE(result.find("Pure ASCII: "), std::string::npos);
}

// Test 2: UTF-8 (most common Unicode encoding)
TEST_F(LumexStringifyTest, Encoding_UTF8_Dirty)
{
  // UTF-8 encoded strings
  std::string utf8_basic = "Hello, 世界! Мир! العالم!";
  EXPECT_EQ(stringify("UTF-8: ", utf8_basic), "UTF-8: Hello, 世界! Мир! العالم!");

  // Various UTF-8 characters
  std::string utf8_emoji   = "🌍🌎🌏 📚💻🔬";
  std::string emoji_result = stringify("Emoji: ", utf8_emoji);
  EXPECT_FALSE(emoji_result.empty());

  // UTF-8 mathematical symbols
  std::string utf8_math = "∑∀∃∈∅∞±≠≤≥";
  EXPECT_EQ(stringify("Math: ", utf8_math), "Math: ∑∀∃∈∅∞±≠≤≥");
}

// Test 3: Latin-1 (ISO 8859-1)
TEST_F(LumexStringifyTest, Encoding_Latin1_Dirty)
{
  // Latin-1 supplement characters (128-255)
  std::string latin1 = "Café, naïve, résumé, piñata";
  EXPECT_EQ(stringify("Latin-1: ", latin1), "Latin-1: Café, naïve, résumé, piñata");

  // Extended Latin characters
  std::string extended_latin;
  // Adding some high-bit characters that are valid in Latin-1
  extended_latin += "àáâãäåæçèéêë";
  extended_latin += "ìíîïðñòóôõö";
  extended_latin += "ùúûüýþÿ";

  std::string latin_result = stringify("Extended Latin: ", extended_latin);
  EXPECT_FALSE(latin_result.empty());
}

// Test 4: Windows-1252 (Western European)
TEST_F(LumexStringifyTest, Encoding_Windows1252_Dirty)
{
#ifdef _WIN32
  // Windows-1252 specific characters
  std::string win1252 = "Smart quotes: "
                        "'' • … – — ™";
  std::string result  = stringify("Win-1252: ", win1252);
  EXPECT_FALSE(result.empty());

  // Currency symbols in Windows-1252
  std::string currency = "€£¥¢";
  EXPECT_EQ(stringify("Currency: ", currency), "Currency: €£¥¢");
#else
  // On Unix, we'll test basic compatibility
  std::string fallback = "Windows-1252 fallback test";
  EXPECT_EQ(stringify("Fallback: ", fallback), "Fallback: Windows-1252 fallback test");
#endif
}

// Test 5: Multi-byte character boundaries
TEST_F(LumexStringifyTest, Encoding_MultiByte_Dirty)
{
  // Test UTF-8 multi-byte sequences
  std::string mb2 = "£¤¥";   // 2-byte UTF-8
  std::string mb3 = "€₹₽";   // 3-byte UTF-8
  std::string mb4 = "𝒽𝑒𝓁𝓁𝑜"; // 4-byte UTF-8 (mathematical script)

  EXPECT_EQ(stringify("2-byte: ", mb2), "2-byte: £¤¥");
  EXPECT_EQ(stringify("3-byte: ", mb3), "3-byte: €₹₽");

  std::string mb4_result = stringify("4-byte: ", mb4);
  EXPECT_FALSE(mb4_result.empty());
  EXPECT_NE(mb4_result.find("4-byte: "), std::string::npos);
}

// Test 6: Mixed encodings in single string
TEST_F(LumexStringifyTest, Encoding_Mixed_Dirty)
{
  // Mix of ASCII, Latin-1, and UTF-8
  std::string mixed  = "ASCII123 + Café + 世界 + 🌍 = Mixed!";
  std::string result = stringify("Mixed: ", mixed);
  EXPECT_FALSE(result.empty());
  EXPECT_NE(result.find("Mixed: "), std::string::npos);
  EXPECT_NE(result.find("ASCII123"), std::string::npos);
}

// === Locale-Specific Tests ===

TEST_F(LumexStringifyTest, Locale_Specific_Dirty)
{
  // Save current locale
  std::string original_locale = std::setlocale(LC_ALL, nullptr);

  // Test C locale (default)
  std::setlocale(LC_ALL, "C");
  std::string c_result = stringify("C locale: ", 3.14159);
  EXPECT_FALSE(c_result.empty());

  // Try different locales if available
  char const *test_locales[] = {"en_US.UTF-8", "en_US.utf8",  "en_US",      "de_DE.UTF-8", "de_DE.utf8",
                                "de_DE",       "fr_FR.UTF-8", "fr_FR.utf8", "fr_FR",       "C"};

  for(char const *locale : test_locales)
  {
    if(std::setlocale(LC_ALL, locale))
    {
      std::string locale_result = stringify("Locale ", locale, ": ", 1234.5678);
      EXPECT_FALSE(locale_result.empty());
      break; // Found a working locale
    }
  }

  // Restore original locale
  std::setlocale(LC_ALL, original_locale.c_str());
}

// === Windows-Specific Tests ===

#ifdef _WIN32
TEST_F(LumexStringifyTest, Windows_Specific_Dirty)
{
  // Test Windows-specific string handling
  std::string win_path = "C:\\Users\\Test\\Documents\\file.txt";
  EXPECT_EQ(stringify("Path: ", win_path), "Path: C:\\Users\\Test\\Documents\\file.txt");

  // Windows line endings
  std::string win_newlines   = "Line1\r\nLine2\r\nLine3";
  std::string newline_result = stringify("Lines: ", win_newlines);
  EXPECT_NE(newline_result.find("\r\n"), std::string::npos);

  // Windows code page test
  std::string cp1252    = "Testing Windows-1252 ™ © ® ℠";
  std::string cp_result = stringify("CP1252: ", cp1252);
  EXPECT_FALSE(cp_result.empty());
}

// Wide character tests for Windows
TEST_F(LumexStringifyTest, Windows_WideChar_Dirty)
{
  // Test that we can stringify strings that might come from wide char APIs
  std::string from_wide = "Converted from wide: Ω α β γ δ";
  EXPECT_EQ(stringify("Wide: ", from_wide), "Wide: Converted from wide: Ω α β γ δ");

  // Test Windows-specific characters
  std::string win_chars = "Windows: " + std::string(1, static_cast<char>(0x80))
                          + std::string(1, static_cast<char>(0x82)) + std::string(1, static_cast<char>(0x83));
  std::string win_result = stringify("WinChars: ", win_chars);
  EXPECT_FALSE(win_result.empty());
}
#endif

// === Unix/Linux-Specific Tests ===

#ifndef _WIN32
TEST_F(LumexStringifyTest, Unix_Specific_Dirty)
{
  // Test Unix-specific string handling
  std::string unix_path = "/home/user/documents/file.txt";
  EXPECT_EQ(stringify("Path: ", unix_path), "Path: /home/user/documents/file.txt");

  // Unix line endings
  std::string unix_newlines  = "Line1\nLine2\nLine3";
  std::string newline_result = stringify("Lines: ", unix_newlines);
  EXPECT_EQ(newline_result.find("\r\n"), std::string::npos);
  EXPECT_NE(newline_result.find("\n"), std::string::npos);

  // Test locale-specific characters
  std::string locale_chars = "ñáéíóúüç";
  EXPECT_EQ(stringify("Locale: ", locale_chars), "Locale: ñáéíóúüç");
}

TEST_F(LumexStringifyTest, Unix_Encoding_Dirty)
{
  // Test common Unix encodings
  std::string utf8_test = "UTF-8: 中文 日本語 한글 العربية";
  EXPECT_EQ(stringify("Unix UTF-8: ", utf8_test), "Unix UTF-8: UTF-8: 中文 日本語 한글 العربية");

  // Test environment-specific encoding
  char const *lang_env = std::getenv("LANG");
  if(lang_env)
  {
    std::string env_result = stringify("LANG=", lang_env, " test: åäö");
    EXPECT_FALSE(env_result.empty());
  }
}
#endif

// === Binary Data Tests ===

TEST_F(LumexStringifyTest, BinaryData_Dirty)
{
  // Test with null bytes and binary data
  std::string binary_data;
  for(int i = 0; i < 256; ++i) binary_data += static_cast<char>(i);

  std::string binary_result = stringify("Binary length: ", binary_data.length());
  EXPECT_EQ(binary_result, "Binary length: 256");

  // Test string with embedded nulls
  std::string with_nulls = "Before";
  with_nulls += std::string(1, '\0');
  with_nulls += "After";

  std::string null_result = stringify("With nulls: ", with_nulls);
  EXPECT_FALSE(null_result.empty());
  EXPECT_NE(null_result.find("Before"), std::string::npos);
}

// === Endianness Tests ===

TEST_F(LumexStringifyTest, Endianness_Dirty)
{
  // Test endianness-dependent values
  union EndianTest {
    std::uint32_t value;
    char bytes[4];
  };

  EndianTest test;
  test.value                = 0x12345678;

  std::string endian_result = stringify("Endian test: ", test.value);
  EXPECT_EQ(endian_result, "Endian test: 305419896");

  // Test different sized integers
  std::uint16_t u16_val   = 0x1234;
  std::uint32_t u32_val   = 0x12345678;
  std::uint64_t u64_val   = 0x123456789ABCDEF0ULL;

  std::string size_result = stringify("Sizes: ", u16_val, " ", u32_val, " ", u64_val);
  EXPECT_FALSE(size_result.empty());
}

// === Memory Alignment Tests ===

struct SpecialAlignment {
  alignas(64) char data[64];
  SpecialAlignment() { std::fill(data, data + 64, 'X'); }
  friend std::ostream &
  operator<<(std::ostream &os, SpecialAlignment const &sa)
  {
    return os << "SpecialAlignment[" << static_cast<void const *>(sa.data) << "]";
  }
};

TEST_F(LumexStringifyTest, MemoryAlignment_Dirty)
{
  SpecialAlignment aligned;
  std::string aligned_result = stringify("Aligned: ", aligned);
  EXPECT_FALSE(aligned_result.empty());
  EXPECT_NE(aligned_result.find("SpecialAlignment["), std::string::npos);

  // Verify alignment
  EXPECT_EQ(reinterpret_cast<uintptr_t>(aligned.data) % 64, 0);
}

// === Extreme Character Tests ===

TEST_F(LumexStringifyTest, ExtremeCharacters_Dirty)
{
  // Test all printable ASCII
  std::string printable_ascii;
  for(int i = 32; i <= 126; ++i) printable_ascii += static_cast<char>(i);

  std::string ascii_result = stringify("Printable: ", printable_ascii);
  EXPECT_FALSE(ascii_result.empty());

  // Test control characters
  std::string control_chars;
  for(int i = 1; i < 32; ++i)
    if(i != '\n' && i != '\r' && i != '\t') control_chars += static_cast<char>(i);

  std::string control_result = stringify("Control chars length: ", control_chars.length());
  EXPECT_FALSE(control_result.empty());

  // Test high-bit characters
  std::string high_bit;
  for(int i = 128; i < 256; ++i) high_bit += static_cast<char>(i);

  std::string high_result = stringify("High-bit length: ", high_bit.length());
  EXPECT_EQ(high_result, "High-bit length: 128");
}

// === Cross-Platform Path Tests ===

TEST_F(LumexStringifyTest, CrossPlatformPaths_Dirty)
{
#ifdef _WIN32
  std::string win_absolute = "C:\\Program Files\\Test\\app.exe";
  std::string win_relative = "..\\..\\data\\file.txt";
  std::string win_unc      = "\\\\server\\share\\file.doc";

  EXPECT_EQ(stringify("Win absolute: ", win_absolute), "Win absolute: C:\\Program Files\\Test\\app.exe");
  EXPECT_EQ(stringify("Win relative: ", win_relative), "Win relative: ..\\..\\data\\file.txt");
  EXPECT_EQ(stringify("Win UNC: ", win_unc), "Win UNC: \\\\server\\share\\file.doc");
#else
  std::string unix_absolute = "/usr/local/bin/app";
  std::string unix_relative = "../../data/file.txt";
  std::string unix_home     = "~/documents/file.doc";

  EXPECT_EQ(stringify("Unix absolute: ", unix_absolute), "Unix absolute: /usr/local/bin/app");
  EXPECT_EQ(stringify("Unix relative: ", unix_relative), "Unix relative: ../../data/file.txt");
  EXPECT_EQ(stringify("Unix home: ", unix_home), "Unix home: ~/documents/file.doc");
#endif
}

// === Numeric Formatting Edge Cases ===

TEST_F(LumexStringifyTest, NumericFormatting_Dirty)
{
  // Very large numbers
  double huge             = 1.7976931348623157e+308; // Near double max
  std::string huge_result = stringify("Huge: ", huge);
  EXPECT_FALSE(huge_result.empty());

  // Very small numbers
  double tiny             = 2.2250738585072014e-308; // Near double min
  std::string tiny_result = stringify("Tiny: ", tiny);
  EXPECT_FALSE(tiny_result.empty());

  // Subnormal numbers
  double subnormal             = 1e-320;
  std::string subnormal_result = stringify("Subnormal: ", subnormal);
  EXPECT_FALSE(subnormal_result.empty());

  // Different precisions
  float f                      = 1.234567890123456789f;
  double d                     = 1.234567890123456789;
  long double ld               = 1.234567890123456789L;

  std::string precision_result = stringify("Precision: ", f, " ", d, " ", ld);
  EXPECT_FALSE(precision_result.empty());
}

// === Memory-Intensive Tests ===

TEST_F(LumexStringifyTest, MemoryIntensive_Dirty)
{
  // Large string test
  std::string large_string(100000, 'A');
  std::string large_result = stringify("Large: ", large_string.length());
  EXPECT_EQ(large_result, "Large: 100000");

  // Many small strings
  std::string many_result;
  for(int i = 0; i < 1000; ++i) many_result += stringify(i, " ");
  EXPECT_FALSE(many_result.empty());
  EXPECT_GT(many_result.length(), 3000); // Should be much larger
}

// === Concurrent Access Tests ===

TEST_F(LumexStringifyTest, ConcurrentAccess_Dirty)
{
  int const num_threads = 50;
  std::vector<std::thread> threads;
  std::vector<std::string> results(num_threads);
  std::atomic<int> counter{0};

  for(int i = 0; i < num_threads; ++i)
  {
    threads.emplace_back(
      [&results, &counter, i]()
      {
        for(int j = 0; j < 100; ++j)
        {
          int count = counter.fetch_add(1);
          results[i] += stringify("Thread", i, "_Iter", j, "_Count", count, " ");
        }
      });
  }

  for(auto &thread : threads)
    if(thread.joinable()) thread.join();

  // Verify all threads completed
  for(int i = 0; i < num_threads; ++i)
  {
    EXPECT_FALSE(results[i].empty());
    EXPECT_NE(results[i].find("Thread" + std::to_string(i)), std::string::npos);
  }
}

// === Error Condition Tests ===

TEST_F(LumexStringifyTest, ErrorConditions_Dirty)
{
  // Test with potentially problematic strings
  std::string empty_string;
  EXPECT_EQ(stringify("Empty: '", empty_string, "'"), "Empty: ''");

  // String with only whitespace
  std::string whitespace = "   \t\n\r   ";
  EXPECT_EQ(stringify("Whitespace: '", whitespace, "'"), "Whitespace: '   \t\n\r   '");

  // Very long line
  std::string long_line(10000, 'X');
  std::string long_result = stringify("Long line length: ", long_line.length());
  EXPECT_EQ(long_result, "Long line length: 10000");
}

// === Final Integration Test ===

TEST_F(LumexStringifyTest, FinalIntegration_Dirty)
{
  // Combine everything: platform detection, encoding, numbers, custom types
  PlatformHelper platform;
  CustomStreamable custom(42);
  AlignedType aligned;

  std::string integration_result = stringify("Platform: ", platform, " | Custom: ", custom, " | Aligned: ", aligned,
                                             " | Unicode: ", "Hello 世界! 🌍", " | Numbers: ", 3.14159, " ", 42ULL,
                                             " | Binary: ", std::string(1, '\0'), "null", " | End");

  EXPECT_FALSE(integration_result.empty());
  EXPECT_NE(integration_result.find("Platform: "), std::string::npos);
  EXPECT_NE(integration_result.find("CustomStreamable(42)"), std::string::npos);
  EXPECT_NE(integration_result.find("AlignedType["), std::string::npos);
  EXPECT_NE(integration_result.find("Hello 世界! 🌍"), std::string::npos);
  EXPECT_NE(integration_result.find("3.14159"), std::string::npos);
  EXPECT_NE(integration_result.find("42"), std::string::npos);
  EXPECT_NE(integration_result.find("End"), std::string::npos);
}
