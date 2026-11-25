cmake_minimum_required(VERSION 3.10)
project(tutorial_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(tutorial_app
  tutorial_app.cpp
  DrpAiRuntimeWrapper.cpp
  PreRuntimeV2H.cpp
)

target_compile_definitions(tutorial_app PRIVATE V2H)
target_compile_options(tutorial_app PRIVATE -O3 -Wall)
target_link_libraries(tutorial_app PRIVATE pthread)
