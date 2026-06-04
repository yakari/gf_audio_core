add_executable(test_transport test_transport.cpp)
target_link_libraries(test_transport PRIVATE gf_audio_core)
add_test(NAME transport COMMAND test_transport)

add_executable(test_ring_buffer test_ring_buffer.cpp)
target_link_libraries(test_ring_buffer PRIVATE gf_audio_core)
add_test(NAME ring_buffer COMMAND test_ring_buffer)
