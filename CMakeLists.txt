cmake_minimum_required(VERSION 3.5) # CMake install : https://cmake.org/download/
project(demo LANGUAGES CXX)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories("./lib/ffmpeg/include")          # 引入外部头文件
link_directories(${CMAKE_SOURCE_DIR}/lib/ffmpeg/lib) # 引入动态链接库
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${CMAKE_SOURCE_DIR}/lib/ffmpeg/lib") # 引入动态链接库, 与link_directories()效果等同

SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin) # 设置可执行文件输出路径

find_package(Qt5 COMPONENTS Widgets Core Multimedia REQUIRED) # Qt COMPONENTS

aux_source_directory(./src srcs)

# Specify MSVC UTF-8 encoding   
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

add_executable(${PROJECT_NAME}
    # WIN32 # If you need a terminal for debug, please comment this statement 
    ${srcs} 
) 
target_link_libraries(${PROJECT_NAME}  Qt5::Widgets Qt5::Core Qt5::Multimedia) # Qt5 Shared Library
target_link_libraries(${PROJECT_NAME} -Wl,--start-group avcodec avformat avutil swresample swscale -Wl,--end-group) # FFmpeg Shared Library