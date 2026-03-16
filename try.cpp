#include "toguznative.h"
#include <iostream>

#define GAME_CLASS ToguzNative

int main(int argc, char const *argv[])
{
    GAME_CLASS game;

    init_masks();

    game.move(12);

    std::cout << game << std::endl;

    return 0;
}
