muSYCL
======

Minimal example using triSYCL
-----------------------------

In the top directory of the repository::

  mkdir build
  # Configure the project with the triSYCL repository somewhere
  cmake .. -DCMAKE_MODULE_PATH=.../triSYCL/cmake
  # Build the project
  cmake --build . --parallel `nproc`
