export module pollcoro;

// Core types (order matters - dependencies first)
export import :waker;
export import :is_blocking;
export import :awaitable;
export import :stream_awaitable;

// Basic awaitables
export import :yield;
export import :ready;
export import :pending;

// Awaitable utilities
export import :ref;
export import :generic;
export import :map;
export import :single_event;
export import :sleep;

// Allocator
export import :allocator;

// Coroutine types
export import :detail_promise;
export import :task;
export import :stream;

// Execution
export import :block_on;
export import :wait_first;
export import :wait_all;

// Stream sources
export import :iter;
export import :range;
export import :repeat;
export import :empty;
export import :enumerate;

// Stream combinators
export import :next;
export import :take;
export import :take_while;
export import :skip;
export import :skip_while;
export import :chain;
export import :zip;
export import :flatten;
export import :window;

// Stream consumers
export import :fold;
export import :last;
export import :nth;
export import :sync_iter;

// Interop
export import :co_awaitable;
export import :resumable;
