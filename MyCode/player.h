/**
 * player.h
 * Interface du joueur
 */
#ifndef PLAYER_H
#define PLAYER_H

#include "gamestate.h"
#include "../tickettorideapi/ticketToRide.h"

// Fonctions principales
void initPlayer(GameState* state, GameData* gameData);
ResultCode playTurn(GameState* state);
ResultCode playFirstTurn(GameState* state);

// Utilitaires
void cleanupMoveResult(MoveResult *moveResult);

#endif