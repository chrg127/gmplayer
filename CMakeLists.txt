cmake_minimum_required(VERSION 3.5)

project(gmplayer VERSION 1.0.0 LANGUAGES CXX)

# default interface
if (NOT DEFINED GMP_INTERFACE)
    SET(GMP_INTERFACE "qt")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
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
find_package(sdbus-c++ REQUIRED)
find_package(PkgConfig REQUIRED)
# pkg_check_modules(MPRISQT5 REQUIRED mpris-qt5)
pkg_check_modules(GME REQUIRED libgme)

include_directories(${SDL2_INCLUDE_DIRS})
include_directories(${MPRISQT5_INCLUDE_DIRS})
include_directories(${GME_INCLUDE_DIRS})
include_directories(external/include)

link_directories(${SDL2_LIBRARY_DIRS})
link_directories(${MPRISQT5_LIBRARY_DIRS})
link_directories(${GME_LIBRARY_DIRS})

if (WIN32)
    add_compile_definitions(MPRIS_SERVER_NO_IMPL)
endif()

set(SOURCE_FILES
    src/main.cpp src/player.cpp src/io.cpp src/format.cpp
    src/mpris_server.cpp src/conf.cpp
)

if (GMP_INTERFACE STREQUAL "qt")
    add_compile_definitions(INTERFACE_QT)
    set(SOURCE_FILES ${SOURCE_FILES}
        src/gui.cpp src/keyrecorder.cpp src/visualizer.cpp resources/icons.qrc
    )
elseif (GMP_INTERFACE STREQUAL "console")
    add_compile_definitions(INTERFACE_CONSOLE)
endif()

add_executable(gmplayer ${SOURCE_FILES})

target_link_libraries(gmplayer PRIVATE pthread Qt5::Widgets ${SDL2_LIBRARIES} ${GME_LIBRARIES} SDBusCpp::sdbus-c++ fmt)

install(FILES resources/linux/gmplayer.desktop DESTINATION share/applications/ COMPONENT desktop)
install(FILES resources/linux/gmplayer.png DESTINATION share/icons/hicolor/256x256/apps COMPONENT desktop)
install(TARGETS gmplayer DESTINATION bin COMPONENT binaries)
