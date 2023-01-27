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
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPRISQT5 REQUIRED mpris-qt5)
pkg_check_modules(GME REQUIRED libgme)

include_directories(${SDL2_INCLUDE_DIRS})
include_directories(${MPRISQT5_INCLUDE_DIRS})
include_directories(${GME_INCLUDE_DIRS})

link_directories(${SDL2_LIBRARY_DIRS})
link_directories(${MPRISQT5_LIBRARY_DIRS})
link_directories(${GME_LIBRARY_DIRS})

add_executable(gmplayer src/main.cpp src/gui.cpp src/player.cpp src/keyrecorder.cpp src/io.cpp resources/icons.qrc)

target_link_libraries(gmplayer pthread Qt5::Widgets ${SDL2_LIBRARIES} ${GME_LIBRARIES} ${MPRISQT5_LIBRARIES})
