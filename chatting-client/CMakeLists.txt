cmake_minimum_required(VERSION 3.16)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# MSVC Special!!
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  add_compile_options("/permissive-")
  add_compile_options("/Zc:__cplusplus")
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
  ChattingClient
  VERSION 3.2.1
  LANGUAGES CXX)

file(GLOB source_file CONFIGURE_DEPENDS *.cpp *.h *.hpp)
file(GLOB ui_file CONFIGURE_DEPENDS *.ui)
file(GLOB resource_file CONFIGURE_DEPENDS style/*.qrc)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets Network)

# Add OpenCV Package find_package(OpenCV REQUIRED)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(Qt6_FOUND)
  if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)

    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

    qt_add_executable(ChattingClient MANUAL_FINALIZATION ${source_file}
                      ${ui_file} ${resource_file})
  endif()

else()

  # we found nothing here, then we download Qt6
  FetchContent_Declare(
    qt6
    GIT_REPOSITORY https://code.qt.io/qt/qt5.git
    GIT_TAG 6.7.2)
  FetchContent_MakeAvailable(Qt6)

  if(ANDROID)
    add_library(ChattingClient SHARED ${source_file} ${ui_file}
                                      ${resource_file})
  else()
    # Define properties for Android with Qt 5 after find_package() calls as:
    # set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")

    add_executable(ChattingClient ${source_file} ${ui_file} ${resource_file})
  endif()

endif()

if(Qt6_FOUND OR Qt5_FOUND)

  target_link_libraries(ChattingClient PRIVATE Qt6::Widgets Qt6::Network
                                               ${OpenCV_LIBS})

else()

  target_link_libraries(
    ChattingClient PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
                           Qt${QT_VERSION_MAJOR}::Network ${OpenCV_LIBS})
endif()

target_include_directories(ChattingClient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                                  ${OpenCV_INCLUDE_DIRS})

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1. If
# you are developing for iOS or macOS you should consider setting an explicit,
# fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.ChattingClient)
endif()

set_target_properties(
  ChattingClient
  PROPERTIES ${BUNDLE_ID_OPTION} MACOSX_BUNDLE_BUNDLE_VERSION
             ${PROJECT_VERSION} MACOSX_BUNDLE_SHORT_VERSION_STRING
             ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR} MACOSX_BUNDLE
             TRUE WIN32_EXECUTABLE
             TRUE)

if(QT_VERSION_MAJOR EQUAL 6)
  qt_finalize_executable(ChattingClient)
endif()

target_compile_definitions(
  ChattingClient PUBLIC -DQT_DEMO_HOME=\"${CMAKE_CURRENT_SOURCE_DIR}/\")
