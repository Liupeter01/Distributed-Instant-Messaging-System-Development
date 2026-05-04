cmake_minimum_required(VERSION 3.16)
project(im-benchmark CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DRESULTS_HOME=\"${CMAKE_CURRENT_SOURCE_DIR}/\")

add_subdirectory(im-bench-client)
add_subdirectory(im-bench-server)
