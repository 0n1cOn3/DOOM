cmake_minimum_required(VERSION 3.16)

project(linuxdoom-legacy LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})
file(GLOB DOOM_SOURCES
    ${SOURCE_DIR}/*.c)

add_executable(linuxdoom ${DOOM_SOURCES})

target_include_directories(linuxdoom PRIVATE ${SOURCE_DIR})

target_compile_definitions(linuxdoom PRIVATE NORMALUNIX LINUX)

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

# Build artifacts live next to the legacy Makefile output.
set_target_properties(linuxdoom PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
