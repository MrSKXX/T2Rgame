#ifndef MANUAL_H
#define MANUAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "gamestate.h"
#include "player.h"  // Pour cleanupMoveResult
#include "../tickettorideapi/ticketToRide.h"

/**
 * Affiche le nom d'une carte couleur (version pour le mode manuel)
 * @param card La couleur de la carte à afficher
 */
void printCardNameManual(CardColor card);

/**
 * Obtient le nom d'une carte couleur sous forme de chaîne
 * @param card La couleur de la carte
 * @return Le nom de la couleur
 */
const char* getCardColorName(CardColor card);

/**
 * Affiche les informations d'un objectif
 * @param objective L'objectif à afficher
 */
void printObjectiveDetails(Objective objective);

/**
 * Affiche l'état actuel du joueur
 * @param state L'état du jeu
 */
void printManualGameState(GameState* state);

/**
 * Affiche les routes disponibles
 * @param state L'état du jeu
 */
void printAvailableRoutes(GameState* state);

/**
 * Affiche les cartes visibles sur le plateau
 * @param visibleCards Tableau des cartes visibles
 */
void printVisibleCards(CardColor* visibleCards);

/**
 * Fonction pour prendre une route en mode manuel
 * @param state L'état du jeu
 * @return Le coup à jouer
 */
MoveData claimRouteManual(GameState* state);

/**
 * Fonction pour obtenir un coup manuel de l'utilisateur
 * @param state L'état du jeu
 * @return Le coup à jouer
 */
MoveData getManualMove(GameState* state);

/**
 * Fonction pour choisir des objectifs en mode manuel
 * @param state L'état du jeu
 * @param objectives Tableau des objectifs proposés
 * @param choices Tableau à remplir avec les choix (true/false)
 */
void chooseObjectivesManual(GameState* state, Objective* objectives, bool* choices);

/**
 * Joue un tour en mode manuel
 * @param state L'état du jeu
 * @return Le code de résultat
 */
ResultCode playManualTurn(GameState* state);

/**
 * Joue le premier tour en mode manuel
 * @param state L'état du jeu
 * @return Le code de résultat
 */
ResultCode playManualFirstTurn(GameState* state);

/**
 * Détermine si un message indique la fin de la partie
 * @param message Le message à vérifier
 * @return 1 si c'est un message de fin de partie, 0 sinon
 */
int isGameOverMessage(const char* message);

#endif // MANUAL_H