/**
 * strategy_simple.h
 * Interface pour la stratégie simplifiée
 */
#ifndef STRATEGY_H
#define STRATEGY_H

#include "gamestate.h"
#include "../tickettorideapi/ticketToRide.h"

int simpleStrategy(GameState* state, MoveData* moveData);
int decideNextMove(GameState* state, MoveData* moveData);

int findBestObjective(GameState* state);
int canTakeRoute(GameState* state, int from, int to, MoveData* moveData);
int drawCardsOrTakeRoute(GameState* state, MoveData* moveData);
int drawBestCard(GameState* state, MoveData* moveData);

void simpleChooseObjectives(GameState* state, Objective* objectives, unsigned char* chooseObjectives);
void chooseObjectivesStrategy(GameState* state, Objective* objectives, unsigned char* chooseObjectives);

int emergencyUnblock(GameState* state, MoveData* moveData);
int alternativeStrategy(GameState* state, MoveData* moveData, void* objData, int objectiveCount);
int findAlternativePath(GameState* state, int from, int to, MoveData* moveData);
int drawCardsForRouteAggressively(GameState* state, int from, int to, MoveData* moveData);

#endif