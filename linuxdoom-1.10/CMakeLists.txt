cmake_minimum_required(VERSION 3.16)

project(linuxdoom-legacy LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})

option(DOOM_USE_SDL2 "Build with the SDL2 video backend" ON)
option(DOOM_ALLOW_DEPRECATED_X11 "Allow the deprecated X11 backend when SDL2 is missing" OFF)

if(DOOM_USE_SDL2)
  find_package(SDL2 QUIET)
  if(NOT SDL2_FOUND)
    # Try pkg-config as a fallback. Unlike find_package, pkg-config sets
    # SDL2_LIBRARIES and SDL2_INCLUDE_DIRS instead of creating SDL2::SDL2 target
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(SDL2 QUIET sdl2)
    endif()

    if(NOT SDL2_FOUND)
      if(DOOM_ALLOW_DEPRECATED_X11)
        message(WARNING "SDL2 not found; using deprecated X11 backend until Wayland-ready SDL2 is available")
        set(DOOM_USE_SDL2 OFF)
      else()
        message(FATAL_ERROR "SDL2 is required going forward (the old X11 backend is deprecated; SDL2 provides modern multi-platform support, including X11, Wayland, and others).\n"
                            "Re-run with DOOM_ALLOW_DEPRECATED_X11=ON to build the old backend temporarily.")
      endif()
    endif()
  endif()
endif()

file(GLOB DOOM_SOURCES
    ${SOURCE_DIR}/*.c)

list(REMOVE_ITEM DOOM_SOURCES
    ${SOURCE_DIR}/i_video.c
    ${SOURCE_DIR}/i_video_sdl.c
    ${SOURCE_DIR}/i_video_x11.c)

if(DOOM_USE_SDL2)
  list(APPEND DOOM_SOURCES ${SOURCE_DIR}/i_video_sdl.c)
else()
  list(APPEND DOOM_SOURCES ${SOURCE_DIR}/i_video_x11.c)
endif()

add_executable(linuxdoom ${DOOM_SOURCES})

target_include_directories(linuxdoom PRIVATE ${SOURCE_DIR})

target_compile_definitions(linuxdoom PRIVATE NORMALUNIX LINUX)

if(DOOM_USE_SDL2)
  # Link against SDL2, using the appropriate method based on how it was found
  if(TARGET SDL2::SDL2)
    target_link_libraries(linuxdoom PRIVATE SDL2::SDL2 m)
  else()
    # SDL2 was found via pkg-config
    target_include_directories(linuxdoom PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(linuxdoom PRIVATE ${SDL2_LIBRARIES} m)
  endif()
else()
  find_package(X11 REQUIRED)
  if(X11_Xext_FOUND)
    target_link_libraries(linuxdoom PRIVATE X11::X11 X11::Xext m)
  else()
    target_link_libraries(linuxdoom PRIVATE X11::X11 m)
  endif()
endif()

# Optional network services library is not always present on modern
# distributions, so link it only when CMake finds it.
find_library(NSL_LIBRARY nsl)
if(NSL_LIBRARY)
  target_link_libraries(linuxdoom PRIVATE ${NSL_LIBRARY})
endif()

# Build artifacts live next to the legacy Makefile output.
set_target_properties(linuxdoom PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

include(CTest)
if(BUILD_TESTING AND DOOM_USE_SDL2)
  add_test(NAME linuxdoom_smoketest
           COMMAND ${CMAKE_COMMAND} -E env
             SDL_VIDEODRIVER=dummy
             SDL_AUDIODRIVER=dummy
             $<TARGET_FILE:linuxdoom> -smoketest)
endif()
