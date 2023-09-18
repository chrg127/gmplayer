#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <vector>
#include <SDL_audio.h> // SDL_AudioDeviceID
#include "common.hpp"
#include "format.hpp"
#include "callback_handler.hpp"

namespace mpris { struct Server; }
namespace io { class File; class MappedFile; }

namespace gmplayer {

struct SDLMutex {
    SDL_AudioDeviceID id;
    SDLMutex() = default;
    SDLMutex(SDL_AudioDeviceID id) : id{id} {}
    void lock()   { SDL_LockAudioDevice(id); }
    void unlock() { SDL_UnlockAudioDevice(id); }
};

struct PlayerOptions {
    int fade_out;
    // bool autoplay;
    bool track_repeat;
    bool file_repeat;
    int default_duration;
    double tempo;
    int volume;
};

struct Playlist {
    enum Type { Track, File };

    std::vector<int> order;
    int current = -1;
    bool repeat = false;

    void regen();
    void regen(int size);
    void shuffle();
    void clear()         { order.clear(); current = -1; }
    void remove(int i)   { order.erase(order.begin() + i); }

    int move(int i, int pos)
    {
        if (i + pos < 0 || i + pos > order.size() - 1)
            return i;
        std::swap(order[i], order[i+pos]);
        return i+pos;
    }

    std::optional<int> get(int off, int min, int max) const
    {
        return repeat && current != -1                    ? std::optional{current}
             : current + off < max && current + off > min ? std::optional{current + off}
             : std::nullopt;
    }

    std::optional<int> next() const { return get(+1, -1, order.size()); }
    std::optional<int> prev() const { return get(-1, -1, order.size()); }
    std::size_t size() const { return order.size(); }
};

class Player {
    std::unique_ptr<FormatInterface> format;
    std::vector<io::MappedFile> file_cache;
    std::vector<Metadata> track_cache;
    Playlist files;
    Playlist tracks;
    std::unique_ptr<mpris::Server> mpris;

    struct {
        SDL_AudioDeviceID dev_id = 0;
        mutable SDLMutex mutex;
        SDL_AudioSpec spec;
    } audio;

    struct {
        bool autoplay = false;
        int volume = 0;
    } options;

    struct {
        std::array<int, NUM_VOICES> volume = { MAX_VOLUME_VALUE / 2, MAX_VOLUME_VALUE / 2,
                                               MAX_VOLUME_VALUE / 2, MAX_VOLUME_VALUE / 2,
                                               MAX_VOLUME_VALUE / 2, MAX_VOLUME_VALUE / 2,
                                               MAX_VOLUME_VALUE / 2, MAX_VOLUME_VALUE / 2, };
    } effects;

    void audio_callback(std::span<u8> stream);

public:
    Player();
    ~Player();

    using AddFileError = std::pair<std::filesystem::path, std::error_code>;

    std::vector<AddFileError> add_file(std::filesystem::path path);
    std::vector<AddFileError> add_files(std::span<std::filesystem::path> paths);
    void remove_file(int id);
    void remove_files(std::span<int> ids);

    void load_file(int id);
    void load_track(int num);
    void load_pair(int file, int track);
    void clear();
    void start_or_resume();
    void pause();
    void play_pause();
    void stop();
    void seek(int ms);
    void seek_relative(int off);
    void next();
    void prev();
    void shuffle(Playlist::Type which);
    int move(Playlist::Type which, int n, int pos);

    bool is_playing() const;
    int position() const;
    int length() const;
    bool is_multi_channel() const;
    bool has_next() const;
    bool has_prev() const;
    int current_track() const;
    int current_file() const;
    int current_of(Playlist::Type type) const;
    int track_count() const;
    int file_count() const;
    int count_of(Playlist::Type type) const;
    const Metadata & track_info(int id) const;
    const io::MappedFile & file_info(int id) const;
    const std::vector<Metadata> file_tracks(int id) const;
    void loop_tracks(std::function<void(int, const Metadata &)> fn) const;
    void loop_files(std::function<void(int, const io::MappedFile &)> fn) const;

    std::vector<std::string> channel_names();
    void mute_channel(int index, bool mute);
    void set_channel_volume(int index, int value);

    mpris::Server &mpris_server();

#define MAKE_SIGNAL(name, ...) \
private:                                            \
    CallbackHandler<void(__VA_ARGS__)> name;        \
public:                                             \
    void on_##name(auto &&fn) { name.add(fn); }     \

    MAKE_SIGNAL(file_changed, int)
    MAKE_SIGNAL(track_changed, int, const Metadata &)
    MAKE_SIGNAL(position_changed, int)
    MAKE_SIGNAL(track_ended, void)
    MAKE_SIGNAL(paused, void)
    MAKE_SIGNAL(played, void)
    MAKE_SIGNAL(seeked, int)
    MAKE_SIGNAL(tempo_changed, double)
    MAKE_SIGNAL(fade_changed, int);
    MAKE_SIGNAL(shuffled, Playlist::Type)
    MAKE_SIGNAL(error, Error)
    MAKE_SIGNAL(cleared, void)
    MAKE_SIGNAL(playlist_changed, Playlist::Type)
    MAKE_SIGNAL(files_removed, std::span<int>)
    MAKE_SIGNAL(samples_played, std::span<i16>, std::span<f32>)
    MAKE_SIGNAL(channel_volume_changed, int, int)
    MAKE_SIGNAL(first_file_load, void)

#undef MAKE_SIGNAL
};

inline bool is_playlist(std::filesystem::path filename) { return filename.extension() == ".playlist"; }
tl::expected<std::vector<std::filesystem::path>, std::error_code> open_playlist(std::filesystem::path file_path);

std::string format_metadata(std::string_view fmt, int track_id, const Metadata &m, int track_count);
std::string format_file(std::string_view fmt, int file_id, const io::MappedFile &file, int file_count);
std::string format_status(std::string_view fmt, const gmplayer::Player &player);

} // namespace gmplayer
