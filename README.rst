muSYCL
======

Minimal example using triSYCL
-----------------------------

In the top directory of the repository::

  mkdir build
  # Configure the project with the triSYCL repository somewhere
  CXX=clang++-11 cmake .. -DCMAKE_MODULE_PATH=<absolute_path>/triSYCL/cmake
  # Build the project
  cmake --build . --parallel `nproc`
