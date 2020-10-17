add_executable(midi midi.cpp)
target_link_libraries(midi PRIVATE ${LIBRARY_RTMIDI})
