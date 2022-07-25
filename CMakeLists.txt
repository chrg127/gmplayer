cmake_minimum_required(VERSION 3.1.0)

project(gmplayer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(CMAKE_VERSION VERSION_LESS "3.7.0")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(SDL2 REQUIRED)
find_package(fmt REQUIRED)

include_directories(${SDL2_INCLUDE_DIRS})
include_directories(gme/include)
link_directories(gme/lib)

add_executable(gmplayer main.cpp gui.cpp player.cpp keyrecorder.cpp)

target_link_libraries(gmplayer Qt5::Widgets ${SDL2_LIBRARIES} fmt pthread gme)
