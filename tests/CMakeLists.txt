cmake_minimum_required(VERSION 3.14)
project(DIMS_test LANGUAGES CXX C)

if (NOT DIMS_BUILD_TESTING)
    return()
endif()

include(FetchContent)
FetchContent_Declare(
  googletest
  # Specify the commit you depend on and update it regularly.
  URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h 
     ${CMAKE_CURRENT_SOURCE_DIR}/*.cc 
     ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(DIMS_test_runner ${TEST_SOURCES})
target_link_libraries(DIMS_test_runner PRIVATE GTest::gtest)
include(GoogleTest)
gtest_discover_tests(DIMS_test_runner)