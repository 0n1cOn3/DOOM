cmake_minimum_required(VERSION 3.16)

project(linuxdoom-legacy LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(VIDEO_BACKEND "SDL2" CACHE STRING "Video backend: SDL2 (Wayland-friendly) or X11 (deprecated)")
set_property(CACHE VIDEO_BACKEND PROPERTY STRINGS X11 SDL2)

set(NETWORK_BACKEND "BSD" CACHE STRING "Network backend: BSD or SDL_NET")
set_property(CACHE NETWORK_BACKEND PROPERTY STRINGS BSD SDL_NET)

set(SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})
file(GLOB DOOM_SOURCES
    ${SOURCE_DIR}/*.c)

list(REMOVE_ITEM DOOM_SOURCES
  ${SOURCE_DIR}/i_video.c
  ${SOURCE_DIR}/i_video_sdl2.c
  ${SOURCE_DIR}/i_net.c
  ${SOURCE_DIR}/i_net_sdl_net.c)

set(BACKEND_SOURCES)
set(BACKEND_LIBS m)

if(VIDEO_BACKEND STREQUAL "SDL2")
  find_package(SDL2 REQUIRED)
  list(APPEND BACKEND_SOURCES ${SOURCE_DIR}/i_video_sdl2.c)
  list(APPEND BACKEND_LIBS SDL2::SDL2)
elseif(VIDEO_BACKEND STREQUAL "X11")
  message(WARNING "VIDEO_BACKEND=X11 is deprecated and scheduled for removal after 2025; use SDL2 for Wayland systems.")
  find_package(X11 REQUIRED)
  list(APPEND BACKEND_SOURCES ${SOURCE_DIR}/i_video.c)
  if(X11_Xext_FOUND)
    list(APPEND BACKEND_LIBS X11::X11 X11::Xext)
  else()
    list(APPEND BACKEND_LIBS X11::X11)
  endif()
else()
  message(FATAL_ERROR "Unsupported VIDEO_BACKEND '${VIDEO_BACKEND}'. Use X11 or SDL2.")
endif()

if(NETWORK_BACKEND STREQUAL "SDL_NET")
  find_package(SDL2_net REQUIRED)
  list(APPEND BACKEND_SOURCES ${SOURCE_DIR}/i_net_sdl_net.c)
  list(APPEND BACKEND_LIBS SDL2_net::SDL2_net)
elseif(NETWORK_BACKEND STREQUAL "BSD")
  list(APPEND BACKEND_SOURCES ${SOURCE_DIR}/i_net.c)
  # Optional network services library is not always present on modern
  # distributions, so link it only when CMake finds it.
  find_library(NSL_LIBRARY nsl)
  if(NSL_LIBRARY)
    list(APPEND BACKEND_LIBS ${NSL_LIBRARY})
  endif()
else()
  message(FATAL_ERROR "Unsupported NETWORK_BACKEND '${NETWORK_BACKEND}'. Use BSD or SDL_NET.")
endif()

add_executable(linuxdoom ${DOOM_SOURCES} ${BACKEND_SOURCES})

target_include_directories(linuxdoom PRIVATE ${SOURCE_DIR})

target_compile_definitions(linuxdoom PRIVATE NORMALUNIX LINUX)

target_link_libraries(linuxdoom PRIVATE ${BACKEND_LIBS})

# Build artifacts live next to the legacy Makefile output.
set_target_properties(linuxdoom PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
