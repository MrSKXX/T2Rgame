/**
 * player.h
 * Interface du joueur pour Ticket to Ride
 */
#ifndef PLAYER_H
#define PLAYER_H

#include "gamestate.h"
#include "../tickettorideapi/ticketToRide.h"
#include "strategy/strategy.h"

// Fonctions principales
void initPlayer(GameState* state, StrategyType strategy, GameData* gameData);
ResultCode playTurn(GameState* state, StrategyType strategy);
ResultCode playFirstTurn(GameState* state);

// Utilitaires
void cleanupMoveResult(MoveResult *moveResult);

#endif