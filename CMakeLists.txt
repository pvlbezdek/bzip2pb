cmake_minimum_required(VERSION 3.16)
project(bzip2pb VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(BZip2 REQUIRED)
find_package(Threads REQUIRED)

add_executable(bzip2pb
    src/main.cpp
    src/compress.cpp
    src/decompress.cpp
    src/thread_pool.cpp
)

target_include_directories(bzip2pb PRIVATE src)
target_link_libraries(bzip2pb PRIVATE BZip2::BZip2 Threads::Threads)

if(MSVC)
    target_compile_options(bzip2pb PRIVATE /W4 /O2 /EHsc)
else()
    target_compile_options(bzip2pb PRIVATE -Wall -Wextra -O2)
endif()

install(TARGETS bzip2pb RUNTIME DESTINATION bin)
