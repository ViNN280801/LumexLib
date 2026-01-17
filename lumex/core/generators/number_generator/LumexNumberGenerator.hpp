/**
 * @file LumexNumberGenerator.hpp
 * @brief Defines the NumberGenerator template class for versatile random number generation.
 * @details This header provides a robust and flexible template class, `NumberGenerator`,
 *          designed for generating random numbers of various numeric types (integral and floating-point)
 *          according to multiple common probability distributions. It encapsulates the complexities
 *          of the C++ Standard Library's `<random>` facilities, providing a clean,
 *          type-safe, and assertion-driven interface. The class leverages the Mersenne Twister
 *          engine (`std::mt19937`) for high-quality pseudo-random number generation,
 *          seeded by `std::random_device` for non-determinism where available,
 *          falling back to `std::time` for deterministic seeding otherwise.
 *          Special care is taken to handle type-specific distribution requirements
 *          through SFINAE (Substitution Failure Is Not An Error) techniques.
 */
#ifndef LUMEX_NUMBER_GENERATOR_HPP
#define LUMEX_NUMBER_GENERATOR_HPP

#include <cstdint>     // For std::uint8_t, std::uintmax_t, std::size_t
#include <ctime>       // For time(nullptr) as a fallback seed
#include <iostream>    // For std::cerr
#include <random>      // For std::random_device, std::mt19937, and various distributions
#include <type_traits> // For std::is_arithmetic, std::is_integral, std::is_floating_point, std::enable_if, std::is_same
#include <utility>     // For std::swap
#include <vector>      // For std::vector

/**
 * @brief Enumeration of supported probability distributions for random number generation.
 * @details This enum defines the various statistical distributions that the `NumberGenerator`
 *          class can utilize to produce random numbers. Each enumerator corresponds
 *          to a specific distribution type from the C++ Standard Library's `<random>` header.
 *          Users can select the desired distribution when constructing or configuring
 *          a `NumberGenerator` instance.
 */
enum class DistributionType : std::uint8_t
{
  UNIFORM,     ///< Uniform distribution (default): All values within a given range are equally likely.
  NORMAL,      ///< Normal (Gaussian) distribution: Values cluster around a mean, following a bell curve.
  EXPONENTIAL, ///< Exponential distribution: Describes the time between events in a Poisson process.
  GAMMA,       ///< Gamma distribution: A versatile family of continuous probability distributions.
  BERNOULLI,   ///< Bernoulli distribution (for integral types): Models a single trial with two outcomes (e.g.,
               ///< success/failure).
  BINOMIAL,    ///< Binomial distribution (for integral types): Models the number of successes in a fixed number of
               ///< independent Bernoulli trials.
  GEOMETRIC,   ///< Geometric distribution (for integral types): Models the number of Bernoulli trials needed to get one
               ///< success.
  POISSON ///< Poisson distribution (for integral types): Models the number of events in a fixed interval of time or
          ///< space.
};

/**
 * @brief Universal template-based number generator supporting multiple probability distributions.
 * @details This class provides a high-level, type-agnostic interface for generating
 *          pseudo-random numbers. It supports both integral and floating-point types
 *          (`T`) and can generate numbers according to uniform, normal, exponential,
 *          gamma, Bernoulli, binomial, geometric, and Poisson distributions.
 *          The internal random engine (`std::mt19937`) is seeded non-deterministically
 *          using `std::random_device` if available, otherwise deterministically
 *          with the current time. The class is `final` to prevent inheritance
 *          and ensures thread-safety by deleting copy operations.
 *
 * @tparam T Numeric type (integral or floating-point) for the generated numbers.
 *           `static_assert` ensures `T` is an arithmetic type but not `bool`, `char`, or `wchar_t`.
 *
 * @warning This class deletes copy constructor and copy assignment operator to ensure
 *          proper behavior and thread safety when dealing with random number engines.
 *          Instances should be moved or passed by reference.
 */
template <typename T> class NumberGenerator final
{
  static_assert(std::is_arithmetic<T>::value && !std::is_same<T, bool>::value && !std::is_same<T, char>::value
                  && !std::is_same<T, wchar_t>::value,
                "Template parameter T must be a numeric type (integral or "
                "floating-point), "
                "but not bool, char, or wchar_t");

private:
  T m_from;                             ///< @brief The lower bound for number generation (inclusive).
                                        ///< For some distributions, this might represent a parameter
                                        ///< (e.g., mean, probability, lambda).
  T m_to;                               ///< @brief The upper bound for number generation (inclusive).
                                        ///< For some distributions, this might represent another parameter
                                        ///< (e.g., standard deviation, number of trials).
  DistributionType m_distribution_type; ///< @brief The currently selected probability distribution type.
  mutable std::random_device m_rdm_dev; ///< @brief Hardware-based random number generator for seeding the engine.
                                        ///< Declared `mutable` to allow seeding in `const` methods.
  mutable std::mt19937 m_engine;        ///< @brief The Mersenne Twister pseudo-random number engine.
                                        ///< Declared `mutable` to allow number generation in `const` methods.

  constexpr static std::size_t m_default_count = 100UL; ///< @brief Default count of elements to generate in a sequence.

  /**
   * @brief Provides the default minimum value for the number generation range.
   * @details This static constexpr function determines the default lower bound
   *          for number generation when no specific bounds are provided.
   *          It ensures a starting value of `0` for any numeric type `T`.
   * @tparam T The numeric type for which to get the default minimum.
   * @return A `T` value representing the default lower bound (0).
   */
  static constexpr T
  get_default_min() noexcept
  {
    return T{0};
  }

  /**
   * @brief Provides the default maximum value for the number generation range.
   * @details This static constexpr function determines the default upper bound
   *          for number generation. For integral types, it defaults to `m_default_count` (100).
   *          For floating-point types, it defaults to `1.0`.
   * @tparam T The numeric type for which to get the default maximum.
   * @return A `T` value representing the default upper bound.
   */
  static constexpr T
  get_default_max() noexcept
  {
    return std::is_integral<T>::value ? T{m_default_count} : T{1};
  }

  /**
   * @brief Dispatches to the appropriate random number generation function based on distribution type.
   * @details This private helper function acts as a central dispatcher for all
   *          number generation requests. It uses a `switch` statement to call the
   *          correct distribution-specific `generate_` method, ensuring that
   *          the configured `DistributionType` is respected.
   * @param from_val The 'from' parameter for the chosen distribution (e.g., lower bound, mean, probability).
   * @param to_val The 'to' parameter for the chosen distribution (e.g., upper bound, standard deviation, number of
   * trials).
   * @param dist_type The `DistributionType` to use for generation.
   * @return A randomly generated number of type `T` according to the specified distribution.
   */
  T
  generate_number(T from_val, T to_val, DistributionType dist_type) const
  {
    switch(dist_type)
    {
    case DistributionType::UNIFORM: return generate_uniform(from_val, to_val);

    case DistributionType::NORMAL: return generate_normal_safe(from_val, to_val);

    case DistributionType::EXPONENTIAL: return generate_exponential_safe(from_val);

    case DistributionType::GAMMA: return generate_gamma_safe(from_val, to_val);

    case DistributionType::BERNOULLI: return generate_bernoulli_safe(static_cast<double>(from_val));

    case DistributionType::BINOMIAL:
      return generate_binomial_safe(static_cast<int>(to_val), static_cast<double>(from_val));

    case DistributionType::GEOMETRIC: return generate_geometric_safe(static_cast<double>(from_val));

    case DistributionType::POISSON: return generate_poisson_safe(static_cast<double>(from_val));

    default: return generate_uniform(from_val, to_val);
    }
  }

  /**
   * @brief Generates a random integral number uniformly distributed within a range.
   * @details This SFINAE-enabled template overload is used when `T` is an integral type.
   *          It creates and uses a `std::uniform_int_distribution` to generate the number.
   * @tparam U An internal template parameter, defaults to `T`. Used for SFINAE.
   * @param from_val The lower bound of the uniform distribution (inclusive).
   * @param to_val The upper bound of the uniform distribution (inclusive).
   * @return A randomly generated integral number of type `U`.
   */
  template <typename U = T>
  typename std::enable_if<std::is_integral<U>::value, U>::type
  generate_uniform(U from_val, U to_val) const
  {
    return std::uniform_int_distribution<U>(from_val, to_val)(m_engine);
  }

  /**
   * @brief Generates a random floating-point number uniformly distributed within a range.
   * @details This SFINAE-enabled template overload is used when `T` is a floating-point type.
   *          It creates and uses a `std::uniform_real_distribution` to generate the number.
   * @tparam U An internal template parameter, defaults to `T`. Used for SFINAE.
   * @param from_val The lower bound of the uniform distribution (inclusive).
   * @param to_val The upper bound of the uniform distribution (inclusive).
   * @return A randomly generated floating-point number of type `U`.
   */
  template <typename U = T>
  typename std::enable_if<std::is_floating_point<U>::value, U>::type
  generate_uniform(U from_val, U to_val) const
  {
    return std::uniform_real_distribution<U>(from_val, to_val)(m_engine);
  }

  /**
   * @brief Generates a random floating-point number from a normal (Gaussian) distribution.
   * @details This SFINAE-enabled template overload is used when `T` is a floating-point type.
   *          It generates numbers following a normal distribution with the given mean and standard deviation.
   * @tparam U An internal template parameter, defaults to `T`. Used for SFINAE.
   * @param mean The mean (average) of the normal distribution.
   * @param stddev The standard deviation of the normal distribution.
   * @return A randomly generated floating-point number of type `U` from the normal distribution.
   */
  template <typename U = T>
  typename std::enable_if<std::is_floating_point<U>::value, U>::type
  generate_normal_safe(U mean, U stddev) const
  {
    return static_cast<U>(
      std::normal_distribution<double>(static_cast<double>(mean), static_cast<double>(stddev))(m_engine));
  }

  /**
   * @brief Fallback for generating "normal" distribution for integral types (uses uniform).
   * @details This SFINAE-enabled template overload is used when `T` is an integral type.
   *          Since `std::normal_distribution` is typically for floating-point types,
   *          this method falls back to a uniform distribution within the specified bounds.
   * @tparam U An internal template parameter, defaults to `T`. Used for SFINAE.
   * @param mean For integral types, treated as lower bound for fallback uniform distribution.
   * @param stddev For integral types, treated as upper bound for fallback uniform distribution.
   * @return A randomly generated integral number of type `U` from a uniform distribution.
   */
  template <typename U = T>
  typename std::enable_if<std::is_integral<U>::value, U>::type
  generate_normal_safe(U mean, U stddev) const
  {
    // For integral types, fall back to uniform distribution
    return generate_uniform(mean, stddev);
  }

  /**
   * @brief Generates a random floating-point number from an exponential distribution.
   * @details This SFINAE-enabled template overload is used when `T` is a floating-point type.
   *          It generates numbers following an exponential distribution with the given lambda parameter.
   * @tparam U An internal template parameter, defaults to `T`. Used for SFINAE.
   * @param lambda The rate parameter (λ) of the exponential distribution.
   * @return A randomly generated floating-point number of type `U` from the exponential distribution.
   */
  template <typename U = T>
  typename std::enable_if<std::is_floating_point<U>::value, U>::type
  generate_exponential_safe(U lambda) const
  {
    return static_cast<U>(std::exponential_distribution<double>(static_cast<double>(lambda))(m_engine));
  }

  /**
   * @brief Fallback for generating "exponential" distribution for integral types (uses uniform).
   * @details This SFINAE-enabled template overload is used when `T` is an integral type.
   *          Since `std::exponential_distribution` is typically for floating-point types,
   *          this method falls back to a uniform distribution from 0 up to `lambda`.
   * @tparam U An internal template parameter, defaults to `T`. Used for SFINAE.
   * @param lambda For integral types, treated as upper bound for fallback uniform distribution (from 0).
   * @return A randomly generated integral number of type `U` from a uniform distribution.
   */
  template <typename U = T>
  typename std::enable_if<std::is_integral<U>::value, U>::type
  generate_exponential_safe(U lambda) const
  {
    // For integral types, fall back to uniform distribution
    return generate_uniform(static_cast<U>(0), lambda);
  }

  /**
   * @brief Generates a random floating-point number from a gamma distribution.
   * @details This SFINAE-enabled template overload is used when `T` is a floating-point type.
   *          It generates numbers following a gamma distribution with the given alpha (shape)
   *          and beta (scale) parameters.
   * @tparam U An internal template parameter, defaults to `T`. Used for SFINAE.
   * @param alpha The shape parameter (α) of the gamma distribution.
   * @param beta The scale parameter (β) of the gamma distribution.
   * @return A randomly generated floating-point number of type `U` from the gamma distribution.
   */
  template <typename U = T>
  typename std::enable_if<std::is_floating_point<U>::value, U>::type
  generate_gamma_safe(U alpha, U beta) const
  {
    return static_cast<U>(
      std::gamma_distribution<double>(static_cast<double>(alpha), static_cast<double>(beta))(m_engine));
  }

  /**
   * @brief Fallback for generating "gamma" distribution for integral types (uses uniform).
   * @details This SFINAE-enabled template overload is used when `T` is an integral type.
   *          Since `std::gamma_distribution` is typically for floating-point types,
   *          this method falls back to a uniform distribution between `alpha` and `beta`.
   * @tparam U An internal template parameter, defaults to `T`. Used for SFINAE.
   * @param alpha For integral types, treated as lower bound for fallback uniform distribution.
   * @param beta For integral types, treated as upper bound for fallback uniform distribution.
   * @return A randomly generated integral number of type `U` from a uniform distribution.
   */
  template <typename U = T>
  typename std::enable_if<std::is_integral<U>::value, U>::type
  generate_gamma_safe(U alpha, U beta) const
  {
    // For integral types, fall back to uniform distribution
    return generate_uniform(alpha, beta);
  }

  /**
   * @brief Generates a random integral number from a Bernoulli distribution.
   * @details This SFINAE-enabled template overload is used when `T` is an integral type.
   *          It generates either 0 or 1 based on the specified probability `p_val`.
   * @tparam U An internal template parameter, defaults to `T`. Used for SFINAE.
   * @param p_val The probability of success (value 1), between 0.0 and 1.0.
   * @return A randomly generated integral number of type `U` (0 or 1) from the Bernoulli distribution.
   */
  template <typename U = T>
  typename std::enable_if<std::is_integral<U>::value, U>::type
  generate_bernoulli_safe(double p_val) const
  {
    return static_cast<U>(std::bernoulli_distribution(p_val)(m_engine));
  }

  /**
   * @brief Generates a random floating-point number from a Bernoulli distribution.
   * @details This SFINAE-enabled template overload is used when `T` is a floating-point type.
   *          It generates either 0.0 or 1.0 based on the specified probability `p_val`.
   * @tparam U An internal template parameter, defaults to `T`. Used for SFINAE.
   * @param p_val The probability of success (value 1.0), between 0.0 and 1.0.
   * @return A randomly generated floating-point number of type `U` (0.0 or 1.0) from the Bernoulli distribution.
   */
  template <typename U = T>
  typename std::enable_if<std::is_floating_point<U>::value, U>::type
  generate_bernoulli_safe(double p_val) const
  {
    // For floating-point types, return 0.0 or 1.0
    return static_cast<U>(std::bernoulli_distribution(p_val)(m_engine));
  }

  /**
   * @brief Generates a random integral number from a binomial distribution.
   * @details This SFINAE-enabled template overload is used when `T` is an integral type.
   *          It generates numbers representing the number of successes in a series of trials.
   * @tparam U An internal template parameter, defaults to `T`. Used for SFINAE.
   * @param trials The number of independent trials.
   * @param p_val The probability of success on each trial, between 0.0 and 1.0.
   * @return A randomly generated integral number of type `U` from the binomial distribution.
   */
  template <typename U = T>
  typename std::enable_if<std::is_integral<U>::value, U>::type
  generate_binomial_safe(int trials, double p_val) const
  {
    return static_cast<U>(std::binomial_distribution<int>(trials, p_val)(m_engine));
  }

  /**
   * @brief Generates a random floating-point number from a binomial distribution.
   * @details This SFINAE-enabled template overload is used when `T` is a floating-point type.
   *          It generates numbers representing the number of successes in a series of trials,
   *          cast to a floating-point type.
   * @tparam U An internal template parameter, defaults to `T`. Used for SFINAE.
   * @param trials The number of independent trials.
   * @param p_val The probability of success on each trial, between 0.0 and 1.0.
   * @return A randomly generated floating-point number of type `U` from the binomial distribution.
   */
  template <typename U = T>
  typename std::enable_if<std::is_floating_point<U>::value, U>::type
  generate_binomial_safe(int trials, double p_val) const
  {
    // For floating-point types, cast result to floating-point
    return static_cast<U>(std::binomial_distribution<int>(trials, p_val)(m_engine));
  }

  /**
   * @brief Generates a random integral number from a geometric distribution.
   * @details This SFINAE-enabled template overload is used when `T` is an integral type.
   *          It generates numbers representing the number of trials until the first success.
   * @tparam U An internal template parameter, defaults to `T`. Used for SFINAE.
   * @param p_val The probability of success on each trial, between 0.0 and 1.0.
   * @return A randomly generated integral number of type `U` from the geometric distribution.
   */
  template <typename U = T>
  typename std::enable_if<std::is_integral<U>::value, U>::type
  generate_geometric_safe(double p_val) const
  {
    return static_cast<U>(std::geometric_distribution<int>(p_val)(m_engine));
  }

  /**
   * @brief Generates a random floating-point number from a geometric distribution.
   * @details This SFINAE-enabled template overload is used when `T` is a floating-point type.
   *          It generates numbers representing the number of trials until the first success,
   *          cast to a floating-point type.
   * @tparam U An internal template parameter, defaults to `T`. Used for SFINAE.
   * @param p_val The probability of success on each trial, between 0.0 and 1.0.
   * @return A randomly generated floating-point number of type `U` from the geometric distribution.
   */
  template <typename U = T>
  typename std::enable_if<std::is_floating_point<U>::value, U>::type
  generate_geometric_safe(double p_val) const
  {
    // For floating-point types, cast result to floating-point
    return static_cast<U>(std::geometric_distribution<int>(p_val)(m_engine));
  }

  /**
   * @brief Generates a random integral number from a Poisson distribution.
   * @details This SFINAE-enabled template overload is used when `T` is an integral type.
   *          It generates numbers representing the number of events in a fixed interval.
   * @tparam U An internal template parameter, defaults to `T`. Used for SFINAE.
   * @param mean The mean (λ) of the Poisson distribution.
   * @return A randomly generated integral number of type `U` from the Poisson distribution.
   */
  template <typename U = T>
  typename std::enable_if<std::is_integral<U>::value, U>::type
  generate_poisson_safe(double mean) const
  {
    return static_cast<U>(std::poisson_distribution<int>(mean)(m_engine));
  }

  /**
   * @brief Generates a random floating-point number from a Poisson distribution.
   * @details This SFINAE-enabled template overload is used when `T` is a floating-point type.
   *          It generates numbers representing the number of events in a fixed interval,
   *          cast to a floating-point type.
   * @tparam U An internal template parameter, defaults to `T`. Used for SFINAE.
   * @param mean The mean (λ) of the Poisson distribution.
   * @return A randomly generated floating-point number of type `U` from the Poisson distribution.
   */
  template <typename U = T>
  typename std::enable_if<std::is_floating_point<U>::value, U>::type
  generate_poisson_safe(double mean) const
  {
    // For floating-point types, cast result to floating-point
    return static_cast<U>(std::poisson_distribution<int>(mean)(m_engine));
  }

  /**
   * @brief Generates a random number from a normal (Gaussian) distribution.
   * @details This method is specifically for floating-point types. It creates and uses a
   *          `std::normal_distribution` to generate a number based on the provided mean and standard deviation.
   * @warning This method includes a `static_assert` that will fail if `T` is not a floating-point type.
   *          For integral types, `generate_normal_safe` should be used, which provides a fallback.
   * @param mean The mean (average) of the normal distribution.
   * @param stddev The standard deviation of the normal distribution.
   * @return A randomly generated number of type `T` from the normal distribution.
   */
  T
  generate_normal(T mean, T stddev) const
  {
    static_assert(std::is_floating_point<T>::value, "Normal distribution requires floating-point type");
    return static_cast<T>(
      std::normal_distribution<double>(static_cast<double>(mean), static_cast<double>(stddev))(m_engine));
  }

  /**
   * @brief Generates a random number from an exponential distribution.
   * @details This method is specifically for floating-point types. It creates and uses an
   *          `std::exponential_distribution` to generate a number based on the provided lambda parameter.
   * @warning This method includes a `static_assert` that will fail if `T` is not a floating-point type.
   *          For integral types, `generate_exponential_safe` should be used, which provides a fallback.
   * @param lambda The rate parameter (λ) of the exponential distribution.
   * @return A randomly generated number of type `T` from the exponential distribution.
   */
  T
  generate_exponential(T lambda) const
  {
    static_assert(std::is_floating_point<T>::value, "Exponential distribution requires floating-point type");
    return static_cast<T>(std::exponential_distribution<double>(static_cast<double>(lambda))(m_engine));
  }

  /**
   * @brief Generates a random number from a gamma distribution.
   * @details This method is specifically for floating-point types. It creates and uses a
   *          `std::gamma_distribution` to generate a number based on the provided alpha and beta parameters.
   * @warning This method includes a `static_assert` that will fail if `T` is not a floating-point type.
   *          For integral types, `generate_gamma_safe` should be used, which provides a fallback.
   * @param alpha The shape parameter (α) of the gamma distribution.
   * @param beta The scale parameter (β) of the gamma distribution.
   * @return A randomly generated number of type `T` from the gamma distribution.
   */
  T
  generate_gamma(T alpha, T beta) const
  {
    static_assert(std::is_floating_point<T>::value, "Gamma distribution requires floating-point type");
    return static_cast<T>(
      std::gamma_distribution<double>(static_cast<double>(alpha), static_cast<double>(beta))(m_engine));
  }

  /**
   * @brief Generates a random number from a Bernoulli distribution.
   * @details This method is specifically for integral types. It creates and uses a
   *          `std::bernoulli_distribution` to generate a binary result (0 or 1).
   * @warning This method includes a `static_assert` that will fail if `T` is not an integral type.
   *          For floating-point types, `generate_bernoulli_safe` should be used, which returns 0.0 or 1.0.
   * @param p_val The probability of success (value 1), between 0.0 and 1.0.
   * @return A randomly generated number of type `T` (0 or 1) from the Bernoulli distribution.
   */
  T
  generate_bernoulli(double p_val) const
  {
    static_assert(std::is_integral<T>::value, "Bernoulli distribution requires integral type");
    return static_cast<T>(std::bernoulli_distribution(p_val)(m_engine));
  }

  /**
   * @brief Generates a random number from a binomial distribution.
   * @details This method is specifically for integral types. It creates and uses a
   *          `std::binomial_distribution` to generate a number representing successes in trials.
   * @warning This method includes a `static_assert` that will fail if `T` is not an integral type.
   *          For floating-point types, `generate_binomial_safe` should be used, which casts the result.
   * @param trials The number of independent trials.
   * @param p_val The probability of success on each trial, between 0.0 and 1.0.
   * @return A randomly generated number of type `T` from the binomial distribution.
   */
  T
  generate_binomial(int trials, double p_val) const
  {
    static_assert(std::is_integral<T>::value, "Binomial distribution requires integral type");
    return static_cast<T>(std::binomial_distribution<int>(trials, p_val)(m_engine));
  }

  /**
   * @brief Generates a random number from a geometric distribution.
   * @details This method is specifically for integral types. It creates and uses a
   *          `std::geometric_distribution` to generate a number representing trials until first success.
   * @warning This method includes a `static_assert` that will fail if `T` is not an integral type.
   *          For floating-point types, `generate_geometric_safe` should be used, which casts the result.
   * @param p_val The probability of success on each trial, between 0.0 and 1.0.
   * @return A randomly generated number of type `T` from the geometric distribution.
   */
  T
  generate_geometric(double p_val) const
  {
    static_assert(std::is_integral<T>::value, "Geometric distribution requires integral type");
    return static_cast<T>(std::geometric_distribution<int>(p_val)(m_engine));
  }

  /**
   * @brief Generates a random number from a Poisson distribution.
   * @details This method is specifically for integral types. It creates and uses a
   *          `std::poisson_distribution` to generate a number representing events in an interval.
   * @warning This method includes a `static_assert` that will fail if `T` is not an integral type.
   *          For floating-point types, `generate_poisson_safe` should be used, which casts the result.
   * @param mean The mean (λ) of the Poisson distribution.
   * @return A randomly generated number of type `T` from the Poisson distribution.
   */
  T
  generate_poisson(double mean) const
  {
    static_assert(std::is_integral<T>::value, "Poisson distribution requires integral type");
    return static_cast<T>(std::poisson_distribution<int>(mean)(m_engine));
  }

public:
  /**
   * @brief Constructs a `NumberGenerator` with default bounds and uniform distribution.
   * @details Initializes the generator to produce numbers within default bounds
   *          (0 to `m_default_count` for integral types, 0.0 to 1.0 for floating-point types)
   *          using a uniform distribution. The internal random engine (`m_engine`) is
   *          seeded using `std::random_device` for non-determinism; if `std::random_device`
   *          does not provide entropy, it falls back to seeding with the current time.
   */
  NumberGenerator()
      : m_from(get_default_min()),
        m_to(get_default_max()),
        m_distribution_type(DistributionType::UNIFORM),
        m_engine(m_rdm_dev.entropy() > 0.0 ? m_rdm_dev() : static_cast<std::mt19937::result_type>(time(nullptr)))
  {}

  /**
   * @brief Constructs a `NumberGenerator` with specified bounds and an optional distribution type.
   * @details Initializes the generator with custom lower (`from_val`) and upper (`to_val`)
   *          bounds. If `from_val` is greater than `to_val`, they are swapped to ensure
   *          correct range definition. The distribution type defaults to `UNIFORM`.
   *          The internal random engine (`m_engine`) is seeded using `std::random_device`
   *          for non-determinism; if `std::random_device` does not provide entropy,
   *          it falls back to seeding with the current time.
   *
   * @param from_val The lower bound of the range for `UNIFORM` distribution, or the primary parameter
   *                 (e.g., probability `p`, mean `µ`, lambda `λ`) for other distributions.
   * @param to_val The upper bound of the range for `UNIFORM` distribution, or the secondary parameter
   *               (e.g., standard deviation `σ`, number of trials `n`) for other distributions.
   * @param dist_type The desired `DistributionType` to use. Defaults to `DistributionType::UNIFORM`.
   *
   * @note The interpretation of `from_val` and `to_val` changes significantly
   *       depending on the `dist_type`. For non-uniform distributions, these
   *       parameters correspond to the distribution's specific parameters.
   *       Refer to the `get_number()` overloads or individual `generate_` methods
   *       for explicit parameter meanings for each distribution.
   */
  NumberGenerator(T from_val, T to_val, DistributionType dist_type = DistributionType::UNIFORM)
      : m_from(from_val > to_val ? to_val : from_val),
        m_to(from_val > to_val ? from_val : to_val),
        m_distribution_type(dist_type),
        m_engine(m_rdm_dev.entropy() > 0.0 ? m_rdm_dev() : static_cast<std::mt19937::result_type>(time(nullptr)))
  {}

  /**
   * @brief Default destructor.
   * @details Cleans up any resources held by the `NumberGenerator` instance.
   */
  ~NumberGenerator() = default;

  // Delete copy constructor and assignment operator for thread safety
  NumberGenerator(NumberGenerator const &)            = delete;
  NumberGenerator &operator=(NumberGenerator const &) = delete;

  // Delete move constructor and assignment operator
  NumberGenerator(NumberGenerator &&) noexcept            = delete;
  NumberGenerator &operator=(NumberGenerator &&) noexcept = delete;

  /**
   * @brief Function call operator to generate a random number using configured bounds and distribution.
   * @details This operator allows the `NumberGenerator` object to be called like a function.
   *          It generates a single random number using the bounds (`m_from`, `m_to`) and
   *          `m_distribution_type` currently configured in the generator.
   * @return A randomly generated number of type `T`.
   */
  T
  operator()() const
  {
    return generate_number(m_from, m_to, m_distribution_type);
  }

  /**
   * @brief Function call operator to generate a random number with specified bounds.
   * @details Generates a single random number using the provided `from_val` and `to_val`
   *          as bounds, and the `m_distribution_type` currently configured in the generator.
   *          This allows for on-the-fly range adjustments without changing the default settings.
   * @param from_val The lower bound for the number generation.
   * @param to_val The upper bound for the number generation.
   * @return A randomly generated number of type `T`.
   */
  T
  operator()(T from_val, T to_val) const
  {
    return generate_number(from_val, to_val, m_distribution_type);
  }

  /**
   * @brief Function call operator to generate a random number with specified bounds and distribution.
   * @details Generates a single random number using the provided `from_val`, `to_val`,
   *          and `dist_type`. This offers full control over the generation parameters
   *          for a single call, overriding the generator's default settings temporarily.
   * @param from_val The 'from' parameter for the chosen distribution.
   * @param to_val The 'to' parameter for the chosen distribution.
   * @param dist_type The `DistributionType` to use for this specific generation.
   * @return A randomly generated number of type `T`.
   */
  T
  operator()(T from_val, T to_val, DistributionType dist_type) const
  {
    return generate_number(from_val, to_val, dist_type);
  }

  /**
   * @brief Gets a random number with explicitly specified parameters.
   * @details This method provides a clear interface for generating a single random
   *          number with full control over the range and distribution type.
   *          Parameters default to the generator's current settings or predefined
   *          defaults if not provided. This is the most explicit way to request
   *          a random number.
   * @param from_val The 'from' parameter for the chosen distribution. Defaults to `get_default_min()`.
   * @param to_val The 'to' parameter for the chosen distribution. Defaults to `get_default_max()`.
   * @param dist_type The `DistributionType` to use for generation. Defaults to `DistributionType::UNIFORM`.
   * @return A randomly generated number of type `T`.
   */
  T
  get_number(T from_val = get_default_min(), T to_val = get_default_max(),
             DistributionType dist_type = DistributionType::UNIFORM) const
  {
    return generate_number(from_val, to_val, dist_type);
  }

  /**
   * @brief Sets the lower bound for subsequent random number generations.
   * @details This method updates the internal `m_from` member, affecting all
   *          future calls to `operator()()` or `get_number()` that do not
   *          explicitly specify a `from_val`.
   * @param val The new lower bound value.
   */
  void
  set_lower_bound(T val) noexcept
  {
    m_from = val;
  }

  /**
   * @brief Sets the upper bound for subsequent random number generations.
   * @details This method updates the internal `m_to` member, affecting all
   *          future calls to `operator()()` or `get_number()` that do not
   *          explicitly specify a `to_val`.
   * @param val The new upper bound value.
   */
  void
  set_upper_bound(T val) noexcept
  {
    m_to = val;
  }

  /**
   * @brief Sets both the lower and upper bounds for subsequent random number generations.
   * @details This method updates both `m_from` and `m_to`. It automatically
   *          swaps `from_val` and `to_val` if `from_val` is initially greater
   *          than `to_val` to ensure `m_from` is always less than or equal to `m_to`.
   *          This affects all future calls to `operator()()` or `get_number()`
   *          that do not explicitly specify bounds.
   * @param from_val The new lower bound.
   * @param to_val The new upper bound.
   */
  void
  set_bounds(T from_val, T to_val) noexcept
  {
    if(from_val > to_val) std::swap(from_val, to_val);
    m_from = from_val;
    m_to   = to_val;
  }

  /**
   * @brief Sets the probability distribution type for subsequent random number generations.
   * @details This method updates the internal `m_distribution_type` member, affecting
   *          all future calls to `operator()()` or `get_number()` that do not
   *          explicitly specify a `dist_type`.
   * @param dist_type The new `DistributionType` to use.
   */
  void
  set_distribution(DistributionType dist_type) noexcept
  {
    m_distribution_type = dist_type;
  }

  /**
   * @brief Generates a sequence (vector) of random numbers.
   * @details This method generates a `std::vector` containing `count` random numbers.
   *          It allows for optional specification of bounds and distribution type,
   *          which will override the generator's internal settings for this sequence.
   *          It includes exception handling to catch potential errors during sequence generation.
   * @param count The number of random numbers to generate in the sequence. If 0, an empty vector is returned.
   * @param from_val The 'from' parameter for the chosen distribution for this sequence. Defaults to
   * `get_default_min()`.
   * @param to_val The 'to' parameter for the chosen distribution for this sequence. Defaults to `get_default_max()`.
   * @param dist_type The `DistributionType` to use for this specific sequence generation. Defaults to
   * `DistributionType::UNIFORM`.
   * @return A `std::vector<T>` containing the generated random numbers. Returns an empty vector if `count` is 0 or an
   * exception occurs.
   * @note If `count` is large, consider potential memory usage.
   */
  std::vector<T>
  get_sequence(std::size_t count, T from_val = get_default_min(), T to_val = get_default_max(),
               DistributionType dist_type = DistributionType::UNIFORM) const
  {
    if(count == 0)
    {
      // Note: Returning empty sequence for zero count request
      return {};
    }

    try
    {
      std::vector<T> sequence;
      sequence.reserve(count); // Pre-allocate memory to avoid reallocations

      for(std::size_t i = 0; i < count; ++i) sequence.push_back(generate_number(from_val, to_val, dist_type));

      return sequence;
    }
    catch(std::exception const &e)
    {
      std::cerr << "Error generating sequence: " << e.what() << ". Parameters: count=" << count << ", from=" << from_val
                << ", to=" << to_val << ". Returning empty sequence.\n";
      return {};
    }
    catch(...)
    {
      std::cerr << "Unknown exception during sequence generation. "
                << "Parameters: count=" << count << ", from=" << from_val << ", to=" << to_val
                << ". Returning empty sequence.\n";
      return {};
    }
  }

  /**
   * @brief Retrieves the current lower bound configured in the generator.
   * @details This method provides read-only access to the `m_from` member.
   * @return The current lower bound of type `T`.
   */
  T
  get_lower_bound() const noexcept
  {
    return m_from;
  }

  /**
   * @brief Retrieves the current upper bound configured in the generator.
   * @details This method provides read-only access to the `m_to` member.
   * @return The current upper bound of type `T`.
   */
  T
  get_upper_bound() const noexcept
  {
    return m_to;
  }

  /**
   * @brief Retrieves the current probability distribution type configured in the generator.
   * @details This method provides read-only access to the `m_distribution_type` member.
   * @return The current `DistributionType`.
   */
  DistributionType
  get_distribution() const noexcept
  {
    return m_distribution_type;
  }
};

/**
 * @brief Type alias for `NumberGenerator` specialized for `int`.
 * @details Provides a convenient way to declare an integer random number generator.
 */
using IntGenerator = NumberGenerator<int>;
/**
 * @brief Type alias for `NumberGenerator` specialized for `long long`.
 * @details Provides a convenient way to declare a `long long` integer random number generator.
 */
using LongGenerator = NumberGenerator<long long>;
/**
 * @brief Type alias for `NumberGenerator` specialized for `float`.
 * @details Provides a convenient way to declare a single-precision floating-point random number generator.
 */
using FloatGenerator = NumberGenerator<float>;
/**
 * @brief Type alias for `NumberGenerator` specialized for `double`.
 * @details Provides a convenient way to declare a double-precision floating-point random number generator.
 */
using DoubleGenerator = NumberGenerator<double>;

/**
 * @brief Backward compatibility alias for `DoubleGenerator`.
 * @details This alias is provided for compatibility with older codebases that might
 *          have used a different name for a double-precision floating-point generator.
 * @deprecated Use `DoubleGenerator` directly for clarity and consistency with modern naming.
 */
using RealNumberGeneratorHost = DoubleGenerator;

#endif // !LUMEX_NUMBER_GENERATOR_HPP
