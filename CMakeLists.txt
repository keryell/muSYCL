cmake_minimum_required(VERSION 3.16)

project(muSYCL
  VERSION 0.0.1
  LANGUAGES CXX)

include(FindTriSYCL)
#find_package(TriSYCL MODULE REQUIRED)

#set_project_properties(muSYCL PROPERTIES CXX_STANDARD 20)
# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(src)
