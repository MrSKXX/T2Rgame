/**
 * strategy.h
 * Interface du système de stratégie
 */
#ifndef STRATEGY_H
#define STRATEGY_H

#include <stdbool.h>
#include "../gamestate.h"
#include "../../tickettorideapi/ticketToRide.h"

// Types essentiels
typedef enum {
    OPPONENT_AGGRESSIVE,
    OPPONENT_HOARDER,
    OPPONENT_OBJECTIVE,
    OPPONENT_BLOCKER,
    OPPONENT_UNKNOWN
} OpponentProfile;

typedef enum { 
    COMPLETE_OBJECTIVES, 
    BLOCK_OPPONENT, 
    BUILD_NETWORK, 
    DRAW_CARDS 
} StrategicPriority;

typedef struct {
    int from;
    int to;
    int objectiveIndex;
    int priority;
    CardColor color;
    int nbLocomotives;
    bool hasEnoughCards;
} CriticalRoute;

typedef struct {
    int from;
    int to;
    int pathLength;
    int path[MAX_CITIES];
    int distance;
    int timestamp;
} PathCacheEntry;

// Constantes
#define PHASE_EARLY 0
#define PHASE_MIDDLE 1
#define PHASE_LATE 2
#define PHASE_FINAL 3
#define PATH_CACHE_SIZE 50

// Variables globales
extern PathCacheEntry pathCache[PATH_CACHE_SIZE];
extern int cacheEntries;
extern int cacheTimestamp;
extern int opponentCitiesOfInterest[MAX_CITIES];
extern OpponentProfile currentOpponentProfile;

// Interface principale
int decideNextMove(GameState* state, MoveData* moveData);
int superAdvancedStrategy(GameState* state, MoveData* moveData);

// Analyse de jeu
int determineGamePhase(GameState* state);
StrategicPriority determinePriority(GameState* state, int phase, CriticalRoute* criticalRoutes, 
                                   int criticalRouteCount, int consecutiveDraws);
int evaluateRouteUtility(GameState* state, int routeIndex);

// Analyse d'objectifs
int calculateObjectiveProgress(GameState* state, int routeIndex);
int countRemainingRoutesForObjective(GameState* state, int objectiveIndex);
void identifyCriticalRoutes(GameState* state, CriticalRoute* criticalRoutes, int* count);
void checkObjectivesPaths(GameState* state);
void chooseObjectivesStrategy(GameState* state, Objective* objectives, bool* chooseObjectives);
bool forceCompleteCriticalObjective(GameState* state, MoveData* moveData);
bool haveEnoughCards(GameState* state, int from, int to, CardColor* color, int* nbLocomotives);

// Pathfinding
int findShortestPath(GameState* state, int start, int end, int* path, int* pathLength);
int findSmartestPath(GameState* state, int start, int end, int* path, int* pathLength);
int isRouteInPath(int from, int to, int* path, int pathLength);
void invalidatePathCache(void);

// Gestion des cartes
int strategicCardDrawing(GameState* state);
CardColor determineOptimalColor(GameState* state, int routeIndex);

// Modélisation adversaire
void updateOpponentObjectiveModel(GameState* state, int from, int to);
void updateOpponentProfile(GameState* state);
OpponentProfile identifyOpponentProfile(GameState* state);
int findCriticalRoutesToBlock(GameState* state, int* routesToBlock, int* blockingPriorities);

// Exécution
int executePriority(GameState* state, MoveData* moveData, StrategicPriority priority, 
                   CriticalRoute* criticalRoutes, int criticalRouteCount, int* consecutiveDraws);
int executeCompleteObjectives(GameState* state, MoveData* moveData, 
                             CriticalRoute* criticalRoutes, int criticalRouteCount, int* consecutiveDraws);
int executeBlockOpponent(GameState* state, MoveData* moveData, int* consecutiveDraws);
int executeBuildNetwork(GameState* state, MoveData* moveData, int* consecutiveDraws);
int executeDrawCards(GameState* state, MoveData* moveData, int* consecutiveDraws);
bool validateRouteMove(GameState* state, MoveData* moveData);
void correctInvalidMove(GameState* state, MoveData* moveData);

#endif