cmake_minimum_required(VERSION 3.16)
project(gf_audio_core LANGUAGES C CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(GF_AUDIO_BUILD_TESTS    "Build unit tests (no audio hardware needed)"     ON)
option(GF_AUDIO_BUILD_BACKEND  "Build the miniaudio backend (fetches miniaudio)" ON)
option(GF_AUDIO_BUILD_DEMO     "Build the CLI demo (needs the backend)"          ON)
option(GF_AUDIO_BUILD_FFI      "Build the C FFI shared library for Flutter"      ON)

# ---------------------------------------------------------------------------
# Core library: pure DSP / timeline logic. NO external dependencies, so it
# builds and unit-tests anywhere (incl. CI with no audio device). Everything
# that touches the audio callback lives here and must stay RT-safe.
# ---------------------------------------------------------------------------
add_library(gf_audio_core
  src/transport.cpp
  src/metronome.cpp
  src/track.cpp
  src/engine.cpp
  src/latency_calibrator.cpp
  src/recorder.cpp
)
target_include_directories(gf_audio_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(gf_audio_core PUBLIC cxx_std_17)

# ---------------------------------------------------------------------------
# Audio backend (miniaudio). Isolated so the core never depends on it; swap
# in raw Oboe / AVAudioEngine later without touching the engine.
# ---------------------------------------------------------------------------
if(GF_AUDIO_BUILD_BACKEND OR GF_AUDIO_BUILD_DEMO OR GF_AUDIO_BUILD_FFI)
  include(cmake/deps.cmake)
  add_library(gf_audio_backend_miniaudio src/backends/miniaudio_backend.cpp)
  target_link_libraries(gf_audio_backend_miniaudio PUBLIC gf_audio_core PRIVATE gf_miniaudio)
  if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(gf_audio_backend_miniaudio PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
  endif()
endif()

# ---------------------------------------------------------------------------
# C FFI shared library consumed by the Flutter side via dart:ffi.
# ---------------------------------------------------------------------------
if(GF_AUDIO_BUILD_FFI)
  add_library(gf_audio_core_ffi SHARED src/ffi.cpp)
  target_link_libraries(gf_audio_core_ffi PRIVATE gf_audio_core gf_audio_backend_miniaudio)
  set_target_properties(gf_audio_core_ffi PROPERTIES C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)
endif()

# ---------------------------------------------------------------------------
# CLI demo: proves the toolchain + audio output (plays the metronome).
# ---------------------------------------------------------------------------
if(GF_AUDIO_BUILD_DEMO)
  add_executable(gf_audio_demo tools/gf_audio_demo.cpp)
  target_link_libraries(gf_audio_demo PRIVATE gf_audio_core gf_audio_backend_miniaudio)

  # Real-hardware round-trip latency probe (acoustic loopback).
  add_executable(gf_latency_probe tools/gf_latency_probe.cpp)
  target_link_libraries(gf_latency_probe PRIVATE gf_audio_core gf_audio_backend_miniaudio)
endif()

if(GF_AUDIO_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
