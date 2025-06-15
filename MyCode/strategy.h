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
void chooseObjectives(GameState* state, Objective* objectives, unsigned char* chooseObjectives);
int emergencyUnblock(GameState* state, MoveData* moveData);
int findAlternativePath(GameState* state, int from, int to, MoveData* moveData);
int drawCardsAggressively(GameState* state, int from, int to, MoveData* moveData);
int isAntiMode(GameState* state);
int handleAntiAdversaire(GameState* state, MoveData* moveData);
int findQuickestObjective(GameState* state);
int buildFromNetwork(GameState* state, MoveData* moveData);
int takeAnyRoute(GameState* state, MoveData* moveData);
int workOnObjective(GameState* state, MoveData* moveData, int objectiveIndex);

#endif