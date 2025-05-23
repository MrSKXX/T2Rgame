#include <stdio.h>
#include <stdlib.h>
#include "rules.h"

extern void debugPrint(int level, const char* format, ...);

// Vérifie si nous avons assez de cartes pour prendre une route
int canClaimRoute(GameState* state, int from, int to, CardColor color, int* nbLocomotives) {
    extern void debugPrint(int level, const char* format, ...);
    
    if (!state || !nbLocomotives) {
        printf("ERROR: Invalid parameters in canClaimRoute\n");
        return 0;
    }
    
    const char* cardNames[] = {"None", "Purple", "White", "Blue", "Yellow", 
                              "Orange", "Black", "Red", "Green", "Locomotive"};
    
    *nbLocomotives = 0;
    
    if (state->wagonsLeft <= 0) {
        debugPrint(2, "DEBUG: canClaimRoute - Not enough wagons left");
        return 0;
    }
    
    int routeIndex = -1;
    for (int i = 0; i < state->nbTracks; i++) {
        if ((state->routes[i].from == from && state->routes[i].to == to) ||
            (state->routes[i].from == to && state->routes[i].to == from)) {
            routeIndex = i;
            break;
        }
    }
    
    if (routeIndex == -1) {
        debugPrint(2, "DEBUG: canClaimRoute - Route from %d to %d not found", from, to);
        return 0;
    }
    
    if (state->routes[routeIndex].owner != 0) {
        debugPrint(2, "DEBUG: canClaimRoute - Route from %d to %d already owned by %d", 
              from, to, state->routes[routeIndex].owner);
        return 0;
    }
    
    int length = state->routes[routeIndex].length;
    if (state->wagonsLeft < length) {
        debugPrint(2, "DEBUG: canClaimRoute - Not enough wagons (%d needed, %d left)", 
              length, state->wagonsLeft);
        return 0;
    }
    
    CardColor routeColor = state->routes[routeIndex].color;
    CardColor routeSecondColor = state->routes[routeIndex].secondColor;

    // Pour les routes grises (représentées par LOCOMOTIVE)
    if (routeColor == LOCOMOTIVE) {
        debugPrint(2, "DEBUG: canClaimRoute - This is a gray route, any color is valid");
        
        if (color == LOCOMOTIVE) {
            int locomotives = state->nbCardsByColor[LOCOMOTIVE];
            if (locomotives >= length) {
                *nbLocomotives = length;
                debugPrint(2, "DEBUG: canClaimRoute - Can claim with %d locomotives", *nbLocomotives);
                return 1;
            } else {
                debugPrint(2, "DEBUG: canClaimRoute - Not enough locomotives (%d needed, %d available)", 
                      length, locomotives);
                return 0;
            }
        }
        
        int colorCards = state->nbCardsByColor[color];
        int locomotives = state->nbCardsByColor[LOCOMOTIVE];
        
        if (colorCards >= length) {
            *nbLocomotives = 0;
            return 1;
        } else if (colorCards + locomotives >= length) {
            *nbLocomotives = length - colorCards;
            return 1;
        } else {
            debugPrint(2, "DEBUG: canClaimRoute - Not enough cards (%d %s + %d locomotives available, %d needed)", 
                  colorCards, cardNames[color], locomotives, length);
            return 0;
        }
    }
    // Pour les routes non grises, on doit ABSOLUMENT respecter la couleur
    else {
        bool validColor = false;
        
        if (color == routeColor) {
            validColor = true;
        }
        else if (routeSecondColor != NONE && color == routeSecondColor) {
            validColor = true;
        }
        else if (color == LOCOMOTIVE) {
            validColor = true;
        }
        
        if (!validColor) {
            debugPrint(1, "ERROR: Invalid color for route %d-%d: expected %s", 
                      from, to, cardNames[routeColor]);
            
            if (routeSecondColor != NONE) {
                debugPrint(1, " or %s", cardNames[routeSecondColor]);
            }
            
            debugPrint(1, ", got %s", cardNames[color]);
            
            return 0;
        }
        
        int colorCards = state->nbCardsByColor[color];
        int locomotives = state->nbCardsByColor[LOCOMOTIVE];
        
        debugPrint(2, "DEBUG: canClaimRoute - We have %d %s cards and %d locomotives, need %d cards total",
              colorCards, cardNames[color], locomotives, length);
        
        if (color == LOCOMOTIVE) {
            if (locomotives >= length) {
                *nbLocomotives = length;
                debugPrint(2, "DEBUG: canClaimRoute - Can claim with %d locomotives", *nbLocomotives);
                return 1;
            } else {
                debugPrint(2, "DEBUG: canClaimRoute - Not enough locomotives (%d needed, %d available)", 
                      length, locomotives);
                return 0;
            }
        }
        
        if (colorCards >= length) {
            *nbLocomotives = 0;
            debugPrint(2, "DEBUG: canClaimRoute - Can claim with %d %s cards, no locomotives needed",
                  length, cardNames[color]);
            return 1;
        } else if (colorCards + locomotives >= length) {
            *nbLocomotives = length - colorCards;
            debugPrint(2, "DEBUG: canClaimRoute - Can claim with %d %s cards and %d locomotives",
                  colorCards, cardNames[color], *nbLocomotives);
            return 1;
        } else {
            debugPrint(2, "DEBUG: canClaimRoute - Not enough cards to claim route (%d %s + %d locomotives available, %d needed)",
                  colorCards, cardNames[color], locomotives, length);
            return 0;
        }
    }
}

// Recherche les routes que nous pouvons prendre avec nos cartes actuelles
int findPossibleRoutes(GameState* state, int* possibleRoutes, CardColor* possibleColors, int* possibleLocomotives) {
    extern void debugPrint(int level, const char* format, ...);
    int count = 0;
    
    if (!state || !possibleRoutes || !possibleColors || !possibleLocomotives) {
        printf("ERROR: Invalid parameters in findPossibleRoutes\n");
        return 0;
    }
    
    const int MAX_ROUTES_TO_PROCESS = 50;
    
    int totalCards = 0;
    for (int c = 1; c < 10; c++) {
        totalCards += state->nbCardsByColor[c];
    }
    
    if (state->nbCards != totalCards) {
        debugPrint(1, "WARNING: Card count mismatch! nbCards = %d, sum of cards by color = %d", 
              state->nbCards, totalCards);
        state->nbCards = totalCards;
    }
    
    debugPrint(2, "DEBUG: findPossibleRoutes - Total cards in hand: %d", state->nbCards);
    const char* cardNames[] = {"None", "Purple", "White", "Blue", "Yellow", 
                               "Orange", "Black", "Red", "Green", "Locomotive"};
    
    debugPrint(2, "DEBUG: findPossibleRoutes - Cards by color:");
    for (int c = 1; c < 10; c++) {
        if (state->nbCardsByColor[c] > 0) {
            debugPrint(2, "  - %s: %d", cardNames[c], state->nbCardsByColor[c]);
        }
    }
    
    debugPrint(2, "DEBUG: findPossibleRoutes - Wagons left: %d", state->wagonsLeft);
    
    int nbTracksToCheck = state->nbTracks;
    if (nbTracksToCheck <= 0 || nbTracksToCheck > 150) {
        printf("WARNING: Invalid number of tracks: %d, limiting to 150\n", nbTracksToCheck);
        nbTracksToCheck = (nbTracksToCheck <= 0) ? 0 : 150;
    }
    
    debugPrint(3, "DEBUG: Checking %d tracks for possible routes", nbTracksToCheck);
    
    for (int i = 0; i < nbTracksToCheck && count < MAX_ROUTES_TO_PROCESS; i++) {
        if (i < 0 || i >= state->nbTracks) {
            printf("WARNING: Invalid track index: %d, skipping\n", i);
            continue;
        }
        
        if (state->routes[i].from < 0 || state->routes[i].from >= state->nbCities || 
            state->routes[i].to < 0 || state->routes[i].to >= state->nbCities) {
            printf("ERREUR CRITIQUE: Route %d contient des villes invalides: %d -> %d\n", 
                   i, state->routes[i].from, state->routes[i].to);
            continue;
        }

        if (state->routes[i].owner == 0) {
            int from = state->routes[i].from;
            int to = state->routes[i].to;
            
            if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
                printf("WARNING: Invalid cities in route %d: from %d to %d, skipping\n", i, from, to);
                continue;
            }
            
            CardColor routeColor = state->routes[i].color;
            CardColor routeSecondColor = state->routes[i].secondColor;
            int length = state->routes[i].length;
            
            if (count < 10) {
                debugPrint(3, "DEBUG: Examining route %d: from %d to %d, length %d, color %s, secondColor %s", 
                      i, from, to, length, 
                      (routeColor < 10) ? cardNames[routeColor] : "Unknown",
                      (routeSecondColor < 10) ? cardNames[routeSecondColor] : "Unknown");
            }
            
            if (state->wagonsLeft < length) {
                continue;
            }
            
            CardColor colorsToCheck[10];
            int numColorsToCheck = 0;
            
            if (routeColor == LOCOMOTIVE) {
                for (int c = 1; c < 10; c++) {
                    if (state->nbCardsByColor[c] > 0) {
                        if (numColorsToCheck < 9) {
                            colorsToCheck[numColorsToCheck++] = c;
                        }
                    }
                }
            } else {
                if (routeColor != NONE && state->nbCardsByColor[routeColor] > 0) {
                    if (numColorsToCheck < 9) {
                        colorsToCheck[numColorsToCheck++] = routeColor;
                    }
                }
                
                if (routeSecondColor != NONE && routeSecondColor != routeColor && 
                    state->nbCardsByColor[routeSecondColor] > 0) {
                    if (numColorsToCheck < 9) {
                        colorsToCheck[numColorsToCheck++] = routeSecondColor;
                    }
                }
            }
            
            if (state->nbCardsByColor[LOCOMOTIVE] > 0) {
                if (numColorsToCheck == 0 || routeColor == LOCOMOTIVE) {
                    if (numColorsToCheck < 9) {
                        colorsToCheck[numColorsToCheck++] = LOCOMOTIVE;
                    }
                }
            }
            
            if (numColorsToCheck == 0) {
                continue;
            }
            
            int maxColorsToCheck = (numColorsToCheck < 5) ? numColorsToCheck : 5;
            
            for (int c = 0; c < maxColorsToCheck; c++) {
                CardColor color = colorsToCheck[c];
                
                int totalAvailableCards = (color == LOCOMOTIVE) ? 
                                         state->nbCardsByColor[LOCOMOTIVE] : 
                                         state->nbCardsByColor[color] + state->nbCardsByColor[LOCOMOTIVE];
                
                if (totalAvailableCards < length) {
                    continue;
                }
                
                int nbLocomotives = 0;
                if (canClaimRoute(state, from, to, color, &nbLocomotives)) {
                    int availableColorCards = state->nbCardsByColor[color];
                    int availableLocomotives = state->nbCardsByColor[LOCOMOTIVE];
                    
                    if (nbLocomotives <= availableLocomotives && 
                        (color == LOCOMOTIVE || (length - nbLocomotives) <= availableColorCards)) {
                        if (count < MAX_ROUTES_TO_PROCESS) {
                            possibleRoutes[count] = i;
                            
                            if (color < 1 || color > 9) {
                                printf("ERREUR DANS findPossibleRoutes: Couleur invalide %d détectée pour route %d->%d, correction à BLACK (6)\n", 
                                    color, from, to);
                                color = 6;
                            }
                            possibleColors[count] = color;
                            possibleLocomotives[count] = nbLocomotives;
                            
                            if (count < 20) {
                                printf("Possible route %d: from %d to %d, color %s, length %d, with %d locomotives\n", 
                                      count, from, to, cardNames[color], length, nbLocomotives);
                            }
                            
                            count++;
                        }
                        
                        if (routeColor != LOCOMOTIVE) {
                            if (state->nbCardsByColor[routeColor] > 0) {
                                if (numColorsToCheck < 9) {
                                    colorsToCheck[numColorsToCheck++] = routeColor;
                                }
                            }
                            
                            if (routeSecondColor != NONE && routeSecondColor != routeColor && 
                                state->nbCardsByColor[routeSecondColor] > 0) {
                                if (numColorsToCheck < 9) {
                                    colorsToCheck[numColorsToCheck++] = routeSecondColor;
                                }
                            }
                            
                            if (state->nbCardsByColor[LOCOMOTIVE] >= length) {
                                if (numColorsToCheck < 9) {
                                    colorsToCheck[numColorsToCheck++] = LOCOMOTIVE;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (count < MAX_ROUTES_TO_PROCESS) {
        possibleRoutes[count] = -1;
    }
    
    printf("Found %d possible routes to claim\n", count);
    
    return count;
}

int canDrawVisibleCard(CardColor color) {
    return 1;
}

int hasEnoughWagons(GameState* state, int length) {
    return state->wagonsLeft >= length;
}

int isLastTurn(GameState* state) {
    return state->lastTurn || state->wagonsLeft <= 2 || state->opponentWagonsLeft <= 2;
}

int routeOwner(GameState* state, int from, int to) {
    for (int i = 0; i < state->nbTracks; i++) {
        if ((state->routes[i].from == from && state->routes[i].to == to) ||
            (state->routes[i].from == to && state->routes[i].to == from)) {
            return state->routes[i].owner;
        }
    }
    return -1;
}

int findRouteIndex(GameState* state, int from, int to) {
    for (int i = 0; i < state->nbTracks; i++) {
        if ((state->routes[i].from == from && state->routes[i].to == to) ||
            (state->routes[i].from == to && state->routes[i].to == from)) {
            return i;
        }
    }
    return -1;
}

int isObjectiveCompleted(GameState* state, Objective objective) {
    return state->cityConnected[objective.from][objective.to];
}

// Calcule le score actuel du joueur
int calculateScore(GameState* state) {
    int score = 0;
    
    for (int i = 0; i < state->nbClaimedRoutes; i++) {
        int routeIndex = state->claimedRoutes[i];
        
        if (routeIndex >= 0 && routeIndex < state->nbTracks) {
            int length = state->routes[routeIndex].length;
            
            if (length >= 0 && length <= 6) {
                int pointsByLength[] = {0, 1, 2, 4, 7, 10, 15};
                score += pointsByLength[length];
            } else {
                printf("Warning: Invalid route length: %d\n", length);
            }
        } else {
            printf("Warning: Invalid route index: %d\n", routeIndex);
        }
    }
    
    int objectivesCompleted = 0;
    int objectivesFailed = 0;
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (isObjectiveCompleted(state, state->objectives[i])) {
            score += state->objectives[i].score;
            objectivesCompleted++;
            printf("Objective %d completed: +%d points\n", i+1, state->objectives[i].score);
        } else {
            score -= state->objectives[i].score;
            objectivesFailed++;
            printf("Objective %d failed: -%d points\n", i+1, state->objectives[i].score);
        }
    }
    
    printf("Score summary: %d points from routes, %d objectives completed, %d objectives failed\n", 
           score - objectivesCompleted + objectivesFailed,
           objectivesCompleted, 
           objectivesFailed);
    
    return score;
}

int completeObjectivesCount(GameState* state) {
    if (!state) return 0;
    
    int count = 0;
    for (int i = 0; i < state->nbObjectives; i++) {
        if (isObjectiveCompleted(state, state->objectives[i])) {
            count++;
        }
    }
    
    return count;
}