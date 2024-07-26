GMPLAYER

gmplayer is a music player for retro game music. It's a minimal player
directed at a more casual audience, and offers the usual features such as
playlists, playback speed, fade, MPRIS support and more.

SUPPORTED FORMATS

The supported formats correspond to GME's supported formats. Here's a
comprehensive list:

    - SPC   (SPC700, Super Nintendo Entertainment System)
    - GYM   (Gensis YM2612, Sega Genesis)
    - NSF   (Nintendo Entertainment System)
    - NSFE  (Nintendo Entertainment System)
    - GBS   (Game Boy)
    - AY    (AY-3-8910 chip, used on Amstrad CPC, ZX Spectrum)
    - KSS   (Konami Sound System chip, used in MSX, Sega Master System and Game Gear)
    - HES   (NEC Home Entertainment System i.e. PC Engine/TurboGrafx)
    - VGM   (Video Game Music, generic)
    - SAP   (Slight Atari Player, used on Atari computers)

OTHER FEATURES

- Playlists, of course.
- An MPRIS interface.
- A small visualizer.
- GUI and console/terminal interfaces.
- Very, very small.

DEPENDENCIES

The following libraries have been used for building this player and are required
when installing:

    - Game_Music_Emu (GME): https://bitbucket.org/mpyne/game-music-emu/wiki/Home
    - Qt5 (base, gui, widgets): https://www.qt.io/
    - SDL2: https://www.libsdl.org
    - libfmt: https://fmt.dev
    - sdbus-c++: https://github.com/Kistler-Group/sdbus-cpp

Assuming you are in a debian-based distribution, required libs can be installed
with the following command:

    sudo apt install qtbase5-dev libsdl2-dev libfmt-dev libsdbus-c++-dev

For other distributions the specific names of the package may vary. Note that
only Qt, SDL, fmt and sdbus-c++ must be installed externally; GME is bundled
using submodules.

If you didn't clone using --recursive, make sure to checkout this repository's
submodules:

    git submodule update --init --recursive

COMPILING AND INSTALLING

Once the dependencies above have been installed, the project can simply be
built using CMake:

    cmake . -B build -DCMAKE_BUILD_TYPE=Release
    make -C build
    make -C build install # currently may not work

The commands should be run under the project's root directory. For cmake:

    - -B specifies the directory to put the build files
    - -DCMAKE_BUILD_TYPE=Release specifies to build in release mode
    - -DCMAKE_CXX_FLAGS="-mwindows" should be added in case you are doing a
      windows build: it will cause not to spanw a useless cmd window when
      running the program

When running CMake, you can also choose what interface to compile using
-DGMP_INTERFACE=[interfacename]. [interfacename] can be:

    - qt: the default interface. The program will use a full GUI interface that
      should be fully cross-platform.
    - console: a more minimal interface, intended for console/terminal/headless
      interfaces. It probably won't work on non-linux OSes.

You can also choose to build with or without MPRIS support by passing
-DBUILD_MPRIS=ON or OFF.

For make, -C specifies the directory where to find the Makefile.
When compilation is completed, the executable should be found inside build/.

WINDOWS

On Windows, you must install both conan and Qt. You should install Qt
through the online installer.
When everything is installed, issue the following commands on the project root:

    set QT_DIR=/path/to/qt
    conan install . --output-folder=build --build=missing
    cd build
    cmake .. -DGMP_INTERFACE=qt -DBUILD_MPRIS=OFF -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_PREFIX_PATH=%QT_DIR%
    cmake --build . --config Release
    cmake --install .

Where:

    - /path/to/qt is where your Qt installation is located. Note that it's not
      enough to specify C:\Qt here; for example, if you've got an installation
      with Qt 6.6.1 and MSVC 2019, you should specify C:\Qt\6.6.1\msvc2019_64
    - conan will install the needed libraries (zlib, sdl, fmt) inside the build/
      directory
    - The CMake command specifies the Qt interface and turns off MPRIS support
    - The second CMake command specifies to build in Release mode
    - In the third CMake command you may specify a directory to install to by
      using --prefix <dir>

