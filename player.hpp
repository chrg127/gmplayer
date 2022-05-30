#pragma once

class QString;
class Music_Emu;
class gme_info_t;

namespace player {

void init();
void use_file(const QString &file);
void start_or_resume();
void stop();
void next();
void prev();

} // namespace player
