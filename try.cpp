#include "toguznative.h"
#include <iostream>

#define GAME_CLASS ToguzNative

int main(int argc, char const *argv[])
{
    GAME_CLASS game;

    init_masks();

    atsyrau(game, 0); // first player

    std::cout << game << std::endl;

    return 0;
}
