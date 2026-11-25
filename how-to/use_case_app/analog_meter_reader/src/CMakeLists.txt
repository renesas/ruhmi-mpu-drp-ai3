cmake_minimum_required(VERSION 3.10)
project(analog_meter_reader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(default_build_type "Release")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(NOT DEFINED ENV{PRODUCT})
  message(SEND_ERROR "Need to specify product name with option PRODUCT")
endif()

set(EXE_NAME analog_reader)
set(PRODUCT $ENV{PRODUCT})

# Runtime wrapper sources shared with apps/ (DrpAiRuntimeWrapper, Fp16Utils)
get_filename_component(RUNTIME_DIR ${CMAKE_SOURCE_DIR}/../../../../apps ABSOLUTE)

if(PRODUCT STREQUAL "V2H")
    include_directories(${RUNTIME_DIR})
    file(GLOB SOURCE *.cpp *.h)
    list(APPEND SOURCE ${RUNTIME_DIR}/DrpAiRuntimeWrapper.cpp)
    add_definitions(-DV2H)
    message("-- PRODUCT [V2H]") 
elseif(PRODUCT STREQUAL "V2N")
    include_directories(${RUNTIME_DIR})
    file(GLOB SOURCE *.cpp *.h)
    list(APPEND SOURCE ${RUNTIME_DIR}/DrpAiRuntimeWrapper.cpp)
    include_directories(${CMAKE_SYSROOT}/usr/src/debug/gstreamer1.0-plugins-base/1.22.12/gst-libs/gst/gl)
    list(APPEND SOURCE ${CMAKE_SYSROOT}/usr/src/debug/gstreamer1.0-plugins-base/1.22.12/gst-libs/gst/gl/xdg-shell-client-protocol.c)
    add_definitions(-DV2H -DV2N)
    message("-- PRODUCT [V2N]")
endif()

add_executable(${EXE_NAME} ${SOURCE})
target_compile_options(${EXE_NAME} PRIVATE -O2)
target_compile_features(${EXE_NAME} PUBLIC cxx_std_17)

target_link_libraries(${EXE_NAME}
    wayland-client 
    wayland-cursor 
    wayland-egl
    opencv_imgcodecs 
    opencv_imgproc 
    opencv_core 
    opencv_highgui
    EGL 
    GLESv2
    pthread
    jpeg
)

target_compile_definitions(${EXE_NAME} PUBLIC KDLDRPAI)

find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)
    target_include_directories(${EXE_NAME} PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(${EXE_NAME} ${OpenCV_LIBS})
endif()


if(PRODUCT STREQUAL "V2H")
    target_compile_definitions(${EXE_NAME} PRIVATE V2H)
endif()
if(PRODUCT STREQUAL "V2N")
    target_compile_definitions(${EXE_NAME} PRIVATE V2N)
endif()
