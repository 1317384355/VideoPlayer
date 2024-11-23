cmake_minimum_required(VERSION 3.5) # CMake install : https://cmake.org/download/
project(demo LANGUAGES CXX)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_PREFIX_PATH "D:/lib/Qt/5.15.2/mingw81_64") # Qt Kit Dir
set(OpenCV_DIR "D:/lib/OpenCV/x64/mingw/lib")  # 用cmake编译后的目录（2）
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories("D:/lib/ffmpeg/include")                                             # 引入外部头文件
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -LD:/lib/ffmpeg/lib/${CMAKE_ANDROID_ARCH_ABI}/") # 引入动态链接库


find_package(OpenCV REQUIRED)  # 添加库
find_package(Qt5 COMPONENTS Widgets Core Multimedia REQUIRED) # Qt COMPONENTS

# include_directories(${OpenCV_INCLUDE_DIRS})  # 添加opencv目录下所包含的全部文件

aux_source_directory(./src srcs)

# Specify MSVC UTF-8 encoding   
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

add_executable(${PROJECT_NAME}
    # WIN32 # If you need a terminal for debug, please comment this statement 
    ${srcs} 
) 
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})        # OpenCV Shared Library
target_link_libraries(${PROJECT_NAME}  Qt5::Widgets Qt5::Core Qt5::Multimedia) # Qt5 Shared Library
target_link_libraries(${PROJECT_NAME} -Wl,--start-group avcodec avfilter avformat avutil swresample swscale -Wl,--end-group) # FFmpeg Shared Library