#include <algorithm>
#include <cassert>
#include <iostream>
#include <type_traits>
#include <vector>

#include "lumex/core/generators/number_generator/LumexNumberGenerator.hpp"

// Simple test framework macros
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "FAILED: " << #condition << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while(0)

#define RUN_TEST(test_func) \
    do { \
        std::cout << "Running " << #test_func << "... "; \
        if (test_func()) { \
            std::cout << "PASSED" << std::endl; \
        } else { \
            std::cout << "FAILED" << std::endl; \
            return 1; \
        } \
    } while(0)

// Test uniform distribution with integers
bool
test_uniform_int_generation()
{
  NumberGenerator<int> gen(1, 10);

  // Generate multiple numbers and check bounds
  for(int i = 0; i < 100; ++i)
  {
    int value = gen();
    TEST_ASSERT(value >= 1 && value <= 10);
  }

  // Test operator() with parameters
  for(int i = 0; i < 100; ++i)
  {
    int value = gen(5, 15);
    TEST_ASSERT(value >= 5 && value <= 15);
  }

  return true;
}

// Test uniform distribution with floating point
bool
test_uniform_double_generation()
{
  NumberGenerator<double> gen(0.0, 1.0);

  // Generate multiple numbers and check bounds
  for(int i = 0; i < 100; ++i)
  {
    double value = gen();
    TEST_ASSERT(value >= 0.0 && value <= 1.0);
  }

  // Test with different bounds
  for(int i = 0; i < 100; ++i)
  {
    double value = gen(-5.5, 5.5);
    TEST_ASSERT(value >= -5.5 && value <= 5.5);
  }

  return true;
}

// Test normal distribution (only for floating point types)
bool
test_normal_distribution()
{
  NumberGenerator<double> gen(0.0, 1.0, DistributionType::NORMAL);

  // Generate sequence and check that values are reasonable
  std::vector<double> values;
  for(int i = 0; i < 1000; ++i) values.push_back(gen());

  // Basic sanity check - most values should be within reasonable range
  int reasonable_count = 0;
  for(double val : values)
    if(val >= -5.0 && val <= 5.0) reasonable_count++;

  // At least 95% should be within 5 standard deviations
  TEST_ASSERT(reasonable_count > 950);

  return true;
}

// Test Bernoulli distribution (works with both integral and floating point types)
bool
test_bernoulli_distribution()
{
  // For Bernoulli, we need to use get_number() with probability directly
  // Using constructor with default bounds and setting distribution type
  NumberGenerator<double> gen;
  gen.set_distribution(DistributionType::BERNOULLI);

  int zeros = 0, ones = 0;
  for(int i = 0; i < 1000; ++i)
  {
    // Pass 0.5 as probability directly via get_number()
    double value = gen.get_number(0.5, 0.0, DistributionType::BERNOULLI);
    TEST_ASSERT(value == 0.0 || value == 1.0);
    if(value == 0.0)
      zeros++;
    else
      ones++;
  }

  // Should be roughly balanced (within reasonable range for random distribution)
  // Using wider range to account for natural variance in random distributions
  TEST_ASSERT(zeros > 200 && zeros < 800);
  TEST_ASSERT(ones > 200 && ones < 800);

  // Also test that we have both zeros and ones (not stuck on one value)
  TEST_ASSERT(zeros > 0 && ones > 0);

  return true;
}

// Test sequence generation
bool
test_sequence_generation()
{
  NumberGenerator<int> gen(1, 100);

  // Test uniform sequence
  auto sequence = gen.get_sequence(50, 1, 10, DistributionType::UNIFORM);
  TEST_ASSERT(sequence.size() == 50);

  for(int value : sequence) TEST_ASSERT(value >= 1 && value <= 10);

  // Test empty sequence
  auto empty_seq = gen.get_sequence(0);
  TEST_ASSERT(empty_seq.empty());

  return true;
}

// Test setters and getters
bool
test_setters_getters()
{
  NumberGenerator<double> gen;

  // Test default values
  TEST_ASSERT(gen.get_distribution() == DistributionType::UNIFORM);

  // Test setting bounds
  gen.set_bounds(-1.0, 1.0);
  TEST_ASSERT(gen.get_lower_bound() == -1.0);
  TEST_ASSERT(gen.get_upper_bound() == 1.0);

  // Test setting individual bounds
  gen.set_lower_bound(-2.0);
  gen.set_upper_bound(2.0);
  TEST_ASSERT(gen.get_lower_bound() == -2.0);
  TEST_ASSERT(gen.get_upper_bound() == 2.0);

  // Test setting distribution
  gen.set_distribution(DistributionType::NORMAL);
  TEST_ASSERT(gen.get_distribution() == DistributionType::NORMAL);

  return true;
}

// Test type aliases (only uniform to avoid static_assert)
bool
test_type_aliases()
{
  IntGenerator int_gen;
  DoubleGenerator double_gen;
  RealNumberGeneratorHost legacy_gen; // Backward compatibility

  // Test that they work as expected
  int int_val       = int_gen();
  double double_val = double_gen();
  double legacy_val = legacy_gen();

  // Simple range checks instead of template assertions
  TEST_ASSERT(int_val >= 0 && int_val <= 100);         // Default range for int
  TEST_ASSERT(double_val >= 0.0 && double_val <= 1.0); // Default range for double
  TEST_ASSERT(legacy_val >= 0.0 && legacy_val <= 1.0); // Legacy compatibility

  return true;
}

// Test different constructor overloads
bool
test_constructors()
{
  // Default constructor
  NumberGenerator<int> gen1;
  TEST_ASSERT(gen1.get_distribution() == DistributionType::UNIFORM);

  // Constructor with bounds
  NumberGenerator<int> gen2(10, 20);
  TEST_ASSERT(gen2.get_lower_bound() == 10);
  TEST_ASSERT(gen2.get_upper_bound() == 20);
  TEST_ASSERT(gen2.get_distribution() == DistributionType::UNIFORM);

  // Constructor with bounds and distribution
  NumberGenerator<int> gen3(0, 1, DistributionType::BERNOULLI);
  TEST_ASSERT(gen3.get_distribution() == DistributionType::BERNOULLI);

  return true;
}

// Test exponential distribution
bool
test_exponential_distribution()
{
  NumberGenerator<double> gen(1.0, 0.0, DistributionType::EXPONENTIAL);

  // Generate values and check they're positive
  for(int i = 0; i < 100; ++i)
  {
    double value = gen();
    TEST_ASSERT(value >= 0.0);
  }

  return true;
}

// Test different numeric types
bool
test_different_types()
{
  // Test various numeric types with uniform distribution only
  NumberGenerator<short> short_gen(-10, 10);
  NumberGenerator<unsigned int> uint_gen(0, 100);
  NumberGenerator<float> float_gen(0.0f, 1.0f);
  NumberGenerator<long long> long_gen(-1000, 1000);

  // Generate some values to test compilation
  short short_val       = short_gen();
  unsigned int uint_val = uint_gen();
  float float_val       = float_gen();
  long long long_val    = long_gen();

  // Basic range checks
  TEST_ASSERT(short_val >= -10 && short_val <= 10);
  TEST_ASSERT(uint_val <= 100);
  TEST_ASSERT(float_val >= 0.0f && float_val <= 1.0f);
  TEST_ASSERT(long_val >= -1000 && long_val <= 1000);

  return true;
}

int
main()
{
  std::cout << "=== NumberGenerator Tests ===" << std::endl;

  RUN_TEST(test_uniform_int_generation);
  RUN_TEST(test_uniform_double_generation);
  RUN_TEST(test_normal_distribution);
  RUN_TEST(test_bernoulli_distribution);
  RUN_TEST(test_sequence_generation);
  RUN_TEST(test_setters_getters);
  RUN_TEST(test_type_aliases);
  RUN_TEST(test_constructors);
  RUN_TEST(test_exponential_distribution);
  RUN_TEST(test_different_types);

  std::cout << "=== All tests passed! ===" << std::endl;
  return 0;
}