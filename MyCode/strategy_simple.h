/**
 * strategy_simple.h
 * Interface pour la stratégie simplifiée
 */
#ifndef STRATEGY_SIMPLE_H
#define STRATEGY_SIMPLE_H

#include "gamestate.h"
#include "../tickettorideapi/ticketToRide.h"

// Fonctions principales
int simpleStrategy(GameState* state, MoveData* moveData);
int decideNextMove(GameState* state, MoveData* moveData);

// Fonctions de support
int findBestObjective(GameState* state);
int canTakeRoute(GameState* state, int from, int to, MoveData* moveData);
int drawCardsOrTakeRoute(GameState* state, MoveData* moveData);
int drawBestCard(GameState* state, MoveData* moveData);

// Choix d'objectifs
void simpleChooseObjectives(GameState* state, Objective* objectives, unsigned char* chooseObjectives);
void chooseObjectivesStrategy(GameState* state, Objective* objectives, unsigned char* chooseObjectives);

#endif