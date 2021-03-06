muSYCL
======

Experimental muSYCaL framework around SYCL 2020
-----------------------------------------------

This is a small synthesizer to experiment with `C++20
<https://isocpp.org/>`_ programming and acceleration on hardware
accelerators like GPU, FPGA or CGRA with the `SYCL 2020
<https://www.khronos.org/sycl/>`_ standard.

It starts as a clean room implementation of a synthesizer to have a
clean modern core architecture which can then be accelerated. So there
is no low-level features like pointers, macros, raw loops, etc. used
in the code. Just high-level generic and functional programming.


Running the examples
--------------------

This requires a modern C++20 compiler, the Range-v3 library and for
the musical side the RtMidi and AudioRt libraries, typically using
JACK on Linux.

It relies also on some extensions from
https://github.com/triSYCL/triSYCL

To get the latest precompiled  Clang compiler you might look at
https://apt.llvm.org/

To install these packages on Debian or Ubuntu, try::

  sudo install clang-13 librange-v3-dev librtmidi-dev librtaudio-dev \
  jackd2 qjackctl

and clone triSYCL somewhere.

In the top directory of the repository::

  mkdir build
  # Configure the project with the triSYCL repository somewhere
  CXX=clang++-13 cmake .. -DCMAKE_MODULE_PATH=<absolute_path>/triSYCL/cmake \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=1
  # Build the project
  make --build . --parallel `nproc` --verbose

Launch ``jackd``, for example with ``qjackctl`` which allows to do the
right connections between the audio and MIDI flows.

The main synthesizer to run is

- ``./src/musycl_synth``

but there are also some simple tests:

- ``./experiment/audio`` generate a square wave with an evolving PWM;

- ``./experiment/midi`` to list the MIDI interfaces and display what
  comes from the MIDI interface;

- ``./experiment/synth`` a minimalistic monodic synthesizer.
