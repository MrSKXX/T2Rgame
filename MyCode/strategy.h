// strategy.h
#ifndef STRATEGY_H
#define STRATEGY_H

#include <stdbool.h>
#include "gamestate.h"
#include "../tickettorideapi/ticketToRide.h"

// Énumération des différentes stratégies
typedef enum {
    STRATEGY_BASIC,
    STRATEGY_DIJKSTRA,
    STRATEGY_ADVANCED
} StrategyType;

// Phases de jeu
#define PHASE_EARLY 0
#define PHASE_MIDDLE 1
#define PHASE_LATE 2
#define PHASE_FINAL 3

// Profils d'adversaire
typedef enum {
    OPPONENT_AGGRESSIVE,
    OPPONENT_HOARDER,
    OPPONENT_OBJECTIVE,
    OPPONENT_BLOCKER,
    OPPONENT_UNKNOWN
} OpponentProfile;

// Interface principale
int decideNextMove(GameState* state, StrategyType strategy, MoveData* moveData);
int basicStrategy(GameState* state, MoveData* moveData);
int dijkstraStrategy(GameState* state, MoveData* moveData);
int advancedStrategy(GameState* state, MoveData* moveData);
void chooseObjectivesStrategy(GameState* state, Objective* objectives, bool* chooseObjectives);

// Analyse stratégique et décisions
int determineGamePhase(GameState* state);
int enhancedEvaluateRouteUtility(GameState* state, int routeIndex);
int calculateObjectiveProgress(GameState* state, int routeIndex);
int strategicCardDrawing(GameState* state);
void updateOpponentObjectiveModel(GameState* state, int from, int to);
int superAdvancedStrategy(GameState* state, MoveData* moveData);
int findCriticalRoutesToBlock(GameState* state, int* routesToBlock, int* blockingPriorities);
int countRemainingRoutesForObjective(GameState* state, int objectiveIndex);

// Algorithmes de pathfinding et d'analyse
int findShortestPath(GameState* state, int start, int end, int* path, int* pathLength);
int isRouteInPath(int from, int to, int* path, int pathLength);
int evaluateRouteUtility(GameState* state, int routeIndex);
void checkObjectivesPaths(GameState* state);
OpponentProfile identifyOpponentProfile(GameState* state);
int evaluateCardEfficiency(GameState* state, int routeIndex);

// Nouvelles fonctions optimisées
bool forceCompleteCriticalObjective(GameState* state, MoveData* moveData);
int findBestRemainingObjective(GameState* state);
CardColor determineOptimalColor(GameState* state, int routeIndex);
void identifyAndPrioritizeBottlenecks(GameState* state, int* prioritizedRoutes, int* count);
int evaluateEndgameScore(GameState* state, int routeIndex);
int estimateOpponentScore(GameState* state);
void planNextRoutes(GameState* state, int* routesPlan, int count);
/**
 * Structure pour les routes critiques pour compléter des objectifs
 */
typedef struct {
    int from;
    int to;
    int objectiveIndex;
    int priority;
    CardColor color;
    int nbLocomotives;
    bool hasEnoughCards;  // Indique si nous avons assez de cartes pour prendre cette route
} CriticalRoute;

// Déclaration des fonctions associées
void identifyCriticalRoutes(GameState* state, CriticalRoute* criticalRoutes, int* count);
bool haveEnoughCards(GameState* state, int from, int to, CardColor* color, int* nbLocomotives);
#endif // STRATEGY_H