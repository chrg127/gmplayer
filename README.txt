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

DEPENDECIES

The following libraries have been used for building this player and are required
when installing:

    - Game_Music_Emu (GME): https://bitbucket.org/mpyne/game-music-emu/wiki/Home
    - SDL2: https://www.libsdl.org
    - QtMpris: https://github.com/sailfishos/qtmpris

Assuming you are in a debian-based distribution, both SDL2 and QtMpris can be
installed with the following command:

    sudo apt install libmpris-qt5-1 libmpris-qt5-dev libsdl2-2.0-0 libsdl2-dev

For other distributions the specific names of the package may vary.

GME must be built and installed manually. Installation instruction for GME can
be found in the link above.

INSTALLING

Once the dependencies above have been installed, the project can simply be
built using CMake:

    cmake . -B build
    make -C build

The commands should be run under the project's root directory. -B for cmake
specifies the directory to put the build files. -C for make specifies where
to find the Makefile.
After the commands are done, the executable should be found inside build/.

