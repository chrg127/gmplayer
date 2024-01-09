cmake_minimum_required(VERSION 3.27)

project(gmplayer VERSION 1.0.0 LANGUAGES CXX)

option(BUILD_MPRIS "Build with or without MPRIS support." ON)
set(GMP_INTERFACE "qt" CACHE STRING "Selects what interface to build. Valid options are: qt, console.")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_VERBOSE_MAKEFILE ON)

if(CMAKE_VERSION VERSION_LESS "3.7.0")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

find_package(SDL2 REQUIRED)
set(GME_DIR external/cmake)
find_package(GME REQUIRED)
find_package(fmt REQUIRED)

add_subdirectory(external/libgsf)

# put this right after the libraries and before add_executable so that we may get
# qt to work correctly and won't have warnings from libraries
if (GMP_INTERFACE STREQUAL "qt")
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
endif()

add_executable(gmplayer
    src/player.cpp src/io.cpp src/conf.cpp src/mpris_server.cpp
    src/format.cpp src/gsf_format.cpp src/gme_format.cpp
)

target_include_directories(gmplayer
    PRIVATE
        ${SDL2_INCLUDE_DIRS} ${GME_INCLUDE_DIRS} external/expected/include
)

target_link_directories(gmplayer
    PRIVATE
        ${SDL2_LIBRARY_DIRS}
        ${GME_LIBRARY_DIRS}
)

target_compile_features(gmplayer PRIVATE cxx_std_20)

target_compile_definitions(gmplayer PRIVATE _CRT_SECURE_NO_WARNINGS)

target_link_libraries(gmplayer
    ${SDL2_LIBRARIES} ${GME_LIBRARIES} fmt::fmt libgsf::libgsf
)

if (GMP_INTERFACE STREQUAL "qt")
    message("gmplayer interface set to \"qt\" -- will compile using Qt")
    find_package(Qt5 REQUIRED COMPONENTS Core Widgets)
    target_sources(gmplayer
        PRIVATE
            src/main_qt.cpp src/gui.cpp src/keyrecorder.cpp
            src/visualizer.cpp resources/icons.qrc
    )
    target_link_libraries(gmplayer Qt5::Widgets)
    target_compile_definitions(gmplayer PRIVATE INTERFACE_QT)
elseif (GMP_INTERFACE STREQUAL "console")
    message("gmplayer interface set to \"console\" -- will compile console/headless/terminal version")
    target_sources(gmplayer PRIVATE src/main_console.cpp)
    target_compile_definitions(gmplayer PRIVATE INTERFACE_CONSOLE)
else()
    message(FATAL_ERROR "Unrecognized interface -- recognized ones are \"qt\" and \"console\"")
endif()

if (BUILD_MPRIS)
    message("BUILD_MPRIS is ON -- Will compile with MPRIS support")
    find_package(sdbus-c++ REQUIRED)
    target_link_libraries(gmplayer SDBusCpp::sdbus-c++)
else()
    message("MPRIS support is off")
    target_compile_definitions(gmplayer PRIVATE MPRIS_SERVER_NO_IMPL)
endif()

# install(FILES resources/linux/gmplayer.desktop DESTINATION share/applications/ COMPONENT desktop)
# install(FILES resources/linux/gmplayer.png DESTINATION share/icons/hicolor/256x256/apps COMPONENT desktop)
# install(TARGETS gmplayer DESTINATION bin COMPONENT binaries)
