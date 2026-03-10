#pragma once

#include <array>
#include <cstddef>
#include <iostream>
#include <sys/types.h>

#define NO_TUZDEK 18

struct Toguz{
    std::array<u_int8_t, 18> cells; 
    std::array<u_int8_t, 2> tuzdeks;
    std::array<u_int8_t, 2> scores;

    virtual ~Toguz() = default;
    virtual void move(u_int8_t idx) = 0;
};

struct ToguzNative : public Toguz{

    ToguzNative();
    void move(u_int8_t idx) override;
    void get_legal_moves(bool player, std::array<u_int8_t, 9>& legal_moves, int& count) const;

    private:
    void who_is_winner(int8_t winner) const;
};