=== Intro ===

gmplayer is a music player for game music (nsf, spc, etc.).

=== Installing ===

1.  Install the GME (Game_Music_Emu) library.
    GME can be downloaded from here: https://bitbucket.org/mpyne/game-music-emu/wiki/Home
    After downloading it, it should be built and installed under a directory gme/ inside
    this project's root directory. The following commands (run under the GME root dir)
    should do the trick:

        GMPLAYER_DIR="path/to/gmplayer"
        mkdir "$GMPLAYER_DIR/gme"
        cmake . -B build -DCMAKE_INSTALL_PREFIX="$GMPLAYER_DIR/gme"
        make -C build
        make -C build install

2.  Build the project using cmake:

        cmake . -B build
        make -B build

3.  The executable should be found inside build/.
