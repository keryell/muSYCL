cmake_minimum_required(VERSION 3.16)

project(muSYCL
  VERSION 0.0.1
  LANGUAGES CXX)

include(FindtriSYCL)
#find_package(TriSYCL MODULE REQUIRED)

# All targets inherit from include dir
include_directories(${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/ext)

find_library(LIBRARY_RTAUDIO NAMES rtaudio REQUIRED)

find_library(LIBRARY_RTMIDI NAMES rtmidi REQUIRED)

#set_project_properties(muSYCL PROPERTIES CXX_STANDARD 20)
# Specify the C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(src)
add_subdirectory(experiment)
