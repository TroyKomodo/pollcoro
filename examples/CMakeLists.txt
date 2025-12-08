cmake_minimum_required(VERSION 3.28)

include(../cmake/CPM.cmake)

# See https://github.com/cpm-cmake/CPM.cmake/issues/577
set(CPM_SOURCE_CACHE ${CMAKE_CURRENT_BINARY_DIR}/.cpm-cache)

project(pollcoro_examples)

add_executable(fib fib.cc)
target_link_libraries(fib PRIVATE pollcoro::pollcoro)
set_target_properties(fib PROPERTIES CXX_STANDARD 20 CXX_SCAN_FOR_MODULES ON)

add_executable(thread_callback thread_callback.cc)
target_link_libraries(thread_callback PRIVATE pollcoro::pollcoro)
set_target_properties(thread_callback PROPERTIES CXX_STANDARD 20 CXX_SCAN_FOR_MODULES ON)

add_executable(modules modules.cc)
target_link_libraries(modules PRIVATE pollcoro::pollcoro)
set_target_properties(modules PROPERTIES 
    CXX_STANDARD 20 
    CXX_SCAN_FOR_MODULES ON
)

add_library(c_interop_impl SHARED c_interop_impl.cc)
target_link_libraries(c_interop_impl PRIVATE pollcoro::pollcoro)
set_target_properties(c_interop_impl PROPERTIES CXX_STANDARD 20 CXX_SCAN_FOR_MODULES ON)

add_executable(c_interop c_interop.c)
target_link_libraries(c_interop PRIVATE c_interop_impl)
set_target_properties(c_interop PROPERTIES C_STANDARD 11 LANGUAGE C)

# This is because clang-scan-deps doesnt parse the `'` literal correctly
# in preprocessor definitions.
# see https://github.com/llvm/llvm-project/issues/88896
CPMAddPackage(
  NAME cppcoro
  GIT_REPOSITORY https://github.com/andreasbuhr/cppcoro.git
  GIT_TAG f05170fbc71aaae06dc31e32812f1caba9525377
  PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/patches/cppcoro.patch"
)
target_compile_options(cppcoro PRIVATE -Wno-unused-result)

find_package(Threads REQUIRED)

CPMAddPackage("gh:chriskohlhoff/asio#asio-1-36-0")
if(asio_ADDED)
  add_library(asio INTERFACE)

  target_include_directories(asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/asio/include)

  target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)

  target_link_libraries(asio INTERFACE Threads::Threads)
endif()

add_executable(cxx_cppcoro_interop cxx_cppcoro_interop.cc)
target_link_libraries(cxx_cppcoro_interop PRIVATE c_interop_impl cppcoro)
set_target_properties(cxx_cppcoro_interop PROPERTIES CXX_STANDARD 20)

add_executable(timer_sleep timer_sleep.cc)
target_link_libraries(timer_sleep PRIVATE pollcoro::pollcoro)
set_target_properties(timer_sleep PROPERTIES CXX_STANDARD 20 CXX_SCAN_FOR_MODULES ON)

add_executable(exceptions exceptions.cc)
target_link_libraries(exceptions PRIVATE pollcoro::pollcoro)
set_target_properties(exceptions PROPERTIES CXX_STANDARD 20 CXX_SCAN_FOR_MODULES ON)

add_executable(stream stream.cc)
target_link_libraries(stream PRIVATE pollcoro::pollcoro)
set_target_properties(stream PROPERTIES CXX_STANDARD 20 CXX_SCAN_FOR_MODULES ON)

add_executable(custom_awaitable custom_awaitable.cc)
target_link_libraries(custom_awaitable PRIVATE pollcoro::pollcoro)
set_target_properties(custom_awaitable PROPERTIES CXX_STANDARD 20 CXX_SCAN_FOR_MODULES ON)

add_executable(generic generic.cc)
target_link_libraries(generic PRIVATE pollcoro::pollcoro)
set_target_properties(generic PROPERTIES CXX_STANDARD 20 CXX_SCAN_FOR_MODULES ON)

add_executable(pipelines pipelines.cc)
target_link_libraries(pipelines PRIVATE pollcoro::pollcoro)
set_target_properties(pipelines PROPERTIES CXX_STANDARD 20 CXX_SCAN_FOR_MODULES ON)

add_executable(wait_combinators wait_combinators.cc)
target_link_libraries(wait_combinators PRIVATE pollcoro::pollcoro)
set_target_properties(wait_combinators PROPERTIES CXX_STANDARD 20 CXX_SCAN_FOR_MODULES ON)

add_executable(allocator allocator.cc)
target_link_libraries(allocator PRIVATE pollcoro::pollcoro)
set_target_properties(allocator PROPERTIES CXX_STANDARD 20 CXX_SCAN_FOR_MODULES ON)

add_executable(interop_cppcoro interop_cppcoro.cc)
target_link_libraries(interop_cppcoro PRIVATE pollcoro::pollcoro cppcoro)
set_target_properties(interop_cppcoro PROPERTIES CXX_STANDARD 20 CXX_SCAN_FOR_MODULES ON)

add_executable(interop_asio interop_asio.cc)
target_link_libraries(interop_asio PRIVATE pollcoro::pollcoro asio)
set_target_properties(interop_asio PROPERTIES CXX_STANDARD 20 CXX_SCAN_FOR_MODULES ON)

add_executable(mutex mutex.cc)
target_link_libraries(mutex PRIVATE pollcoro::pollcoro)
set_target_properties(mutex PROPERTIES CXX_STANDARD 20 CXX_SCAN_FOR_MODULES ON)
