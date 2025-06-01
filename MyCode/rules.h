/**
 * rules.h
 * Règles du jeu et validation des coups
 */
#ifndef RULES_H
#define RULES_H

#include "gamestate.h"
#include "../tickettorideapi/ticketToRide.h"

// Validation des coups
int canClaimRoute(GameState* state, int from, int to, CardColor color, int* nbLocomotives);
int findPossibleRoutes(GameState* state, int* possibleRoutes, CardColor* possibleColors, int* possibleLocomotives);
int canDrawVisibleCard(CardColor color);
int hasEnoughWagons(GameState* state, int length);
int isValidMove(GameState* state, MoveData* move);

// État du jeu
int isLastTurn(GameState* state);
int routeOwner(GameState* state, int from, int to);
int findRouteIndex(GameState* state, int from, int to);

// Objectifs
int isObjectiveCompleted(GameState* state, Objective objective);
int completeObjectivesCount(GameState* state);

// Score
int calculateScore(GameState* state);

#endif