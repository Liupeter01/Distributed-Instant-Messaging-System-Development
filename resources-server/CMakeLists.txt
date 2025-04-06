cmake_minimum_required(VERSION 3.15)
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
  ResourcesServer
  VERSION 1.2.0
  LANGUAGES CXX)

file(GLOB_RECURSE source_file CONFIGURE_DEPENDS src/*.cpp src/*.cc src/*.c)
file(GLOB_RECURSE header_file CONFIGURE_DEPENDS include/*.hpp include/*.h)
file(GLOB_RECURSE proto_file CONFIGURE_DEPENDS proto/*.proto)

add_executable(ResourcesServer ${source_file} ${header_file}
                               ${GENERATED_PROTOBUF_FILES})
target_include_directories(ResourcesServer PUBLIC include ${Boost_INCLUDE_DIR})

target_link_libraries(ResourcesServer PUBLIC boost_resources grpc++ inicpp
                                             spdlog hiredis tbb)

target_compile_definitions(
  ResourcesServer PUBLIC -DCONFIG_HOME=\"${CMAKE_CURRENT_SOURCE_DIR}/\")
