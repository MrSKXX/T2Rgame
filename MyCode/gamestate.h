#ifndef GAMESTATE_H
#define GAMESTATE_H
#include "../tickettorideapi/ticketToRide.h"

#define MAX_CARDS 100
#define MAX_OBJECTIVES 15
#define MAX_ROUTES 150
#define MAX_CITIES 50

typedef struct {
    int from;
    int to;
    int length;
    CardColor color;
    CardColor secondColor;
    int owner; // 0=libre, 1=nous, 2=adversaire
} Route;

typedef struct {
    int nbCities;
    int nbTracks;
    Route routes[MAX_ROUTES];
    
    CardColor cards[MAX_CARDS];
    int nbCards;
    int nbCardsByColor[10];
    
    Objective objectives[MAX_OBJECTIVES];
    int nbObjectives;
    
    CardColor visibleCards[5];
    
    int claimedRoutes[MAX_ROUTES];
    int nbClaimedRoutes;
    
    int cityConnected[MAX_CITIES][MAX_CITIES];
    
    int lastTurn;
    int wagonsLeft;
    int turnCount;
    
    int opponentWagonsLeft;
    int opponentCardCount;
    int opponentObjectiveCount;
} GameState;

void initGameState(GameState* state, GameData* gameData);
void addCardToHand(GameState* state, CardColor card);
void removeCardsForRoute(GameState* state, CardColor color, int length, int nbLocomotives);
void addClaimedRoute(GameState* state, int from, int to);
void updateAfterOpponentMove(GameState* state, MoveData* moveData);
void updateCityConnectivity(GameState* state);
void addObjectives(GameState* state, Objective* objectives, int count);
void printGameState(GameState* state);
void analyzeExistingNetwork(GameState* state, int* cityConnectivity);

#endif