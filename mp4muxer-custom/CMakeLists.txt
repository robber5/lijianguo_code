# CMake 最低版本号要求
cmake_minimum_required (VERSION 3.5)

# 项目信息
project (Mp4Muxer)
SET(CMAKE_C_COMPILER "aarch64-hisiv610-linux-gcc")
SET(CMAKE_CXX_COMPILER "aarch64-hisiv610-linux-g++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# 查找目录下的所有源文件
# 并将名称保存到 DIR_SRCS 变量
aux_source_directory(. DIR_SRCS)

# 添加 math 子目录
add_subdirectory(mp4lib)

# 指定生成目标
add_executable(Mp4Muxer ${DIR_SRCS})

# 添加链接库
target_link_libraries(Mp4Muxer Mp4Functions)
