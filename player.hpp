#pragma once

class QString;
class Music_Emu;
class gme_info_t;

namespace player {

void init();
void quit();

void use_file(const QString &file);
void start_or_resume();
void pause();
void stop();
void next();
void prev();

struct Metadata {
    gme_info_t *info;
    int length;
};

Metadata get_track_metadata();
int get_track_time();

} // namespace player
