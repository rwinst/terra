#include <cstdio>
#include "Game.h"

int main(int /*argc*/, char* /*argv*/[]) {
    Game game;
    if (!game.init()) {
        fprintf(stderr, "Failed to initialise SDL2 window/renderer.\n"
                        "Make sure SDL2 is installed and a display is available.\n");
        return 1;
    }
    game.run();
    return 0;
}
