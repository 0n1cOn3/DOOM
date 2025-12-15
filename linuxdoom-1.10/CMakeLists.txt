cmake_minimum_required(VERSION 3.16)

project(linuxdoom-legacy LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(NETWORK_BACKEND "SDL_NET" CACHE STRING "Network backend: BSD or SDL_NET")
set_property(CACHE NETWORK_BACKEND PROPERTY STRINGS BSD SDL_NET)

set(SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})

file(GLOB DOOM_SOURCES
    ${SOURCE_DIR}/*.c)

list(REMOVE_ITEM DOOM_SOURCES
    ${SOURCE_DIR}/i_video.c
    ${SOURCE_DIR}/i_video_sdl.c
    ${SOURCE_DIR}/i_video_sdl2.c
    ${SOURCE_DIR}/i_video_x11.c
    ${SOURCE_DIR}/i_net.c
    ${SOURCE_DIR}/net_harness.c)

set(VIDEO_SOURCES)
set(VIDEO_LIBS)
set(NET_SOURCES)
set(NET_LIBS m)
set(NET_INCLUDE_DIRS)
set(NET_COMPILE_DEFS)

find_package(SDL2 REQUIRED)
list(APPEND VIDEO_SOURCES ${SOURCE_DIR}/i_video_sdl2.c)
list(APPEND VIDEO_LIBS SDL2::SDL2)

if(NETWORK_BACKEND STREQUAL "SDL_NET")
  find_package(PkgConfig QUIET)
  find_package(SDL2_net QUIET)

  if(NOT SDL2_net_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(SDL2NET SDL2_net)
    if(SDL2NET_FOUND)
      add_library(SDL2_net::SDL2_net INTERFACE IMPORTED)
      target_include_directories(SDL2_net::SDL2_net INTERFACE ${SDL2NET_INCLUDE_DIRS})
      target_link_libraries(SDL2_net::SDL2_net INTERFACE ${SDL2NET_LIBRARIES})
    endif()
  endif()

  if(NOT TARGET SDL2_net::SDL2_net)
    set(STUB_INCLUDE_ROOT ${CMAKE_BINARY_DIR}/generated_stub_includes)
    file(MAKE_DIRECTORY ${STUB_INCLUDE_ROOT}/SDL2)
    configure_file(
      ${SOURCE_DIR}/sdl_net_stub/SDL_net.h
      ${STUB_INCLUDE_ROOT}/SDL2/SDL_net.h
      COPYONLY
    )
    configure_file(
      ${SOURCE_DIR}/sdl_net_stub/SDL_net.h
      ${STUB_INCLUDE_ROOT}/SDL_net.h
      COPYONLY
    )

    add_library(SDL2_net_stub STATIC
      ${SOURCE_DIR}/sdl_net_stub/SDL_net_stub.c)
    target_include_directories(SDL2_net_stub PUBLIC ${STUB_INCLUDE_ROOT})
    add_library(SDL2_net::SDL2_net ALIAS SDL2_net_stub)
    set(NET_INCLUDE_DIRS ${STUB_INCLUDE_ROOT})
  endif()

  list(APPEND NET_SOURCES ${SOURCE_DIR}/i_net.c)
  list(APPEND NET_LIBS SDL2_net::SDL2_net)
elseif(NETWORK_BACKEND STREQUAL "BSD")
  list(APPEND NET_SOURCES ${SOURCE_DIR}/i_net.c)
  list(APPEND NET_COMPILE_DEFS DOOM_USE_LEGACY_NETWORKING)
  # Optional network services library is not always present on modern
  # distributions, so link it only when CMake finds it.
  find_library(NSL_LIBRARY nsl)
  if(NSL_LIBRARY)
    list(APPEND NET_LIBS ${NSL_LIBRARY})
  endif()
else()
  message(FATAL_ERROR "Unsupported NETWORK_BACKEND '${NETWORK_BACKEND}'. Use BSD or SDL_NET.")
endif()

add_executable(linuxdoom
  ${DOOM_SOURCES}
  ${VIDEO_SOURCES}
  ${NET_SOURCES}
)

target_include_directories(linuxdoom PRIVATE ${SOURCE_DIR})
target_include_directories(linuxdoom PRIVATE ${NET_INCLUDE_DIRS})
target_compile_definitions(linuxdoom PRIVATE NORMALUNIX LINUX ${NET_COMPILE_DEFS})

target_link_libraries(linuxdoom PRIVATE ${VIDEO_LIBS} ${NET_LIBS})

add_executable(net_harness
  ${SOURCE_DIR}/net_harness.c
  ${SOURCE_DIR}/m_argv.c
  ${NET_SOURCES}
)

target_include_directories(net_harness PRIVATE ${SOURCE_DIR})
target_include_directories(net_harness PRIVATE ${NET_INCLUDE_DIRS})
target_compile_definitions(net_harness PRIVATE NORMALUNIX LINUX ${NET_COMPILE_DEFS})

target_link_libraries(net_harness PRIVATE ${NET_LIBS})

# Build artifacts live next to the legacy Makefile output.
set_target_properties(linuxdoom PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set_target_properties(net_harness PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
