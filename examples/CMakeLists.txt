cmake_minimum_required(VERSION 3.28)

project(pollcoro_examples)

add_executable(fib fib.cc)

target_link_libraries(fib PRIVATE pollcoro::pollcoro)
set_target_properties(fib PROPERTIES CXX_STANDARD 20)

add_executable(cc17 cc17.cc)
target_link_libraries(cc17 PRIVATE pollcoro::pollcoro)
set_target_properties(cc17 PROPERTIES CXX_STANDARD 17)
target_compile_options(cc17 PRIVATE 
    $<$<CXX_COMPILER_ID:GNU>:-fcoroutines>
    $<$<CXX_COMPILER_ID:Clang>:-fcoroutines-ts>
    $<$<CXX_COMPILER_ID:MSVC>:/await>
)

add_executable(thread_callback thread_callback.cc)
target_link_libraries(thread_callback PRIVATE pollcoro::pollcoro)
set_target_properties(thread_callback PROPERTIES CXX_STANDARD 20)

add_executable(modules modules.cc)
target_link_libraries(modules PRIVATE pollcoro::module)
set_target_properties(modules PROPERTIES 
    CXX_STANDARD 20 
    CXX_SCAN_FOR_MODULES ON
)

add_library(c_interop_impl SHARED c_interop_impl.cc)
target_link_libraries(c_interop_impl PRIVATE pollcoro::pollcoro)
set_target_properties(c_interop_impl PROPERTIES CXX_STANDARD 20)

add_executable(c_interop c_interop.c)
target_link_libraries(c_interop PRIVATE c_interop_impl)
set_target_properties(c_interop PROPERTIES C_STANDARD 11 LANGUAGE C)
