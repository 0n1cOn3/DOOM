cmake_minimum_required(VERSION 3.16)

project(linuxdoom-legacy LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

option(DOOM_USE_LEGACY_NETWORKING "Build with the original BSD sockets backend" OFF)

set(SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})
file(GLOB DOOM_SOURCES
    ${SOURCE_DIR}/*.c)

list(REMOVE_ITEM DOOM_SOURCES ${SOURCE_DIR}/net_harness.c)

add_executable(linuxdoom ${DOOM_SOURCES})

target_include_directories(linuxdoom PRIVATE ${SOURCE_DIR})

target_compile_definitions(linuxdoom PRIVATE NORMALUNIX LINUX)

if(DOOM_USE_LEGACY_NETWORKING)
  target_compile_definitions(linuxdoom PRIVATE DOOM_USE_LEGACY_NETWORKING)
endif()

find_package(X11 REQUIRED)

if(X11_Xext_FOUND)
  target_link_libraries(linuxdoom PRIVATE X11::X11 X11::Xext m)
else()
  target_link_libraries(linuxdoom PRIVATE X11::X11 m)
endif()

# Optional network services library is not always present on modern
# distributions, so link it only when CMake finds it.
find_library(NSL_LIBRARY nsl)
if(NSL_LIBRARY)
  target_link_libraries(linuxdoom PRIVATE ${NSL_LIBRARY})
endif()

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
  add_library(SDL2_net_stub STATIC
    ${SOURCE_DIR}/sdl_net_stub/SDL_net_stub.c)
  target_include_directories(SDL2_net_stub PUBLIC ${SOURCE_DIR}/sdl_net_stub)
  add_library(SDL2_net::SDL2_net ALIAS SDL2_net_stub)
endif()

target_link_libraries(linuxdoom PRIVATE SDL2_net::SDL2_net)

add_executable(net_harness
  ${SOURCE_DIR}/net_harness.c
  ${SOURCE_DIR}/i_net.c
  ${SOURCE_DIR}/m_argv.c
)

target_include_directories(net_harness PRIVATE ${SOURCE_DIR})
target_compile_definitions(net_harness PRIVATE NORMALUNIX LINUX)

if(DOOM_USE_LEGACY_NETWORKING)
  target_compile_definitions(net_harness PRIVATE DOOM_USE_LEGACY_NETWORKING)
endif()

target_link_libraries(net_harness PRIVATE SDL2_net::SDL2_net)

# Build artifacts live next to the legacy Makefile output.
set_target_properties(linuxdoom PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set_target_properties(net_harness PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
