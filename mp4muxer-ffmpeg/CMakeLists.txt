cmake_minimum_required(VERSION 3.5)
project(mp4Muxer)
SET(CMAKE_C_COMPILER "aarch64-hisiv610-linux-gcc")

#指定C++交叉编译器
SET(CMAKE_CXX_COMPILER "aarch64-hisiv610-linux-g++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(FFMPEG_INCLUDE_DIR "/home/lijianguo/ffmpeg_build/include")
set(FFMPEG_LIB_DIR "/home/lijianguo/ffmpeg_build/lib")

include_directories(${FFMPEG_INCLUDE_DIR})
link_directories(${FFMPEG_LIB_DIR})

set(SOURCE_FILES main.cpp)
add_executable(mp4Muxer ${SOURCE_FILES})
target_link_libraries(mp4Muxer avcodec avformat swscale avutil pthread)
