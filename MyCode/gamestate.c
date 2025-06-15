#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gamestate.h"
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
    
    for (int i = 0; i < MAX_CITIES; i++) {
        for (int j = 0; j < MAX_CITIES; j++) {
            state->cityConnected[i][j] = 0;
        }
    }
    
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
}

void addCardToHand(GameState* state, CardColor card) {
    if (!state) {
        return;
    }
    
    if (state->nbCards >= 50) {
        return;
    }
    
    state->cards[state->nbCards++] = card;
    state->nbCardsByColor[card]++;
}

void removeCardsForRoute(GameState* state, CardColor color, int length, int nbLocomotives) {
    if (!state || length <= 0) {
        return;
    }
    
    if (state->nbCardsByColor[color] + state->nbCardsByColor[LOCOMOTIVE] < length) {
        return;
    }
    
    state->nbCardsByColor[color] -= (length - nbLocomotives);
    state->nbCardsByColor[LOCOMOTIVE] -= nbLocomotives;
    state->nbCards -= length;
    
    if (state->nbCardsByColor[color] < 0) {
        state->nbCardsByColor[color] = 0;
    }
    if (state->nbCardsByColor[LOCOMOTIVE] < 0) {
        state->nbCardsByColor[LOCOMOTIVE] = 0;
    }
    if (state->nbCards < 0) {
        state->nbCards = 0;
    }
    
    state->wagonsLeft -= length;
}

void addClaimedRoute(GameState* state, int from, int to) {
    if (!state || from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
        return;
    }
    
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
            return;
        }
        
        state->routes[routeIndex].owner = 1;
        if (state->nbClaimedRoutes < MAX_ROUTES) {
            state->claimedRoutes[state->nbClaimedRoutes++] = routeIndex;
        }
        
        updateCityConnectivity(state);
    }
}

void updateAfterOpponentMove(GameState* state, MoveData* moveData) {
    if (!state || !moveData) {
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
                    
                    if (state->opponentWagonsLeft <= 2) {
                        state->lastTurn = 1;
                    }
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
            break;
    }
}

void updateCityConnectivity(GameState* state) {
    if (!state) {
        return;
    }
    
    if (state->nbCities <= 0 || state->nbCities > MAX_CITIES) {
        return;
    }
    
    for (int i = 0; i < state->nbCities; i++) {
        for (int j = 0; j < state->nbCities; j++) {
            state->cityConnected[i][j] = 0;
        }
    }
    
    // Add direct connections from our routes
    for (int i = 0; i < state->nbClaimedRoutes; i++) {
        int routeIndex = state->claimedRoutes[i];
        
        if (routeIndex < 0 || routeIndex >= state->nbTracks) {
            continue;
        }
        
        int from = state->routes[routeIndex].from;
        int to = state->routes[routeIndex].to;
        
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            continue;
        }
        
        state->cityConnected[from][to] = 1;
        state->cityConnected[to][from] = 1;
    }
    
    // Floyd-Warshall to find all connections
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