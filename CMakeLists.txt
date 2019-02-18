cmake_minimum_required(VERSION 3.12.1)

project(LightFieldsPlayer)

SET(CMAKE_CXX_STANDARD 14)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)

set(MAJOR_VERSION 1)
set(MINOR_VERSION 0)
set(REVISION_VERSION 0)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules")

find_package(ArgumentViewer 1.0 CONFIG REQUIRED)
find_package(BasicCamera 1.0 CONFIG REQUIRED)
find_package(Simple3DApp 1.0.0 CONFIG REQUIRED)
find_package(Vars CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
#find_package(Compressonator CONFIG REQUIRED)

find_package(FFMPEG REQUIRED)
find_package(FreeImage REQUIRED)
find_package(FreeImagePlus REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(src/render)
#add_subdirectory(src/convert)
