#include <stdio.h>
#include <stdlib.h>
#include "rules.h"

// Vérifie si nous avons assez de cartes pour prendre une route
int canClaimRoute(GameState* state, int from, int to, CardColor color, int* nbLocomotives) {
    if (!state || !nbLocomotives) {
        printf("ERROR: Invalid parameters in canClaimRoute\n");
        return 0;
    }
    
    *nbLocomotives = 0;
    
    if (state->wagonsLeft <= 0) {
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
        return 0;
    }
    
    if (state->routes[routeIndex].owner != 0) {
        return 0;
    }
    
    int length = state->routes[routeIndex].length;
    if (state->wagonsLeft < length) {
        return 0;
    }
    
    CardColor routeColor = state->routes[routeIndex].color;
    CardColor routeSecondColor = state->routes[routeIndex].secondColor;

    // Pour les routes grises (représentées par LOCOMOTIVE)
    if (routeColor == LOCOMOTIVE) {
        if (color == LOCOMOTIVE) {
            int locomotives = state->nbCardsByColor[LOCOMOTIVE];
            if (locomotives >= length) {
                *nbLocomotives = length;
                return 1;
            } else {
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
            return 0;
        }
    }
    // Pour les routes non grises, on doit respecter la couleur
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
            return 0;
        }
        
        int colorCards = state->nbCardsByColor[color];
        int locomotives = state->nbCardsByColor[LOCOMOTIVE];
        
        if (color == LOCOMOTIVE) {
            if (locomotives >= length) {
                *nbLocomotives = length;
                return 1;
            } else {
                return 0;
            }
        }
        
        if (colorCards >= length) {
            *nbLocomotives = 0;
            return 1;
        } else if (colorCards + locomotives >= length) {
            *nbLocomotives = length - colorCards;
            return 1;
        } else {
            return 0;
        }
    }
}

// Recherche les routes que nous pouvons prendre avec nos cartes actuelles
int findPossibleRoutes(GameState* state, int* possibleRoutes, CardColor* possibleColors, int* possibleLocomotives) {
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
        state->nbCards = totalCards;
    }
    
    int nbTracksToCheck = state->nbTracks;
    if (nbTracksToCheck <= 0 || nbTracksToCheck > 150) {
        nbTracksToCheck = (nbTracksToCheck <= 0) ? 0 : 150;
    }
    
    for (int i = 0; i < nbTracksToCheck && count < MAX_ROUTES_TO_PROCESS; i++) {
        if (i < 0 || i >= state->nbTracks) {
            continue;
        }
        
        if (state->routes[i].from < 0 || state->routes[i].from >= state->nbCities || 
            state->routes[i].to < 0 || state->routes[i].to >= state->nbCities) {
            printf("ERROR: Route %d has invalid cities: %d -> %d\n", 
                   i, state->routes[i].from, state->routes[i].to);
            continue;
        }

        if (state->routes[i].owner == 0) {
            int from = state->routes[i].from;
            int to = state->routes[i].to;
            
            if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
                continue;
            }
            
            CardColor routeColor = state->routes[i].color;
            CardColor routeSecondColor = state->routes[i].secondColor;
            int length = state->routes[i].length;
            
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
                                color = 6; // BLACK par défaut
                            }
                            possibleColors[count] = color;
                            possibleLocomotives[count] = nbLocomotives;
                            
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
    
    printf("Found %d possible routes\n", count);
    
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
            }
        }
    }
    
    int objectivesCompleted = 0;
    int objectivesFailed = 0;
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (isObjectiveCompleted(state, state->objectives[i])) {
            score += state->objectives[i].score;
            objectivesCompleted++;
        } else {
            score -= state->objectives[i].score;
            objectivesFailed++;
        }
    }
    
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

// Validation simple d'un mouvement avant exécution
int isValidMove(GameState* state, MoveData* move) {
    if (!state || !move) {
        return 0;
    }
    
    switch (move->action) {
        case CLAIM_ROUTE: {
            int from = move->claimRoute.from;
            int to = move->claimRoute.to;
            CardColor color = move->claimRoute.color;
            
            // Vérifier villes
            if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
                printf("VALIDATION FAILED: Invalid cities %d->%d\n", from, to);
                return 0;
            }
            
            // Vérifier couleur
            if (color < PURPLE || color > LOCOMOTIVE) {
                printf("VALIDATION FAILED: Invalid color %d\n", color);
                return 0;
            }
            
            // Vérifier que la route existe et est libre
            int routeIndex = findRouteIndex(state, from, to);
            if (routeIndex < 0) {
                printf("VALIDATION FAILED: Route %d->%d does not exist\n", from, to);
                return 0;
            }
            
            if (state->routes[routeIndex].owner != 0) {
                printf("VALIDATION FAILED: Route %d->%d already taken\n", from, to);
                return 0;
            }
            
            // Vérifier cartes suffisantes
            int nbLoco;
            if (!canClaimRoute(state, from, to, color, &nbLoco)) {
                printf("VALIDATION FAILED: Not enough cards for %d->%d\n", from, to);
                return 0;
            }
            
            return 1;
        }
        
        case DRAW_CARD: {
            CardColor card = move->drawCard;
            if (card < PURPLE || card > LOCOMOTIVE) {
                printf("VALIDATION FAILED: Invalid card to draw %d\n", card);
                return 0;
            }
            return 1;
        }
        
        case DRAW_BLIND_CARD:
        case DRAW_OBJECTIVES:
            return 1; // Toujours valides
            
        default:
            printf("VALIDATION FAILED: Unknown action %d\n", move->action);
            return 0;
    }
}