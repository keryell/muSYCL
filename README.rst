muSYCL
======

Experimental muSYCaL framework around SYCL

Minimal examples using triSYCL
------------------------------


Running the examples
--------------------

This requires a modern C++20 compiler, the Range-v3 library and for
the musical side the RtMidi and AudioRt libraries, typically using
JACK on Linux.

To install these packages on Debian or Ubuntu, try::

  sudo install clang-11 librange-v3-dev librtmidi-dev librtaudio-dev \
  jackd2 qjackctl



In the top directory of the repository::

  mkdir build
  # Configure the project with the triSYCL repository somewhere
  CXX=clang++-11 cmake .. -DCMAKE_MODULE_PATH=<absolute_path>/triSYCL/cmake
  # Build the project
  make --build . --parallel `nproc` --verbose
