/**
 * strategy.h
 * Implémentation des stratégies pour Ticket to Ride
 */
#ifndef STRATEGY_H
#define STRATEGY_H

#include "gamestate.h"
#include "../tickettorideapi/ticketToRide.h"

// Énumération des différentes stratégies
typedef enum {
    STRATEGY_BASIC,      // Stratégie de base
    STRATEGY_DIJKSTRA,   // Stratégie utilisant Dijkstra pour optimiser les chemins
    STRATEGY_ADVANCED    // Stratégie avancée (combinaison de plusieurs approches)
} StrategyType;

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
 * Implémentation de l'algorithme de Dijkstra
 * Trouve le chemin le plus court entre deux villes
 * @param state État actuel du jeu
 * @param start Ville de départ
 * @param end Ville d'arrivée
 * @param path Tableau à remplir avec les indices des villes formant le chemin
 * @return Longueur du chemin ou -1 si aucun chemin n'est trouvé
 */
int findShortestPath(GameState* state, int start, int end, int* path);

#endif // STRATEGY_H