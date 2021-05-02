add_executable(musycl_synth musycl_synth.cpp)
add_sycl_to_target(musycl_synth)
target_link_libraries(musycl_synth
  PRIVATE ${LIBRARY_RTAUDIO} ${LIBRARY_RTMIDI})
