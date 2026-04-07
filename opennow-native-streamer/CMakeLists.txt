cmake_minimum_required(VERSION 3.21)
project(opennow-native-streamer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OPENNOW_ENABLE_WARNINGS "Enable extra compiler warnings" ON)

set(OPENNOW_SOURCES
  src/main.cpp
  src/app/application.cpp
  src/ipc/ipc_client.cpp
  src/ipc/protocol.cpp
  src/input/input_protocol.cpp
  src/input/input_bridge.cpp
  src/media/media_pipeline.cpp
  src/platform/platform_info.cpp
  src/webrtc/sdp_helpers.cpp
  src/webrtc/webrtc_session.cpp
)

add_executable(opennow-native-streamer ${OPENNOW_SOURCES})
target_include_directories(opennow-native-streamer PRIVATE include)


if(OPENNOW_ENABLE_WARNINGS)
  if(MSVC)
    target_compile_options(opennow-native-streamer PRIVATE /W4 /permissive-)
  else()
    target_compile_options(opennow-native-streamer PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

find_package(SDL3 CONFIG QUIET)
if(SDL3_FOUND)
  target_link_libraries(opennow-native-streamer PRIVATE SDL3::SDL3)
  target_compile_definitions(opennow-native-streamer PRIVATE OPENNOW_HAS_SDL3=1)
else()
  message(WARNING "SDL3 was not found. The native streamer requires SDL3 to build.")
endif()

find_package(LibDataChannel CONFIG QUIET)
if(LibDataChannel_FOUND)
  target_link_libraries(opennow-native-streamer PRIVATE LibDataChannel::datachannel)
  target_compile_definitions(opennow-native-streamer PRIVATE OPENNOW_HAS_LIBDATACHANNEL=1)
else()
  message(WARNING "libdatachannel was not found. WebRTC negotiation will remain disabled until the dependency is installed.")
endif()

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(FFMPEG QUIET IMPORTED_TARGET libavcodec libavformat libavutil libswresample libswscale)
  if(FFMPEG_FOUND)
    target_link_libraries(opennow-native-streamer PRIVATE PkgConfig::FFMPEG)
    target_compile_definitions(opennow-native-streamer PRIVATE OPENNOW_HAS_FFMPEG=1)
  else()
    message(WARNING "FFmpeg development libraries were not found. Decode and audio playback remain scaffold-only.")
  endif()
endif()

if(WIN32)
  target_compile_definitions(opennow-native-streamer PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(opennow-native-streamer PRIVATE ws2_32)
endif()
