// A port of the engine of the libc++ pull request llvm/llvm-project#218218 as
// a series of the hazard pointer benchmark. The pull request is not merged and
// its code is not part of this repository: LUMEX_HAZARD_POINTER_BENCH_LLVM_DIR
// must point at a directory with hazard_pointer_port.hpp, a port of the engine
// (libcxx/src/hazard_pointer.cpp and the header) to a normal translation unit
// in the namespace llvm_port with the classes hazard_pointer,
// hazard_pointer_obj_base and the function make_hazard_pointer. Only the names
// are used here.

#ifndef LUMEX_BENCHMARKS_HAZARD_POINTER_ADAPTER_LLVM_HPP
#define LUMEX_BENCHMARKS_HAZARD_POINTER_ADAPTER_LLVM_HPP

#include <atomic>
#include <cstdint>

#include "hazard_pointer_port.hpp"

struct llvm_series
{
  static char const *key () { return "llvm_port"; }
  static char const *label () { return "libc++ PR engine (port)"; }
  static char const *detail () { return "llvm-project#218218, asymmetric fence where available"; }

  struct node : llvm_port::hazard_pointer_obj_base<node>
  {
    explicit node (std::uint64_t v) : value (v) {}
    std::uint64_t value;
  };

  struct holder_type
  {
    llvm_port::hazard_pointer holder;
    holder_type () : holder (llvm_port::make_hazard_pointer ()) {}
    node *protect (std::atomic<node *> const &source) { return holder.protect (source); }
    void reset () { holder.reset_protection (); }
  };

  static void settle () {}
};

#endif
