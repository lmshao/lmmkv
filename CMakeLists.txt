cmake_minimum_required(VERSION 3.10)
project(lmmkv VERSION 0.1.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(BUILD_STATIC_LIBS "Build static libraries" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(INSTALL_TO_USER_LOCAL "Install to ~/.local instead of system-wide" OFF)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

message(STATUS "Building lmmkv (${CMAKE_BUILD_TYPE})")

# Handle user-local installation
if(INSTALL_TO_USER_LOCAL AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "User local install path" FORCE)
    message(STATUS "Installing to user directory: ${CMAKE_INSTALL_PREFIX}")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Try to find installed lmcore first, fallback to local source if not found
find_package(lmcore QUIET)
if(lmcore_FOUND)
    message(STATUS "Using system-installed lmcore library")
    set(LMCORE_SOURCE "system")
else()
    set(LMCORE_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lmcore")
    if(EXISTS "${LMCORE_LOCAL_DIR}/CMakeLists.txt")
        if(NOT TARGET lmcore)
            add_subdirectory(../lmcore lmcore_build)
        endif()
        message(STATUS "Using local lmcore from sibling directory")
        set(LMCORE_SOURCE "local")
    else()
        message(STATUS "")
        message(STATUS "========== DEPENDENCY ERROR ==========")
        message(FATAL_ERROR
            "Cannot find lmcore dependency!\n\n"
            "Searched locations:\n"
            "  1. System installation (using find_package lmcore)\n"
            "  2. Local directory: ${LMCORE_LOCAL_DIR}\n\n"
            "Solution - Download lmcore to sibling directory:\n\n"
            "    cd ${CMAKE_CURRENT_SOURCE_DIR}/..\n"
            "    git clone ssh://git@github.com/lmshao/lmcore.git\n\n"
            "    # OR if SSH is not available:\n"
            "    git clone https://github.com/lmshao/lmcore.git\n\n"
            "After downloading, re-run: cmake .."
        )
    endif()
endif()

# Include dirs
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/src)
if(LMCORE_SOURCE STREQUAL "local")
    include_directories(${LMCORE_LOCAL_DIR}/include)
endif()

# Sources
file(GLOB_RECURSE LMMKV_SOURCES "src/*.cpp")
set(SOURCES ${LMMKV_SOURCES})

if(NOT BUILD_STATIC_LIBS AND NOT BUILD_SHARED_LIBS)
    message(FATAL_ERROR "At least one of BUILD_STATIC_LIBS or BUILD_SHARED_LIBS must be ON")
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
    set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Zi /Od /DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /DNDEBUG /DRELEASE")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error -Wno-format-truncation -Wno-unused-result")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -DRELEASE")
endif()

find_package(Threads REQUIRED)

# Static library
if(BUILD_STATIC_LIBS)
    add_library(lmmkv_static STATIC ${SOURCES})
    target_include_directories(lmmkv_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    target_sources(lmmkv_static PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/mkv_muxer.cpp
    )
    if(lmcore_FOUND)
        target_link_libraries(lmmkv_static PUBLIC lmcore::lmcore Threads::Threads)
    else()
        target_link_libraries(lmmkv_static PUBLIC lmcore Threads::Threads)
    endif()
    set_target_properties(lmmkv_static PROPERTIES
        OUTPUT_NAME lmmkv
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    message(STATUS "Static library target: lmmkv_static")
endif()

# Shared library
if(BUILD_SHARED_LIBS)
    add_library(lmmkv_shared SHARED ${SOURCES})
    target_include_directories(lmmkv_shared PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    target_sources(lmmkv_shared PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/mkv_muxer.cpp
    )
    if(lmcore_FOUND)
        target_link_libraries(lmmkv_shared PUBLIC lmcore::lmcore Threads::Threads)
    else()
        target_link_libraries(lmmkv_shared PUBLIC lmcore Threads::Threads)
    endif()
    set_target_properties(lmmkv_shared PROPERTIES
        OUTPUT_NAME lmmkv
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    message(STATUS "Shared library target: lmmkv_shared")
endif()

# Examples (placeholder)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()