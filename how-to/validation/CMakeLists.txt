cmake_minimum_required(VERSION 3.24)
set(CMAKE_CXX_STANDARD 17)
project(validation)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(default_build_type "Release")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(NOT DEFINED ENV{REPO_ROOT})
  message(SEND_ERROR "Need to specify the repository location with option REPO_ROOT")
endif()

set(REPO_ROOT "$ENV{REPO_ROOT}")
include_directories(${REPO_ROOT}/apps)

if(V2H OR V2N)
  set(SRC validation.cpp ${REPO_ROOT}/apps/DrpAiRuntimeWrapper.cpp)
  set(EXE_NAME validation)
else()
   message(SEND_ERROR "Supported Device is V2H or V2N only")
endif()

add_executable(${EXE_NAME} ${SRC})
target_link_libraries(${EXE_NAME} PRIVATE ${OpenCV_LIBS})
target_link_libraries(${EXE_NAME} PRIVATE pthread)

target_compile_options(${EXE_NAME} PRIVATE -O3 -mtune=cortex-a55 -Wall -DDEBUG_LOG)
