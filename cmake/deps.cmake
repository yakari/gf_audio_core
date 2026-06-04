# Downloads the single miniaudio header (MIT) into the build tree and exposes it
# as the INTERFACE target `gf_miniaudio`. We define MINIAUDIO_IMPLEMENTATION
# ourselves in src/backends/miniaudio_backend.cpp, so we only need the header.
set(GF_MINIAUDIO_VERSION 0.11.21 CACHE STRING "Pinned miniaudio version")
set(GF_MINIAUDIO_DIR ${CMAKE_BINARY_DIR}/_deps/miniaudio)
set(GF_MINIAUDIO_HEADER ${GF_MINIAUDIO_DIR}/miniaudio.h)

if(NOT EXISTS ${GF_MINIAUDIO_HEADER})
  message(STATUS "Downloading miniaudio ${GF_MINIAUDIO_VERSION}")
  file(DOWNLOAD
    https://raw.githubusercontent.com/mackron/miniaudio/${GF_MINIAUDIO_VERSION}/miniaudio.h
    ${GF_MINIAUDIO_HEADER}
    TLS_VERIFY ON
    STATUS _gf_ma_status)
  list(GET _gf_ma_status 0 _gf_ma_code)
  if(NOT _gf_ma_code EQUAL 0)
    file(REMOVE ${GF_MINIAUDIO_HEADER})
    message(FATAL_ERROR "Failed to download miniaudio: ${_gf_ma_status}")
  endif()
endif()

add_library(gf_miniaudio INTERFACE)
target_include_directories(gf_miniaudio INTERFACE ${GF_MINIAUDIO_DIR})
