cmake_minimum_required(VERSION 3.22)
project(sao800 VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── OBS headers via FetchContent ──────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
  obs-studio
  GIT_REPOSITORY https://github.com/obsproject/obs-studio.git
  GIT_TAG        32.1.2
  GIT_SHALLOW    TRUE
  SOURCE_SUBDIR  cmake  # prevent building OBS itself
)
# We only need headers, not the full build
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
FetchContent_MakeAvailable(obs-studio)

set(OBS_HEADERS_DIR "${obs-studio_SOURCE_DIR}/libobs")

# ── GStreamer ─────────────────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED
  gstreamer-1.0
  gstreamer-app-1.0
  gstreamer-video-1.0
)

# ── Plugin target ─────────────────────────────────────────────────────────────
add_library(sao800 MODULE
  src/plugin-main.cpp
  src/rtsp-source.cpp
)

target_include_directories(sao800 PRIVATE
  ${OBS_HEADERS_DIR}
  ${CMAKE_BINARY_DIR}  # obsconfig.h (generated or manually placed here)
  ${GST_INCLUDE_DIRS}
  /usr/local/include   # simde (SIMD Everywhere) required by OBS headers
)

target_link_directories(sao800 PRIVATE ${GST_LIBRARY_DIRS})

target_link_libraries(sao800 PRIVATE
  ${GST_LIBRARIES}
  "-F/Applications/OBS.app/Contents/Frameworks"
  "-framework libobs"
)

target_link_options(sao800 PRIVATE
  "-F/Applications/OBS.app/Contents/Frameworks"
)

target_compile_options(sao800 PRIVATE ${GST_CFLAGS_OTHER})

# macOS: .plugin bundle
set_target_properties(sao800 PROPERTIES
  BUNDLE TRUE
  BUNDLE_EXTENSION "plugin"
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in"
  PREFIX ""
  SUFFIX ".plugin"
)

# ── Install to OBS plugin dir ─────────────────────────────────────────────────
set(OBS_PLUGIN_DIR "$ENV{HOME}/Library/Application Support/obs-studio/plugins")
install(TARGETS sao800
  LIBRARY DESTINATION "${OBS_PLUGIN_DIR}/sao800/bin"
  BUNDLE  DESTINATION "${OBS_PLUGIN_DIR}/sao800/bin"
)
