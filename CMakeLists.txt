cmake_minimum_required(VERSION 3.16)
project(DistributedIMSystem)

option(DIMS_BUILD_TESTING "Enable DistributedIMSystem building tests" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(FETCHCONTENT_QUIET OFF)

# disable boringssl assembly language
set(OPENSSL_NO_ASM ON)

# disable boost assembly language
set(BOOST_NO_ASSEMBLY
    ON
    CACHE BOOL "Disable Boost Assembly")

# we have to disable this to prevent upb_alloc_global
set(protobuf_BUILD_LIBUPB OFF)

set(gRPC_SSL_PROVIDER package)
set(ABSL_ENABLE_INSTALL ON) # Very important!!!!

# Force to build protoc
set(gRPC_BUILD_TESTS OFF)
set(gRPC_INSTALL ON)
set(protobuf_BUILD_PROTOC_BINARIES ON)
set(protobuf_BUILD_TESTS OFF)
set(protobuf_USE_STATIC_LIBS OFF)

include(FetchContent)

set(BOOST_ENABLE_CMAKE
    ON
    CACHE BOOL "" FORCE)
set(BOOST_INCLUDE_LIBRARIES
    asio
    filesystem
    uuid
    system
    beast
    mysql
    url
    json)

# Enable MSVC exceptions (as above)
if(MSVC)
  message(STATUS "MSVC detected, enabling /EHsc")
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} /EHsc"
      CACHE STRING "MSVC exception flag" FORCE)
  add_compile_options(/EHsc)
endif()

# -----------------------------
# External Dependencies (FetchContent)
# -----------------------------
FetchContent_Declare(
  boost
  GIT_REPOSITORY https://github.com/boostorg/boost.git
  GIT_TAG boost-1.84.0
  GIT_SUBMODULES_RECURSE TRUE)

FetchContent_Declare(
  gRPC
  GIT_REPOSITORY https://github.com/grpc/grpc
  GIT_TAG v1.50.2
  GIT_SUBMODULES_RECURSE TRUE)

FetchContent_MakeAvailable(boost gRPC)

set(PROTOBUF_PROTOC_EXECUTABLE $<TARGET_FILE:protoc>)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)

if (DIMS_BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

add_library(boost_common INTERFACE)
target_link_libraries(boost_common INTERFACE Boost::asio Boost::system
                                             Boost::filesystem Boost::uuid)

add_library(boost_balance INTERFACE)
target_link_libraries(boost_balance INTERFACE boost_common)

add_library(boost_gateway INTERFACE)
target_link_libraries(
  boost_gateway INTERFACE Boost::beast Boost::mysql boost_common Boost::url
                          Boost::json)

add_library(boost_chatting INTERFACE)
target_link_libraries(boost_chatting INTERFACE boost_common Boost::mysql
                                               Boost::json)

add_library(boost_resources INTERFACE)
target_link_libraries(boost_resources INTERFACE boost_common Boost::mysql
                                                Boost::json)

# Add external dependencies once
add_subdirectory(external/inifile)
add_subdirectory(external/spdlog)
add_subdirectory(external/hiredis)
add_subdirectory(external/oneTBB)

# Add submodules (servers)
add_subdirectory(gateway-server)
add_subdirectory(balance-server)
add_subdirectory(chatting-server)
add_subdirectory(resources-server)
