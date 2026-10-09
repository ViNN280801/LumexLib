#include <iostream>
#include <memory>
#include <string>

#include "lumex/core/smart_ptr/LumexSmartPtr"

// The names of the module live in lumex::core::smart_ptr and are not injected
// into the global namespace: shared_ptr here is the module's class, next to
// std::shared_ptr, never an alias of it.
namespace sp = lumex::core::smart_ptr;

namespace
{
struct Shape
{
  virtual ~Shape () {}
  virtual double area () const = 0;
};

struct Square : Shape
{
  explicit Square (double side) : side_ (side) {}
  double
  area () const override
  {
    return side_ * side_;
  }

private:
  double side_;
};

// A deleter that reports when it runs.
struct Reporter
{
  void
  operator() (int *value) const
  {
    std::cout << "deleter: dropping " << *value << '\n';
    delete value;
  }
};

// A class that can hand out owners of itself.
struct Session : sp::enable_shared_from_this<Session>
{
  sp::shared_ptr<Session>
  self ()
  {
    return shared_from_this ();
  }
};
} // namespace

int
main ()
{
  std::cout << "=== smart_ptr: shared_ptr and weak_ptr with an own control "
               "block ===\n\n";

  std::cout << "--- 1. Shared ownership ---\n";
  sp::shared_ptr<std::string> name = sp::make_shared<std::string> ("lumex");
  sp::shared_ptr<std::string> copy = name;
  std::cout << "*name=" << *name << " use_count=" << name.use_count ()
            << " same=" << (name == copy ? "yes" : "no")
            << " sizeof(shared_ptr)=" << sizeof (name) << '\n';
  copy.reset ();
  std::cout << "after reset: use_count=" << name.use_count () << '\n';

  std::cout << "\n--- 2. Deleters, base classes, casts ---\n";
  {
    sp::shared_ptr<int> counted (new int (7), Reporter ());
    std::cout << "get_deleter<Reporter>: "
              << (sp::get_deleter<Reporter> (counted) != nullptr ? "found"
                                                                 : "none")
              << '\n';
  } // the deleter runs here
  sp::shared_ptr<Shape> shape = sp::make_shared<Square> (3.0);
  sp::shared_ptr<Square> square = sp::dynamic_pointer_cast<Square> (shape);
  std::cout << "area=" << shape->area ()
            << " cast ok=" << (square ? "yes" : "no") << " same owner="
            << (!shape.owner_before (square) && !square.owner_before (shape)
                    ? "yes"
                    : "no")
            << '\n';

  std::cout << "\n--- 3. Weak pointers ---\n";
  sp::weak_ptr<std::string> observer = name;
  std::cout << "expired=" << (observer.expired () ? "yes" : "no")
            << " lock()=" << *observer.lock () << '\n';
  name.reset ();
  std::cout << "after the last owner went: expired="
            << (observer.expired () ? "yes" : "no") << " lock() is "
            << (observer.lock () ? "set" : "empty") << '\n';
  try
    {
      sp::shared_ptr<std::string> promoted (observer);
    }
  catch (std::bad_weak_ptr const &)
    {
      std::cout << "shared_ptr (weak_ptr) threw bad_weak_ptr\n";
    }

  std::cout << "\n--- 4. enable_shared_from_this ---\n";
  sp::shared_ptr<Session> session = sp::make_shared<Session> ();
  sp::shared_ptr<Session> again = session->self ();
  std::cout << "self() shares the owner: use_count=" << session.use_count ()
            << '\n';

  std::cout << "\n--- 5. Arrays ---\n";
  sp::shared_ptr<int[]> numbers (new int[4]);
  for (int i = 0; i < 4; ++i)
    numbers[i] = i * i;
  std::cout << "numbers[3]=" << numbers[3] << '\n';

  std::cout
      << "\n--- 6. Explicit conversion to and from std::shared_ptr ---\n";
  std::shared_ptr<int> standard = std::make_shared<int> (11);
  sp::shared_ptr<int> wrapped = sp::from_std (standard);
  std::shared_ptr<int> back = sp::to_std (wrapped);
  std::cout << "wrapped=" << *wrapped << " round trip gives the original: "
            << (back == standard ? "yes" : "no") << '\n';
  return 0;
}
