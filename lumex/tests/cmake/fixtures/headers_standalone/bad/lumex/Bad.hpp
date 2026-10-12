// Uses std::size_t without <cstddef>: compiles only after a neighbour has
// included it.
#ifndef LUMEX_BAD_HPP
#define LUMEX_BAD_HPP

namespace lumex
{
inline std::size_t
bad_size ()
{
  return 3;
}
} // namespace lumex

#endif // !LUMEX_BAD_HPP
