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

# generate include/message/*.pb.h include/message/*.pb.cc
set(PROTO_PATH ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(MESSAGE_PROTO ${PROTO_PATH}/message.proto)
set(GEN_PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/message)

set(MESSAGE_PB_CPP_FILE ${GEN_PROTO_DIR}/message.pb.cc)
set(MESSAGE_PB_H_FILE ${GEN_PROTO_DIR}/message.pb.h)
set(MESSAGE_GRPC_PB_CPP_FILE ${GEN_PROTO_DIR}/message.grpc.pb.cc)
set(MESSAGE_GRPC_PB_H_FILE ${GEN_PROTO_DIR}/message.grpc.pb.h)

set(GENERATED_PROTOBUF_FILES
    ${MESSAGE_PB_H_FILE} ${MESSAGE_PB_CPP_FILE} ${MESSAGE_GRPC_PB_H_FILE}
    ${MESSAGE_GRPC_PB_CPP_FILE})

set(PROTOBUF_PROTOC_EXECUTABLE $<TARGET_FILE:protoc>)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)

# create directory
if(EXISTS ${GEN_PROTO_DIR} AND IS_DIRECTORY ${GEN_PROTO_DIR})
  message(info " ${GEN_PROTO_DIR} ALREADY EXISTS")
else()
  file(MAKE_DIRECTORY ${GEN_PROTO_DIR})
endif()

add_custom_command(
  OUTPUT "${MESSAGE_PB_H_FILE}" "${MESSAGE_PB_CPP_FILE}"
         "${MESSAGE_GRPC_PB_H_FILE}" "${MESSAGE_GRPC_PB_CPP_FILE}"
  COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} ARGS --proto_path=${PROTO_PATH}
          --cpp_out=${GEN_PROTO_DIR} ${MESSAGE_PROTO}
  COMMAND
    ${PROTOBUF_PROTOC_EXECUTABLE} ARGS --proto_path=${PROTO_PATH}
    --grpc_out=${GEN_PROTO_DIR}
    --plugin=protoc-gen-grpc=${_GRPC_CPP_PLUGIN_EXECUTABLE} ${MESSAGE_PROTO}
  COMMENT "Processing proto files...")

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
  LoadBalanceServer
  VERSION 2.1.1
  LANGUAGES CXX)

file(GLOB_RECURSE source_file CONFIGURE_DEPENDS src/*.cpp src/*.cc src/*.c)
file(GLOB_RECURSE header_file CONFIGURE_DEPENDS include/*.hpp include/*.h)
file(GLOB_RECURSE proto_file CONFIGURE_DEPENDS proto/*.proto)

add_executable(LoadBalanceServer ${source_file} ${header_file}
                                 ${GENERATED_PROTOBUF_FILES})
target_include_directories(LoadBalanceServer PUBLIC include external/inifile/include/
                                                    external/hiredis/include/)

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
add_subdirectory(external/hiredis)

target_link_libraries(
  LoadBalanceServer
  PUBLIC Boost::asio
         Boost::uuid
         grpc++
         libprotobuf
         inicpp::inicpp
         spdlog::spdlog
         hiredis::hiredis)

target_compile_definitions(
  LoadBalanceServer PUBLIC -DCONFIG_HOME=\"${CMAKE_CURRENT_SOURCE_DIR}/\")
