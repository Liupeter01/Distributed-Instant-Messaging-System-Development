cmake_minimum_required(VERSION 3.10)
project(test_helloworld  LANGUAGES CXX C)

include(GoogleTest)

if (NOT LIBHPC_BUILD_TESTING)
    return()
endif()

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt
    ON
    CACHE BOOL "" FORCE)

file(GLOB TEST_SOURCES *.cc)
add_executable(test_helloworld ${TEST_SOURCES})
target_link_libraries(test_helloworld PRIVATE GTest::gtest)
gtest_discover_tests(test_helloworld)