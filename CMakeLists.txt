cmake_minimum_required(VERSION 3.27)

project(gmplayer VERSION 1.0.0 LANGUAGES CXX)

if (NOT DEFINED GMP_INTERFACE)
    SET(GMP_INTERFACE "qt")
endif()

if (NOT DEFINED BUILD_MPRIS)
    SET(BUILD_MPRIS ON)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_VERBOSE_MAKEFILE ON)
set(GME_DIR external/cmake)

if(CMAKE_VERSION VERSION_LESS "3.7.0")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

find_package(SDL2 REQUIRED)
find_package(GME REQUIRED)
find_package(fmt REQUIRED)

include_directories(${SDL2_INCLUDE_DIRS})
include_directories(${GME_INCLUDE_DIRS})
include_directories(external/expected/include)

link_directories(${SDL2_LIBRARY_DIRS})
link_directories(${GME_LIBRARY_DIRS})

add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

set(SOURCE_FILES src/player.cpp src/io.cpp src/format.cpp src/conf.cpp src/mpris_server.cpp)
set(LIBS ${SDL2_LIBRARIES} ${GME_LIBRARIES} fmt::fmt)

if (GMP_INTERFACE STREQUAL "qt")
    message("gmplayer interface set to \"qt\" -- will compile using QT")
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
    find_package(Qt5 COMPONENTS Widgets REQUIRED)
    add_compile_definitions(INTERFACE_QT)
    set(SOURCE_FILES ${SOURCE_FILES} src/main_qt.cpp src/gui.cpp src/keyrecorder.cpp src/visualizer.cpp resources/icons.qrc)
    set(LIBS ${LIBS} Qt5::Widgets)
elseif (GMP_INTERFACE STREQUAL "console")
    message("gmplayer interface set to \"console\" -- will compile console/headless/terminal version")
    set(SOURCE_FILES ${SOURCE_FILES} src/main_console.cpp)
else()
    message(FATAL_ERROR "Unrecognized interface -- recognized ones are \"qt\" and \"console\"")
endif()

if (BUILD_MPRIS STREQUAL ON)
    message("BUILD_MPRIS is ON -- Will compile with MPRIS support")
    find_package(sdbus-c++ REQUIRED)
    set(LIBS ${LIBS} SDBusCpp::sdbus-c++)
else()
    message("MPRIS support is off")
    add_compile_definitions(MPRIS_SERVER_NO_IMPL)
endif()

add_executable(gmplayer ${SOURCE_FILES})
target_link_libraries(gmplayer PRIVATE ${LIBS})

install(FILES resources/linux/gmplayer.desktop DESTINATION share/applications/ COMPONENT desktop)
install(FILES resources/linux/gmplayer.png DESTINATION share/icons/hicolor/256x256/apps COMPONENT desktop)
install(TARGETS gmplayer DESTINATION bin COMPONENT binaries)
