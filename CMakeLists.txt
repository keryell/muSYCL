cmake_minimum_required(VERSION 3.16)

project(muSYCL
        VERSION 0.0.1
        LANGUAGES CXX)

# To avoid refetching everything when no Internet
#set(FETCHCONTENT_FULLY_DISCONNECTED ON)

include(FindtriSYCL)
#find_package(TriSYCL MODULE REQUIRED)

# All targets inherit from include dir
include_directories(${PROJECT_SOURCE_DIR}/include
                    ${PROJECT_SOURCE_DIR}/ext)

find_library(LIBRARY_RTAUDIO NAMES rtaudio REQUIRED)

find_library(LIBRARY_RTMIDI NAMES rtmidi REQUIRED)

find_package(Doxygen
             REQUIRED dot)

#set_project_properties(muSYCL PROPERTIES CXX_STANDARD 20)
# Specify the C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

# Apply Cppcheck to compilation (apt install cppcheck)
set(CMAKE_CXX_CPPCHECK "cppcheck")

# Apply include-what-you-use to compilation (apt install iwyu)
# Does not work with C++2b and mdspan
#set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "include-what-you-use")

add_subdirectory(src)
add_subdirectory(experiment)

# Create a "doxygen" target to produce documentation from directories
# include and src
doxygen_add_docs(doxygen
                 include
                 src)
