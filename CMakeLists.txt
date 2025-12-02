cmake_minimum_required(VERSION 3.28)

project(pollcoro
    VERSION 1.0.0
    LANGUAGES CXX
    DESCRIPTION "A header-only C++17 coroutine-ts library polling library"
)

option(POLLCORO_INSTALL "Generate install targets" ${PROJECT_IS_TOP_LEVEL})
option(POLLCORO_PCH "Enable precompiled headers" ON)
option(POLLCORO_MODULES "Enable C++20 module support (experimental)" ON)
option(POLLCORO_TESTS "Enable tests" ${PROJECT_IS_TOP_LEVEL})
option(POLLCORO_EXAMPLES "Enable examples" ${PROJECT_IS_TOP_LEVEL})

add_library(pollcoro INTERFACE)
add_library(pollcoro::pollcoro ALIAS pollcoro)

file(GLOB POLLCORO_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/pollcoro/*.hpp")

target_sources(pollcoro INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include/pollcoro
    FILES ${POLLCORO_HEADERS}
)

target_include_directories(pollcoro INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# -----------------------------------------------------------------------------
# Precompiled Headers (PCH) support
# -----------------------------------------------------------------------------
if(POLLCORO_PCH)
    add_library(pollcoro_pch INTERFACE)
    add_library(pollcoro::pch ALIAS pollcoro_pch)
    
    target_link_libraries(pollcoro_pch INTERFACE pollcoro)
    
    target_precompile_headers(pollcoro_pch INTERFACE
        <algorithm>
        <coroutine>
        <cstddef>
        <functional>
        <iterator>
        <map>
        <memory>
        <optional>
        <tuple>
        <typeinfo>
        <utility>
        <variant>
        <vector>
        <mutex>
        <condition_variable>
        <stdint.h>
    )
endif()

# -----------------------------------------------------------------------------
# C++20 Modules support (experimental)
# -----------------------------------------------------------------------------
if(POLLCORO_MODULES)
    add_library(pollcoro_module)
    add_library(pollcoro::module ALIAS pollcoro_module)
    
    target_compile_features(pollcoro_module PUBLIC cxx_std_20)
    
    target_sources(pollcoro_module
        PUBLIC
            FILE_SET CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
            FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/pollcoro.cppm
    )

    target_include_directories(pollcoro_module PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
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
        FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pollcoro
    )
    
    if(POLLCORO_PCH)
        install(TARGETS pollcoro_pch
            EXPORT pollcoro-targets
        )
    endif()
    
    if(POLLCORO_MODULES)
        install(TARGETS pollcoro_module
            EXPORT pollcoro-targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_LIBDIR}/pollcoro
        )
    endif()
    
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
