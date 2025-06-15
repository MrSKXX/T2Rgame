#ifndef PLAYER_H
#define PLAYER_H
#include "gamestate.h"
#include "../tickettorideapi/ticketToRide.h"

void initPlayer(GameState* state, GameData* gameData);
ResultCode playTurn(GameState* state);
ResultCode playFirstTurn(GameState* state);
void cleanupMoveResult(MoveResult *moveResult);

#endif