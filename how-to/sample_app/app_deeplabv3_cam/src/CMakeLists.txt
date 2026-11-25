# CMakeList.txt : CMake projects in app_deeplabv3_cam
cmake_minimum_required (VERSION 3.24)
set(CMAKE_CXX_STANDARD 17)

if(NOT DEFINED APP_NAME)
  message(FATAL_ERROR "-DAPP_NAME is required. \n")
endif()

project(${APP_NAME} LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(default_build_type "Release")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Setting and including  Directory
set(REPO_ROOT "$ENV{REPO_ROOT}")
include_directories(./)
include_directories(${REPO_ROOT}/apps)
include_directories(${REPO_ROOT}/how-to/sample_app_v2h/common_files)
include_directories(${REPO_ROOT}/how-to/sample_app_v2h/common_files/package)
#include_directories(SYSTEM $ENV{OECORE_NATIVE_SYSROOT} ${POKY_PATH}/usr/include )

set(EXE_NAME ${PROJECT_NAME}) 

file(GLOB SRC * ${REPO_ROOT}/how-to/sample_app_v2h/common_files/* ${REPO_ROOT}/apps/DrpAiRuntimeWrapper.*
    ${REPO_ROOT}/apps/PreRuntime.h ${REPO_ROOT}/apps/PreRuntimeV2H.cpp)

if("${APP_NAME}" STREQUAL "app_yolov2_cam")
    list(APPEND SRC
        "${CMAKE_CURRENT_LIST_DIR}/../../app_yolox_cam/src/image_yolox.cpp"
    )
endif()

include_directories(${CMAKE_SYSROOT}/usr/src/debug/gstreamer1.0-plugins-base/1.22.12/gst-libs/gst/gl)
list(APPEND SRC ${CMAKE_SYSROOT}/usr/src/debug/gstreamer1.0-plugins-base/1.22.12/gst-libs/gst/gl/xdg-shell-client-protocol.c)

set(PRODUCT $ENV{PRODUCT})

if(PRODUCT STREQUAL "V2H")
    add_definitions(-DV2H)
    message("-- PRODUCT [V2H]")
elseif(PRODUCT STREQUAL "V2N")
    add_definitions(-DV2H -DV2N)
    message("-- PRODUCT [V2N]")
else()
    message("-- PRODUCT [???]")
endif()

add_executable(${EXE_NAME} ${SRC})


target_compile_definitions(${EXE_NAME} PRIVATE IMAGE_MODE)
target_compile_definitions(${EXE_NAME} PRIVATE V2H)
target_compile_definitions(${EXE_NAME} PRIVATE DEBUG_LOG)
target_compile_definitions(${EXE_NAME} PRIVATE NDEBUG)
target_compile_features(${EXE_NAME} PUBLIC cxx_std_17)

#link_directories(${POKY_PATH}/usr/lib64/)
#include_directories("include")
target_include_directories(${EXE_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(${EXE_NAME} PUBLIC ${REPO_ROOT}/3rdparty/spdlog/include)

find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)
    target_include_directories(${EXE_NAME} PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(${EXE_NAME} PRIVATE ${OpenCV_LIBS})
endif()

target_link_libraries(${EXE_NAME} PRIVATE
  pthread
  turbojpeg
  jpeg
  EGL 
  GLESv2
  wayland-cursor 
  wayland-egl 
  wayland-client
  mmngr 
  mmngrbuf
)
target_compile_options(${EXE_NAME} PRIVATE -O3 -mtune=cortex-a55 -Wall)
