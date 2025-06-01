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
    
    printf("Game initialized: %d cities, %d tracks\n", state->nbCities, state->nbTracks);
}

void addCardToHand(GameState* state, CardColor card) {
    if (!state) {
        printf("ERROR: Null state in addCardToHand\n");
        return;
    }
    
    if (state->nbCards >= 50) {
        printf("WARNING: Hand full, cannot add card\n");
        return;
    }
    
    state->cards[state->nbCards++] = card;
    state->nbCardsByColor[card]++;
}

void removeCardsForRoute(GameState* state, CardColor color, int length, int nbLocomotives) {
    if (!state || length <= 0) {
        printf("ERROR: Invalid parameters in removeCardsForRoute\n");
        return;
    }
    
    if (state->nbCardsByColor[color] + state->nbCardsByColor[LOCOMOTIVE] < length) {
        printf("ERROR: Not enough cards to remove\n");
        return;
    }
    
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
}

void addClaimedRoute(GameState* state, int from, int to) {
    if (!state || from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
        printf("ERROR: Invalid parameters in addClaimedRoute (%d -> %d)\n", from, to);
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
            printf("ERROR: Route already owned (owner: %d)\n", state->routes[routeIndex].owner);
            return;
        }
        
        state->routes[routeIndex].owner = 1;
        if (state->nbClaimedRoutes < MAX_ROUTES) {
            state->claimedRoutes[state->nbClaimedRoutes++] = routeIndex;
        } else {
            printf("ERROR: Cannot add more routes (maximum reached)\n");
        }
        
        updateCityConnectivity(state);
    } else {
        printf("WARNING: Could not find route from %d to %d\n", from, to);
    }
}

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
                    
                    printf("Opponent took route %d->%d\n", from, to);
                    
                    if (state->opponentWagonsLeft <= 2) {
                        state->lastTurn = 1;
                        printf("LAST TURN: Opponent has <= 2 wagons left\n");
                    }
                } else {
                    printf("WARNING: Could not find opponent route %d->%d\n", from, to);
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
            }
            break;

        case DRAW_OBJECTIVES:
            break;
            
        default:
            printf("WARNING: Unknown action %d in updateAfterOpponentMove\n", moveData->action);
            break;
    }
    
    if (moveData->action == CLAIM_ROUTE) {
        updateOpponentObjectiveModel(state, moveData->claimRoute.from, moveData->claimRoute.to);
    }
}

void updateCityConnectivity(GameState* state) {
    extern void invalidatePathCache(void);
    invalidatePathCache();
    
    if (!state) {
        printf("ERROR: NULL state in updateCityConnectivity\n");
        return;
    }
    
    if (state->nbCities <= 0 || state->nbCities > MAX_CITIES) {
        printf("WARNING: Invalid number of cities: %d\n", state->nbCities);
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
            printf("WARNING: Invalid route index %d\n", routeIndex);
            continue;
        }
        
        int from = state->routes[routeIndex].from;
        int to = state->routes[routeIndex].to;
        
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            printf("WARNING: Invalid city indices (%d, %d) in route %d\n", from, to, routeIndex);
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

void addObjectives(GameState* state, Objective* objectives, int count) {
    for (int i = 0; i < count && state->nbObjectives < MAX_OBJECTIVES; i++) {
        state->objectives[state->nbObjectives++] = objectives[i];
        printf("Added objective: %d->%d (%d pts)\n", 
               objectives[i].from, objectives[i].to, objectives[i].score);
    }
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
}