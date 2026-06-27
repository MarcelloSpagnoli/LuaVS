# 3.14 minimum for FetchContent_MakeAvailable (used for raylib below).
cmake_minimum_required(VERSION 3.14)
project(LuaVS LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Put the binary in the project root
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFTW REQUIRED fftw3 fftw3f)
pkg_check_modules(GPIOD REQUIRED libgpiod)

# raylib: 2D+3D graphics framework (OpenGL under the hood). If not installed on
# the system, fetch and build 5.0 (needs network on the first configure).
find_package(raylib 4.5 QUIET)
if(NOT raylib_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.0
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    # Use the system GLFW for window/context/input (Wayland or X11).
    set(USE_EXTERNAL_GLFW ON CACHE STRING "" FORCE)
    # The Pi's Mesa V3D driver has no desktop OpenGL context: use ES 3.0.
    set(OPENGL_VERSION "ES 3.0" CACHE STRING "" FORCE)
    FetchContent_MakeAvailable(raylib)
endif()

include_directories(
    src
    include
    /usr/include/luajit-2.1
    /usr/include/luajit-5.1
    ${FFTW_INCLUDE_DIRS}
    ${GPIOD_INCLUDE_DIRS}
)

set(SOURCES
    src/main.cpp
    src/graphics/Renderer.cpp
    src/core/FeatureExtractor.cpp
    src/core/InputManager.cpp
    src/core/LuaEngine.cpp
    src/audio/AudioCapture.cpp
    src/core/FftProcessor.cpp
)

add_executable(LuaVS ${SOURCES})

target_link_libraries(LuaVS
    PRIVATE
    raylib
    # raylib is static and does not propagate its dependencies: GL/EGL must be
    # repeated here (in ES3 the glDrawBuffersEXT symbol lives in libGL).
    GL
    EGL
    asound
    pthread
    luajit-5.1
    ${FFTW_LIBRARIES}
    ${GPIOD_LIBRARIES}
)

# ====================================================================
# MANUAL TEST EXECUTABLES (not part of the graphics pipeline)
# ====================================================================

add_executable(test_scan
    src/tests/test_scan.cpp
    src/audio/AudioCapture.cpp
)
target_link_libraries(test_scan PRIVATE asound pthread)

add_executable(test_fft
    src/tests/test_fft.cpp
    src/audio/AudioCapture.cpp
    src/core/FftProcessor.cpp
    src/core/FeatureExtractor.cpp
)
target_link_libraries(test_fft PRIVATE asound pthread ${FFTW_LIBRARIES})

add_executable(mcp3008_test src/tests/mcp3008_test.cpp)
target_include_directories(mcp3008_test PRIVATE ${GPIOD_INCLUDE_DIRS})
target_link_libraries(mcp3008_test PRIVATE pthread ${GPIOD_LIBRARIES})

message(STATUS "Configuration finished successfully.")