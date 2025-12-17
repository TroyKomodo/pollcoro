cmake_minimum_required(VERSION 3.28)

project(pollcoro
    VERSION 1.0.0
    LANGUAGES CXX
    DESCRIPTION "A C++20 module-based coroutine polling library"
)

option(POLLCORO_INSTALL "Generate install targets" ${PROJECT_IS_TOP_LEVEL})
option(POLLCORO_TESTS "Enable tests" ${PROJECT_IS_TOP_LEVEL})
option(POLLCORO_EXAMPLES "Enable examples" ${PROJECT_IS_TOP_LEVEL})
option(POLLCORO_IMPORT_STD "Import std" OFF)

if(PROJECT_IS_TOP_LEVEL)
   option(ADDRESS_SANITIZER "Enable address sanitizer" OFF)

    if(ADDRESS_SANITIZER)
        add_compile_options(-fsanitize=address)
        add_link_options(-fsanitize=address)
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
        else()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wno-#warnings")
        endif()
    endif()
endif()

# -----------------------------------------------------------------------------
# Main library (C++20 modules)
# -----------------------------------------------------------------------------
add_library(pollcoro)
add_library(pollcoro::pollcoro ALIAS pollcoro)

target_compile_features(pollcoro PUBLIC cxx_std_20)

target_sources(pollcoro
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES
            # Main module
            ${CMAKE_CURRENT_SOURCE_DIR}/src/pollcoro.cppm
            # Core partitions
            ${CMAKE_CURRENT_SOURCE_DIR}/src/waker.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/is_blocking.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/awaitable.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/stream_awaitable.cppm
            # Basic awaitables
            ${CMAKE_CURRENT_SOURCE_DIR}/src/yield.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ready.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/pending.cppm
            # Utilities
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ref.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/generic.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/map.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/single_event.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/sleep.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/mutex.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/shared_mutex.cppm
            # Allocator
            ${CMAKE_CURRENT_SOURCE_DIR}/src/allocator.cppm
            # Coroutine types
            ${CMAKE_CURRENT_SOURCE_DIR}/src/detail_promise.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/task.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/stream.cppm
            # Execution
            ${CMAKE_CURRENT_SOURCE_DIR}/src/block_on.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/wait_first.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/wait_all.cppm
            # Stream sources
            ${CMAKE_CURRENT_SOURCE_DIR}/src/iter.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/range.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/repeat.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/empty.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/enumerate.cppm
            # Stream combinators
            ${CMAKE_CURRENT_SOURCE_DIR}/src/next.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/take.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/take_while.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/skip.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/skip_while.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/chain.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/zip.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/flatten.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/window.cppm
            # Stream consumers
            ${CMAKE_CURRENT_SOURCE_DIR}/src/fold.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/last.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/nth.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/sync_iter.cppm
            # Interop
            ${CMAKE_CURRENT_SOURCE_DIR}/src/co_awaitable.cppm
            ${CMAKE_CURRENT_SOURCE_DIR}/src/resumable.cppm
)

if(POLLCORO_IMPORT_STD)
    target_compile_definitions(pollcoro
        PUBLIC POLLCORO_IMPORT_STD=1
    )
endif()

# -----------------------------------------------------------------------------
# Installation targets
# -----------------------------------------------------------------------------
if(POLLCORO_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
    
    install(TARGETS pollcoro
        EXPORT pollcoro-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_LIBDIR}/pollcoro
    )
    
    install(EXPORT pollcoro-targets
        FILE pollcoro-targets.cmake
        NAMESPACE pollcoro::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pollcoro
    )
    
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/pollcoro-config-version.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    
    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/pollcoro-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/pollcoro-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pollcoro
    )
    
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/pollcoro-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/pollcoro-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pollcoro
    )
endif()

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------
if(POLLCORO_TESTS)
    add_subdirectory(tests)
endif()

# -----------------------------------------------------------------------------
# Examples
# -----------------------------------------------------------------------------
if(POLLCORO_EXAMPLES)
    add_subdirectory(examples)
endif()
