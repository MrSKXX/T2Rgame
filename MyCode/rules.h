#ifndef RULES_H
#define RULES_H
#include "gamestate.h"
#include "../tickettorideapi/ticketToRide.h"

int canClaimRoute(GameState* state, int from, int to, CardColor color, int* nbLocomotives);
int findPossibleRoutes(GameState* state, int* possibleRoutes, CardColor* possibleColors, int* possibleLocomotives);
int canDrawVisibleCard(CardColor color);
int hasEnoughWagons(GameState* state, int length);
int isValidMove(GameState* state, MoveData* move);
int isLastTurn(GameState* state);
int routeOwner(GameState* state, int from, int to);
int findRouteIndex(GameState* state, int from, int to);
int isObjectiveCompleted(GameState* state, Objective objective);
int completeObjectivesCount(GameState* state);
int calculateScore(GameState* state);

#endif