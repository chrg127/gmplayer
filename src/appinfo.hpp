#pragma once

#include <QString>

static const QString application_name = "gmplayer";

static const QString version = "v0.1";

static const QString about_text = R"(
<p style="white-space: pre-wrap; margin: 25px;">

A music player for retro game music.
Supports the following file formats: SPC, GYM, NSF, NSFE, GBS, AY, KSS, HES, VGM, SAP.
</p>
<p style="white-space: pre-wrap; margin: 25px;">
gmplayer is distributed under the GNU GPLv3 license.

<a href="https://github.com/chrg127/gmplayer">Home page</a>.
</p>
)";

static const QString lib_text = R"(
<p style="white-space: pre-wrap; margin: 25px;">

gmplayer uses the following libraries:
</p>
<ul>
    <li><a href="https://www.qt.io/">Qt5 (Base, GUI, Widgets, DBus)</a></li>
    <li><a href="https://bitbucket.org/mpyne/game-music-emu/wiki/Home">Game_Music_Emu</a></li>
    <li><a href="https://www.libsdl.org">SDL2</a></li>
    <li><a href="https://github.com/sailfishos/qtmpris">MprisQt</a></li>
</ul>

<p style="white-space: pre-wrap; margin: 25px;">

</p>
)";
