#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

#include "lumex/core/hazard_pointer/LumexHazardPointer"

// The names of the module live in lumex::core::hazard_pointer and are not
// injected into the global namespace.
namespace hp = lumex::core::hazard_pointer;

namespace
{
// An object that hazard pointers protect derives from hazard_pointer_obj_base
// with itself as the first argument. The destructor tells the example when the
// object is really gone.
struct Config : hp::hazard_pointer_obj_base<Config>
{
  explicit Config (int the_limit) : limit (the_limit) { ++alive; }
  ~Config () { --alive; }

  int limit;
  static std::atomic<int> alive;
};

std::atomic<int> Config::alive (0);

std::atomic<Config *> g_config (nullptr);
} // namespace

int
main ()
{
  std::cout << "=== hazard_pointer: safe reading of objects that are replaced "
               "===\n\n";

  std::cout << "--- 1. Protect, use, release ---\n";
  g_config.store (new Config (10));
  {
    // A holder owns one hazard slot. protect () loads the pointer, announces
    // it and checks that the pointer is still there.
    hp::hazard_pointer holder = hp::make_hazard_pointer ();
    Config *config = holder.protect (g_config);
    std::cout << "limit read under protection: " << config->limit << '\n';
  } // the protection ends with the holder

  std::cout << "\n--- 2. Retire: deleted once nobody protects it ---\n";
  {
    hp::hazard_pointer reader = hp::make_hazard_pointer ();
    Config *seen = reader.protect (g_config);

    // The writer replaces the object and retires the old one.
    Config *old = g_config.exchange (new Config (20));
    old->retire ();
    hp::clean_up (); // a pass now (an extension; retire also triggers them)
    std::cout << "alive while a reader holds the old one: "
              << Config::alive.load ()
              << " (the old limit is still readable: " << seen->limit << ")\n";

    reader.reset_protection ();
    hp::clean_up ();
    std::cout << "alive after the reader let go: " << Config::alive.load ()
              << '\n';
  }

  std::cout
      << "\n--- 3. try_protect: one attempt, and what to do on failure ---\n";
  {
    hp::hazard_pointer holder = hp::make_hazard_pointer ();
    Config *guess = g_config.load ();
    Config *fresh = new Config (30);
    Config *replaced = g_config.exchange (fresh);
    replaced->retire ();

    // guess is stale now: try_protect fails, guess receives the new pointer
    // and nothing stays protected.
    bool const ok = holder.try_protect (guess, g_config);
    std::cout << "try_protect on a stale pointer: " << (ok ? "true" : "false")
              << ", ptr now has limit " << guess->limit << '\n';
  }

  std::cout
      << "\n--- 4. Hand-over-hand: reset_protection (ptr) and swap ---\n";
  {
    hp::hazard_pointer first = hp::make_hazard_pointer ();
    hp::hazard_pointer second = hp::make_hazard_pointer ();
    Config *a = first.protect (g_config);
    // The second holder protects the same object (it is already protected by
    // the first), then the first is free for the next step.
    second.reset_protection (a);
    swap (first, second); // no protection starts or ends
    std::cout << "both holders protect limit " << a->limit
              << " during the hand over\n";
  }

  std::cout << "\n--- 5. Many holders at once: the batch functions ---\n";
  {
    std::vector<hp::hazard_pointer> holders (4);
    hp::make_hazard_pointer_batch (
        lumex::core::span::view::span<hp::hazard_pointer> (holders.data (),
                                                           holders.size ()));
    std::cout << "holders filled: ";
    for (hp::hazard_pointer const &holder : holders)
      {
        std::cout << (holder.empty () ? "empty " : "owns ");
      }
    std::cout << '\n';
    hp::clear_hazard_pointer_batch (
        lumex::core::span::view::span<hp::hazard_pointer> (holders.data (),
                                                           holders.size ()));
  }

  Config *last = g_config.exchange (nullptr);
  last->retire ();
  hp::clean_up ();
  std::cout << "\nalive at the end: " << Config::alive.load () << '\n';
  return Config::alive.load () == 0 ? 0 : 1;
}
