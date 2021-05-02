add_executable(musycl_synth musycl_synth.cpp)
# musycl_synth is a SYCL program
add_sycl_to_target(musycl_synth)
target_link_libraries(musycl_synth
  PRIVATE ${LIBRARY_RTAUDIO} ${LIBRARY_RTMIDI})

#target_compile_options(musycl_synth PRIVATE
#  -fno-omit-frame-pointer -fsanitize=thread)
#target_link_options(musycl_synth PRIVATE -fsanitize=thread)
#target_compile_options(musycl_synth PRIVATE
#  -fno-omit-frame-pointer -fsanitize=undefined)
#target_link_options(musycl_synth PRIVATE -fsanitize=undefined)
