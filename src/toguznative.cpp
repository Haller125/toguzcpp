#include "toguznative.h"

ToguzNative::ToguzNative(){
        this->cells.fill(9);
        this->tuzdeks.fill(NO_TUZDEK);
        this->scores.fill(0);
}

void ToguzNative::move(u_int8_t idx){
    int pits = this->cells[idx];
    this->cells[idx] = 0;

    for (u_int8_t i = 0; i < pits; i++) {
        u_int8_t id = (idx + i) % 18;
        this->cells[ id ] += 1; 
    }

    u_int8_t id = (idx + pits - 1) % 18;
    bool player = idx / 9;
    bool last_ball_side = id / 9;
    // from -1 0 1 to 0 1
    // bool is_on_opposite = (last_ball_side - player) * (last_ball_side - player);

    if ( player != last_ball_side && (this->cells[id] % 2 == 0)) {
        this->scores[player] += this->cells[id];
    }

    if (
        player != last_ball_side &&
        this->cells[id] == 3 &&
        id != this->tuzdeks[1 - player] &&
        this->tuzdeks[player] == NO_TUZDEK
    ){
        this->scores[player] += this->cells[id];
        this->tuzdeks[player] = id;
    }

    if (this->tuzdeks[0] != NO_TUZDEK){
        this->scores[0] += this->cells[this->tuzdeks[0]];
        this->cells[this->tuzdeks[0]] = 0;
    }

    if (this->tuzdeks[1] != NO_TUZDEK){
        this->scores[1] += this->cells[this->tuzdeks[1]];
        this->cells[this->tuzdeks[1]] = 0;
    }

    
}

void ToguzNative::get_legal_moves(bool player, std::array<u_int8_t, 9>& legal_moves, int& count) const {
    count = 0;
    for (u_int8_t i = 0; i < 9; i++) {
        if (this->cells[i + player * 9] > 0) {
            legal_moves[count] = i;
            count++;
        }
    }
}   

// first player - 1, second player - -1, 0 - draw
void ToguzNative::who_is_winner(int8_t winner) const {
    // A player wins by collecting 82 or more stones in their score.
    if (this->scores[0] >= 82) {
        winner = 1;
    } else if (this->scores[1] >= 82) {
        winner = -1;
    } else if (this->scores[0] + this->scores[1] == 162 && this->scores[0] == this->scores[1]) {
        winner = 0;
    } 
}
