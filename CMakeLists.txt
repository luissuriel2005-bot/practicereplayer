cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "x86_64")

project(PracticeReplayer VERSION 1.0.0)

# Ruta al SDK de Geode (descargado automaticamente por el CLI)
if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "La variable de entorno GEODE_SDK no esta definida. Instala el CLI de Geode.")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

file(GLOB_RECURSE SOURCES src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE src)
setup_geode_mod(${PROJECT_NAME})
