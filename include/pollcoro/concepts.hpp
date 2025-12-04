#pragma once

#if defined(__cpp_concepts) && __cpp_concepts >= 201907L && \
    (!defined(POLLCORO_USE_CONCEPTS) || POLLCORO_USE_CONCEPTS)
#define POLLCORO_CONCEPT(...) __VA_ARGS__
#define POLLCORO_USE_CONCEPTS 1
#include <concepts>
#else
#define POLLCORO_CONCEPT(...) typename
#define POLLCORO_USE_CONCEPTS 0
#endif
