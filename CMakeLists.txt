cmake_minimum_required(VERSION 3.20)
project(screensaver-openmp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Warnings
if (MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# SDL2
find_package(SDL2 REQUIRED)

# OpenMP (futuro; no usado aquí)
find_package(OpenMP)
if (OpenMP_CXX_FOUND)
  message(STATUS "OpenMP encontrado: ${OpenMP_CXX_VERSION}")
endif()

add_executable(screensaver
  src/main.cpp
  src/waves.cpp
  src/model_seq.cpp
  src/shading.cpp
  src/render_sdl.cpp
  src/ink.cpp
)

target_include_directories(screensaver PRIVATE
  ${SDL2_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Enlazar SDL2
target_link_libraries(screensaver PRIVATE SDL2::SDL2)
if (TARGET SDL2::SDL2main)
  target_link_libraries(screensaver PRIVATE SDL2::SDL2main)
endif()

# OpenMP (para fase paralela)
if (OpenMP_CXX_FOUND)
  target_compile_definitions(screensaver PRIVATE HAVE_OPENMP=1)
  target_link_libraries(screensaver PRIVATE OpenMP::OpenMP_CXX)
endif()

# Optimización
if (CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
  if (MSVC)
    target_compile_options(screensaver PRIVATE /O2 /fp:fast)
  else()
    target_compile_options(screensaver PRIVATE -O3 -ffast-math -fno-math-errno -fno-trapping-math)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-march=native" HAS_MARCH_NATIVE)
    if (HAS_MARCH_NATIVE)
      target_compile_options(screensaver PRIVATE -march=native)
    endif()
  endif()
endif()
