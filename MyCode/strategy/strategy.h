/**
 * strategy.h
 * Interface unifiée pour tous les modules de stratégie
 * 
 * Ce fichier centralise toutes les définitions, types et fonctions
 * des modules de stratégie pour simplifier les includes.
 */

#ifndef STRATEGY_H
#define STRATEGY_H

#include <stdbool.h>
#include "../gamestate.h"
#include "../../tickettorideapi/ticketToRide.h"

// ============================================================================
// TYPES ET ÉNUMÉRATIONS COMMUNES
// ============================================================================

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

// Priorités stratégiques
typedef enum { 
    COMPLETE_OBJECTIVES, 
    BLOCK_OPPONENT, 
    BUILD_NETWORK, 
    DRAW_CARDS 
} StrategicPriority;

// Structure pour les routes critiques
typedef struct {
    int from;
    int to;
    int objectiveIndex;
    int priority;
    CardColor color;
    int nbLocomotives;
    bool hasEnoughCards;
} CriticalRoute;

// Carte des régions stratégiques
typedef struct {
    int cities[12];
    int cityCount;
    int strategicValue;
} MapRegion;

// Paramètres de la personnalité du bot
typedef struct {
    float aggressiveness;    // 0.0-1.0: tendance à bloquer
    float objectiveFocus;    // 0.0-1.0: priorité aux objectifs
    float riskTolerance;     // 0.0-1.0: tolérance au risque
    float opportunism;       // 0.0-1.0: changement de plan si opportunité
    float territorialControl; // 0.0-1.0: contrôle territorial
} BotPersonality;

// Cache pour l'algorithme de pathfinding
#define PATH_CACHE_SIZE 50

typedef struct {
    int from;
    int to;
    int pathLength;
    int path[MAX_CITIES];
    int distance;
    int timestamp; // Pour invalidation du cache
} PathCacheEntry;

// Constantes pour l'évaluation stratégique
#define STRATEGIC_OBJECTIVE_MULTIPLIER 15
#define CRITICAL_PATH_MULTIPLIER 30
#define BLOCKING_VALUE_MULTIPLIER 12
#define CARD_EFFICIENCY_MULTIPLIER 8
#define MAX_CENTRAL_CITIES 10
#define MAX_HUB_CONNECTIONS 6

// ============================================================================
// VARIABLES GLOBALES EXTERNES
// ============================================================================

// Variables globales du cache (définies dans pathfinding.c)
extern PathCacheEntry pathCache[PATH_CACHE_SIZE];
extern int cacheEntries;
extern int cacheTimestamp;

// Variables globales pour le modèle de l'adversaire (définies dans opponent_modeling.c)
extern int opponentCitiesOfInterest[MAX_CITIES];
extern OpponentProfile currentOpponentProfile;

// ============================================================================
// FONCTIONS PRINCIPALES - STRATEGY_CORE
// ============================================================================

// Interface principale - Point d'entrée unique
int decideNextMove(GameState* state, StrategyType strategy, MoveData* moveData);

// Stratégie principale
int superAdvancedStrategy(GameState* state, MoveData* moveData);

// Interfaces pour les anciennes stratégies (redirigent vers superAdvancedStrategy)
int basicStrategy(GameState* state, MoveData* moveData);
int dijkstraStrategy(GameState* state, MoveData* moveData);
int advancedStrategy(GameState* state, MoveData* moveData);

// ============================================================================
// FONCTIONS D'ANALYSE DE JEU - GAME_ANALYSIS
// ============================================================================

// Analyse de phase de jeu
int determineGamePhase(GameState* state);

// Détermination des priorités stratégiques
StrategicPriority determinePriority(GameState* state, int phase, CriticalRoute* criticalRoutes, 
                                   int criticalRouteCount, int consecutiveDraws);

// Évaluation de routes
int evaluateRouteUtility(GameState* state, int routeIndex);
int enhancedEvaluateRouteUtility(GameState* state, int routeIndex);
int evaluateCardEfficiency(GameState* state, int routeIndex);

// Analyse de fin de partie
int evaluateEndgameScore(GameState* state, int routeIndex);

// Analyse de réseau
void identifyAndPrioritizeBottlenecks(GameState* state, int* prioritizedRoutes, int* count);

// Planification
void planNextRoutes(GameState* state, int* routesPlan, int count);

// ============================================================================
// FONCTIONS D'ANALYSE D'OBJECTIFS - OBJECTIVE_ANALYSIS
// ============================================================================

// Analyse des objectifs
int calculateObjectiveProgress(GameState* state, int routeIndex);
int countRemainingRoutesForObjective(GameState* state, int objectiveIndex);
void identifyCriticalRoutes(GameState* state, CriticalRoute* criticalRoutes, int* count);
void checkObjectivesPaths(GameState* state);

// Choix d'objectifs
void chooseObjectivesStrategy(GameState* state, Objective* objectives, bool* chooseObjectives);
void improvedObjectiveEvaluation(GameState* state, Objective* objectives, bool* chooseObjectives);

// Fonctions de fin de partie
bool forceCompleteCriticalObjective(GameState* state, MoveData* moveData);
int findBestRemainingObjective(GameState* state);

// Utilitaires pour cartes et routes
bool haveEnoughCards(GameState* state, int from, int to, CardColor* color, int* nbLocomotives);

// ============================================================================
// FONCTIONS DE PATHFINDING - PATHFINDING
// ============================================================================

// Fonctions de pathfinding principales
int findShortestPath(GameState* state, int start, int end, int* path, int* pathLength);
int findSmartestPath(GameState* state, int start, int end, int* path, int* pathLength);

// Utilitaires de chemin
int isRouteInPath(int from, int to, int* path, int pathLength);

// Gestion du cache
void invalidatePathCache(void);
void updateCacheTimestamp(void);

// ============================================================================
// FONCTIONS DE GESTION DES CARTES - CARD_MANAGEMENT
// ============================================================================

// Gestion des cartes
int strategicCardDrawing(GameState* state);
CardColor determineOptimalColor(GameState* state, int routeIndex);

// Analyse des besoins en cartes
void analyzeCardNeeds(GameState* state, int colorNeeds[10]);
int evaluateVisibleCard(GameState* state, CardColor card, int colorNeeds[10]);

// Efficacité des cartes
int calculateCardEfficiency(GameState* state, CardColor color, int routeLength);

// ============================================================================
// FONCTIONS DE MODÉLISATION ADVERSE - OPPONENT_MODELING
// ============================================================================

// Modélisation de l'adversaire
void updateOpponentObjectiveModel(GameState* state, int from, int to);
void updateOpponentProfile(GameState* state);
OpponentProfile identifyOpponentProfile(GameState* state);

// Analyse des routes à bloquer
int findCriticalRoutesToBlock(GameState* state, int* routesToBlock, int* blockingPriorities);

// Estimation de score
int estimateOpponentScore(GameState* state);

// Analyse des patterns adverses
void analyzeOpponentPatterns(GameState* state);
void detectOpponentStrategy(GameState* state);

// ============================================================================
// FONCTIONS D'EXÉCUTION - EXECUTION
// ============================================================================

// Fonction principale d'exécution
int executePriority(GameState* state, MoveData* moveData, StrategicPriority priority, 
                   CriticalRoute* criticalRoutes, int criticalRouteCount, int* consecutiveDraws);

// Exécution par type de priorité
int executeCompleteObjectives(GameState* state, MoveData* moveData, 
                             CriticalRoute* criticalRoutes, int criticalRouteCount, int* consecutiveDraws);

int executeBlockOpponent(GameState* state, MoveData* moveData, int* consecutiveDraws);

int executeBuildNetwork(GameState* state, MoveData* moveData, int* consecutiveDraws);

int executeDrawCards(GameState* state, MoveData* moveData, int* consecutiveDraws);

// Utilitaires d'exécution
bool validateRouteMove(GameState* state, MoveData* moveData);
void correctInvalidMove(GameState* state, MoveData* moveData);

// Sécurité et validation
bool isValidRouteAction(GameState* state, int from, int to, CardColor color);
void logDecision(const char* decision, int from, int to, int score);

#endif // STRATEGY_H