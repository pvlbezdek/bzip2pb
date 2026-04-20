cmake_minimum_required(VERSION 3.20)
project(bzip2pb VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(BZip2 REQUIRED)
find_package(Threads REQUIRED)

# Platform string embedded in the binary via version.h
if(WIN32)
    set(BZIP2PB_PLATFORM "Windows")
elseif(APPLE)
    set(BZIP2PB_PLATFORM "macOS")
else()
    set(BZIP2PB_PLATFORM "Linux")
endif()

# Architecture string — CMAKE_GENERATOR_PLATFORM covers MSVC cross-compiles (e.g. ARM64)
if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
    set(BZIP2PB_ARCH "ARM64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64|aarch64")
    set(BZIP2PB_ARCH "ARM64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(BZIP2PB_ARCH "x64")
else()
    set(BZIP2PB_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Generate version header into the build tree so the source tree stays clean
configure_file(src/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/version.h @ONLY)

# macOS universal binary — must be set before add_executable
if(APPLE)
    option(BZIP2PB_UNIVERSAL "Build universal binary (macOS x86_64 + arm64)" OFF)
    if(BZIP2PB_UNIVERSAL)
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
    endif()
endif()

add_executable(bzip2pb
    src/main.cpp
    src/compress.cpp
    src/decompress.cpp
    src/thread_pool.cpp
)

target_include_directories(bzip2pb PRIVATE
    src
    ${CMAKE_CURRENT_BINARY_DIR}/src
)
target_link_libraries(bzip2pb PRIVATE BZip2::BZip2 Threads::Threads)

if(MSVC)
    target_compile_options(bzip2pb PRIVATE /W4 /WX /O2 /EHsc)
    if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
        target_compile_definitions(bzip2pb PRIVATE BZIP2PB_ARCH_ARM64)
    endif()
elseif(APPLE)
    target_compile_options(bzip2pb PRIVATE -Wall -Wextra -Werror -O2)
else()
    target_compile_options(bzip2pb PRIVATE -Wall -Wextra -Werror -O2)
    option(BZIP2PB_STATIC "Link statically (Linux portable binary)" OFF)
    if(BZIP2PB_STATIC)
        target_link_options(bzip2pb PRIVATE -static-libgcc -static-libstdc++)
    endif()
endif()

install(TARGETS bzip2pb
    RUNTIME DESTINATION bin
    BUNDLE  DESTINATION bin
)
