/**
 * opponent_modeling.c
 * Modélisation et prédiction du comportement de l'adversaire 
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "strategy.h"
#include "../rules.h"      
#include "../gamestate.h"  

// Variables globales pour le modèle de l'adversaire
int opponentCitiesOfInterest[MAX_CITIES] = {0};
OpponentProfile currentOpponentProfile = OPPONENT_UNKNOWN;

// Identifie le profil stratégique de l'adversaire
OpponentProfile identifyOpponentProfile(GameState* state) {
    // Analyse des actions adverses pour déterminer son profil
    float routeRatio = 0.0;
    if (state->turnCount > 0) {
        routeRatio = (float)(state->nbTracks - state->nbClaimedRoutes - 
                    (state->opponentWagonsLeft * 5 / 45)) / state->turnCount;
    }
    
    // Routes longues vs courtes
    float avgRouteLength = 0;
    int routeCount = 0;
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 2) {
            avgRouteLength += state->routes[i].length;
            routeCount++;
        }
    }
    
    if (routeCount > 0) {
        avgRouteLength /= routeCount;
    }
    
    // Déduction du profil
    if (routeRatio > 0.7 || avgRouteLength < 2.5) {
        return OPPONENT_AGGRESSIVE;
    }
    
    if (state->opponentCardCount > 12 && routeCount < state->turnCount / 3) {
        return OPPONENT_HOARDER;
    }
    
    if (state->opponentObjectiveCount > 3) {
        return OPPONENT_OBJECTIVE;
    }
    
    // Détection de stratégie de blocage
    int blockingMoves = 0;
    for (int i = 0; i < MAX_CITIES; i++) {
        if (opponentCitiesOfInterest[i] > 10) {
            blockingMoves++;
        }
    }
    
    if (blockingMoves > 3) {
        return OPPONENT_BLOCKER;
    }
    
    return OPPONENT_UNKNOWN;
}

// Modèle les objectifs probables de l'adversaire à partir de ses actions
void updateOpponentObjectiveModel(GameState* state, int from, int to) {
    if (from < 0 || from >= MAX_CITIES || to < 0 || to >= MAX_CITIES) {
        return;
    }
    
    static int opponentCityVisits[MAX_CITIES] = {0};
    static int opponentLikelyObjectives[MAX_CITIES][MAX_CITIES] = {0};
    static int opponentConsecutiveRoutesInDirection[MAX_CITIES] = {0};
    
    // Mettre à jour le compteur de visites
    opponentCityVisits[from]++;
    opponentCityVisits[to]++;
    
    // Détecter les patterns de routes consécutives dans une direction
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 2) {
            if (state->routes[i].from == from || state->routes[i].from == to || 
                state->routes[i].to == from || state->routes[i].to == to) {
                
                int otherCity = -1;
                if (state->routes[i].from == from || state->routes[i].from == to) {
                    otherCity = state->routes[i].to;
                } else {
                    otherCity = state->routes[i].from;
                }
                
                if (otherCity != from && otherCity != to && otherCity >= 0 && otherCity < MAX_CITIES) {
                    opponentConsecutiveRoutesInDirection[otherCity]++;
                    
                    if (opponentConsecutiveRoutesInDirection[otherCity] >= 2) {
                        for (int dest = 0; dest < state->nbCities && dest < MAX_CITIES; dest++) {
                            if (dest != otherCity) {
                                int dx = abs(dest - otherCity) % 10;
                                int distance = 10 - dx;
                                if (distance > 0) {
                                    opponentLikelyObjectives[otherCity][dest] += distance * 15;
                                    opponentLikelyObjectives[dest][otherCity] += distance * 15;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Mettre à jour les probabilités d'objectifs
    for (int i = 0; i < state->nbCities && i < MAX_CITIES; i++) {
        if (opponentCityVisits[i] >= 2) {
            for (int j = i+1; j < state->nbCities && j < MAX_CITIES; j++) {
                if (opponentCityVisits[j] >= 2) {
                    int path[MAX_CITIES];
                    int pathLength = 0;
                    int distance = findShortestPath(state, i, j, path, &pathLength);
                    
                    if (distance > 0) {
                        int distanceScore = 0;
                        if (distance >= 4 && distance <= 9) {
                            distanceScore = 10;
                        } else if (distance >= 2 && distance <= 12) {
                            distanceScore = 5;
                        }
                        
                        int opponentRoutesOnPath = 0;
                        for (int k = 0; k < pathLength - 1; k++) {
                            int cityA = path[k];
                            int cityB = path[k + 1];
                            
                            for (int r = 0; r < state->nbTracks; r++) {
                                if (((state->routes[r].from == cityA && state->routes[r].to == cityB) ||
                                     (state->routes[r].from == cityB && state->routes[r].to == cityA)) &&
                                    state->routes[r].owner == 2) {
                                    opponentRoutesOnPath++;
                                    break;
                                }
                            }
                        }
                        
                        if (opponentRoutesOnPath >= 2) {
                            opponentLikelyObjectives[i][j] += 30 * opponentRoutesOnPath;
                            opponentLikelyObjectives[j][i] += 30 * opponentRoutesOnPath;
                        }
                        else if (distanceScore > 0) {
                            opponentLikelyObjectives[i][j] += distanceScore;
                            opponentLikelyObjectives[j][i] += distanceScore;
                        }
                    }
                }
            }
        }
    }
    
    // Analyse des objectifs probables de l'adversaire
    int threshold = 30;
    int count = 0;
    int topObjectives[5][2];
    int topScores[5] = {0};
    
    for (int i = 0; i < state->nbCities && i < MAX_CITIES; i++) {
        for (int j = i+1; j < state->nbCities && j < MAX_CITIES; j++) {
            if (opponentLikelyObjectives[i][j] > threshold) {
                // Insérer dans le top 5
                for (int k = 0; k < 5; k++) {
                    if (opponentLikelyObjectives[i][j] > topScores[k]) {
                        // Décaler les éléments pour insérer
                        for (int m = 4; m > k; m--) {
                            topScores[m] = topScores[m-1];
                            topObjectives[m][0] = topObjectives[m-1][0];
                            topObjectives[m][1] = topObjectives[m-1][1];
                        }
                        // Insérer le nouvel élément
                        topScores[k] = opponentLikelyObjectives[i][j];
                        topObjectives[k][0] = i;
                        topObjectives[k][1] = j;
                        break;
                    }
                }
                count++;
            }
        }
    }
    
    // Mettre à jour le tableau global des points d'intérêt
    for (int i = 0; i < MAX_CITIES; i++) {
        opponentCitiesOfInterest[i] = 0;
    }
    
    // Attribuer des points d'intérêt aux villes des objectifs probables
    for (int i = 0; i < 5; i++) {
        if (topScores[i] > 0) {
            int city1 = topObjectives[i][0];
            int city2 = topObjectives[i][1];
            if (city1 >= 0 && city1 < MAX_CITIES) {
                opponentCitiesOfInterest[city1] += (5-i) * 2;
            }
            if (city2 >= 0 && city2 < MAX_CITIES) {
                opponentCitiesOfInterest[city2] += (5-i) * 2;
            }
            
            int path[MAX_CITIES];
            int pathLength = 0;
            if (findShortestPath(state, city1, city2, path, &pathLength) > 0) {
                // Identifier les routes critiques sur ce chemin
                for (int j = 0; j < pathLength - 1; j++) {
                    int pathFrom = path[j];
                    int pathTo = path[j+1];
                    
                    if (pathFrom >= 0 && pathFrom < MAX_CITIES) {
                        opponentCitiesOfInterest[pathFrom] += 1;
                    }
                    if (pathTo >= 0 && pathTo < MAX_CITIES) {
                        opponentCitiesOfInterest[pathTo] += 1;
                    }
                }
            }
        }
    }
}

// Met à jour le profil de l'adversaire
void updateOpponentProfile(GameState* state) {
    currentOpponentProfile = identifyOpponentProfile(state);
}

// Identifie les routes critiques à bloquer pour l'adversaire
int findCriticalRoutesToBlock(GameState* state, int* routesToBlock, int* blockingPriorities) {
    int count = 0;
    const int MAX_BLOCKING_ROUTES = 10;
    
    // Initialiser les tableaux
    for (int i = 0; i < MAX_BLOCKING_ROUTES; i++) {
        routesToBlock[i] = -1;
        blockingPriorities[i] = 0;
    }
    
    // Analyse des objectifs probables de l'adversaire
    int topObjectives[5][2] = {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};
    int topScores[5] = {0};
    
    // Trouver les 5 objectifs les plus probables de l'adversaire
    for (int i = 0; i < state->nbCities && i < MAX_CITIES; i++) {
        if (opponentCitiesOfInterest[i] > 0) {
            for (int j = i+1; j < state->nbCities && j < MAX_CITIES; j++) {
                if (opponentCitiesOfInterest[j] > 0) {
                    // Score basé sur l'intérêt combiné
                    int score = opponentCitiesOfInterest[i] + opponentCitiesOfInterest[j];
                    
                    int path[MAX_CITIES];
                    int pathLength = 0;
                    if (findShortestPath(state, i, j, path, &pathLength) > 0) {
                        // Compter combien de routes l'adversaire a déjà sur ce chemin
                        int opponentRoutes = 0;
                        for (int k = 0; k < pathLength - 1; k++) {
                            int cityA = path[k];
                            int cityB = path[k+1];
                            
                            for (int r = 0; r < state->nbTracks; r++) {
                                if (((state->routes[r].from == cityA && state->routes[r].to == cityB) ||
                                     (state->routes[r].from == cityB && state->routes[r].to == cityA)) &&
                                    state->routes[r].owner == 2) {
                                    opponentRoutes++;
                                    break;
                                }
                            }
                        }
                        
                        // Augmenter le score si l'adversaire a déjà des routes sur ce chemin
                        score += opponentRoutes * 5;
                        
                        // Insérer dans le top 5 si score suffisant
                        for (int k = 0; k < 5; k++) {
                            if (score > topScores[k]) {
                                // Décaler les éléments
                                for (int m = 4; m > k; m--) {
                                    topScores[m] = topScores[m-1];
                                    topObjectives[m][0] = topObjectives[m-1][0];
                                    topObjectives[m][1] = topObjectives[m-1][1];
                                }
                                // Insérer
                                topScores[k] = score;
                                topObjectives[k][0] = i;
                                topObjectives[k][1] = j;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Pour chaque objectif probable, identifier les routes critiques à bloquer
    for (int i = 0; i < 5 && topScores[i] > 0; i++) {
        int from = topObjectives[i][0];
        int to = topObjectives[i][1];
        
        int path[MAX_CITIES];
        int pathLength = 0;
        if (findShortestPath(state, from, to, path, &pathLength) > 0) {
            // Pour chaque segment du chemin
            for (int j = 0; j < pathLength - 1 && count < MAX_BLOCKING_ROUTES; j++) {
                int cityA = path[j];
                int cityB = path[j+1];
                
                // Trouver la route correspondante
                for (int r = 0; r < state->nbTracks; r++) {
                    if (((state->routes[r].from == cityA && state->routes[r].to == cityB) ||
                         (state->routes[r].from == cityB && state->routes[r].to == cityA)) &&
                        state->routes[r].owner == 0) {
                        
                        // Vérifier s'il existe un chemin alternatif sans cette route
                        bool isBottleneck = true;
                        
                        // Temporairement marquer cette route comme prise par l'adversaire
                        int originalOwner = state->routes[r].owner;
                        state->routes[r].owner = 2;
                        
                        // Vérifier s'il existe encore un chemin
                        int altPath[MAX_CITIES];
                        int altPathLength = 0;
                        int altDistance = findShortestPath(state, from, to, altPath, &altPathLength);
                        
                        // Restaurer l'état original
                        state->routes[r].owner = originalOwner;
                        
                        // Si un chemin alternatif existe, ce n'est pas un bottleneck
                        if (altDistance > 0) {
                            isBottleneck = false;
                        }
                        
                        // Si c'est un bottleneck, c'est une route critique à bloquer
                        if (isBottleneck) {
                            // Calculer la priorité
                            int priority = topScores[i] * (3-i);
                            
                            // Bonus pour les routes courtes (plus faciles à prendre)
                            if (state->routes[r].length <= 2) {
                                priority += 10;
                            }
                            
                            // Bonus si c'est près du début du chemin (plus stratégique)
                            if (j <= 1 || j >= pathLength - 2) {
                                priority += 10;
                            }
                            
                            // Ajouter à notre liste de routes à bloquer
                            if (count < MAX_BLOCKING_ROUTES) {
                                routesToBlock[count] = r;
                                blockingPriorities[count] = priority;
                                count++;
                            }
                            
                            // Eviter les doublons
                            break;
                        }
                    }
                }
            }
        }
    }
    
    // Trier les routes par priorité (tri à bulles simple)
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (blockingPriorities[j] < blockingPriorities[j+1]) {
                // Swap routes
                int tempRoute = routesToBlock[j];
                routesToBlock[j] = routesToBlock[j+1];
                routesToBlock[j+1] = tempRoute;
                
                // Swap priorities
                int tempPriority = blockingPriorities[j];
                blockingPriorities[j] = blockingPriorities[j+1];
                blockingPriorities[j+1] = tempPriority;
            }
        }
    }
    
    return count;
}