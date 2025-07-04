#ifndef LUMEX_NUMBER_GENERATOR_HPP
#define LUMEX_NUMBER_GENERATOR_HPP

#include <cstdint>
#include <iostream>
#include <random>
#include <type_traits>
#include <vector>

/**
 * @brief Enumeration of supported probability distributions.
 */
enum class DistributionType : std::uint8_t
{
  UNIFORM,     ///< Uniform distribution (default)
  NORMAL,      ///< Normal (Gaussian) distribution
  EXPONENTIAL, ///< Exponential distribution
  GAMMA,       ///< Gamma distribution
  BERNOULLI,   ///< Bernoulli distribution (for integral types)
  BINOMIAL,    ///< Binomial distribution (for integral types)
  GEOMETRIC,   ///< Geometric distribution (for integral types)
  POISSON      ///< Poisson distribution (for integral types)
};

/**
 * @brief Universal template-based number generator supporting multiple distributions.
 *
 * This class generates random numbers of various numeric types using different
 * probability distributions. It uses the Mersenne Twister engine and supports
 * both integral and floating-point types with appropriate distribution selection.
 *
 * @tparam T Numeric type (integral or floating-point)
 */
template <typename T> class NumberGenerator final
{
  static_assert(std::is_arithmetic<T>::value && !std::is_same<T, bool>::value && !std::is_same<T, char>::value
                  && !std::is_same<T, wchar_t>::value,
                "Template parameter T must be a numeric type (integral or "
                "floating-point), "
                "but not bool, char, or wchar_t");

private:
  T m_from;                             ///< Lower bound
  T m_to;                               ///< Upper bound
  DistributionType m_distribution_type; ///< Current distribution type
  mutable std::random_device m_rdm_dev; ///< Hardware random device
  mutable std::mt19937 m_engine;        ///< Mersenne Twister engine

  constexpr static std::size_t m_default_count = 100UL; ///< Default count of elements in sequence to generate

  // Default bounds based on type
  static constexpr T
  get_default_min() noexcept
  {
    return T{0};
  }

  static constexpr T
  get_default_max() noexcept
  {
    return std::is_integral<T>::value ? T{m_default_count} : T{1};
  }

  /**
   * @brief Generates a random number using the specified distribution.
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
   * @brief Template helper for uniform distribution generation.
   */
  template <typename U = T>
  typename std::enable_if<std::is_integral<U>::value, U>::type
  generate_uniform(U from_val, U to_val) const
  {
    return std::uniform_int_distribution<U>(from_val, to_val)(m_engine);
  }

  template <typename U = T>
  typename std::enable_if<std::is_floating_point<U>::value, U>::type
  generate_uniform(U from_val, U to_val) const
  {
    return std::uniform_real_distribution<U>(from_val, to_val)(m_engine);
  }

  /**
   * @brief Generate normal distribution (mean, stddev) - safe version with SFINAE.
   */
  template <typename U = T>
  typename std::enable_if<std::is_floating_point<U>::value, U>::type
  generate_normal_safe(U mean, U stddev) const
  {
    return static_cast<U>(
      std::normal_distribution<double>(static_cast<double>(mean), static_cast<double>(stddev))(m_engine));
  }

  template <typename U = T>
  typename std::enable_if<std::is_integral<U>::value, U>::type
  generate_normal_safe(U mean, U stddev) const
  {
    // For integral types, fall back to uniform distribution
    return generate_uniform(mean, stddev);
  }

  /**
   * @brief Generate exponential distribution - safe version with SFINAE.
   */
  template <typename U = T>
  typename std::enable_if<std::is_floating_point<U>::value, U>::type
  generate_exponential_safe(U lambda) const
  {
    return static_cast<U>(std::exponential_distribution<double>(static_cast<double>(lambda))(m_engine));
  }

  template <typename U = T>
  typename std::enable_if<std::is_integral<U>::value, U>::type
  generate_exponential_safe(U lambda) const
  {
    // For integral types, fall back to uniform distribution
    return generate_uniform(static_cast<U>(0), lambda);
  }

  /**
   * @brief Generate gamma distribution (alpha, beta) - safe version with SFINAE.
   */
  template <typename U = T>
  typename std::enable_if<std::is_floating_point<U>::value, U>::type
  generate_gamma_safe(U alpha, U beta) const
  {
    return static_cast<U>(
      std::gamma_distribution<double>(static_cast<double>(alpha), static_cast<double>(beta))(m_engine));
  }

  template <typename U = T>
  typename std::enable_if<std::is_integral<U>::value, U>::type
  generate_gamma_safe(U alpha, U beta) const
  {
    // For integral types, fall back to uniform distribution
    return generate_uniform(alpha, beta);
  }

  /**
   * @brief Generate Bernoulli distribution - safe version with SFINAE.
   */
  template <typename U = T>
  typename std::enable_if<std::is_integral<U>::value, U>::type
  generate_bernoulli_safe(double p_val) const
  {
    return static_cast<U>(std::bernoulli_distribution(p_val)(m_engine));
  }

  template <typename U = T>
  typename std::enable_if<std::is_floating_point<U>::value, U>::type
  generate_bernoulli_safe(double p_val) const
  {
    // For floating-point types, return 0.0 or 1.0
    return static_cast<U>(std::bernoulli_distribution(p_val)(m_engine));
  }

  /**
   * @brief Generate binomial distribution - safe version with SFINAE.
   */
  template <typename U = T>
  typename std::enable_if<std::is_integral<U>::value, U>::type
  generate_binomial_safe(int trials, double p_val) const
  {
    return static_cast<U>(std::binomial_distribution<int>(trials, p_val)(m_engine));
  }

  template <typename U = T>
  typename std::enable_if<std::is_floating_point<U>::value, U>::type
  generate_binomial_safe(int trials, double p_val) const
  {
    // For floating-point types, cast result to floating-point
    return static_cast<U>(std::binomial_distribution<int>(trials, p_val)(m_engine));
  }

  /**
   * @brief Generate geometric distribution - safe version with SFINAE.
   */
  template <typename U = T>
  typename std::enable_if<std::is_integral<U>::value, U>::type
  generate_geometric_safe(double p_val) const
  {
    return static_cast<U>(std::geometric_distribution<int>(p_val)(m_engine));
  }

  template <typename U = T>
  typename std::enable_if<std::is_floating_point<U>::value, U>::type
  generate_geometric_safe(double p_val) const
  {
    // For floating-point types, cast result to floating-point
    return static_cast<U>(std::geometric_distribution<int>(p_val)(m_engine));
  }

  /**
   * @brief Generate Poisson distribution - safe version with SFINAE.
   */
  template <typename U = T>
  typename std::enable_if<std::is_integral<U>::value, U>::type
  generate_poisson_safe(double mean) const
  {
    return static_cast<U>(std::poisson_distribution<int>(mean)(m_engine));
  }

  template <typename U = T>
  typename std::enable_if<std::is_floating_point<U>::value, U>::type
  generate_poisson_safe(double mean) const
  {
    // For floating-point types, cast result to floating-point
    return static_cast<U>(std::poisson_distribution<int>(mean)(m_engine));
  }

  /**
   * @brief Generate normal distribution (mean, stddev).
   */
  T
  generate_normal(T mean, T stddev) const
  {
    static_assert(std::is_floating_point<T>::value, "Normal distribution requires floating-point type");
    return static_cast<T>(
      std::normal_distribution<double>(static_cast<double>(mean), static_cast<double>(stddev))(m_engine));
  }

  /**
   * @brief Generate exponential distribution.
   */
  T
  generate_exponential(T lambda) const
  {
    static_assert(std::is_floating_point<T>::value, "Exponential distribution requires floating-point type");
    return static_cast<T>(std::exponential_distribution<double>(static_cast<double>(lambda))(m_engine));
  }

  /**
   * @brief Generate gamma distribution (alpha, beta).
   */
  T
  generate_gamma(T alpha, T beta) const
  {
    static_assert(std::is_floating_point<T>::value, "Gamma distribution requires floating-point type");
    return static_cast<T>(
      std::gamma_distribution<double>(static_cast<double>(alpha), static_cast<double>(beta))(m_engine));
  }

  /**
   * @brief Generate Bernoulli distribution.
   */
  T
  generate_bernoulli(double p_val) const
  {
    static_assert(std::is_integral<T>::value, "Bernoulli distribution requires integral type");
    return static_cast<T>(std::bernoulli_distribution(p_val)(m_engine));
  }

  /**
   * @brief Generate binomial distribution.
   */
  T
  generate_binomial(int trials, double p_val) const
  {
    static_assert(std::is_integral<T>::value, "Binomial distribution requires integral type");
    return static_cast<T>(std::binomial_distribution<int>(trials, p_val)(m_engine));
  }

  /**
   * @brief Generate geometric distribution.
   */
  T
  generate_geometric(double p_val) const
  {
    static_assert(std::is_integral<T>::value, "Geometric distribution requires integral type");
    return static_cast<T>(std::geometric_distribution<int>(p_val)(m_engine));
  }

  /**
   * @brief Generate Poisson distribution.
   */
  T
  generate_poisson(double mean) const
  {
    static_assert(std::is_integral<T>::value, "Poisson distribution requires integral type");
    return static_cast<T>(std::poisson_distribution<int>(mean)(m_engine));
  }

public:
  /**
   * @brief Constructs NumberGenerator with default bounds and uniform distribution.
   */
  NumberGenerator()
      : m_from(get_default_min()),
        m_to(get_default_max()),
        m_distribution_type(DistributionType::UNIFORM),
        m_engine(m_rdm_dev.entropy() > 0.0 ? m_rdm_dev() : static_cast<std::mt19937::result_type>(time(nullptr)))
  {}

  /**
   * @brief Constructs NumberGenerator with specified bounds.
   *
   * @param from_val For UNIFORM: lower bound; For BERNOULLI: probability; For NORMAL: mean; For EXPONENTIAL: lambda
   * @param to_val For UNIFORM: upper bound; For NORMAL: std deviation; For BINOMIAL: number of trials
   * @param dist_type Distribution type to use
   *
   * @note For non-uniform distributions, parameter meanings differ. Use get_number() for explicit control.
   */
  NumberGenerator(T from_val, T to_val, DistributionType dist_type = DistributionType::UNIFORM)
      : m_from(from_val > to_val ? to_val : from_val),
        m_to(from_val > to_val ? from_val : to_val),
        m_distribution_type(dist_type),
        m_engine(m_rdm_dev.entropy() > 0.0 ? m_rdm_dev() : static_cast<std::mt19937::result_type>(time(nullptr)))
  {}

  /**
   * @brief Default destructor.
   */
  ~NumberGenerator() = default;

  // Delete copy constructor and assignment operator for thread safety
  NumberGenerator(NumberGenerator const &)            = delete;
  NumberGenerator &operator=(NumberGenerator const &) = delete;

  /**
   * @brief Generates a random number using configured bounds and distribution.
   */
  T
  operator()() const
  {
    return generate_number(m_from, m_to, m_distribution_type);
  }

  /**
   * @brief Generates a random number with specified bounds.
   */
  T
  operator()(T from_val, T to_val) const
  {
    return generate_number(from_val, to_val, m_distribution_type);
  }

  /**
   * @brief Generates a random number with specified bounds and distribution.
   */
  T
  operator()(T from_val, T to_val, DistributionType dist_type) const
  {
    return generate_number(from_val, to_val, dist_type);
  }

  /**
   * @brief Gets a random number with specified parameters.
   */
  T
  get_number(T from_val = get_default_min(), T to_val = get_default_max(),
             DistributionType dist_type = DistributionType::UNIFORM) const
  {
    return generate_number(from_val, to_val, dist_type);
  }

  /**
   * @brief Sets the lower bound.
   */
  void
  set_lower_bound(T val) noexcept
  {
    m_from = val;
  }

  /**
   * @brief Sets the upper bound.
   */
  void
  set_upper_bound(T val) noexcept
  {
    m_to = val;
  }

  /**
   * @brief Sets both bounds.
   */
  void
  set_bounds(T from_val, T to_val) noexcept
  {
    if(from_val > to_val) std::swap(from_val, to_val);
    m_from = from_val;
    m_to   = to_val;
  }

  /**
   * @brief Sets the distribution type.
   */
  void
  set_distribution(DistributionType dist_type) noexcept
  {
    m_distribution_type = dist_type;
  }

  /**
   * @brief Generates a sequence of random numbers.
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
      sequence.reserve(count);

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
   * @brief Gets current lower bound.
   */
  T
  get_lower_bound() const noexcept
  {
    return m_from;
  }

  /**
   * @brief Gets current upper bound.
   */
  T
  get_upper_bound() const noexcept
  {
    return m_to;
  }

  /**
   * @brief Gets current distribution type.
   */
  DistributionType
  get_distribution() const noexcept
  {
    return m_distribution_type;
  }
};

// Type aliases for common use cases
using IntGenerator    = NumberGenerator<int>;
using LongGenerator   = NumberGenerator<long long>;
using FloatGenerator  = NumberGenerator<float>;
using DoubleGenerator = NumberGenerator<double>;

// Backward compatibility alias
using RealNumberGeneratorHost = DoubleGenerator;

#endif // !LUMEX_NUMBER_GENERATOR_HPP
