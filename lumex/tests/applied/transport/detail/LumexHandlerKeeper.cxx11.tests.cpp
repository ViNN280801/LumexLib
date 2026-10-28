// The shutdown service of the transport (detail/LumexHandlerKeeper.hpp): an
// object that keeps completion handlers in its members is enlisted with the
// execution_context of its executor, and the shutdown of the context makes it
// destroy them uncalled. Keepers here are plain objects, so the service is
// checked apart from the transports that use it.

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/transport/detail/LumexAsio.hpp"
#include "lumex/applied/transport/detail/LumexHandlerKeeper.hpp"

namespace
{
namespace tr = lumex::applied::transport;

// Records the order of the drops and how often each keeper was asked.
struct drop_log
{
  std::vector<int> order;
};

class recording_keeper : public tr::detail::handler_keeper
{
public:
  recording_keeper (int id, drop_log *log) : id_ (id), log_ (log), drops_ (0)
  {
  }

  void
  drop_kept_handlers () LUMEX_NOEXCEPT override
  {
    ++drops_;
    if (log_ != nullptr)
      log_->order.push_back (id_);
  }

  int
  drops () const
  {
    return drops_;
  }

private:
  int id_;
  drop_log *log_;
  int drops_;
};

// Enlists `keeper` the way an implementation does (flag + executor).
void
enlist (std::atomic<bool> &flag, asio::io_context &context,
        std::shared_ptr<tr::detail::handler_keeper> const &keeper)
{
  tr::detail::enlist_keeper (flag, context.get_executor (), keeper);
}
} // namespace

TEST (LumexHandlerKeeperTest,
      GivenAnEnlistedKeeper_WhenTheContextIsDestroyed_ThenItIsAskedToDropOnce)
{
  std::shared_ptr<recording_keeper> const keeper
      = std::make_shared<recording_keeper> (1, nullptr);
  std::atomic<bool> flag (false);
  {
    asio::io_context context;
    enlist (flag, context, keeper);
    EXPECT_TRUE (flag.load ());
    EXPECT_EQ (keeper->drops (), 0) << "asked before the shutdown";
  }
  // io_context's destructor shuts the services down twice (its own, then the
  // execution_context's): the keeper is told once.
  EXPECT_EQ (keeper->drops (), 1);
}

TEST (LumexHandlerKeeperTest,
      GivenTheFlagIsSet_WhenEnlistedAgain_ThenTheKeeperIsNotEnlistedTwice)
{
  std::shared_ptr<recording_keeper> const keeper
      = std::make_shared<recording_keeper> (1, nullptr);
  std::atomic<bool> flag (false);
  {
    asio::io_context context;
    enlist (flag, context, keeper);
    enlist (flag, context, keeper);
    enlist (flag, context, keeper);
  }
  // One entry: told once, not three times.
  EXPECT_EQ (keeper->drops (), 1);
}

TEST (
    LumexHandlerKeeperTest,
    GivenSeveralKeepers_WhenTheContextIsDestroyed_ThenTheNewestIsDroppedFirst)
{
  drop_log log;
  std::shared_ptr<recording_keeper> const first
      = std::make_shared<recording_keeper> (1, &log);
  std::shared_ptr<recording_keeper> const second
      = std::make_shared<recording_keeper> (2, &log);
  std::shared_ptr<recording_keeper> const third
      = std::make_shared<recording_keeper> (3, &log);
  std::atomic<bool> a (false);
  std::atomic<bool> b (false);
  std::atomic<bool> c (false);
  {
    asio::io_context context;
    enlist (a, context, first);
    enlist (b, context, second);
    enlist (c, context, third);
  }
  ASSERT_EQ (log.order.size (), 3u);
  EXPECT_EQ (log.order[0], 3);
  EXPECT_EQ (log.order[1], 2);
  EXPECT_EQ (log.order[2], 1);
}

TEST (LumexHandlerKeeperTest,
      GivenAKeeperThatDiedFirst_WhenTheContextIsDestroyed_ThenNothingIsCalled)
{
  std::atomic<bool> flag (false);
  asio::io_context context;
  {
    std::shared_ptr<recording_keeper> const keeper
        = std::make_shared<recording_keeper> (1, nullptr);
    enlist (flag, context, keeper);
  }
  // The registry held the keeper weakly: destroying the context after the
  // keeper is gone calls nothing (a call would be a use after free).
  SUCCEED ();
}

TEST (
    LumexHandlerKeeperTest,
    GivenManyShortLivedKeepers_WhenTheContextLivesOn_ThenALiveOneIsStillDropped)
{
  std::shared_ptr<recording_keeper> const survivor
      = std::make_shared<recording_keeper> (0, nullptr);
  std::atomic<bool> survivor_flag (false);
  {
    asio::io_context context;
    enlist (survivor_flag, context, survivor);
    for (int i = 0; i < 5000; ++i)
      {
        std::atomic<bool> flag (false);
        std::shared_ptr<recording_keeper> const gone
            = std::make_shared<recording_keeper> (i, nullptr);
        enlist (flag, context, gone);
      }
  }
  // The dead entries were swept as the list grew; the live keeper that
  // came first was not lost with them.
  EXPECT_EQ (survivor->drops (), 1);
}

TEST (LumexHandlerKeeperTest,
      GivenTwoContexts_WhenOneIsDestroyed_ThenOnlyItsKeepersAreDropped)
{
  std::shared_ptr<recording_keeper> const mine
      = std::make_shared<recording_keeper> (1, nullptr);
  std::shared_ptr<recording_keeper> const other
      = std::make_shared<recording_keeper> (2, nullptr);
  std::atomic<bool> a (false);
  std::atomic<bool> b (false);
  asio::io_context longer;
  {
    asio::io_context shorter;
    enlist (a, shorter, mine);
    enlist (b, longer, other);
  }
  EXPECT_EQ (mine->drops (), 1);
  EXPECT_EQ (other->drops (), 0);
}

TEST (
    LumexHandlerKeeperTest,
    GivenAKeeperThatEnlistsAnotherDuringTheShutdown_ThenNothingIsLeftOrCrashes)
{
  // A dropped handler may build a transport and start an operation: the
  // registry of a context that is shutting down ignores the new keeper.
  struct enlisting_keeper : tr::detail::handler_keeper
  {
    enlisting_keeper (asio::io_context *context,
                      std::shared_ptr<recording_keeper> late)
        : context_ (context), late_ (std::move (late)), flag_ (false)
    {
    }

    void
    drop_kept_handlers () LUMEX_NOEXCEPT override
    {
      try
        {
          tr::detail::enlist_keeper (flag_, context_->get_executor (), late_);
        }
      catch (...)
        {
        }
    }

    asio::io_context *context_;
    std::shared_ptr<recording_keeper> late_;
    std::atomic<bool> flag_;
  };
  std::shared_ptr<recording_keeper> const late
      = std::make_shared<recording_keeper> (9, nullptr);
  std::atomic<bool> flag (false);
  std::shared_ptr<enlisting_keeper> first;
  {
    asio::io_context context;
    first = std::make_shared<enlisting_keeper> (&context, late);
    enlist (flag, context, first);
  }
  // Not asked: it came after the shutdown began.
  EXPECT_EQ (late->drops (), 0);
}
