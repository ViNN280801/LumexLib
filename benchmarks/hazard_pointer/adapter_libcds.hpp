// libcds HP as a series of the hazard pointer benchmark. Compiled only when
// LUMEX_HAZARD_POINTER_BENCH_LIBCDS_DIR points at a libcds checkout (libcds is
// Boost Software License 1.0 and is not part of this repository). The adapter
// gives libcds the shape of the other series: holder_type protects a pointer
// and resets it, node is retired through cds::gc::HP::retire.
//
// libcds needs cds::Initialize () once, a cds::gc::HP object for the program
// and cds::threading::Manager::attachThread () in every thread that touches
// it; this adapter does the first two in a static object and the third in the
// holder (a thread attaches when it makes its first holder).

#ifndef LUMEX_BENCHMARKS_HAZARD_POINTER_ADAPTER_LIBCDS_HPP
#define LUMEX_BENCHMARKS_HAZARD_POINTER_ADAPTER_LIBCDS_HPP

#include <atomic>
#include <cstdint>

#include <cds/gc/hp.h>
#include <cds/init.h>

namespace libcds_adapter
{
struct runtime
{
  runtime ()
  {
    cds::Initialize ();
    gc = new cds::gc::HP (16, 4, 4);
  }
  cds::gc::HP *gc;
};

inline runtime &
instance ()
{
  static runtime value;
  return value;
}

struct thread_guard
{
  thread_guard ()
  {
    instance ();
    cds::threading::Manager::attachThread ();
  }
  ~thread_guard () { cds::threading::Manager::detachThread (); }
};

inline void
ensure_attached ()
{
  static thread_local thread_guard guard;
  (void) guard;
}
} // namespace libcds_adapter

struct libcds_series
{
  static char const *key () { return "libcds_hp"; }
  static char const *label () { return "libcds HP"; }
  static char const *detail () { return "cds::gc::HP, per-thread arrays"; }

  struct node
  {
    explicit node (std::uint64_t v) : value (v) {}

    void
    retire ()
    {
      cds::gc::HP::retire<disposer> (this);
    }

    struct disposer
    {
      void operator() (node *p) const { delete p; }
    };

    std::uint64_t value;
  };

  struct holder_type
  {
    holder_type ()
    {
      libcds_adapter::ensure_attached ();
    }

    node *
    protect (std::atomic<node *> const &source)
    {
      return guard.protect (source);
    }

    void reset () { guard.clear (); }

    cds::gc::HP::Guard guard;
  };

  static void settle () { cds::gc::hp::smr::scan (); }
};

#endif
