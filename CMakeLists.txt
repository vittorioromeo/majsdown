cmake_minimum_required(VERSION 3.9)

#
#
# -----------------------------------------------------------------------------
# Colored output
# -----------------------------------------------------------------------------

# From: https://medium.com/@alasher/colored-c-compiler-output-with-ninja-clang-gcc-10bfe7f2b949
option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." FALSE)

if(${FORCE_COLORED_OUTPUT})
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        add_compile_options("-fdiagnostics-color=always")
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        add_compile_options("-fcolor-diagnostics")
    endif()
endif()

#
#
# -----------------------------------------------------------------------------
# CCache
# -----------------------------------------------------------------------------

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found CCACHE")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

#
#
# -----------------------------------------------------------------------------
# CPM
# -----------------------------------------------------------------------------

set(CPM_DOWNLOAD_VERSION 0.34.0)

if(CPM_SOURCE_CACHE)
    set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
    set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()

if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
    message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
    file(DOWNLOAD
        https://github.com/TheLartians/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION}
    )
endif()

include(${CPM_DOWNLOAD_LOCATION})

#
#
# -----------------------------------------------------------------------------
# CPM: vrm_pp
# -----------------------------------------------------------------------------

CPMAddPackage(
    NAME vrm_pp
    GIT_REPOSITORY https://github.com/SuperV1234/vrm_pp
    GIT_TAG d14dbb0486e2d0755443c333bb56aabe11079e3f
    DOWNLOAD_ONLY YES
)

if (vrm_pp_ADDED)
    set(VRM_PP_INCLUDE_DIR "${vrm_pp_SOURCE_DIR}/include")
endif()

#
#
# -----------------------------------------------------------------------------
# CPM: discount
# -----------------------------------------------------------------------------

CPMAddPackage(
    NAME discount
    GIT_REPOSITORY https://github.com/Orc/discount.git
    GIT_TAG v2.2.7
    DOWNLOAD_ONLY YES
)

if (discount_ADDED)
    add_definitions("-DDESTRUCTOR=")
    include("${discount_SOURCE_DIR}/cmake/CMakeLists.txt")
endif()

#
#
# -----------------------------------------------------------------------------
# CPM: quickjs
# -----------------------------------------------------------------------------

CPMAddPackage(
    NAME quickjs
    GIT_REPOSITORY https://github.com/vittorioromeo/quickjs
    GIT_TAG 4eeb83a19b2772222f044655543337b563874d18
)

#
#
# -----------------------------------------------------------------------------
# CPM: doctest
# -----------------------------------------------------------------------------

CPMAddPackage("gh:onqtam/doctest#2.4.7")
list(APPEND CMAKE_MODULE_PATH ${doctest_SOURCE_DIR}/scripts/cmake)
include(doctest)

#
#
# -----------------------------------------------------------------------------
# C++ Standard
# -----------------------------------------------------------------------------

message(STATUS "setting required C++ standard to 20")
set(CMAKE_CXX_STANDARD "20")
set(CMAKE_CXX_STANDARD_REQUIRED on)

#
#
# -----------------------------------------------------------------------------
# Project setup
# -----------------------------------------------------------------------------

project(majsdown CXX C)
enable_testing()

set("${PROJECT_NAME_UPPER}_SOURCE_DIR" "${CMAKE_CURRENT_SOURCE_DIR}")

list(APPEND CMAKE_MODULE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

message(STATUS "initialized project ${PROJECT_NAME}")

#
#
# -----------------------------------------------------------------------------
# Global unity builds
# -----------------------------------------------------------------------------

set(CMAKE_UNITY_BUILD OFF) # TODO

#
#
# -----------------------------------------------------------------------------
# Source and includes
# -----------------------------------------------------------------------------

set(MAJSDOWN_INC_DIR "src" CACHE STRING "")
set(MAJSDOWN_SRC_DIR "src" CACHE STRING "")

file(GLOB_RECURSE MAJSDOWN_SRC_LIST "${MAJSDOWN_SRC_DIR}/majsdown/*.cpp")

#
#
# -----------------------------------------------------------------------------
# Compile commands
# -----------------------------------------------------------------------------

set(CMAKE_EXPORT_COMPILE_COMMANDS true)

# TODO: LTO

#
#
# -----------------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------------

add_library(majsdown STATIC ${MAJSDOWN_SRC_LIST})

target_include_directories(majsdown PUBLIC "./")
target_include_directories(majsdown PUBLIC "./${MAJSDOWN_INC_DIR}")

target_include_directories(majsdown SYSTEM PRIVATE "${VRM_PP_INCLUDE_DIR}")
target_include_directories(majsdown SYSTEM PRIVATE "f${quickjs_SOURCE_DIR}")

target_link_libraries(majsdown PRIVATE libmarkdown quickjs)

add_executable(majsdown-converter "${MAJSDOWN_SRC_DIR}/majsdown-converter/main.cpp")
target_link_libraries(majsdown-converter PRIVATE majsdown)

#
#
# -----------------------------------------------------------------------------
# PCH
# -----------------------------------------------------------------------------

target_precompile_headers(majsdown PRIVATE "${MAJSDOWN_INC_DIR}/majsdown/pch.hpp")
target_precompile_headers(majsdown-converter REUSE_FROM majsdown)

#
#
# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------

set(MAJSDOWN_TEST_DIR "test" CACHE STRING "")
file(GLOB_RECURSE MAJSDOWN_TEST_LIST "${MAJSDOWN_TEST_DIR}/*.cpp")

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Build and then run all the tests."
)

message(STATUS "Test list '${MAJSDOWN_TEST_LIST}'")

foreach(_file ${MAJSDOWN_TEST_LIST})
    message(STATUS "Found test file '${_file}'")

    file(RELATIVE_PATH _relative "${${PROJECT_NAME_UPPER}_SOURCE_DIR}" ${_file})
    string(REPLACE ".cpp" "" _name ${_relative})
    string(REGEX REPLACE "/" "." _name ${_name})
    set(_target "${_name}")

    message(STATUS "Target '${_target}'")

    add_executable(${_target} EXCLUDE_FROM_ALL "${_file}")
    target_link_libraries(${_target} doctest majsdown)
    doctest_discover_tests(${_target})
    add_dependencies(check ${_target})
    target_precompile_headers(${_target} REUSE_FROM majsdown)
endforeach()

