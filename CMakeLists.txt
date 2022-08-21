cmake_minimum_required(VERSION 3.9)
project(future)
set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

include(FetchContent)
FetchContent_Declare(
  channel
  GIT_REPOSITORY https://github.com/tenheadedlion/cpp-channel.git
  GIT_TAG 871097be591dee9e692312f1f6dff9a89a0ab13f
)
#FetchContent_Populate(channel)
FetchContent_MakeAvailable(channel)
include_directories(${channel_SOURCE_DIR}/include)


set(future_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(future_LIB future_lib)
add_library(${future_LIB} INTERFACE)
target_include_directories(${future_LIB} INTERFACE
  ${future_INCLUDE_DIR}
)


set(future_EXEC timer)
add_executable(${future_EXEC} 
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/timer.cpp)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)


