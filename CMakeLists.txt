cmake_minimum_required(VERSION 3.20)
project(apkx LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

# Build source list
set(SOURCES
  src/main.cpp
  src/cli.cpp
  src/paths.cpp
  src/registry.cpp
  src/install.cpp
  src/run.cpp
  src/fetch.cpp
  src/elf_loader.cpp
  src/dex_loader.cpp
  # Runtime core
  src/runtime.cpp
  src/vm.cpp
  src/bionic_darwin.cpp
  src/jni_bridge.cpp
  src/graphics_metal.cpp
  src/system_services.cpp
  src/dex_interpreter.cpp
  src/syscall_hook.cpp
  src/dex_executor.cpp
  src/real_dex_executor.cpp
  src/app_execution.cpp
)

# Graphics backend (Objective-C++ for macOS)
if(APPLE)
  list(APPEND SOURCES src/graphics_cocoa.mm src/android_ui_renderer.mm)
  set_source_files_properties(src/graphics_cocoa.mm src/android_ui_renderer.mm PROPERTIES COMPILE_FLAGS "-ObjC++")
endif()

add_executable(apkx ${SOURCES})

target_include_directories(apkx PRIVATE include)
target_include_directories(apkx PRIVATE third_party)

find_package(CURL REQUIRED)
target_link_libraries(apkx PRIVATE CURL::libcurl)

# Link macOS frameworks
if(APPLE)
  find_library(METAL_FRAMEWORK Metal)
  find_library(METALKIT_FRAMEWORK MetalKit)
  find_library(COCOA_FRAMEWORK Cocoa)
  find_library(QUARTZ_FRAMEWORK QuartzCore)
  
  target_link_libraries(apkx PRIVATE
    ${METAL_FRAMEWORK}
    ${METALKIT_FRAMEWORK}
    ${COCOA_FRAMEWORK}
    ${QUARTZ_FRAMEWORK}
  )
endif()

