/*
 * Based on code from gbsplay and mpv.
 * If in doubt, consult those two projects too.
 */

#include <cstdio>
#include <cstdlib>
#include <utility>
#include "common.hpp"

#ifdef PLATFORM_LINUX

#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>

struct Terminal {
    termios ots = {};

    Terminal()
    {
        termios ts = {};
        if (tcgetattr(STDIN_FILENO, &ts) == -1)
            return;
        // storing this so we can restore stdin options when quitting
        ots = ts;
        // disable canonical mode, (ICANON)
        // don't echo input characters, (ECHO)
        // don't echo newline character (ECHONL)
        ts.c_lflag &= ~(ICANON | ECHO | ECHONL);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &ts);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    }

    ~Terminal()
    {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &ots);
    }

    std::pair<bool, char> get_input()
    {
        char c;
        auto r = ::read(STDIN_FILENO, &c, 1);
        return std::make_pair(r == 1, c);
    }
};

std::pair<int, int> get_terminal_size()
{
    struct winsize w;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &w);
    return std::make_pair(w.ws_col, w.ws_row);
}

#elif defined(PLATFORM_WINDOWS)

#include <windows.h>

// https://docs.microsoft.com/en-us/windows/console/setconsolemode
// These values are effective on Windows 10 build 16257 (August 2017) or later
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
    #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef DISABLE_NEWLINE_AUTO_RETURN
    #define DISABLE_NEWLINE_AUTO_RETURN 0x0008
#endif

struct Terminal {
    HANDLE stdin_handle, stdout_handle;
    DWORD in_mode, out_mode;

    Terminal()
    {
        stdin_handle  = GetStdHandle(STD_INPUT_HANDLE);
        stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        if (stdin_handle == INVALID_HANDLE_VALUE || stdout_handle == INVALID_HANDLE_VALUE) {
            fprintf(stderr, "Failed to get console handles\n");
            return;
        }
        if (!GetConsoleMode(stdin_handle, &in_mode)) {
            fprintf(stderr, "Failed to get input mode\n");
            return;
        }
        if (!GetConsoleMode(stdout_handle, &out_mode)) {
            fprintf(stderr, "Failed to get output mode\n");
            return;
        }
        if (!SetConsoleMode(stdin_handle, ENABLE_WINDOW_INPUT)) {
            fprintf(stderr, "Failed to set new input mode\n");
        }
        if (!SetConsoleMode(stdout_handle, out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN)) {
            if (!SetConsoleMode(stdout_handle, out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
                fprintf(stderr, "Failed to enable virtual terminal mode\n");
            }
        }
    }

    ~Terminal()
    {
        if (stdin_handle  != INVALID_HANDLE_VALUE) { SetConsoleMode(stdin_handle, in_mode); }
        if (stdout_handle != INVALID_HANDLE_VALUE) { SetConsoleMode(stdout_handle, out_mode); }
    }

    std::pair<bool, char> get_input()
    {
        DWORD n = 0;
        if (!GetNumberOfConsoleInputEvents(stdin_handle, &n) || n == 0)
            return {false, '\0'};
        INPUT_RECORD inputbuf;
        if (!ReadConsoleInput(stdin_handle, &inputbuf, 1, &n))
            return {false, '\0'};
        if (n == 0 || inputbuf.EventType != KEY_EVENT)
            return {false, '\0'};
        KEY_EVENT_RECORD *key = &inputbuf.Event.KeyEvent;
        if (key->bKeyDown)
            return std::make_pair(false, '\0');
        return std::make_pair(true, key->uChar.AsciiChar);
    }
};

std::pair<int, int> get_terminal_size()
{
    auto is_native_out_vt(HANDLE handle) -> bool {
        DWORD cmode;
        return GetConsoleMode(handle, &cmode) &&
               (cmode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) &&
               !(cmode & DISABLE_NEWLINE_AUTO_RETURN);
    };
    CONSOLE_SCREEN_BUFFER_INFO cinfo;
    HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleScreenBufferInfo(stdout_handle, &cinfo)) {
        return std::make_pair(
            cinfo.dwMaximumWindowSize.X - (is_native_out_vt(stdout_handle) ? 0 : 1),
            cinfo.dwMaximumWindowSize.Y
        );
    }
    // assume these dimension (based on mpv)
    return {80, 24};
}

#endif
