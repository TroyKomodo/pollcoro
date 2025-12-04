module;

#include <algorithm>
#include <array>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <tuple>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

#define POLLCORO_MODULE_EXPORT

export module pollcoro;

// Core types (order matters - dependencies first)
#include "pollcoro/waker.hpp"
#include "pollcoro/is_blocking.hpp"
#include "pollcoro/awaitable.hpp"
#include "pollcoro/stream_awaitable.hpp"
#include "pollcoro/concepts.hpp"

// Basic awaitables
#include "pollcoro/yield.hpp"
#include "pollcoro/ready.hpp"
#include "pollcoro/pending.hpp"

// Awaitable utilities
#include "pollcoro/ref.hpp"
#include "pollcoro/generic.hpp"
#include "pollcoro/map.hpp"
#include "pollcoro/single_event.hpp"
#include "pollcoro/sleep.hpp"

// Coroutine types
#include "pollcoro/task.hpp"
#include "pollcoro/stream.hpp"

// Execution
#include "pollcoro/block_on.hpp"
#include "pollcoro/wait_first.hpp"
#include "pollcoro/wait_all.hpp"

// Stream sources
#include "pollcoro/iter.hpp"
#include "pollcoro/range.hpp"       
#include "pollcoro/repeat.hpp"
#include "pollcoro/empty.hpp"
#include "pollcoro/enumerate.hpp"

// Stream combinators
#include "pollcoro/next.hpp"
#include "pollcoro/take.hpp"
#include "pollcoro/take_while.hpp"
#include "pollcoro/skip.hpp"
#include "pollcoro/skip_while.hpp"
#include "pollcoro/chain.hpp"
#include "pollcoro/zip.hpp"
#include "pollcoro/flatten.hpp"
#include "pollcoro/window.hpp"

// Stream consumers
#include "pollcoro/fold.hpp"
#include "pollcoro/last.hpp"
#include "pollcoro/nth.hpp"
#include "pollcoro/sync_iter.hpp"
