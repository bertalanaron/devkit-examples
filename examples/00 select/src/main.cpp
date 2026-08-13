#include <devkit/common/utils.h>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <filesystem>
#include <iostream>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

namespace fs = std::filesystem;
using namespace ftxui;

fs::path getExecutableDirectory() {
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return fs::path(path).parent_path();
#else
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    return fs::path(std::string(result, count)).parent_path();
#endif
}

void clearConsole() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

int main() {
    std::vector<std::string> options = {
        "01 Asteroid Belt",
        "02 RTS Game"
    };
    int selected = 0;

    auto screen = ScreenInteractive::TerminalOutput();
    auto menu = Menu(&options, &selected);

    bool closeRequested = false;

    auto renderer = Renderer(menu, [&] {
        return vbox({
            text("Select example:"),
            separator(),
            menu->Render(),
            separator(),
            text("Press ENTER to run.")
            });
        });

    auto app = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Return) {
            fs::path dir = getExecutableDirectory();
            std::string path;

            if (selected == 0)
                path = (dir / "01_basic.exe").string();
            else
                path = (dir / "02_rts.exe").string();

            screen.Exit(); // Stop FTXUI before launching

            // Run in same console window
            clearConsole();
            system(("\"" + path + "\"").c_str());
            return true;
        }
        else if (event == Event::Escape) {
            screen.Exit(); // Stop FTXUI before launching
            closeRequested = true;
        }
        return false;
    });

    while (!closeRequested)
    {
        screen.Loop(app);
        clearConsole();
    }

    return 0;
}
