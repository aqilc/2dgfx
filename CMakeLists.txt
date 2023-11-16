cmake_minimum_required(VERSION 3.21)
project(cdungeon C)

set(CMAKE_C_STANDARD 23)
# set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

include_directories(. ./deps/include ./lib)
link_directories(./deps/lib)
link_libraries(freetype.lib glew32s.lib shell32.lib Gdi32.lib user32.lib opengl32.lib glfw3.lib)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(CMAKE_EXE_LINKER_FLAGS /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:libcmtd.lib /NODEFAULTLIB:msvcrtd.lib)
endif()

add_executable(graphics
        deps/hash.c
        deps/hashfunc.c
        deps/vec.c
        lib/2dgfx.c
        main.c)
