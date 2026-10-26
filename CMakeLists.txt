cmake_minimum_required(VERSION 3.21)

project(ShoresOfHazeron VERSION 7.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(AU_OUTPUT_ROOT "${CMAKE_BINARY_DIR}" CACHE PATH
    "Directory that receives executables and runtime libraries"
)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${AU_OUTPUT_ROOT}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${AU_OUTPUT_ROOT}/lib")
# Static libraries and Windows import libraries are only needed while linking.
# Keep them with the CMake intermediates (the Docker build-cache volume).
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
foreach(configuration Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${configuration}" configuration_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${configuration_upper} "${AU_OUTPUT_ROOT}/bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${configuration_upper} "${AU_OUTPUT_ROOT}/lib")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${configuration_upper} "${CMAKE_BINARY_DIR}/lib")
endforeach()

if(UNIX AND NOT APPLE)
    set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
    set(CMAKE_BUILD_RPATH "$ORIGIN/../lib")
endif()

if(WIN32)
    enable_language(RC)
endif()

set(AU_OPENGL_VERSION "32" CACHE STRING "OpenGL shader version: 32, 33, or 41")
set_property(CACHE AU_OPENGL_VERSION PROPERTY STRINGS 32 33 41)
if(NOT AU_OPENGL_VERSION MATCHES "^(32|33|41)$")
    message(FATAL_ERROR "AU_OPENGL_VERSION must be 32, 33, or 41")
endif()

set(AU_LINUX_PLATFORM "11" CACHE STRING
    "Linux runtime bundle identifier reported to legacy peers"
)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/templates/AuGLVer.h.in"
    "${CMAKE_BINARY_DIR}/generated/AuGLVer.h"
    @ONLY
)
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/templates/AuPlatform.h.in"
    "${CMAKE_BINARY_DIR}/generated/AuPlatform.h"
    @ONLY
)

include(cmake/Dependencies.cmake)
include(cmake/AuTargets.cmake)

add_library(AuBuildSettings INTERFACE)
target_compile_features(AuBuildSettings INTERFACE cxx_std_11)
target_include_directories(AuBuildSettings INTERFACE
    "${CMAKE_BINARY_DIR}/generated"
)

if(MINGW)
    # Large generated Qt resource objects can exceed MinGW's default section limit.
    target_compile_options(AuBuildSettings INTERFACE -Wa,-mbig-obj)
endif()

add_library(AuQt INTERFACE)
target_link_libraries(AuQt INTERFACE
    Qt5::Core
    Qt5::Gui
    Qt5::Network
    Qt5::OpenGL
    Qt5::Sql
    Qt5::Widgets
)

add_library(AuPlatform INTERFACE)
target_link_libraries(AuPlatform INTERFACE
    SDL2::SDL2
    OpenAL::OpenAL
    OpenGL::GL
    OpenGL::GLU
)

add_subdirectory(lib)
add_subdirectory(resources)
add_subdirectory(src)

add_custom_target(client DEPENDS AuSceneClient)
add_custom_target(clients DEPENDS
    AuSceneClient AuMailClient AuChatClient AuLauncher
)
add_custom_target(servers DEPENDS AuLoginServer AuMailServer AuSceneServer)
add_custom_target(web-tools DEPENDS AuServerStatus AuAcctNew AuAcctEmail AuAcctPassword AuEmpireReport)
add_custom_target(server-deployment DEPENDS servers web-tools)
add_custom_target(cgi DEPENDS
    AuAcctEmail AuAcctExpire AuAcctNew AuAcctPassword
    AuAcctSendPassword AuServerStatus
)
add_custom_target(tools DEPENDS
    AuEmpireReport AuScript AuServerMon AuViewer hashpack
)
add_custom_target(programs DEPENDS clients servers cgi tools)
