// Built with exceptions: an operation of expected whose constructor throws
// leaves the old contents intact and every object is destroyed once, whichever
// unit the linker meets first, while the unit of lib.cpp (no exceptions)
// instantiates the same operations. Exit code 0 on success.
#include <cstdio>
#include <utility>

#include "lumex/core/expected/Expected"

#include "shared.hpp"

int g_live = 0;
int g_destroyed_twice = 0;
int g_countdown = 0;

[[noreturn]] void
shared_fail ()
{
  g_countdown = 0;
  throw 7;
}

namespace
{
int g_failures = 0;

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          std::fprintf (stderr, "FAILED line %d: %s\n", __LINE__, #cond);     \
          ++g_failures;                                                       \
        }                                                                     \
    }                                                                         \
  while (0)

// Runs `operation` with the k-th construction throwing; returns whether it
// threw.
template <typename Operation>
bool
throws_at (int k, Operation operation)
{
  g_countdown = k;
  bool threw = false;
  try
    {
      operation ();
    }
  catch (int)
    {
      threw = true;
    }
  g_countdown = 0;
  return threw;
}

void
assign_value_over_error ()
{
  value_thrower_t x (lx::unexpect, 5);
  thrower_t source (7);
  CHECK (throws_at (1, [&] { x = std::move (source); }));
  CHECK (!x.has_value () && x.error () == 5);
}

void
copy_assign_over_error ()
{
  value_thrower_t x (lx::unexpect, 5);
  value_thrower_t y (lx::in_place, 7);
  CHECK (throws_at (1, [&] { x = y; }));
  CHECK (!x.has_value () && x.error () == 5);
  CHECK (y.has_value () && y.value ().v == 7);
}

void
move_assign_over_error ()
{
  value_thrower_t x (lx::unexpect, 5);
  value_thrower_t y (lx::in_place, 7);
  CHECK (throws_at (1, [&] { x = std::move (y); }));
  CHECK (!x.has_value () && x.error () == 5);
}

void
emplace_error_over_value ()
{
  error_thrower_t x (3);
  CHECK (throws_at (1, [&] { x.emplace_error (9); }));
  CHECK (x.has_value () && x.value () == 3);
}

void
emplace_error_over_error ()
{
  error_thrower_t x (lx::unexpect, thrower_t (4));
  // The first construction moves the old error aside, the second builds the
  // new one and throws, the third (not throwing) puts the old one back.
  CHECK (throws_at (2, [&] { x.emplace_error (9); }));
  CHECK (!x.has_value () && x.error ().v == 4);
}

void
swap_value_with_error_nothrow_error ()
{
  value_thrower_t x (lx::in_place, 7);
  value_thrower_t y (lx::unexpect, 5);
  CHECK (throws_at (1, [&] { x.swap (y); }));
  CHECK (x.has_value () && x.value ().v == 7);
  CHECK (!y.has_value () && y.error () == 5);
}

void
swap_value_with_error_throwing_error ()
{
  error_thrower_t x (1);
  error_thrower_t y (lx::unexpect, thrower_t (6));
  CHECK (throws_at (1, [&] { x.swap (y); }));
  CHECK (x.has_value () && x.value () == 1);
  CHECK (!y.has_value () && y.error ().v == 6);
}

void
copy_assign_error_over_value ()
{
  error_thrower_t x (3);
  error_thrower_t y (lx::unexpect, thrower_t (6));
  CHECK (throws_at (1, [&] { x = y; }));
  CHECK (x.has_value () && x.value () == 3);
  CHECK (!y.has_value () && y.error ().v == 6);
}

void
move_assign_error_over_value ()
{
  error_thrower_t x (3);
  error_thrower_t y (lx::unexpect, thrower_t (6));
  CHECK (throws_at (1, [&] { x = std::move (y); }));
  CHECK (x.has_value () && x.value () == 3);
}

// operator= (unexpected) over a value of expected<int, thrower_t>: the guard
// is over the int.
void
assign_unexpected_over_value ()
{
  error_thrower_t x (3);
  unexpected_thrower_t const source (lx::in_place, 6);
  CHECK (throws_at (1, [&] { x = source; }));
  CHECK (x.has_value () && x.value () == 3);
  unexpected_thrower_t movable (lx::in_place, 7);
  CHECK (throws_at (1, [&] { x = std::move (movable); }));
  CHECK (x.has_value () && x.value () == 3);
}

// The other direction: the object that holds the error is the left one.
void
swap_error_with_value ()
{
  error_thrower_t x (1);
  error_thrower_t y (lx::unexpect, thrower_t (6));
  CHECK (throws_at (1, [&] { y.swap (x); }));
  CHECK (x.has_value () && x.value () == 1);
  CHECK (!y.has_value () && y.error ().v == 6);
  value_thrower_t a (lx::in_place, 7);
  value_thrower_t b (lx::unexpect, 5);
  CHECK (throws_at (1, [&] { b.swap (a); }));
  CHECK (a.has_value () && a.value ().v == 7);
  CHECK (!b.has_value () && b.error () == 5);
}

// The free swap, called unqualified (argument-dependent lookup), both ways.
void
free_swap ()
{
  error_thrower_t x (1);
  error_thrower_t y (lx::unexpect, thrower_t (6));
  CHECK (throws_at (1, [&] { swap (x, y); }));
  CHECK (x.has_value () && x.value () == 1);
  CHECK (!y.has_value () && y.error ().v == 6);
  CHECK (throws_at (1, [&] { swap (y, x); }));
  CHECK (x.has_value () && x.value () == 1);
  CHECK (!y.has_value () && y.error ().v == 6);
  value_thrower_t a (lx::in_place, 7);
  value_thrower_t b (lx::unexpect, 5);
  CHECK (throws_at (1, [&] { swap (a, b); }));
  CHECK (a.has_value () && a.value ().v == 7);
  CHECK (!b.has_value () && b.error () == 5);
  CHECK (throws_at (1, [&] { swap (b, a); }));
  CHECK (a.has_value () && a.value ().v == 7);
  CHECK (!b.has_value () && b.error () == 5);
  void_thrower_t u;
  void_thrower_t v (lx::unexpect, thrower_t (8));
  CHECK (throws_at (1, [&] { swap (u, v); }));
  CHECK (u.has_value () && !v.has_value () && v.error ().v == 8);
  CHECK (throws_at (1, [&] { swap (v, u); }));
  CHECK (u.has_value () && !v.has_value () && v.error ().v == 8);
}

void
void_operations ()
{
  void_thrower_t x;
  CHECK (throws_at (1, [&] { x.emplace_error (9); }));
  CHECK (x.has_value ());
  void_thrower_t y (lx::unexpect, thrower_t (6));
  CHECK (throws_at (1, [&] { x.swap (y); }));
  CHECK (x.has_value () && !y.has_value () && y.error ().v == 6);
  CHECK (throws_at (1, [&] { y.swap (x); }));
  CHECK (x.has_value () && !y.has_value () && y.error ().v == 6);
  // An error over an error.
  CHECK (throws_at (2, [&] { y.emplace_error (8); }));
  CHECK (!y.has_value () && y.error ().v == 6);
}
} // namespace

int
main ()
{
  // The unit without exceptions instantiates the same operations first.
  CHECK (lib_exercise () == 0);
  CHECK (g_live == 0);

  assign_value_over_error ();
  copy_assign_over_error ();
  move_assign_over_error ();
  copy_assign_error_over_value ();
  move_assign_error_over_value ();
  assign_unexpected_over_value ();
  swap_error_with_value ();
  free_swap ();
  emplace_error_over_value ();
  emplace_error_over_error ();
  swap_value_with_error_nothrow_error ();
  swap_value_with_error_throwing_error ();
  void_operations ();

  // Every object was destroyed once: none lives and none went twice.
  CHECK (g_live == 0);
  CHECK (g_destroyed_twice == 0);
  if (g_failures != 0)
    {
      std::fprintf (stderr, "%d check(s) failed\n", g_failures);
      return 1;
    }
  std::puts ("expected mixed_strong_guarantee: ok");
  return 0;
}
