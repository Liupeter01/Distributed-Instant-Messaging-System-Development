cmake_minimum_required(VERSION 3.15)
project(im-bench-server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE source_file CONFIGURE_DEPENDS src/*.cpp src/*.cc src/*.c)
file(GLOB_RECURSE header_file CONFIGURE_DEPENDS include/*.hpp include/*.h)

add_executable(im-bench-server ${source_file} ${header_file})

target_include_directories(im-bench-server PUBLIC include ${Boost_INCLUDE_DIR})

target_link_libraries(im-bench-server PUBLIC boost_common)