/**
 * gamestate.h
 * Structure et gestion de l'état du jeu
 */
#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "../tickettorideapi/ticketToRide.h"

// Constantes
#define MAX_CARDS 100
#define MAX_OBJECTIVES 15
#define MAX_ROUTES 150
#define MAX_CITIES 50

// Structures
typedef struct {
    int from;
    int to;
    int length;
    CardColor color;
    CardColor secondColor;
    int owner;  // 0=libre, 1=nous, 2=adversaire
} Route;

typedef struct {
    int city;
    int connectionsNeeded;
    int priority;
} MissingConnection;

typedef struct {
    // Plateau
    int nbCities;
    int nbTracks;
    Route routes[MAX_ROUTES];
    
    // Nos cartes
    CardColor cards[MAX_CARDS];
    int nbCards;
    int nbCardsByColor[10];
    
    // Nos objectifs
    Objective objectives[MAX_OBJECTIVES];
    int nbObjectives;
    
    // Cartes visibles
    CardColor visibleCards[5];
    
    // Routes prises
    int claimedRoutes[MAX_ROUTES];
    int nbClaimedRoutes;
    
    // Connectivité
    int cityConnected[MAX_CITIES][MAX_CITIES];
    
    // État de jeu
    int lastTurn;
    int wagonsLeft;
    int turnCount;
    
    // Adversaire
    int opponentWagonsLeft;
    int opponentCardCount;
    int opponentObjectiveCount;
} GameState;

// Fonctions principales
void initGameState(GameState* state, GameData* gameData);
void addCardToHand(GameState* state, CardColor card);
void removeCardsForRoute(GameState* state, CardColor color, int length, int nbLocomotives);
void addClaimedRoute(GameState* state, int from, int to);
void updateAfterOpponentMove(GameState* state, MoveData* moveData);
void updateCityConnectivity(GameState* state);
void addObjectives(GameState* state, Objective* objectives, int count);

// Fonctions d'affichage (conditionnelles)
void printGameState(GameState* state);
void printConnectivityMatrix(GameState* state);

// Analyse de réseau
void analyzeExistingNetwork(GameState* state, int* cityConnectivity);
void findMissingConnections(GameState* state, int* cityConnectivity, 
                           MissingConnection* missingConnections, int* count);

// Pathfinding intelligent
int findSmartestPath(GameState* state, int start, int end, int* path, int* pathLength);

// Modélisation adversaire
void updateOpponentObjectiveModel(GameState* state, int from, int to);

#endif