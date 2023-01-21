=== Intro ===
gmplayer is a music player for game music (nsf, spc, etc.).

INSTALLING

1.  Install the GME (Game_Music_Emu) library.
    GME can be downloaded from here: https://bitbucket.org/mpyne/game-music-emu/wiki/Home
    After downloading, it should be built and installed under a directory gme/ inside
    this project's root directory. The following commands (run under the GME root dir)
    should do the trick:

        GMPLAYER_DIR="path/to/gmplayer"
        mkdir "$GMPLAYER_DIR/gme"
        cmake . -B build -DCMAKE_INSTALL_PREFIX="$GMPLAYER_DIR/gme"
        make -C build
        make -C build install

2.  Build the project using cmake:

        cmake . -B build
        make -C build

3.  The executable should be found inside build/.

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

