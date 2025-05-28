#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gamestate.h"
#include "strategy/strategy.h"
#include "rules.h"
void initGameState(GameState* state, GameData* gameData) {
    memset(state, 0, sizeof(GameState));
    state->nbCities = gameData->nbCities;
    state->nbTracks = gameData->nbTracks;
    state->nbCards = 4; 
    state->nbObjectives = 0;
    state->nbClaimedRoutes = 0;
    state->lastTurn = 0;
    state->wagonsLeft = 45; 
    state->opponentWagonsLeft = 45;
    state->opponentCardCount = 4; 
    state->opponentObjectiveCount = 0;
    state->turnCount = 0;
    
    // Initialise la matrice de connectivité
    for (int i = 0; i < MAX_CITIES; i++) {
        for (int j = 0; j < MAX_CITIES; j++) {
            state->cityConnected[i][j] = 0;
        }
    }
    
    // Parse les données de routes
    int* trackData = gameData->trackData;
    for (int i = 0; i < state->nbTracks; i++) {
        int from = trackData[i*5];
        int to = trackData[i*5 + 1];
        int length = trackData[i*5 + 2];
        CardColor color = (CardColor)trackData[i*5 + 3];
        CardColor secondColor = (CardColor)trackData[i*5 + 4];
        
        Route route;
        route.from = from;
        route.to = to;
        route.length = length;
        route.color = color;
        route.secondColor = secondColor;
        route.owner = 0; 
        state->routes[i] = route;
    }
    
    memset(state->visibleCards, 0, sizeof(state->visibleCards));
    
    printf("GameState initialized with %d cities and %d tracks\n", state->nbCities, state->nbTracks);
}

// Met à jour l'état du jeu après avoir reçu une carte
void addCardToHand(GameState* state, CardColor card) {
    if (!state) {
        printf("ERROR: Null state in addCardToHand\n");
        return;
    }
    
    if (state->nbCards >= 50) {
        printf("WARNING: Cannot add more cards, hand is full (50 max)!\n");
        return;
    }
    
    state->cards[state->nbCards++] = card;
    state->nbCardsByColor[card]++;
    
    printf("Added card to hand: ");
    const char* cardNames[] = {"None", "Purple", "White", "Blue", "Yellow", 
                              "Orange", "Black", "Red", "Green", "Locomotive"};
    printf("%s\n", cardNames[card]);
    
    printf("Current hand: %d cards total\n", state->nbCards);
}

// Met à jour l'état du jeu après avoir joué des cartes pour prendre une route
void removeCardsForRoute(GameState* state, CardColor color, int length, int nbLocomotives) {
    extern void debugPrint(int level, const char* format, ...);
    
    if (!state || length <= 0) {
        printf("ERROR: Invalid parameters in removeCardsForRoute\n");
        return;
    }
    
    if (state->nbCardsByColor[color] + state->nbCardsByColor[LOCOMOTIVE] < length) {
        printf("ERROR: Not enough cards to remove in removeCardsForRoute\n");
        return;
    }
    
    const char* cardNames[] = {"None", "Purple", "White", "Blue", "Yellow", 
                              "Orange", "Black", "Red", "Green", "Locomotive"};
    
    printf("Removing cards for route: %d %s cards and %d locomotives\n", 
           length - nbLocomotives, 
           (color < 10 && color >= 0) ? cardNames[color] : "Unknown",
           nbLocomotives);
    
    // Mettre à jour directement les compteurs
    state->nbCardsByColor[color] -= (length - nbLocomotives);
    state->nbCardsByColor[LOCOMOTIVE] -= nbLocomotives;
    state->nbCards -= length;
    
    // Vérification de sécurité pour éviter les valeurs négatives
    if (state->nbCardsByColor[color] < 0) {
        printf("WARNING: Negative card count corrected for color %d\n", color);
        state->nbCardsByColor[color] = 0;
    }
    if (state->nbCardsByColor[LOCOMOTIVE] < 0) {
        printf("WARNING: Negative locomotive count corrected\n");
        state->nbCardsByColor[LOCOMOTIVE] = 0;
    }
    if (state->nbCards < 0) {
        printf("WARNING: Negative total card count corrected\n");
        state->nbCards = 0;
    }
    
    state->wagonsLeft -= length;
    
    printf("Removed %d cards (%d %s and %d locomotives) for claiming route\n", 
           length, length - nbLocomotives, 
           (color < 10 && color >= 0) ? cardNames[color] : "Unknown",
           nbLocomotives);
}

// Met à jour l'état du jeu après avoir pris une route
void addClaimedRoute(GameState* state, int from, int to) {
    if (!state || from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
        printf("ERREUR CRITIQUE: Paramètres invalides dans addClaimedRoute (%d -> %d)\n", from, to);
        return;
    }
    // Trouve l'index de la route
    int routeIndex = -1;
    for (int i = 0; i < state->nbTracks; i++) {
        if ((state->routes[i].from == from && state->routes[i].to == to) ||
            (state->routes[i].from == to && state->routes[i].to == from)) {
            routeIndex = i;
            break;
        }
    }
    
    if (routeIndex != -1) {
        if (state->routes[routeIndex].owner != 0) {
            printf("ERREUR: Tentative de prendre une route déjà possédée (owner: %d)\n", 
                   state->routes[routeIndex].owner);
            return;
        }
        
        state->routes[routeIndex].owner = 1;
        if (state->nbClaimedRoutes < MAX_ROUTES) {
            state->claimedRoutes[state->nbClaimedRoutes++] = routeIndex;
            printf("Added route from %d to %d to our claimed routes\n", from, to);
        } else {
            printf("ERREUR: Impossible d'ajouter plus de routes (maximum atteint)\n");
        }
        
        updateCityConnectivity(state);
    } else {
        printf("WARNING: Could not find route from %d to %d\n", from, to);
    }
}

// Met à jour l'état du jeu après une action de l'adversaire
void updateAfterOpponentMove(GameState* state, MoveData* moveData) {
    if (!state || !moveData) {
        printf("ERROR: NULL parameters in updateAfterOpponentMove\n");
        return;
    }

    switch (moveData->action) {
        case CLAIM_ROUTE:
            {
                int from = moveData->claimRoute.from;
                int to = moveData->claimRoute.to;
                
                int routeIndex = -1;
                for (int i = 0; i < state->nbTracks; i++) {
                    if ((state->routes[i].from == from && state->routes[i].to == to) ||
                        (state->routes[i].from == to && state->routes[i].to == from)) {
                        routeIndex = i;
                        break;
                    }
                }
                
                if (routeIndex != -1) {
                    state->routes[routeIndex].owner = 2;
                    state->opponentWagonsLeft -= state->routes[routeIndex].length;
                    
                    printf("ATTENTION: Adversaire a pris la route %d à %d (route #%d, longueur %d)\n", 
                           from, to, routeIndex, state->routes[routeIndex].length);
                    
                    if (state->opponentWagonsLeft <= 2) {
                        state->lastTurn = 1;
                        printf("LAST TURN: Opponent has <= 2 wagons left\n");
                    }
                } else {
                    printf("WARNING: Could not find route claimed by opponent from %d to %d\n", from, to);
                }
            }
            break;
            
        case DRAW_CARD:
        case DRAW_BLIND_CARD:
            state->opponentCardCount++;
            break;
            
        case CHOOSE_OBJECTIVES:
            {
                int keptObjectives = 0;
                for (int i = 0; i < 3; i++) {
                    if (moveData->chooseObjectives[i]) {
                        keptObjectives++;
                    }
                }
                state->opponentObjectiveCount += keptObjectives;
                printf("Opponent kept %d objectives\n", keptObjectives);
            }
            break;

        case DRAW_OBJECTIVES:
            printf("Opponent is drawing objective cards\n");
            break;
            
        default:
            printf("WARNING: Unknown action %d in updateAfterOpponentMove\n", moveData->action);
            break;
    }
    
    if (moveData->action == CLAIM_ROUTE) {
        updateOpponentObjectiveModel(state, moveData->claimRoute.from, moveData->claimRoute.to);
    }
}

// Met à jour la matrice de connectivité des villes
void updateCityConnectivity(GameState* state) {
    extern void invalidatePathCache(void);
    invalidatePathCache();
    if (!state) {
        printf("Error: Game state is NULL in updateCityConnectivity\n");
        return;
    }
    
    if (state->nbCities <= 0 || state->nbCities > MAX_CITIES) {
        printf("Warning: Invalid number of cities: %d\n", state->nbCities);
        return;
    }
    
    // Réinitialise la matrice
    for (int i = 0; i < state->nbCities; i++) {
        for (int j = 0; j < state->nbCities; j++) {
            state->cityConnected[i][j] = 0;
        }
    }
    
    // Pour chaque route que nous possédons
    for (int i = 0; i < state->nbClaimedRoutes; i++) {
        int routeIndex = state->claimedRoutes[i];
        
        if (routeIndex < 0 || routeIndex >= state->nbTracks) {
            printf("Warning: Invalid route index %d in updateCityConnectivity\n", routeIndex);
            continue;
        }
        
        int from = state->routes[routeIndex].from;
        int to = state->routes[routeIndex].to;
        
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            printf("Warning: Invalid city indices (%d, %d) in route %d\n", from, to, routeIndex);
            continue;
        }
        
        state->cityConnected[from][to] = 1;
        state->cityConnected[to][from] = 1;
    }
    
    // Algorithme de Floyd-Warshall pour trouver toutes les connectivités transitives
    for (int k = 0; k < state->nbCities; k++) {
        for (int i = 0; i < state->nbCities; i++) {
            for (int j = 0; j < state->nbCities; j++) {
                if (state->cityConnected[i][k] && state->cityConnected[k][j]) {
                    state->cityConnected[i][j] = 1;
                }
            }
        }
    }
}

// Ajoute des objectifs à notre main
void addObjectives(GameState* state, Objective* objectives, int count) {
    for (int i = 0; i < count && state->nbObjectives < MAX_OBJECTIVES; i++) {
        state->objectives[state->nbObjectives++] = objectives[i];
        printf("Added objective: From %d to %d, score %d\n", 
               objectives[i].from, objectives[i].to, objectives[i].score);
    }
}

// Affiche l'état du jeu actuel
void printGameState(GameState* state) {
    if (!state) {
        printf("ERROR: Cannot print NULL game state\n");
        return;
    }
    
    printf("\n--- GAME STATE ---\n");
    
    printf("Cities: %d, Tracks: %d\n", state->nbCities, state->nbTracks);
    
    printf("Cards in hand (%d):\n", state->nbCards);
    const char* cardNames[] = {"None", "Purple", "White", "Blue", "Yellow", 
                              "Orange", "Black", "Red", "Green", "Locomotive"};
    
    for (int i = 0; i < 10; i++) {
        if (state->nbCardsByColor[i] > 0) {
            printf("  %s: %d\n", cardNames[i], state->nbCardsByColor[i]);
        }
    }
    
    printf("Objectives (%d):\n", state->nbObjectives);
    for (int i = 0; i < state->nbObjectives; i++) {
        printf("  %d. From %d to %d, score %d", 
               i+1, state->objectives[i].from, state->objectives[i].to, state->objectives[i].score);
        
        int from = state->objectives[i].from;
        int to = state->objectives[i].to;
        
        if (from >= 0 && from < state->nbCities && to >= 0 && to < state->nbCities) {
            if (state->cityConnected[from][to]) {
                printf(" [COMPLETED]");
            }
        } else {
            printf(" [INVALID CITIES]");
        }
        printf("\n");
    }
    
    printf("Claimed routes (%d):\n", state->nbClaimedRoutes);
    for (int i = 0; i < state->nbClaimedRoutes; i++) {
        int routeIndex = state->claimedRoutes[i];
        if (routeIndex >= 0 && routeIndex < state->nbTracks) {
            printf("  %d. From %d to %d, length %d, color %s\n", 
                   i+1, state->routes[routeIndex].from, state->routes[routeIndex].to, 
                   state->routes[routeIndex].length, 
                   (state->routes[routeIndex].color < 10) ? cardNames[state->routes[routeIndex].color] : "Invalid");
        } else {
            printf("  %d. Invalid route index: %d\n", i+1, routeIndex);
        }
    }
    
    printf("Wagons left: %d\n", state->wagonsLeft);
    printf("Opponent wagons left: %d\n", state->opponentWagonsLeft);
    printf("Visible cards:\n");
    for (int i = 0; i < 5; i++) {
        if (state->visibleCards[i] >= 0 && state->visibleCards[i] < 10) {
            printf("  %d. %s\n", i+1, cardNames[state->visibleCards[i]]);
        } else {
            printf("  %d. Invalid card: %d\n", i+1, state->visibleCards[i]);
        }
    }
    printf("------------------\n\n");
}

// Affiche une représentation visuelle de la matrice de connectivité
void printConnectivityMatrix(GameState* state) {
    if (!state) {
        printf("ERROR: NULL state in printConnectivityMatrix\n");
        return;
    }

    printf("\n=== CONNECTIVITY MATRIX ===\n");
    
    int maxCitiesToShow = 10;
    if (state->nbCities < maxCitiesToShow) {
        maxCitiesToShow = state->nbCities;
    }
    
    printf("    ");
    for (int j = 0; j < maxCitiesToShow; j++) {
        printf("%2d ", j);
    }
    printf("\n");
    
    printf("   ");
    for (int j = 0; j < maxCitiesToShow; j++) {
        printf("---");
    }
    printf("\n");
    
    for (int i = 0; i < maxCitiesToShow; i++) {
        printf("%2d | ", i);
        for (int j = 0; j < maxCitiesToShow; j++) {
            if (i < state->nbCities && j < state->nbCities) {
                printf("%2d ", state->cityConnected[i][j]);
            } else {
                printf(" - ");
            }
        }
        printf("\n");
    }
    
    int connectedPairs = 0;
    for (int i = 0; i < state->nbCities; i++) {
        for (int j = i+1; j < state->nbCities; j++) {
            if (state->cityConnected[i][j]) {
                connectedPairs++;
            }
        }
    }
    
    printf("\nTotal connected city pairs: %d out of %d possible pairs\n", 
           connectedPairs, (state->nbCities * (state->nbCities - 1)) / 2);
    
    printf("\nObjective connectivity status:\n");
    for (int i = 0; i < state->nbObjectives; i++) {
        if (i < 0 || i >= MAX_OBJECTIVES) continue;
        
        int from = state->objectives[i].from;
        int to = state->objectives[i].to;
        
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            printf("  Objective %d: Invalid cities\n", i+1);
            continue;
        }
        
        bool connected = state->cityConnected[from][to];
        printf("  Objective %d: From %d to %d - %s\n", 
               i+1, from, to, connected ? "CONNECTED" : "not connected");
    }
    
    printf("=========================\n\n");
}

void analyzeExistingNetwork(GameState* state, int* cityConnectivity) {
    for (int i = 0; i < state->nbCities; i++) {
        cityConnectivity[i] = 0;
    }
    
    for (int i = 0; i < state->nbClaimedRoutes; i++) {
        int routeIndex = state->claimedRoutes[i];
        if (routeIndex >= 0 && routeIndex < state->nbTracks) {
            int from = state->routes[routeIndex].from;
            int to = state->routes[routeIndex].to;
            
            if (from >= 0 && from < state->nbCities) {
                cityConnectivity[from]++;
            }
            if (to >= 0 && to < state->nbCities) {
                cityConnectivity[to]++;
            }
        }
    }
    
    printf("Analyse du réseau existant:\n");
    for (int i = 0; i < state->nbCities; i++) {
        if (cityConnectivity[i] > 0) {
            printf("  Ville %d: %d connexions\n", i, cityConnectivity[i]);
        }
    }
}

void findMissingConnections(GameState* state, int* cityConnectivity, MissingConnection* missingConnections, int* count) {
    *count = 0;
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            int objFrom = state->objectives[i].from;
            int objTo = state->objectives[i].to;
            int objScore = state->objectives[i].score;
            
            for (int city = 0; city < state->nbCities && city < MAX_CITIES; city++) {
                if (cityConnectivity[city] >= 2) {
                    if (state->cityConnected[city][objFrom] || state->cityConnected[city][objTo]) {
                        int targetCity = state->cityConnected[city][objFrom] ? objTo : objFrom;
                        
                        int path[MAX_CITIES];
                        int pathLength = 0;
                        if (!state->cityConnected[city][targetCity] && 
                            findShortestPath(state, city, targetCity, path, &pathLength) > 0) {
                            
                            if (*count < MAX_CITIES) {
                                missingConnections[*count].city = city;
                                missingConnections[*count].connectionsNeeded = pathLength - 1;
                                missingConnections[*count].priority = 
                                    (objScore * 100) / missingConnections[*count].connectionsNeeded;
                                (*count)++;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Trier par priorité
    for (int i = 0; i < *count - 1; i++) {
        for (int j = 0; j < *count - i - 1; j++) {
            if (missingConnections[j].priority < missingConnections[j+1].priority) {
                MissingConnection temp = missingConnections[j];
                missingConnections[j] = missingConnections[j+1];
                missingConnections[j+1] = temp;
            }
        }
    }
    
    printf("Connexions manquantes identifiées:\n");
    for (int i = 0; i < *count && i < 5; i++) {
        printf("  Ville %d: %d connexions nécessaires, priorité %d\n", 
               missingConnections[i].city, 
               missingConnections[i].connectionsNeeded, 
               missingConnections[i].priority);
    }
}