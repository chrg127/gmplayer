cmake_minimum_required(VERSION 3.27)

project(gmplayer VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

option(BUILD_MPRIS "Build with or without MPRIS support." ON)
set(GMP_INTERFACE "qt" CACHE STRING "Selects what interface to build. Valid options are: qt, console.")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
set(GME_DIR external/cmake)
find_package(GME REQUIRED)
find_package(fmt REQUIRED)

add_subdirectory(external/game-music-emu)
add_subdirectory(external/libgsf)

# put this right after the libraries and before add_executable so that we may get
# qt to work correctly and won't have warnings from libraries
if (GMP_INTERFACE STREQUAL "qt")
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
endif()

if (GMP_INTERFACE STREQUAL "qt")
    message("gmplayer interface set to \"qt\" -- will compile using Qt")

    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    qt_standard_project_setup()

    qt_add_executable(gmplayer
        src/player.cpp src/io.cpp src/conf.cpp src/mpris_server.cpp
        src/format.cpp src/gsf_format.cpp src/gme_format.cpp src/audio.cpp
        src/main_qt.cpp src/gui.cpp src/keyrecorder.cpp
        src/visualizer.cpp resources/icons.qrc
    )

    target_link_libraries(gmplayer PRIVATE Qt6::Widgets)

    target_compile_definitions(gmplayer PRIVATE INTERFACE_QT)

    set_target_properties(gmplayer PROPERTIES
        WIN32_EXECUTABLE ON
        MACOSX_BUNDLE ON
    )

    qt_add_resources(gmplayer imageresources
        PREFIX "/icons"
        FILES resources/icons/gmplayer32.png
    )
elseif (GMP_INTERFACE STREQUAL "console")
    message("gmplayer interface set to \"console\" -- will compile console/headless/terminal version")

    add_executable(gmplayer
        src/player.cpp src/io.cpp src/conf.cpp src/mpris_server.cpp
        src/format.cpp src/gsf_format.cpp src/gme_format.cpp src/audio.cpp
        src/main_console.cpp
    )

    target_compile_definitions(gmplayer PRIVATE INTERFACE_CONSOLE)
else()
    message(FATAL_ERROR "Unrecognized interface -- recognized ones are \"qt\" and \"console\"")
endif()

if (BUILD_MPRIS)
    message("BUILD_MPRIS is ON -- Will compile with MPRIS support")
    find_package(sdbus-c++ REQUIRED)
    target_link_libraries(gmplayer PRIVATE SDBusCpp::sdbus-c++)
else()
    message("MPRIS support is off")
    target_compile_definitions(gmplayer PRIVATE MPRIS_SERVER_NO_IMPL)
endif()

target_include_directories(gmplayer
    PRIVATE
        ${SDL2_INCLUDE_DIRS} ${GME_INCLUDE_DIRS} external/expected/include
)

target_link_directories(gmplayer
    PRIVATE
        ${SDL2_LIBRARY_DIRS} ${GME_LIBRARY_DIRS}
)

target_compile_features(gmplayer PRIVATE cxx_std_20)

target_compile_definitions(gmplayer PRIVATE _CRT_SECURE_NO_WARNINGS)

target_link_libraries(gmplayer
    PRIVATE
        SDL2::SDL2 ${GME_LIBRARIES} fmt::fmt libgsf::libgsf
)

if (GMP_INTERFACE STREQUAL "qt")
    install(TARGETS gmplayer
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    qt_generate_deploy_app_script(
        TARGET gmplayer
        FILENAME_VARIABLE deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()
