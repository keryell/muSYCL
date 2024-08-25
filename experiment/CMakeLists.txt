add_executable(audio audio.cpp)
target_link_libraries(audio PRIVATE ${LIBRARY_RTAUDIO})

add_executable(midi midi.cpp)
target_link_libraries(midi PRIVATE ${LIBRARY_RTMIDI})

add_executable(midi-libre midi-libre.cpp)
target_link_libraries(midi-libre PRIVATE libremidi)

add_executable(synth synth.cpp)
target_link_libraries(synth PRIVATE ${LIBRARY_RTAUDIO} ${LIBRARY_RTMIDI})
