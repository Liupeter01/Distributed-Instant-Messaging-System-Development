cmake_minimum_required(VERSION 3.15)

include(FetchContent)

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

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT MSVC)
  find_program(CCACHE_PROGRAM ccache)
  if(CCACHE_PROGRAM)
    message(STATUS "Found CCache: ${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_PROGRAM})
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_PROGRAM})
  endif()
endif()

project(
  ResourcesServer
  VERSION 0.0.1
  LANGUAGES CXX)

file(GLOB_RECURSE source_file CONFIGURE_DEPENDS src/*.cpp src/*.cc src/*.c)
file(GLOB_RECURSE header_file CONFIGURE_DEPENDS include/*.hpp include/*.h)

add_executable(ResourcesServer ${source_file} ${header_file})
target_include_directories(ResourcesServer PUBLIC include external/inifile/include external/jsoncpp/include)

# import boost 1.84.0 library
FetchContent_Declare(
  boost
  GIT_REPOSITORY https://github.com/boostorg/boost.git
  GIT_TAG boost-1.84.0)
  
# import grpc1.50.2 library strong not recommend to use grpc 1.60.x+ versions
FetchContent_Declare(
  gRPC
  GIT_REPOSITORY https://github.com/grpc/grpc
  GIT_TAG v1.50.2)

FetchContent_MakeAvailable(boost gRPC)

add_subdirectory(external/inifile)
add_subdirectory(external/spdlog)
add_subdirectory(external/jsoncpp)

target_link_libraries(
  ResourcesServer
  PUBLIC Boost::asio
         Boost::uuid
         Boost::mysql
          grpc++
         libprotobuf
         inicpp::inicpp
         spdlog::spdlog
         jsoncpp_object)

target_compile_definitions(
  ResourcesServer PUBLIC -DCONFIG_HOME=\"${CMAKE_CURRENT_SOURCE_DIR}/\")
