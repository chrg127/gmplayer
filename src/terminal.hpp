#include <cstdio>
#include <cstdlib>
#include <utility>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>

struct Terminal {
    // bool redraw = false;
    termios ots = {};

    Terminal()
    {
        // setup handlers
        // struct sigaction sa = {};
        // sa.sa_handler = [](int signum) {
        //     printf("bye\n");
        //     terminal->restore();
        //     std::exit(1);
        // };
        // sigaction(SIGTERM, &sa, nullptr);
        // sigaction(SIGINT , &sa, nullptr);
        // sigaction(SIGSEGV, &sa, nullptr);

        // sa.sa_handler = [](int signum) {
        //     terminal->restore();
        // };
        // sigaction(SIGSTOP, &sa, nullptr);
        // sigaction(SIGABRT, &sa, nullptr);

        // sa.sa_handler = [](int signum) {
        //     terminal->setup();
        //     terminal->set_redraw(true);
        // };
        // sigaction(SIGCONT, &sa, nullptr);

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

    // bool get_redraw() const { return redraw; }
    // void set_redraw(bool value) { redraw = value; }
};

std::pair<int, int> get_terminal_size(int tty = STDIN_FILENO)
{
    struct winsize w;
    ioctl(tty, TIOCGWINSZ, &w);
    return std::make_pair(w.ws_col, w.ws_row);
}
