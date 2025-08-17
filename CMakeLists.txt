cmake_minimum_required(VERSION 3.20)
project(screensaver-openmp LANGUAGES C CXX)

# Estándar y warnings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# OpenMP (aún no se usa, pero lo detectamos)
find_package(OpenMP)
if (OpenMP_CXX_FOUND)
  message(STATUS "OpenMP encontrado: ${OpenMP_CXX_VERSION}")
endif()

# SDL2 (requerido) y SDL2_ttf (opcional)
find_package(SDL2 REQUIRED)
find_package(SDL2_ttf QUIET)

add_executable(screensaver
  src/main.cpp
)

target_include_directories(screensaver PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(screensaver PRIVATE SDL2::SDL2)

# SDL2_ttf opcional (aún no lo usamos en fase 1)
if (SDL2_TTF_FOUND)
  target_compile_definitions(screensaver PRIVATE HAVE_SDL_TTF=1)
  target_link_libraries(screensaver PRIVATE SDL2_ttf::SDL2_ttf)
endif()

# OpenMP opcional (no usado en fase 1)
if (OpenMP_CXX_FOUND)
  target_compile_definitions(screensaver PRIVATE HAVE_OPENMP=1)
  target_link_libraries(screensaver PRIVATE OpenMP::OpenMP_CXX)
endif()

# Optimización razonable en release
if (CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
  if (NOT MSVC)
    target_compile_options(screensaver PRIVATE -O2)
  endif()
endif()
