cmake_minimum_required(VERSION 3.20)
project(SabakCore CXX)
set(CMAKE_CXX_STANDARD 17)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_library(sabakcore STATIC src/mesh.cpp src/renderer.cpp src/game.cpp src/ui.cpp src/app.cpp src/fonts/Poppins_Bold.cpp src/fonts/Poppins_Medium.cpp)
target_include_directories(sabakcore PUBLIC src third_party third_party/khr)
# headless simulator (Linux): needs libegl1 + libgles2 + mesa runtime
if(NOT IOS AND NOT APPLE)
  add_executable(sabaksim src/sim.cpp src/platform_egl.cpp)
  target_link_libraries(sabaksim sabakcore -l:libEGL.so.1 -l:libGLESv2.so.2)
endif()

# Windowed app (iOS / desktop) via SDL3:  cmake -S core -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DSABAK_SDL=ON
option(SABAK_SDL "Build the SDL3 windowed app" OFF)
if(SABAK_SDL)
  include(FetchContent)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-3.2.22 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(SDL3)
  add_executable(SabakGame MACOSX_BUNDLE src/main_sdl.cpp)
  target_link_libraries(SabakGame PRIVATE sabakcore SDL3::SDL3)
  if(IOS)
    set_target_properties(SabakGame PROPERTIES
      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
      MACOSX_BUNDLE_GUI_IDENTIFIER com.sabak.engine
      MACOSX_BUNDLE_BUNDLE_NAME SabakEngine
      XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO
      XCODE_ATTRIBUTE_ENABLE_BITCODE NO)
    target_compile_definitions(sabakcore PUBLIC GLES_SILENCE_DEPRECATION=1)
  endif()
endif()
