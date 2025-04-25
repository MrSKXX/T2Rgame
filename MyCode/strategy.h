/**
 * strategy.h
 * Implémentation des stratégies pour Ticket to Ride
 */
#ifndef STRATEGY_H
#define STRATEGY_H

#include <stdbool.h>
#include "gamestate.h"
#include "../tickettorideapi/ticketToRide.h"

// Énumération des différentes stratégies
typedef enum {
    STRATEGY_BASIC,      // Stratégie de base
    STRATEGY_DIJKSTRA,   // Stratégie utilisant Dijkstra pour optimiser les chemins
    STRATEGY_ADVANCED    // Stratégie avancée (combinaison de plusieurs approches)
} StrategyType;

// Game phase constants - ensure these match the values in strategy.c
#define PHASE_EARLY 0    // Focus on drawing cards and initial positioning
#define PHASE_MIDDLE 1   // Focus on completing objectives and blocking opponent
#define PHASE_LATE 2     // Focus on completing remaining objectives and claiming valuable routes
#define PHASE_FINAL 3    // Last few turns, maximize points

/**
 * Décide quelle action effectuer en fonction de l'état du jeu
 * @param state État actuel du jeu
 * @param strategy Stratégie à utiliser
 * @param moveData Structure à remplir avec l'action décidée
 * @return 1 si une action a été décidée, 0 sinon
 */
int decideNextMove(GameState* state, StrategyType strategy, MoveData* moveData);

/**
 * Stratégie de base: priorise les objectifs et essaie de compléter les routes nécessaires
 * @param state État actuel du jeu
 * @param moveData Structure à remplir avec l'action décidée
 * @return 1 si une action a été décidée, 0 sinon
 */
int basicStrategy(GameState* state, MoveData* moveData);

/**
 * Stratégie Dijkstra: utilise l'algorithme de Dijkstra pour trouver le chemin le plus court
 * @param state État actuel du jeu
 * @param moveData Structure à remplir avec l'action décidée
 * @return 1 si une action a été décidée, 0 sinon
 */
int dijkstraStrategy(GameState* state, MoveData* moveData);

/**
 * Stratégie avancée: combine plusieurs approches
 * @param state État actuel du jeu
 * @param moveData Structure à remplir avec l'action décidée
 * @return 1 si une action a été décidée, 0 sinon
 */
int advancedStrategy(GameState* state, MoveData* moveData);

/**
 * Décide quels objectifs garder parmi les 3 proposés
 * @param state État actuel du jeu
 * @param objectives Tableau des 3 objectifs proposés
 * @param chooseObjectives Tableau à remplir avec les choix (true/false)
 */
void chooseObjectivesStrategy(GameState* state, Objective* objectives, bool* chooseObjectives);

/**
 * Décide quelle carte visible piocher
 * @param state État actuel du jeu
 * @param visibleCards Cartes visibles sur le plateau
 * @return Index de la carte à piocher (0-4) ou -1 pour piocher une carte aveugle
 */
int chooseCardToDraw(GameState* state, CardColor* visibleCards);

/**
 * Vérifie si une route fait partie d'un chemin
 * @param from Ville de départ de la route
 * @param to Ville d'arrivée de la route
 * @param path Tableau des indices des villes formant le chemin
 * @param pathLength Longueur du chemin
 * @return 1 si la route fait partie du chemin, 0 sinon
 */
int isRouteInPath(int from, int to, int* path, int pathLength);

/**
 * Évalue l'utilité d'une route par rapport aux objectifs
 * @param state État actuel du jeu
 * @param routeIndex Index de la route à évaluer
 * @return Score d'utilité de la route
 */
int evaluateRouteUtility(GameState* state, int routeIndex);

/**
 * Trie les routes possibles par ordre d'utilité décroissante
 * @param state État actuel du jeu
 * @param possibleRoutes Tableau des indices des routes possibles
 * @param possibleColors Tableau des couleurs possibles pour chaque route
 * @param possibleLocomotives Tableau du nombre de locomotives pour chaque route
 * @param numPossibleRoutes Nombre de routes possibles
 */
void sortRoutesByUtility(GameState* state, int* possibleRoutes, CardColor* possibleColors, 
                         int* possibleLocomotives, int numPossibleRoutes);

/**
 * Implémentation de l'algorithme de Dijkstra
 * Trouve le chemin le plus court entre deux villes
 * @param state État actuel du jeu
 * @param start Ville de départ
 * @param end Ville d'arrivée
 * @param path Tableau à remplir avec les indices des villes formant le chemin
 * @param pathLength Pointeur à remplir avec la longueur du chemin
 * @return Distance du chemin ou -1 si aucun chemin n'est trouvé
 */
int findShortestPath(GameState* state, int start, int end, int* path, int* pathLength);

/**
 * Enhanced evaluation of route utility that considers multiple factors
 */
int enhancedEvaluateRouteUtility(GameState* state, int routeIndex);

/**
 * Calculate how much a route helps with objective completion
 */
int calculateObjectiveProgress(GameState* state, int routeIndex);

/**
 * Strategic card drawing based on needs
 */
int strategicCardDrawing(GameState* state);

/**
 * Update opponent model based on their moves
 */
void updateOpponentObjectiveModel(GameState* state, int from, int to);

/**
 * Enhanced AI strategy that uses all advanced features
 */
int superAdvancedStrategy(GameState* state, MoveData* moveData);

/**
 * Enhanced update after opponent move that also updates our model of opponent behavior
 */
void enhancedUpdateAfterOpponentMove(GameState* state, MoveData* moveData);

#endif // STRATEGY_H