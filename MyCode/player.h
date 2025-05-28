/**
 * player.h
 * Interface du joueur pour Ticket to Ride
 */
#ifndef PLAYER_H
#define PLAYER_H

#include "gamestate.h"
#include "../tickettorideapi/ticketToRide.h"
#include "strategy/strategy.h"

/**
 * Initialise le joueur avec un état de jeu et une stratégie
 * @param state État du jeu à initialiser
 * @param strategy Stratégie à utiliser
 * @param gameData Données de jeu initiales
 */
void initPlayer(GameState* state, StrategyType strategy, GameData* gameData);

/**
 * Joue un tour complet
 * @param state État actuel du jeu
 * @param strategy Stratégie à utiliser
 * @return ResultCode indiquant le résultat de l'action
 */
ResultCode playTurn(GameState* state, StrategyType strategy);

/**
 * Gère le premier tour (choix des objectifs)
 * @param state État actuel du jeu
 * @return ResultCode indiquant le résultat de l'action
 */
ResultCode playFirstTurn(GameState* state);

/**
 * Libère la mémoire allouée dans une MoveResult
 * @param moveResult Structure à nettoyer
 */
void cleanupMoveResult(MoveResult *moveResult);

#endif // PLAYER_H