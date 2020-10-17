add_executable(midi midi.cpp)
target_link_libraries(midi PRIVATE ${LIBRARY_RTMIDI})

add_executable(audio audio.cpp)
target_link_libraries(audio PRIVATE ${LIBRARY_RTAUDIO})
