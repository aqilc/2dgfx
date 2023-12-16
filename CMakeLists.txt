cmake_minimum_required(VERSION 3.21)

include(FetchContent)
find_package(OpenGL REQUIRED)
project(2dgfx C CXX)

#- GLFW ---------------------------------------------------------------------

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG        3.3.8
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
set(GLFW_INSTALL OFF CACHE BOOL "")
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
FetchContent_MakeAvailable(glfw)

#- GLEW ---------------------------------------------------------------------

FetchContent_Declare(
  glew
  GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
  GIT_TAG        origin/master
)
FetchContent_GetProperties(glew)
if(NOT glew_POPULATED)
  FetchContent_Populate(glew)

  add_subdirectory(${glew_SOURCE_DIR} ${glew_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

#- STB ---------------------------------------------------------------------

FetchContent_Declare(
    stb
    GIT_REPOSITORY  https://github.com/nothings/stb.git
    GIT_TAG         master
    GIT_SHALLOW     TRUE
    GIT_PROGRESS    TRUE
)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)

    add_library(stb_image INTERFACE ${stb_SOURCE_DIR}/stb_image.h)
    target_include_directories(stb_image INTERFACE ${stb_SOURCE_DIR})
endif()

#- FREETYPE -----------------------------------------------------------------

FetchContent_Declare(
	freetype
	GIT_REPOSITORY https://github.com/aseprite/freetype2.git
	GIT_TAG		   VER-2-6-3 
)

FetchContent_GetProperties(freetype)
if(NOT freetype_POPULATED)
	FetchContent_Populate(freetype)
	add_subdirectory(${freetype_SOURCE_DIR} ${freetype_BINARY_DIR})
endif()

# --------------------

set(CMAKE_C_STANDARD 23)
# set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(CMAKE_EXE_LINKER_FLAGS /NODEFAULTLIB:msvcrt.lib /NODEFAULTLIB:libcmtd.lib /NODEFAULTLIB:msvcrtd.lib)
        add_compile_definitions(/DTRACY_ENABLE)
else()
        add_compile_definitions(-DTRACY_ENABLE)
endif()


include_directories(. ./deps/include ./lib)
link_directories(./deps/lib)
# link_libraries(freetype.lib glew32s.lib shell32.lib Gdi32.lib user32.lib opengl32.lib glfw3.lib)

add_executable(graphics
        deps/hash.c
        deps/hashfunc.c
        deps/extern/tracy/TracyClient.cpp
        lib/2dgfx.c
        main.c)

target_link_libraries(graphics PRIVATE glfw freetype libglew_static stb_image)

add_custom_command(TARGET graphics POST_BUILD        # Adds a post-build event to MyTest
    COMMAND ${CMAKE_COMMAND} -E copy_if_different  # which executes "cmake - E copy_if_different..."
        "${PROJECT_SOURCE_DIR}/roboto.ttf"      # <--this is in-file
        $<TARGET_FILE_DIR:graphics>)

add_custom_command(TARGET graphics POST_BUILD        # Adds a post-build event to MyTest
    COMMAND ${CMAKE_COMMAND} -E copy_if_different  # which executes "cmake - E copy_if_different..."
        "${PROJECT_SOURCE_DIR}/hyperAngery.png"      # <--this is in-file
        $<TARGET_FILE_DIR:graphics>)