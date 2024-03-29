muSYCL
======

Experimental muSYCaL framework around SYCL 2020
-----------------------------------------------

This is a small music synthesizer to experiment with `C++23
<https://isocpp.org/>`_ programming, design patterns and acceleration
on hardware accelerators like GPU, FPGA or CGRA with the `SYCL 2020
<https://www.khronos.org/sycl/>`_ standard.

It starts as a clean room implementation of a synthesizer to have a
clean modern core architecture which can then be accelerated. So
low-level features like pointers, macros, raw loops, etc. are avoided
in the code as much as possible. Just mostly high-level generic and
functional programming with C++ ranges, concepts...

This can be seen as a small DSL to define synthesizers in a
programmatic way on steroids comparable to modular synthesizers in an
electronic way.

This is a huge work-in-progress moving slowly and the most complex
part is actually not making the sounds themselves but interfacing with
a MIDI controller to have a playful experience...

Running the examples
--------------------

This requires a modern C++ compiler, the Range-v3 library and for
the musical side the RtMidi and AudioRt libraries, typically using
JACK on Linux.

It relies also on some extensions from
https://github.com/triSYCL/triSYCL

To get the latest precompiled  Clang compiler you might look at
https://apt.llvm.org/

To install these packages on Debian or Ubuntu, try::

  sudo install clang-18 librange-v3-dev librtmidi-dev librtaudio-dev \
  jackd2 qjackctl

and clone triSYCL somewhere.

In the top directory of the repository::

  mkdir build
  # Configure the project with the triSYCL repository somewhere
  CXX=clang++-18 cmake .. -DCMAKE_MODULE_PATH=<absolute_path>/triSYCL/cmake \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=RelWithDebInfo
  # Build the project
  cmake --build . --parallel `nproc` --verbose

Launch ``jackd``, for example with ``qjackctl`` which allows to do the
right connections between the audio and MIDI flows.

The main synthesizer to run is

- ``./src/musycl_synth``

but there are also some simple tests:

- ``./experiment/audio`` generate a square wave with an evolving PWM;

- ``./experiment/midi`` to list the MIDI interfaces and display what
  comes from the MIDI interface;

- ``./experiment/synth`` a minimalistic monodic synthesizer.

To debug the project, add ``-DCMAKE_BUILD_TYPE=Debug`` configuration
line of ``cmake``. When using GDB, think to use::

  handle SIG32 nostop

to ignore the noisy user signal 32 used by the audio/MIDI library.
