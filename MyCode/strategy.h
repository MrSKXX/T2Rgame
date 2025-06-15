#ifndef STRATEGY_H
#define STRATEGY_H
#include "gamestate.h"
#include "../tickettorideapi/ticketToRide.h"

int simpleStrategy(GameState* state, MoveData* moveData);
int decideNextMove(GameState* state, MoveData* moveData);
int findBestObjective(GameState* state);
int canTakeRoute(GameState* state, int from, int to, MoveData* moveData);
int drawCardsForRoute(GameState* state, int from, int to, MoveData* moveData);
int drawBestCard(GameState* state, MoveData* moveData);
void simpleChooseObjectives(GameState* state, Objective* objectives, unsigned char* chooseObjectives);
void chooseObjectivesStrategy(GameState* state, Objective* objectives, unsigned char* chooseObjectives);
int emergencyUnblock(GameState* state, MoveData* moveData);
int alternativeStrategy(GameState* state, MoveData* moveData, void* objData, int objectiveCount);
int findAlternativePath(GameState* state, int from, int to, MoveData* moveData);
int drawCardsForRouteAggressively(GameState* state, int from, int to, MoveData* moveData);
int isAntiAdversaireMode(GameState* state);
int handleAntiAdversaire(GameState* state, MoveData* moveData);
int findQuickestObjective(GameState* state);
int buildFromExistingNetwork(GameState* state, MoveData* moveData);
int takeAnyProfitableRoute(GameState* state, MoveData* moveData);
int workOnSpecificObjective(GameState* state, MoveData* moveData, int objectiveIndex);

#endif