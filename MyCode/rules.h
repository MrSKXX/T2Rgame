/**
 * rules.h
 * Implémentation des règles de Ticket to Ride
 */
#ifndef RULES_H
#define RULES_H

#include "gamestate.h"
#include "../tickettorideapi/ticketToRide.h"

/**
 * Vérifie si nous avons assez de cartes pour prendre une route
 * @param state État actuel du jeu
 * @param from Ville de départ
 * @param to Ville d'arrivée
 * @param color Couleur à utiliser
 * @param nbLocomotives Nombre de locomotives à utiliser
 * @return 1 si le coup est légal, 0 sinon
 */
int canClaimRoute(GameState* state, int from, int to, CardColor color, int* nbLocomotives);

/**
 * Recherche les routes que nous pouvons prendre avec nos cartes actuelles
 * @param state État actuel du jeu
 * @param possibleRoutes Tableau à remplir avec les indices des routes disponibles
 * @param possibleColors Tableau à remplir avec les couleurs possibles pour chaque route
 * @param possibleLocomotives Tableau à remplir avec le nombre de locomotives à utiliser
 * @return Nombre de routes possibles
 */
int findPossibleRoutes(GameState* state, int* possibleRoutes, CardColor* possibleColors, int* possibleLocomotives);

/**
 * Vérifie si une couleur de carte visible peut être piochée
 * @param color Couleur à vérifier
 * @return 1 si la carte peut être piochée, 0 sinon
 */
int canDrawVisibleCard(CardColor color);

/**
 * Vérifie s'il reste suffisamment de wagons pour prendre une route
 * @param state État actuel du jeu
 * @param length Longueur de la route
 * @return 1 si nous avons assez de wagons, 0 sinon
 */
int hasEnoughWagons(GameState* state, int length);

/**
 * Détermine si c'est le dernier tour du jeu
 * @param state État actuel du jeu
 * @return 1 si c'est le dernier tour, 0 sinon
 */
int isLastTurn(GameState* state);

/**
 * Vérifie si une route est déjà prise
 * @param state État actuel du jeu
 * @param from Ville de départ
 * @param to Ville d'arrivée
 * @return 0 si la route est libre, 1 si nous l'avons, 2 si l'adversaire l'a
 */
int routeOwner(GameState* state, int from, int to);

/**
 * Vérifie si un objectif est complété
 * @param state État actuel du jeu
 * @param objective Objectif à vérifier
 * @return 1 si l'objectif est complété, 0 sinon
 */
int isObjectiveCompleted(GameState* state, Objective objective);

/**
 * Calcule le score actuel du joueur
 * @param state État actuel du jeu
 * @return Score actuel
 */
int calculateScore(GameState* state);


/**
 * Trouve l'index d'une route entre deux villes
 * @param state État actuel du jeu
 * @param from Ville de départ
 * @param to Ville d'arrivée
 * @return Index de la route, -1 si non trouvée
 */
int findRouteIndex(GameState* state, int from, int to);

/**
 * Compte le nombre d'objectifs complétés
 * @param state État actuel du jeu
 * @return Nombre d'objectifs complétés
 */
int completeObjectivesCount(GameState* state);

int isValidMove(GameState* state, MoveData* move);

#endif // RULES_H