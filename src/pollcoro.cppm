module;

#include <algorithm>
#include <coroutine>
#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <stdint.h>

#define POLLCORO_MODULE_EXPORT

export module pollcoro;

#include <pollcoro/waker.hpp>
#include <pollcoro/awaitable.hpp>
#include <pollcoro/concepts.hpp>
#include <pollcoro/yield.hpp>
#include <pollcoro/ref.hpp>
#include <pollcoro/single_event.hpp>
#include <pollcoro/generic.hpp>
#include <pollcoro/task.hpp>
#include <pollcoro/block_on.hpp>
#include <pollcoro/wait_first.hpp>
#include <pollcoro/wait_all.hpp>
#include <pollcoro/map.hpp>
#include <pollcoro/stream.hpp>
#include <pollcoro/iter.hpp>
#include <pollcoro/next.hpp>
#include <pollcoro/take.hpp>
#include <pollcoro/stream_awaitable.hpp>
#include <pollcoro/sleep.hpp>
