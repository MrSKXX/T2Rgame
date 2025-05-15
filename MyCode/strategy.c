#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include "strategy.h"
#include "rules.h"

/* Constants for utility calculations */
#define OBJECTIVE_PATH_MULTIPLIER 15  // Value of routes that are part of objective paths
#define CRITICAL_PATH_MULTIPLIER 20   // Value of routes that are the only path between cities
#define BLOCKING_VALUE_MULTIPLIER 8   // Value of blocking opponent's potential paths
#define CARD_EFFICIENCY_MULTIPLIER 5  // Value of efficiently using cards

// Global variable to track opponent interest in cities - initialize with zero
int opponentCitiesOfInterest[MAX_CITIES] = {0};

// Forward declarations for internal functions
bool isCriticalBridge(GameState* state, int city1, int city2);
bool isConnectedToOpponentNetwork(GameState* state, int city1, int city2);
void checkOpponentObjectiveProgress(GameState* state);
void improvedObjectiveEvaluation(GameState* state, Objective* objectives, bool* chooseObjectives);
void advancedRoutePrioritization(GameState* state, int* possibleRoutes, CardColor* possibleColors, 
                               int* possibleLocomotives, int numPossibleRoutes);

int decideNextMove(GameState* state, StrategyType strategy, MoveData* moveData) {
    switch (strategy) {
        case STRATEGY_BASIC:
            return basicStrategy(state, moveData);
        case STRATEGY_DIJKSTRA:
            return dijkstraStrategy(state, moveData);
        case STRATEGY_ADVANCED:
            return advancedStrategy(state, moveData);
        default:
            return basicStrategy(state, moveData);
    }
}

// Determine the current game phase based on game state
int determineGamePhase(GameState* state) {
    // Early game: first few turns or many wagons left
    if (state->turnCount < 5 || state->wagonsLeft > 35) {
        return PHASE_EARLY;
    }
    // Late game: few wagons left or last turn flag
    else if (state->wagonsLeft < 15 || state->lastTurn) {
        return PHASE_FINAL;
    }
    // Late mid-game: getting low on wagons
    else if (state->wagonsLeft < 25) {
        return PHASE_LATE;
    }
    // Middle game: everything else
    else {
        return PHASE_MIDDLE;
    }
}

// Enhanced evaluation of route utility
int enhancedEvaluateRouteUtility(GameState* state, int routeIndex) {
    // Safety check - ensure routeIndex is valid
    if (routeIndex < 0 || routeIndex >= state->nbTracks) {
        printf("ERROR: Invalid route index %d in enhancedEvaluateRouteUtility\n", routeIndex);
        return 0;
    }

    // Start with the basic utility calculation
    int utility = evaluateRouteUtility(state, routeIndex);
    
    // Add additional factors
    int from = state->routes[routeIndex].from;
    int to = state->routes[routeIndex].to;
    int length = state->routes[routeIndex].length;
    
    // Bonus for connecting to our existing network
    for (int i = 0; i < state->nbClaimedRoutes; i++) {
        int claimedRouteIndex = state->claimedRoutes[i];
        if (claimedRouteIndex >= 0 && claimedRouteIndex < state->nbTracks) {
            if (state->routes[claimedRouteIndex].from == from || 
                state->routes[claimedRouteIndex].to == from ||
                state->routes[claimedRouteIndex].from == to || 
                state->routes[claimedRouteIndex].to == to) {
                utility += 10;
                break;
            }
        }
    }
    
    // Bonus for longer routes (more points)
    if (length >= 4) {
        utility += length * 5;
    }
    
    return utility;
}

// Calculate how much a route helps with objective completion
int calculateObjectiveProgress(GameState* state, int routeIndex) {
    // Vérification de sécurité
    if (!state || routeIndex < 0 || routeIndex >= state->nbTracks) {
        printf("ERROR: Invalid parameters in calculateObjectiveProgress\n");
        return 0;
    }

    int from = state->routes[routeIndex].from;
    int to = state->routes[routeIndex].to;
    int progress = 0;
    
    // Vérification des limites
    if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
        printf("ERROR: Invalid cities in route %d: from %d to %d\n", routeIndex, from, to);
        return 0;
    }
    
    // AUGMENTATION DU MULTIPLICATEUR - rendre ces routes beaucoup plus attractives
    const int OBJECTIVE_MULTIPLIER = 50;  // Augmenté de 15 à 50
    
    // Vérifier chaque objectif non complété
    for (int i = 0; i < state->nbObjectives; i++) {
        // Vérification des limites
        if (i < 0 || i >= MAX_OBJECTIVES) {
            printf("ERROR: Invalid objective index %d\n", i);
            continue;
        }
        
        // Vérifier si l'objectif est déjà complété
        if (isObjectiveCompleted(state, state->objectives[i])) {
            continue;  // Passer à l'objectif suivant
        }
        
        // Récupérer les informations de l'objectif
        int objFrom = state->objectives[i].from;
        int objTo = state->objectives[i].to;
        int objScore = state->objectives[i].score;
        
        // Vérification des limites
        if (objFrom < 0 || objFrom >= state->nbCities || objTo < 0 || objTo >= state->nbCities) {
            printf("ERROR: Invalid cities in objective %d: from %d to %d\n", i, objFrom, objTo);
            continue;
        }
        
        // Bonus spécial si la route relie directement les villes de l'objectif
        if ((from == objFrom && to == objTo) || (from == objTo && to == objFrom)) {
            progress += objScore * OBJECTIVE_MULTIPLIER * 2;  // Double bonus pour connexion directe
            printf("Direct connection for objective %d! Adding %d points\n", 
                  i+1, objScore * OBJECTIVE_MULTIPLIER * 2);
            continue;
        }
        
        // Vérifier si cette route fait partie du chemin pour cet objectif
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findShortestPath(state, objFrom, objTo, path, &pathLength);
        
        if (distance > 0 && pathLength > 0) {
            bool routeOnPath = false;
            
            // Chercher si notre route est sur ce chemin
            for (int j = 0; j < pathLength - 1 && j < MAX_CITIES - 1; j++) {
                // Vérification des limites pour les villes du chemin
                if (path[j] < 0 || path[j] >= state->nbCities || 
                    path[j+1] < 0 || path[j+1] >= state->nbCities) {
                    continue;
                }
                
                if ((path[j] == from && path[j+1] == to) ||
                    (path[j] == to && path[j+1] == from)) {
                    
                    routeOnPath = true;
                    
                    // Plus l'objectif vaut de points, plus la route est importante
                    int basePoints = objScore * OBJECTIVE_MULTIPLIER;
                    progress += basePoints;
                    
                    // Bonus pour les routes courtes (plus efficaces)
                    int length = state->routes[routeIndex].length;
                    if (length <= 2) {
                        progress += 20;  // Bonus pour routes courtes
                    }
                    
                    // Bonus pour les routes plus tôt dans le chemin (priorité)
                    if (j <= 1) {
                        progress += 30;  // Bonus pour routes proches du début du chemin
                    }
                    
                    // Bonus si la route crée un pont crucial (pas d'autres chemins)
                    if (isCriticalBridge(state, from, to)) {
                        progress += 40;  // Bonus important pour les ponts critiques
                    }
                    
                    printf("Route on path for objective %d! Adding %d points\n", 
                          i+1, basePoints);
                    break;
                }
            }
            
            // Si cette route est sur le chemin et que le chemin est court, c'est très important
            if (routeOnPath && pathLength <= 4) {
                progress += 30;  // Bonus supplémentaire pour les chemins courts
            }
        }
    }
    
    return progress;
}


void checkObjectivesPaths(GameState* state) {
    if (!state) {
        printf("ERROR: Null state in checkObjectivesPaths\n");
        return;
    }
    
    printf("\n=== OBJECTIVE PATHS ANALYSIS ===\n");
    
    for (int i = 0; i < state->nbObjectives; i++) {
        // Vérification des limites
        if (i < 0 || i >= MAX_OBJECTIVES) {
            printf("ERROR: Invalid objective index %d\n", i);
            continue;
        }
        
        int from = state->objectives[i].from;
        int to = state->objectives[i].to;
        int score = state->objectives[i].score;
        
        // Vérification des limites
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            printf("ERROR: Invalid cities in objective %d: from %d to %d\n", i, from, to);
            continue;
        }
        
        // Vérifier si l'objectif est déjà complété
        bool completed = isObjectiveCompleted(state, state->objectives[i]);
        
        printf("Objective %d: From %d to %d, score %d %s\n", 
              i+1, from, to, score, completed ? "[COMPLETED]" : "");
        
        if (completed) {
            continue;  // Passer à l'objectif suivant si déjà complété
        }
        
        // Trouver le chemin optimal
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findShortestPath(state, from, to, path, &pathLength);
        
        if (distance > 0 && pathLength > 0) {
            printf("  Path exists, length %d: ", pathLength);
            for (int j = 0; j < pathLength && j < MAX_CITIES; j++) {
                printf("%d ", path[j]);
            }
            printf("\n");
            
            // Vérifier quelles routes sont déjà prises et lesquelles sont encore disponibles
            printf("  Routes needed:\n");
            int availableRoutes = 0;
            int claimedRoutes = 0;
            int blockedRoutes = 0;
            
            for (int j = 0; j < pathLength - 1 && j < MAX_CITIES - 1; j++) {
                int cityA = path[j];
                int cityB = path[j+1];
                
                // Vérification des limites
                if (cityA < 0 || cityA >= state->nbCities || cityB < 0 || cityB >= state->nbCities) {
                    printf("    WARNING: Invalid cities in path: %d to %d\n", cityA, cityB);
                    continue;
                }
                
                // Trouver la route correspondante
                bool routeFound = false;
                
                for (int k = 0; k < state->nbTracks; k++) {
                    if ((state->routes[k].from == cityA && state->routes[k].to == cityB) ||
                        (state->routes[k].from == cityB && state->routes[k].to == cityA)) {
                        
                        routeFound = true;
                        printf("    %d to %d: ", cityA, cityB);
                        
                        if (state->routes[k].owner == 0) {
                            printf("Available (length %d, color ", state->routes[k].length);
                            printCardName(state->routes[k].color);
                            printf(")\n");
                            availableRoutes++;
                        } else if (state->routes[k].owner == 1) {
                            printf("Already claimed by us\n");
                            claimedRoutes++;
                        } else {
                            printf("BLOCKED by opponent\n");
                            blockedRoutes++;
                        }
                        break;
                    }
                }
                
                if (!routeFound) {
                    printf("    WARNING: No route found between %d and %d\n", cityA, cityB);
                }
            }
            
            // Analyse de l'état du chemin
            printf("  Path status: %d available, %d claimed, %d blocked\n", 
                  availableRoutes, claimedRoutes, blockedRoutes);
            
            if (blockedRoutes > 0) {
                printf("  WARNING: Path is BLOCKED by opponent! Objective might be impossible.\n");
            } else if (availableRoutes == 0) {
                printf("  GOOD: All routes on path already claimed by us. Objective will be completed.\n");
            } else {
                printf("  ACTION NEEDED: %d more routes to claim to complete this objective.\n", 
                     availableRoutes);
            }
        } else {
            printf("  NO PATH EXISTS! Objective is impossible to complete.\n");
        }
    }
    
    printf("================================\n\n");
}


// Evaluate how efficiently a route uses our available cards
int evaluateCardEfficiency(GameState* state, int routeIndex) {
    // Safety check
    if (routeIndex < 0 || routeIndex >= state->nbTracks) {
        printf("ERROR: Invalid route index %d in evaluateCardEfficiency\n", routeIndex);
        return 0;
    }

    CardColor routeColor = state->routes[routeIndex].color;
    int length = state->routes[routeIndex].length;
    
    // For gray routes, find the most efficient color to use
    if (routeColor == LOCOMOTIVE) {
        int bestEfficiency = 0;
        for (int c = 1; c < 9; c++) {  // Skip NONE and LOCOMOTIVE
            if (state->nbCardsByColor[c] > 0) {
                // Calculate efficiency as cards used / cards available
                int cardsNeeded = length;
                int cardsAvailable = state->nbCardsByColor[c];
                
                if (cardsNeeded <= cardsAvailable) {
                    // Perfect match
                    int efficiency = 100;
                    if (efficiency > bestEfficiency) {
                        bestEfficiency = efficiency;
                    }
                } else {
                    // Need to supplement with locomotives
                    int locosNeeded = cardsNeeded - cardsAvailable;
                    if (locosNeeded <= state->nbCardsByColor[LOCOMOTIVE]) {
                        // Calculate efficiency (lower is better)
                        int efficiency = 80 - (locosNeeded * 10);
                        if (efficiency > bestEfficiency) {
                            bestEfficiency = efficiency;
                        }
                    }
                }
            }
        }
        return bestEfficiency;
    } 
    // For colored routes
    else {
        int cardsNeeded = length;
        int cardsAvailable = state->nbCardsByColor[routeColor];
        
        if (cardsNeeded <= cardsAvailable) {
            // Perfect match
            return 100;
        } else {
            // Need to supplement with locomotives
            int locosNeeded = cardsNeeded - cardsAvailable;
            if (locosNeeded <= state->nbCardsByColor[LOCOMOTIVE]) {
                // Calculate efficiency (lower is better)
                return 80 - (locosNeeded * 10);
            }
        }
    }
    
    return 0;  // Fallback
}

// Strategic card drawing based on needs
int strategicCardDrawing(GameState* state) {
    if (!state) {
        printf("ERROR: Invalid state in strategicCardDrawing\n");
        return -1;
    }
    
    // Definir les tableaux pour l'analyse
    int colorNeeds[10] = {0};  // Besoins par couleur (index = couleur)
    int totalColorNeeds = 0;   // Besoin total de cartes
    
    // Récupérer les couleurs nécessaires pour les objectifs non complétés
    printf("Analyzing card needs for incomplete objectives:\n");
    
    // Pour chaque objectif non complété
    for (int i = 0; i < state->nbObjectives; i++) {
        // Vérifier que l'objectif est dans les limites
        if (i < 0 || i >= MAX_OBJECTIVES) {
            continue;
        }
        
        // Si l'objectif est déjà complété, passer au suivant
        if (isObjectiveCompleted(state, state->objectives[i])) {
            continue;
        }
        
        int objFrom = state->objectives[i].from;
        int objTo = state->objectives[i].to;
        
        // Vérifier que les villes sont dans les limites
        if (objFrom < 0 || objFrom >= state->nbCities || objTo < 0 || objTo >= state->nbCities) {
            printf("  WARNING: Invalid cities in objective %d: from %d to %d\n", i, objFrom, objTo);
            continue;
        }
        
        printf("  Objective %d: From %d to %d\n", i+1, objFrom, objTo);
        
        // Trouver le chemin optimal
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findShortestPath(state, objFrom, objTo, path, &pathLength);
        
        if (distance > 0 && pathLength > 0) {
            // Pour chaque segment du chemin
            for (int j = 0; j < pathLength - 1 && j < MAX_CITIES - 1; j++) {
                int pathFrom = path[j];
                int pathTo = path[j+1];
                
                // Vérifier que les villes sont dans les limites
                if (pathFrom < 0 || pathFrom >= state->nbCities || 
                    pathTo < 0 || pathTo >= state->nbCities) {
                    continue;
                }
                
                // Trouver la route pour ce segment
                for (int k = 0; k < state->nbTracks; k++) {
                    if (((state->routes[k].from == pathFrom && state->routes[k].to == pathTo) ||
                         (state->routes[k].from == pathTo && state->routes[k].to == pathFrom)) &&
                        state->routes[k].owner == 0) {  // Route non prise
                        
                        CardColor routeColor = state->routes[k].color;
                        int length = state->routes[k].length;
                        
                        if (routeColor != LOCOMOTIVE) {
                            // Pour les routes colorées, noter la couleur et la longueur
                            colorNeeds[routeColor] += length;
                            totalColorNeeds += length;
                            printf("    Need route from %d to %d: %d %s cards\n", 
                                  pathFrom, pathTo, length, 
                                  (routeColor < 10) ? (const char*[]){
                                      "None", "Purple", "White", "Blue", "Yellow", 
                                      "Orange", "Black", "Red", "Green", "Locomotive"
                                  }[routeColor] : "Unknown");
                        } else {
                            // Pour les routes grises, besoin de n'importe quelle couleur
                            // Ajout d'un besoin de locomotives également
                            colorNeeds[LOCOMOTIVE] += 1;  // Besoin potentiel de locomotives
                            printf("    Need route from %d to %d: %d cards of any color (gray route)\n", 
                                  pathFrom, pathTo, length);
                        }
                        
                        // Si nous avons déjà une route pour ce segment, sortir
                        break;
                    }
                }
            }
        } else {
            printf("    WARNING: No path found for this objective!\n");
        }
    }
    
    // Afficher les besoins par couleur
    printf("Color needs summary:\n");
    for (int c = 1; c < 10; c++) {
        if (colorNeeds[c] > 0) {
            printf("  %s: %d cards needed\n", 
                  (c < 10) ? (const char*[]){
                      "None", "Purple", "White", "Blue", "Yellow", 
                      "Orange", "Black", "Red", "Green", "Locomotive"
                  }[c] : "Unknown", 
                  colorNeeds[c]);
        }
    }
    
    // Trouver la meilleure carte visible selon les besoins
    int bestCardIndex = -1;
    int bestCardScore = 0;
    
    for (int i = 0; i < 5; i++) {
        CardColor card = state->visibleCards[i];
        // Vérifier que la carte est valide
        if (card == NONE || card < 0 || card >= 10) {
            continue;
        }
        
        int score = 0;
        
        // Les locomotives sont toujours très précieuses
        if (card == LOCOMOTIVE) {
            score = 100;
            // Bonus supplémentaire si nous avons un besoin particulier de locomotives
            score += colorNeeds[LOCOMOTIVE] * 10;
        } 
        // Score des autres cartes basé sur les besoins
        else {
            score = colorNeeds[card] * 5;
            
            // Bonus si nous avons déjà des cartes de cette couleur
            if (state->nbCardsByColor[card] > 0) {
                score += state->nbCardsByColor[card] * 3;
                
                // Bonus supplémentaire si nous sommes proches de compléter une route
                // Vérifier chaque route pour voir si plus de cartes de cette couleur nous aideraient
                for (int r = 0; r < state->nbTracks; r++) {
                    if (state->routes[r].owner == 0) {  // Route non prise
                        CardColor routeColor = state->routes[r].color;
                        int length = state->routes[r].length;
                        
                        // Si la route est de cette couleur (ou grise)
                        if (routeColor == card || routeColor == LOCOMOTIVE) {
                            // Si nous sommes à 1-2 cartes de pouvoir prendre cette route
                            int cardsNeeded = length - state->nbCardsByColor[card];
                            if (cardsNeeded > 0 && cardsNeeded <= 2) {
                                score += (3 - cardsNeeded) * 15;  // 15 points pour 1 carte manquante, 30 pour 2
                            }
                        }
                    }
                }
            }
            
            // Pénalité si nous avons trop de cartes de cette couleur (>8)
            if (state->nbCardsByColor[card] > 8) {
                score -= (state->nbCardsByColor[card] - 8) * 5;
            }
        }
        
        printf("Visible card %d: %s, score %d\n", 
              i+1, 
              (card < 10) ? (const char*[]){
                  "None", "Purple", "White", "Blue", "Yellow", 
                  "Orange", "Black", "Red", "Green", "Locomotive"
              }[card] : "Unknown", 
              score);
        
        if (score > bestCardScore) {
            bestCardScore = score;
            bestCardIndex = i;
        }
    }
    
    // Si aucune carte visible n'est bonne ou si le score est faible, suggérer une pioche aveugle
    if (bestCardIndex == -1 || bestCardScore < 20) {
        int blindScore = 40;  // Score de base pour une pioche aveugle
        
        // Si nous avons besoin de beaucoup de couleurs différentes, la pioche aveugle est meilleure
        int uniqueNeeds = 0;
        for (int c = 1; c < 10; c++) {
            if (colorNeeds[c] > 0) {
                uniqueNeeds++;
            }
        }
        
        if (uniqueNeeds >= 3) {
            blindScore += 20;  // Bonus si nous avons besoin de plusieurs couleurs
        }
        
        printf("Blind draw score: %d (unique color needs: %d)\n", blindScore, uniqueNeeds);
        
        if (blindScore > bestCardScore) {
            printf("Recommending blind draw (score %d > best visible card score %d)\n", 
                  blindScore, bestCardScore);
            return -1;  // Recommander une pioche aveugle
        }
    }
    
    if (bestCardIndex >= 0) {
        printf("Recommending visible card %d: %s (score %d)\n", 
              bestCardIndex + 1, 
              (state->visibleCards[bestCardIndex] < 10) ? (const char*[]){
                  "None", "Purple", "White", "Blue", "Yellow", 
                  "Orange", "Black", "Red", "Green", "Locomotive"
              }[state->visibleCards[bestCardIndex]] : "Unknown", 
              bestCardScore);
    } else {
        printf("No good visible card found, recommending blind draw\n");
    }
    
    return bestCardIndex;
}

void updateOpponentObjectiveModel(GameState* state, int from, int to) {
    // Safety checks
    if (from < 0 || from >= MAX_CITIES || to < 0 || to >= MAX_CITIES) {
        printf("ERROR: Invalid city indices in updateOpponentObjectiveModel: %d, %d\n", from, to);
        return;
    }
    
    static int opponentCityVisits[MAX_CITIES] = {0};  // Count how many connections each city has
    static int opponentLikelyObjectives[MAX_CITIES][MAX_CITIES] = {0};  // Score for each potential opponent objective
    
    // Update city visit counts
    opponentCityVisits[from]++;
    opponentCityVisits[to]++;
    
    // Cities with multiple connections are likely objective endpoints
    // Update the likelihood of each possible objective pair
    for (int i = 0; i < state->nbCities; i++) {
        if (i < MAX_CITIES && opponentCityVisits[i] >= 2) {
            for (int j = 0; j < state->nbCities; j++) {
                if (j < MAX_CITIES && i != j && opponentCityVisits[j] >= 2) {
                    // Calculate distance between these potential objective cities
                    int path[MAX_CITIES];
                    int pathLength = 0;
                    int distance = findShortestPath(state, i, j, path, &pathLength);
                    
                    if (distance > 0) {
                        // Cities that are further apart are more likely to be objectives
                        // And if one of the cities is the one the opponent just connected to, increase score
                        int baseScore = distance * 2;
                        if (i == from || i == to || j == from || j == to) {
                            baseScore += 5;
                        }
                        
                        opponentLikelyObjectives[i][j] += baseScore;
                        opponentLikelyObjectives[j][i] += baseScore; // Symmetric
                        
                        // If the opponent connects cities that are already well-connected, it's likely part of an objective
                        if (opponentCityVisits[i] >= 3 && opponentCityVisits[j] >= 3) {
                            opponentLikelyObjectives[i][j] += 10;
                            opponentLikelyObjectives[j][i] += 10;
                        }
                    }
                }
            }
        }
    }

    // Log the most likely opponent objectives for debugging
    printf("Likely opponent objectives:\n");
    int threshold = 15; // Only show high-probability objectives
    int count = 0;
    for (int i = 0; i < state->nbCities && i < MAX_CITIES; i++) {
        for (int j = i+1; j < state->nbCities && j < MAX_CITIES; j++) {
            if (opponentLikelyObjectives[i][j] > threshold) {
                printf("  From %d to %d: score %d\n", i, j, opponentLikelyObjectives[i][j]);
                if (++count >= 5) break; // Limit output to 5 objectives
            }
        }
        if (count >= 5) break;
    }
    
    // Update the global opponent cities of interest for blocking decisions
    if (from < MAX_CITIES) opponentCitiesOfInterest[from] += 2;
    if (to < MAX_CITIES) opponentCitiesOfInterest[to] += 2;
    
    // Also update cities that are likely connected to these
    for (int i = 0; i < state->nbCities && i < MAX_CITIES; i++) {
        if (i < MAX_CITIES && 
            ((from < MAX_CITIES && opponentLikelyObjectives[from][i] > threshold) || 
             (to < MAX_CITIES && opponentLikelyObjectives[to][i] > threshold)) && 
            opponentCityVisits[i] > 0) {
            opponentCitiesOfInterest[i] += 1;
        }
    }
}

/**
 * Identifies critical routes that should be claimed to block the opponent
 */
/**
 * Identifies critical routes that should be claimed to block the opponent
 * This version is optimized to prevent excessive memory use and logging
 */
int findCriticalBlockingRoutes(GameState* state, int* blockingRoutes, int* blockingPriorities) {
    int count = 0;
    const int MAX_BLOCKING_ROUTES = 30;  // Reasonable limit
    
    // CORRECTION: Vérification des paramètres
    if (!state || !blockingRoutes || !blockingPriorities) {
        printf("ERROR: Invalid parameters in findCriticalBlockingRoutes\n");
        return 0;
    }
    
    // CORRECTION: Initialiser les tableaux pour éviter les valeurs aléatoires
    for (int i = 0; i < MAX_BLOCKING_ROUTES; i++) {
        blockingRoutes[i] = -1;
        blockingPriorities[i] = 0;
    }
    
    // CORRECTION: Vérification des limites du nombre de voies
    int nbTracksToCheck = state->nbTracks;
    if (nbTracksToCheck <= 0 || nbTracksToCheck > 150) {
        printf("WARNING: Invalid number of tracks: %d, limiting to 150\n", nbTracksToCheck);
        nbTracksToCheck = (nbTracksToCheck <= 0) ? 0 : 150;
    }
    
    // For each unclaimed route
    for (int i = 0; i < nbTracksToCheck && count < MAX_BLOCKING_ROUTES; i++) {
        // Vérifier que l'index de la route est valide
        if (i < 0 || i >= state->nbTracks) {
            printf("WARNING: Invalid track index: %d in findCriticalBlockingRoutes\n", i);
            continue;
        }
        
        if (state->routes[i].owner == 0) {
            int from = state->routes[i].from;
            int to = state->routes[i].to;
            
            // CORRECTION: Vérification des limites des villes
            if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
                printf("WARNING: Invalid cities in route %d: from %d to %d\n", i, from, to);
                continue;
            }
            
            // Skip if we don't have enough wagons
            if (state->routes[i].length > state->wagonsLeft) {
                continue;
            }
            
            // CORRECTION: Vérification des limites pour l'accès au tableau opponentCitiesOfInterest
            int fromInterest = (from < MAX_CITIES) ? opponentCitiesOfInterest[from] : 0;
            int toInterest = (to < MAX_CITIES) ? opponentCitiesOfInterest[to] : 0;
            
            int blockingValue = 0;
            
            // 1. Route connects cities the opponent seems interested in
            if (fromInterest > 0 && toInterest > 0) {
                blockingValue += fromInterest + toInterest;
            }
            
            // 2. Route is a bridge (removing it disconnects parts of the board)
            // Only check this for a small number of routes to avoid excessive computation
            if (count < 10 && blockingValue > 0) {
                bool isBridge = isCriticalBridge(state, from, to);
                if (isBridge) {
                    blockingValue += 20;
                }
            }
            
            // 3. Route is short (2 or less) - these are more contested
            if (state->routes[i].length <= 2) {
                blockingValue += 10 - state->routes[i].length * 3;
            }
            
            // 4. Route connects to opponent's existing network
            if (isConnectedToOpponentNetwork(state, from, to)) {
                blockingValue += 15;
            }
            
            // 5. Route is likely part of opponent's planned path
            // SIMPLIFIED: Only check a small sample of paths to reduce computation
            if (count < 15 && blockingValue > 0) {  // Only do this check for promising routes
                // Find top 3 city pairs with highest interest
                int topCityPairs[3][2] = {{-1, -1}, {-1, -1}, {-1, -1}};
                int topInterest[3] = {0, 0, 0};
                
                // CORRECTION: Limiter la recherche pour éviter les temps de calcul excessifs
                int maxCitiesToCheck = state->nbCities < 20 ? state->nbCities : 20;
                int citiesChecked = 0;
                
                for (int j = 0; j < maxCitiesToCheck && citiesChecked < 10; j++) {
                    // Vérifier que l'index de la ville est valide
                    if (j < 0 || j >= state->nbCities) {
                        continue;
                    }
                    
                    // Only check cities with significant interest
                    int jInterest = (j < MAX_CITIES) ? opponentCitiesOfInterest[j] : 0;
                    if (jInterest < 2) continue;
                    citiesChecked++;
                    
                    int pairsChecked = 0;
                    const int MAX_PAIRS_PER_CITY = 5;  // Limit pairs per city
                    
                    for (int k = j+1; k < maxCitiesToCheck && pairsChecked < MAX_PAIRS_PER_CITY; k++) {
                        // Vérifier que l'index de la ville est valide
                        if (k < 0 || k >= state->nbCities) {
                            continue;
                        }
                        
                        // Only check cities with significant interest
                        int kInterest = (k < MAX_CITIES) ? opponentCitiesOfInterest[k] : 0;
                        if (kInterest < 2) continue;
                        pairsChecked++;
                        
                        // Calculer l'intérêt combiné
                        int interest = jInterest + kInterest;
                        
                        // Check if this pair has more interest than any in our top 3
                        for (int t = 0; t < 3; t++) {
                            if (interest > topInterest[t]) {
                                // Shift everything down
                                for (int s = 2; s > t; s--) {
                                    topCityPairs[s][0] = topCityPairs[s-1][0];
                                    topCityPairs[s][1] = topCityPairs[s-1][1];
                                    topInterest[s] = topInterest[s-1];
                                }
                                
                                // Insert this pair
                                topCityPairs[t][0] = j;
                                topCityPairs[t][1] = k;
                                topInterest[t] = interest;
                                break;
                            }
                        }
                    }
                }
                
                // Now check if our route is on a path between any of these top interest pairs
                for (int p = 0; p < 3; p++) {
                    int cityA = topCityPairs[p][0];
                    int cityB = topCityPairs[p][1];
                    
                    // CORRECTION: Vérifier que les villes sont valides
                    if (cityA < 0 || cityA >= state->nbCities || cityB < 0 || cityB >= state->nbCities) {
                        continue;  // Skip invalid pairs
                    }
                    
                    int path[MAX_CITIES];
                    int pathLength = 0;
                    
                    int distance = findShortestPath(state, cityA, cityB, path, &pathLength);
                    
                    // CORRECTION: Vérifier que le chemin est valide et ne dépasse pas les limites
                    if (distance > 0 && pathLength > 0 && pathLength < MAX_CITIES) {
                        for (int s = 0; s < pathLength - 1; s++) {
                            // Vérifier que les indices des villes dans le chemin sont valides
                            if (path[s] < 0 || path[s] >= state->nbCities || 
                                path[s+1] < 0 || path[s+1] >= state->nbCities) {
                                continue;
                            }
                            
                            if ((path[s] == from && path[s+1] == to) || 
                                (path[s] == to && path[s+1] == from)) {
                                blockingValue += 15;
                                // Only count once
                                p = 3;  // Exit outer loop
                                break;
                            }
                        }
                    }
                }
            }
            
            // If this route has good blocking value, add it to our list
            if (blockingValue > 0) {
                blockingRoutes[count] = i;
                blockingPriorities[count] = blockingValue;
                
                // Only print a subset of blocking routes to avoid excessive logging
                if (count < 10 || blockingValue > 10) {
                    printf("Route %d has blocking value %d\n", i, blockingValue);
                }
                
                count++;
                
                if (count >= MAX_BLOCKING_ROUTES) {
                    printf("WARNING: Maximum blocking routes reached (%d)\n", MAX_BLOCKING_ROUTES);
                    break;
                }
            }
        }
    }
    
    // Set sentinel value
    if (count < MAX_BLOCKING_ROUTES) {
        blockingRoutes[count] = -1; // Null terminator for safety
    }
    
    return count;
}

/**
 * Checks if a route forms a critical bridge in the network
 */
bool isCriticalBridge(GameState* state, int city1, int city2) {
    // Safety checks
    if (city1 < 0 || city1 >= state->nbCities || city2 < 0 || city2 >= state->nbCities) {
        printf("ERROR: Invalid city indices in isCriticalBridge: %d, %d\n", city1, city2);
        return false;
    }
    
    // A bridge is a connection that, if removed, would disconnect the graph
    // We implement a simple approximation: if removing this route makes certain
    // cities unreachable, it's a bridge
    
    // Temporarily mark this route as owned by opponent
    int routeIndex = -1;
    for (int i = 0; i < state->nbTracks; i++) {
        if ((state->routes[i].from == city1 && state->routes[i].to == city2) ||
            (state->routes[i].from == city2 && state->routes[i].to == city1)) {
            routeIndex = i;
            break;
        }
    }
    
    if (routeIndex == -1) {
        return false;
    }
    
    int originalOwner = state->routes[routeIndex].owner;
    state->routes[routeIndex].owner = 2; // Temporarily mark as opponent-owned
    
    // Check if this creates a disconnection in the network
    bool isBridge = false;
    int connectedCities = 0;
    int totalCitiesToCheck = state->nbCities < 20 ? state->nbCities : 20; // Limit check to 20 cities
    
    // Count connected cities from city1
    for (int i = 0; i < totalCitiesToCheck; i++) {
        if (i != city1) {
            int path[MAX_CITIES];
            int pathLength = 0;
            int distance = findShortestPath(state, city1, i, path, &pathLength);
            
            if (distance > 0) {
                connectedCities++;
            }
        }
    }
    
    // If removing this route disconnects a significant portion of the board, it's a bridge
    if (connectedCities < totalCitiesToCheck / 3) {
        isBridge = true;
    }
    
    // Restore the original owner
    state->routes[routeIndex].owner = originalOwner;
    
    return isBridge;
}

/**
 * Checks if a route connects to the opponent's existing network
 */
bool isConnectedToOpponentNetwork(GameState* state, int city1, int city2) {
    // Safety checks
    if (city1 < 0 || city1 >= state->nbCities || city2 < 0 || city2 >= state->nbCities) {
        printf("ERROR: Invalid city indices in isConnectedToOpponentNetwork: %d, %d\n", city1, city2);
        return false;
    }
    
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 2) { // Opponent-owned
            if (state->routes[i].from == city1 || state->routes[i].from == city2 || 
                state->routes[i].to == city1 || state->routes[i].to == city2) {
                return true;
            }
        }
    }
    return false;
}

/**
 * Improved objective evaluation that considers early-game planning
 */
void improvedObjectiveEvaluation(GameState* state, Objective* objectives, bool* chooseObjectives) {
    // Cette fonction améliore la sélection des objectifs en analysant leur faisabilité
    
    printf("Advanced objective evaluation:\n");
    
    // Vérification des paramètres
    if (!state || !objectives || !chooseObjectives) {
        printf("ERROR: Invalid parameters in improvedObjectiveEvaluation\n");
        return;
    }
    
    // Initialiser les choix à false
    for (int i = 0; i < 3; i++) {
        chooseObjectives[i] = false;
    }
    
    // Score array for ranking objectives
    float scores[3];
    
    // Analysis of each objective
    for (int i = 0; i < 3; i++) {
        int from = objectives[i].from;
        int to = objectives[i].to;
        int value = objectives[i].score;
        
        // Safety check - ensure the cities are within bounds
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            printf("Objective %d: Invalid cities - From %d to %d, score %d\n", i+1, from, to, value);
            scores[i] = -1000; // Invalid objective
            continue;
        }
        
        printf("Objective %d: From %d to %d, score %d\n", i+1, from, to, value);
        
        // Find optimal path for this objective
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findShortestPath(state, from, to, path, &pathLength);
        
        if (distance < 0) {
            // No path found - impossible objective
            scores[i] = -1000;
            printf("  - No path available, objective impossible\n");
            continue;
        }
        
        // AJOUT: Pénaliser les objectifs avec des chemins très longs
        if (distance > 15) {
            printf("  - WARNING: Very long path (%d) for this objective\n", distance);
            scores[i] = -500;  // Forte pénalité pour les objectifs très difficiles
            continue;
        }
        
        // Analyze path complexity
        int routesNeeded = 0;
        int routesOwnedByUs = 0;
        int routesOwnedByOpponent = 0;
        int routesAvailable = 0;
        int totalLength = 0;
        
        for (int j = 0; j < pathLength - 1 && j < MAX_CITIES - 1; j++) {
            int pathFrom = path[j];
            int pathTo = path[j+1];
            
            // Vérification des limites
            if (pathFrom < 0 || pathFrom >= state->nbCities || 
                pathTo < 0 || pathTo >= state->nbCities) {
                printf("  - WARNING: Invalid cities in path\n");
                continue;
            }
            
            // Find route between these cities
            bool routeFound = false;
            
            for (int k = 0; k < state->nbTracks; k++) {
                if ((state->routes[k].from == pathFrom && state->routes[k].to == pathTo) ||
                    (state->routes[k].from == pathTo && state->routes[k].to == pathFrom)) {
                    
                    routeFound = true;
                    totalLength += state->routes[k].length;
                    
                    if (state->routes[k].owner == 0) {
                        routesAvailable++;
                    } else if (state->routes[k].owner == 1) {
                        routesOwnedByUs++;
                    } else if (state->routes[k].owner == 2) {
                        routesOwnedByOpponent++;
                        // Si l'adversaire possède une route, ce chemin est bloqué
                        printf("  - Path blocked by opponent\n");
                        scores[i] = -800;
                    }
                }
            }
            
            if (!routeFound) {
                // No route between these cities - shouldn't happen if findShortestPath worked
                scores[i] = -800;
                printf("  - Error: Path contains non-existent route\n");
            }
        }
        
        routesNeeded = routesAvailable + routesOwnedByUs;
        
        // If the objective is already blocked by opponent routes, skip it
        if (scores[i] < -100) {
            continue;
        }
        
        // Base score: points-to-length ratio
        float pointsPerLength = (totalLength > 0) ? (float)value / totalLength : 0;
        float baseScore = pointsPerLength * 50.0;
        
        // Completion progress: how many routes we already own
        float completionProgress = 0;
        if (routesNeeded > 0) {
            completionProgress = (float)routesOwnedByUs / routesNeeded;
        }
        
        // Card matching: how many cards we have that match needed routes
        float cardMatchScore = 0;
        int colorMatchCount = 0;
        
        for (int j = 0; j < pathLength - 1 && j < MAX_CITIES - 1; j++) {
            int pathFrom = path[j];
            int pathTo = path[j+1];
            
            // Vérification des limites
            if (pathFrom < 0 || pathFrom >= state->nbCities || 
                pathTo < 0 || pathTo >= state->nbCities) {
                continue;
            }
            
            for (int k = 0; k < state->nbTracks; k++) {
                if (((state->routes[k].from == pathFrom && state->routes[k].to == pathTo) ||
                     (state->routes[k].from == pathTo && state->routes[k].to == pathFrom)) &&
                    state->routes[k].owner == 0) {
                    
                    CardColor routeColor = state->routes[k].color;
                    int length = state->routes[k].length;
                    
                    if (routeColor != LOCOMOTIVE) {
                        if (state->nbCardsByColor[routeColor] >= length/2) {
                            colorMatchCount++;
                        }
                    } else {
                        // Gray route - check if we have enough of any color
                        for (int c = 1; c < 9; c++) {
                            if (state->nbCardsByColor[c] >= length/2) {
                                colorMatchCount++;
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        if (routesAvailable > 0) {
            cardMatchScore = (float)colorMatchCount / routesAvailable * 100.0;
        }
        
        // Synergy with other objectives
        float synergyScore = 0;
        
        // Check synergy with other potential objectives
        for (int j = 0; j < 3; j++) {
            if (i != j) {
                int otherFrom = objectives[j].from;
                int otherTo = objectives[j].to;
                
                // Skip invalid objectives
                if (otherFrom < 0 || otherFrom >= state->nbCities || 
                    otherTo < 0 || otherTo >= state->nbCities) {
                    continue;
                }
                
                // Shared endpoints
                if (from == otherFrom || from == otherTo || to == otherFrom || to == otherTo) {
                    synergyScore += 15;
                }
                
                // Shared routes
                int otherPath[MAX_CITIES];
                int otherPathLength = 0;
                
                if (findShortestPath(state, otherFrom, otherTo, otherPath, &otherPathLength) >= 0) {
                    int sharedRoutes = 0;
                    
                    for (int p1 = 0; p1 < pathLength - 1 && p1 < MAX_CITIES - 1; p1++) {
                        for (int p2 = 0; p2 < otherPathLength - 1 && p2 < MAX_CITIES - 1; p2++) {
                            if ((path[p1] == otherPath[p2] && path[p1+1] == otherPath[p2+1]) ||
                                (path[p1] == otherPath[p2+1] && path[p1+1] == otherPath[p2])) {
                                sharedRoutes++;
                            }
                        }
                    }
                    
                    synergyScore += sharedRoutes * 10;
                }
            }
        }
        
        // Competition with opponent (risk assessment)
        float competitionPenalty = 0;
        
        for (int j = 0; j < pathLength - 1 && j < MAX_CITIES - 1; j++) {
            int pathFrom = path[j];
            int pathTo = path[j+1];
            
            // Check if opponent is active near these cities (with bounds checking)
            if (pathFrom < MAX_CITIES && pathTo < MAX_CITIES && 
                (opponentCitiesOfInterest[pathFrom] > 0 || opponentCitiesOfInterest[pathTo] > 0)) {
                // Calculate penalty safely
                int fromPenalty = (pathFrom < MAX_CITIES) ? opponentCitiesOfInterest[pathFrom] : 0;
                int toPenalty = (pathTo < MAX_CITIES) ? opponentCitiesOfInterest[pathTo] : 0;
                competitionPenalty += (fromPenalty + toPenalty) * 2;
            }
        }
        
        // Difficulty penalty: harder objectives have lower scores
        float difficultyPenalty = 0;
        if (routesNeeded > 5) {
            difficultyPenalty = (routesNeeded - 5) * 10;
        }
        
        // AJOUT: Pénalité pour les objectifs avec des chemins longs
        float lengthPenalty = 0;
        if (distance > 10) {
            lengthPenalty = (distance - 10) * 8;
        }
        
        // Calculate final score with the new length penalty
        scores[i] = baseScore + (completionProgress * 100) + cardMatchScore + synergyScore 
                  - competitionPenalty - difficultyPenalty - lengthPenalty;
        
        // Log components for debugging
        printf("  - Base score (points/length): %.1f\n", baseScore);
        printf("  - Completion: %.1f%%\n", completionProgress * 100);
        printf("  - Card matching: %.1f\n", cardMatchScore);
        printf("  - Synergy: %.1f\n", synergyScore);
        printf("  - Competition: -%.1f\n", competitionPenalty);
        printf("  - Difficulty: -%.1f\n", difficultyPenalty);
        printf("  - Length penalty: -%.1f\n", lengthPenalty);
        printf("  - FINAL SCORE: %.1f\n", scores[i]);
    }
    
    // Sort objectives by score
    int sortedIndices[3] = {0, 1, 2};
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2 - i; j++) {
            if (scores[sortedIndices[j]] < scores[sortedIndices[j+1]]) {
                int temp = sortedIndices[j];
                sortedIndices[j] = sortedIndices[j+1];
                sortedIndices[j+1] = temp;
            }
        }
    }
    
    // Selection logic based on game phase
    int phase = determineGamePhase(state);
    int numToChoose = 0;
    
    // Take the highest scoring objective if it's reasonable
    if (scores[sortedIndices[0]] > -100) {  // MODIFICATION: Accepter même les objectifs moins bien notés
        chooseObjectives[sortedIndices[0]] = true;
        numToChoose++;
    }
    
    // Adaptive selection based on game phase
    if (phase == PHASE_EARLY) {
        // Early game: more aggressive with taking objectives
        if (scores[sortedIndices[1]] > 30) {
            chooseObjectives[sortedIndices[1]] = true;
            numToChoose++;
        }
        
        if (scores[sortedIndices[2]] > 60) {
            chooseObjectives[sortedIndices[2]] = true;
            numToChoose++;
        }
    } 
    else if (phase == PHASE_MIDDLE) {
        // Middle game: balanced approach
        if (scores[sortedIndices[1]] > 80) {
            chooseObjectives[sortedIndices[1]] = true;
            numToChoose++;
        }
    }
    else {
        // Late game: conservative, take only high-value objectives
        if (scores[sortedIndices[1]] > 150) {
            chooseObjectives[sortedIndices[1]] = true;
            numToChoose++;
        }
    }
    
    // CORRECTION: Forcer à prendre au moins un objectif, peu importe le score
    if (numToChoose == 0) {
        chooseObjectives[sortedIndices[0]] = true;
        numToChoose = 1;
        printf("FORCING selection of objective %d even with low score %.1f\n", 
              sortedIndices[0] + 1, scores[sortedIndices[0]]);
    }
    
    // MODIFICATION: Être plus sélectif avec les objectifs
    // Ne pas prendre plus de 3 objectifs, surtout si certains sont difficiles
    int totalChosenObjectives = state->nbObjectives;  // Objectifs déjà choisis
    int maxAdditionalObjectives = 3 - totalChosenObjectives;
    
    if (maxAdditionalObjectives <= 0) {
        // Déjà trop d'objectifs, n'en prendre qu'un seul si très attractif
        if (scores[sortedIndices[0]] > 100) {
            // Garder uniquement le meilleur
            chooseObjectives[sortedIndices[0]] = true;
            chooseObjectives[sortedIndices[1]] = false;
            chooseObjectives[sortedIndices[2]] = false;
            numToChoose = 1;
        } else {
            // On doit quand même prendre au moins un objectif
            chooseObjectives[sortedIndices[0]] = true;
            chooseObjectives[sortedIndices[1]] = false;
            chooseObjectives[sortedIndices[2]] = false;
            numToChoose = 1;
        }
    } else {
        // Prendre au maximum maxAdditionalObjectives
        if (numToChoose > maxAdditionalObjectives) {
            // Garder uniquement les meilleurs
            for (int i = maxAdditionalObjectives; i < 3; i++) {
                if (i < numToChoose) {
                    int indexToRemove = sortedIndices[i];
                    chooseObjectives[indexToRemove] = false;
                }
            }
            numToChoose = maxAdditionalObjectives;
        }
    }
    
    // VÉRIFICATION FINALE: S'assurer qu'au moins un objectif est sélectionné
    bool anySelected = false;
    for (int i = 0; i < 3; i++) {
        if (chooseObjectives[i]) {
            anySelected = true;
            break;
        }
    }
    
    if (!anySelected) {
        // Si aucun objectif n'est sélectionné, forcer la sélection du premier
        chooseObjectives[0] = true;
        numToChoose = 1;
        printf("EMERGENCY FIX: No objectives were selected! Forcing selection of objective 1\n");
    }
    
    printf("Choosing %d objectives: ", numToChoose);
    for (int i = 0; i < 3; i++) {
        if (chooseObjectives[i]) {
            printf("%d ", i+1);
        }
    }
    printf("\n");
}

/**
 * Advanced route prioritization that considers objectives, blocking, and card efficiency
 */
/**
 * Advanced route prioritization that considers objectives, blocking, and card efficiency
 * With memory optimizations to prevent stack overflow
 */
/**
 * Advanced route prioritization that considers objectives, blocking, and card efficiency
 * With memory optimizations to prevent stack overflow
 */
void advancedRoutePrioritization(GameState* state, int* possibleRoutes, CardColor* possibleColors, 
    int* possibleLocomotives, int numPossibleRoutes) {

    if (numPossibleRoutes == 0) {
        return;
    }

    // CORRECTION: Sécuriser la fonction avec des vérifications supplémentaires
    if (!state || !possibleRoutes || !possibleColors || !possibleLocomotives) {
        printf("ERROR: Invalid parameters in advancedRoutePrioritization\n");
        return;
    }

    // Safety check - limit the number of routes to process
    const int MAX_ROUTES_TO_SORT = 50;
    if (numPossibleRoutes > MAX_ROUTES_TO_SORT) {
        printf("WARNING: Too many possible routes (%d), limiting to %d\n", 
            numPossibleRoutes, MAX_ROUTES_TO_SORT);
        numPossibleRoutes = MAX_ROUTES_TO_SORT;
    }

    // Create a copy of the arrays to avoid modifying the originals
    int routes[MAX_ROUTES_TO_SORT];
    CardColor colors[MAX_ROUTES_TO_SORT];
    int locomotives[MAX_ROUTES_TO_SORT];
    int scores[MAX_ROUTES_TO_SORT];

    // Initialize all scores to 0
    for (int i = 0; i < MAX_ROUTES_TO_SORT; i++) {
        scores[i] = 0;
        routes[i] = -1;  // Marquer comme invalide par défaut
    }

    // Create a copy of the input arrays to avoid modifying originals during sorting
    for (int i = 0; i < numPossibleRoutes && i < MAX_ROUTES_TO_SORT; i++) {
        // CORRECTION: Vérification des indices valides
        if (possibleRoutes[i] >= 0 && possibleRoutes[i] < state->nbTracks) {
            routes[i] = possibleRoutes[i];
            colors[i] = possibleColors[i];
            locomotives[i] = possibleLocomotives[i];
        } else {
            printf("WARNING: Invalid route index %d in advancedRoutePrioritization\n", possibleRoutes[i]);
            routes[i] = -1;  // Marquer comme invalide
            scores[i] = -1000;  // Score très bas pour le mettre à la fin
        }
    }

    // 1. Find critical blocking routes - limit to a reasonable number
    int blockingRoutes[MAX_ROUTES_TO_SORT];
    int blockingPriorities[MAX_ROUTES_TO_SORT];
    
    // CORRECTION: Initialiser les tableaux pour éviter les valeurs aléatoires
    for (int i = 0; i < MAX_ROUTES_TO_SORT; i++) {
        blockingRoutes[i] = -1;
        blockingPriorities[i] = 0;
    }
    
    int numBlockingRoutes = findCriticalBlockingRoutes(state, blockingRoutes, blockingPriorities);

    // Limit blocking routes to process
    if (numBlockingRoutes > 20) {
        numBlockingRoutes = 20;
    }

    // 2. Evaluate each route's utility
    for (int i = 0; i < numPossibleRoutes && i < MAX_ROUTES_TO_SORT; i++) {
        int routeIndex = routes[i];

        // Skip invalid routes
        if (routeIndex < 0 || routeIndex >= state->nbTracks) {
            scores[i] = -1000; // Very low score to place at the end of sorting
            continue;
        }

        // Start with the basic utility
        int baseScore = enhancedEvaluateRouteUtility(state, routeIndex);
        scores[i] = baseScore;

        // Check if this is a blocking route - but limit the search
        for (int j = 0; j < numBlockingRoutes && j < MAX_ROUTES_TO_SORT; j++) {
            if (blockingRoutes[j] == routeIndex) {
                // Add blocking value to score
                scores[i] += blockingPriorities[j];
                break;
            }
        }

        // Phase-specific adjustments
        int phase = determineGamePhase(state);

        if (phase == PHASE_EARLY) {
            // In early game, prioritize routes that start objective completion
            int objectiveBonus = 0;

            // Only check a limited number of objectives to prevent excessive computation
            int maxObjectivesToCheck = state->nbObjectives < 5 ? state->nbObjectives : 5;

            for (int j = 0; j < maxObjectivesToCheck; j++) {
                // CORRECTION: Vérifier les indices valides
                if (j >= state->nbObjectives) break;
                
                if (!isObjectiveCompleted(state, state->objectives[j])) {
                    // Find if this route starts a path to the objective
                    int from = state->routes[routeIndex].from;
                    int to = state->routes[routeIndex].to;

                    // CORRECTION: Vérification explicite des limites
                    if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
                        printf("WARNING: Invalid cities in route %d: from %d to %d\n", routeIndex, from, to);
                        continue;
                    }

                    // Check if this route connects to one of the objective cities
                    int objFrom = state->objectives[j].from;
                    int objTo = state->objectives[j].to;

                    // CORRECTION: Vérification explicite des limites
                    if (objFrom < 0 || objFrom >= state->nbCities || objTo < 0 || objTo >= state->nbCities) {
                        printf("WARNING: Invalid cities in objective %d: from %d to %d\n", j, objFrom, objTo);
                        continue;
                    }

                    if (objFrom == from || objTo == from || objFrom == to || objTo == to) {
                        objectiveBonus += state->objectives[j].score * 3;
                    }
                }
            }
            scores[i] += objectiveBonus;
        }
        else if (phase == PHASE_LATE || phase == PHASE_FINAL) {
            // In late game, prioritize routes that complete objectives
            scores[i] += calculateObjectiveProgress(state, routeIndex) * 2;

            // Also prioritize longer routes for more points
            int length = state->routes[routeIndex].length;
            if (length >= 4) {
                scores[i] += length * 5;
            }
        }

        // Final adjustment: card efficiency
        // Routes that efficiently use our cards get a bonus
        scores[i] += evaluateCardEfficiency(state, routeIndex) * 3;
    }

    // CORRECTION: Algorithme de tri optimisé et sécurisé (bubble sort)
    for (int i = 0; i < numPossibleRoutes - 1 && i < MAX_ROUTES_TO_SORT - 1; i++) {
        for (int j = 0; j < numPossibleRoutes - i - 1 && j < MAX_ROUTES_TO_SORT - 1; j++) {
            if (scores[j] < scores[j+1]) {
                // Swap routes
                int tempRoute = routes[j];
                routes[j] = routes[j+1];
                routes[j+1] = tempRoute;

                // Swap colors
                CardColor tempColor = colors[j];
                colors[j] = colors[j+1];
                colors[j+1] = tempColor;

                // Swap locomotives
                int tempLoco = locomotives[j];
                locomotives[j] = locomotives[j+1];
                locomotives[j+1] = tempLoco;

                // Swap scores
                int tempScore = scores[j];
                scores[j] = scores[j+1];
                scores[j+1] = tempScore;
            }
        }
    }

    // Print top routes - limit logging to prevent console spam
    printf("Routes sorted by advanced priority:\n");
    int routesToShow = numPossibleRoutes < 5 ? numPossibleRoutes : 5;
    int uniqueRoutesShown = 0;

    for (int i = 0; i < numPossibleRoutes && uniqueRoutesShown < routesToShow && i < MAX_ROUTES_TO_SORT; i++) {
        int routeIndex = routes[i];

        // Skip invalid routes
        if (routeIndex < 0 || routeIndex >= state->nbTracks) {
            continue;
        }

        int from = state->routes[routeIndex].from;
        int to = state->routes[routeIndex].to;

        // Vérifier que les villes sont valides
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            printf("WARNING: Invalid cities in route %d: from %d to %d\n", routeIndex, from, to);
            continue;
        }

        printf("  %d. From %d to %d, score: %d\n", 
            uniqueRoutesShown+1, from, to, scores[i]);
        uniqueRoutesShown++;
    }

    // Copy the sorted routes back to the original arrays
    for (int i = 0; i < numPossibleRoutes && i < MAX_ROUTES_TO_SORT; i++) {
        possibleRoutes[i] = routes[i];
        possibleColors[i] = colors[i];
        possibleLocomotives[i] = locomotives[i];
    }
}

/**
 * Enhanced AI that combines all the improved strategies
 */
int superAdvancedStrategy(GameState* state, MoveData* moveData) {
    printf("Using super advanced strategy\n");
    
    // CORRECTION: Vérification des paramètres
    if (!state || !moveData) {
        printf("ERROR: Invalid parameters in superAdvancedStrategy\n");
        return 0;
    }
    
    // Determine the current game phase
    int phase = determineGamePhase(state);
    printf("Current game phase: %d\n", phase);
    
    // Increment turn counter
    state->turnCount++;
    
    // AJOUT: Analyser l'état des chemins pour les objectifs
    checkObjectivesPaths(state);
    
    // AJOUT: Afficher la matrice de connectivité pour le débogage
    printConnectivityMatrix(state);
    
    // Find possible routes
    int possibleRoutes[MAX_ROUTES] = {0};
    CardColor possibleColors[MAX_ROUTES] = {0};
    int possibleLocomotives[MAX_ROUTES] = {0};
    
    // CORRECTION: Initialiser à -1 pour identifier les routes invalides
    for (int i = 0; i < MAX_ROUTES; i++) {
        possibleRoutes[i] = -1;
    }
    
    // Trouver les routes possibles
    int numPossibleRoutes = findPossibleRoutes(state, possibleRoutes, possibleColors, possibleLocomotives);
    printf("Found %d possible routes to claim\n", numPossibleRoutes);
    
    // Safety check - don't exceed array bounds
    if (numPossibleRoutes > MAX_ROUTES - 1) {
        printf("WARNING: Too many possible routes (%d), limiting to %d\n", 
              numPossibleRoutes, MAX_ROUTES - 1);
        numPossibleRoutes = MAX_ROUTES - 1;
    }
    
    // CORRECTION: Vérifier que nous avons des objectifs
    if (state->nbObjectives == 0) {
        moveData->action = DRAW_OBJECTIVES;
        printf("Strategy decided: draw new objectives (we have none)\n");
        return 1;
    }
    
    // Check objective completion status
    int completedObjectives = 0;
    int totalObjectiveScore = 0;
    for (int i = 0; i < state->nbObjectives; i++) {
        // Vérifier que l'objectif est valide
        if (state->objectives[i].from < 0 || state->objectives[i].from >= state->nbCities ||
            state->objectives[i].to < 0 || state->objectives[i].to >= state->nbCities) {
            printf("WARNING: Invalid objective %d: from %d to %d\n", i, 
                  state->objectives[i].from, state->objectives[i].to);
            continue;
        }
        
        totalObjectiveScore += state->objectives[i].score;
        if (isObjectiveCompleted(state, state->objectives[i])) {
            completedObjectives++;
        }
    }
    int incompleteObjectives = state->nbObjectives - completedObjectives;
    
    printf("Objectives: %d completed, %d incomplete, total score: %d\n",
          completedObjectives, incompleteObjectives, totalObjectiveScore);
    
    // AJOUT: Vérifier si un objectif n'a besoin que d'une seule route pour être complété
    bool objectiveNearlyCompleted = false;
    int nearlyCompletedObjective = -1;
    int routeForNearlyCompletedObjective = -1;

    for (int i = 0; i < state->nbObjectives; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            int objFrom = state->objectives[i].from;
            int objTo = state->objectives[i].to;
            
            // Skip invalid objectives
            if (objFrom < 0 || objFrom >= state->nbCities || 
                objTo < 0 || objTo >= state->nbCities) {
                continue;
            }
            
            // Trouver le chemin
            int path[MAX_CITIES];
            int pathLength = 0;
            if (findShortestPath(state, objFrom, objTo, path, &pathLength) > 0) {
                int missingRoutes = 0;
                int lastMissingRouteFrom = -1;
                int lastMissingRouteTo = -1;
                
                // Compter les routes manquantes
                for (int j = 0; j < pathLength - 1 && j < MAX_CITIES - 1; j++) {
                    int cityA = path[j];
                    int cityB = path[j+1];
                    bool routeFound = false;
                    
                    // Skip invalid cities
                    if (cityA < 0 || cityA >= state->nbCities || 
                        cityB < 0 || cityB >= state->nbCities) {
                        continue;
                    }
                    
                    // Chercher si nous avons déjà cette route
                    for (int k = 0; k < state->nbTracks; k++) {
                        if (((state->routes[k].from == cityA && state->routes[k].to == cityB) ||
                             (state->routes[k].from == cityB && state->routes[k].to == cityA)) &&
                            state->routes[k].owner == 1) {  // Route nous appartenant
                            routeFound = true;
                            break;
                        }
                    }
                    
                    if (!routeFound) {
                        missingRoutes++;
                        lastMissingRouteFrom = cityA;
                        lastMissingRouteTo = cityB;
                    }
                }
                
                // Si une seule route manque, c'est prioritaire
                if (missingRoutes == 1) {
                    printf("PRIORITY: Objective %d needs just one more route to complete!\n", i+1);
                    objectiveNearlyCompleted = true;
                    nearlyCompletedObjective = i;
                    
                    // Chercher cette route parmi les routes possibles
                    for (int r = 0; r < numPossibleRoutes; r++) {
                        int routeIndex = possibleRoutes[r];
                        
                        if (routeIndex >= 0 && routeIndex < state->nbTracks) {
                            int from = state->routes[routeIndex].from;
                            int to = state->routes[routeIndex].to;
                            
                            if ((from == lastMissingRouteFrom && to == lastMissingRouteTo) || 
                                (from == lastMissingRouteTo && to == lastMissingRouteFrom)) {
                                routeForNearlyCompletedObjective = r;
                                printf("Found missing route for objective %d: from %d to %d\n",
                                       i+1, from, to);
                                break;
                            }
                        }
                    }
                    
                    if (routeForNearlyCompletedObjective >= 0) {
                        break;  // Sortir de la boucle, nous avons trouvé une priorité
                    } else {
                        printf("WARNING: Missing route not found among possible routes!\n");
                        objectiveNearlyCompleted = false;  // Reset if we can't find the route
                    }
                }
            }
        }
    }
    
    // Si un objectif n'a besoin que d'une seule route, c'est prioritaire
    if (objectiveNearlyCompleted && routeForNearlyCompletedObjective >= 0) {
        // Mettre cette route en première position
        int tempRoute = possibleRoutes[0];
        CardColor tempColor = possibleColors[0];
        int tempLoco = possibleLocomotives[0];
        
        possibleRoutes[0] = possibleRoutes[routeForNearlyCompletedObjective];
        possibleColors[0] = possibleColors[routeForNearlyCompletedObjective];
        possibleLocomotives[0] = possibleLocomotives[routeForNearlyCompletedObjective];
        
        possibleRoutes[routeForNearlyCompletedObjective] = tempRoute;
        possibleColors[routeForNearlyCompletedObjective] = tempColor;
        possibleLocomotives[routeForNearlyCompletedObjective] = tempLoco;
        
        printf("PRIORITIZED ROUTE for nearly completed objective!\n");
    }
    else if (numPossibleRoutes > 0) {
        // PRIORITIZATION: Routes that help with objectives
        // First, find the best route for objectives
        int bestRouteForObjectives = -1;
        int maxObjectiveProgress = -1;
        
        for (int i = 0; i < numPossibleRoutes; i++) {
            if (possibleRoutes[i] < 0 || possibleRoutes[i] >= state->nbTracks) {
                continue;  // Skip invalid routes
            }
            
            int progress = calculateObjectiveProgress(state, possibleRoutes[i]);
            if (progress > maxObjectiveProgress) {
                maxObjectiveProgress = progress;
                bestRouteForObjectives = i;
            }
        }
        
        // If a good route for objectives is found, prioritize it
        if (bestRouteForObjectives >= 0 && maxObjectiveProgress > 0) {
            printf("Found route with high objective value (progress: %d)\n", maxObjectiveProgress);
            
            // Put this route at the front of the array
            int tempRoute = possibleRoutes[0];
            CardColor tempColor = possibleColors[0];
            int tempLoco = possibleLocomotives[0];
            
            possibleRoutes[0] = possibleRoutes[bestRouteForObjectives];
            possibleColors[0] = possibleColors[bestRouteForObjectives];
            possibleLocomotives[0] = possibleLocomotives[bestRouteForObjectives];
            
            possibleRoutes[bestRouteForObjectives] = tempRoute;
            possibleColors[bestRouteForObjectives] = tempColor;
            possibleLocomotives[bestRouteForObjectives] = tempLoco;
        }
        
        // Still use advancedRoutePrioritization for additional sorting
        advancedRoutePrioritization(state, possibleRoutes, possibleColors, possibleLocomotives, numPossibleRoutes);
    }
    
    // Decision making based on game phase
    bool shouldClaimRoute = false;
    
    // Si nous avons une route prioritaire pour un objectif presque complété
    if (objectiveNearlyCompleted && routeForNearlyCompletedObjective >= 0 && numPossibleRoutes > 0) {
        shouldClaimRoute = true;
        printf("Priority decision: claim route for nearly completed objective!\n");
    }
    // EARLY GAME STRATEGY
    else if (phase == PHASE_EARLY) {
        // MODIFIED: In early game, prioritize routes that help with objectives
        
        // If we have few cards, draw more unless there's a high-value objective route
        if (state->nbCards < 7 && numPossibleRoutes > 0) {
            // Make sure the index is valid
            if (possibleRoutes[0] >= 0 && possibleRoutes[0] < state->nbTracks) {
                int objectiveProgress = calculateObjectiveProgress(state, possibleRoutes[0]);
                
                // Always claim routes that help significantly with objectives
                if (objectiveProgress > 30) {
                    shouldClaimRoute = true;
                    printf("Early game: claiming high-objective-value route (progress: %d)\n", objectiveProgress);
                } else {
                    printf("Early game: building hand (only %d cards)\n", state->nbCards);
                }
            }
        }
        // Claim route if we have enough cards for good routes
        else if (numPossibleRoutes > 0) {
            shouldClaimRoute = true;
            printf("Early game: good card count (%d), claiming route\n", state->nbCards);
        }
        
        // Consider drawing objectives if we have room and don't need to claim a route yet
        if (state->nbObjectives < 3 && !shouldClaimRoute) {
            moveData->action = DRAW_OBJECTIVES;
            printf("Early game: drawing more objectives (only have %d)\n", state->nbObjectives);
            return 1;
        }
    }
    // MIDDLE GAME STRATEGY
    else if (phase == PHASE_MIDDLE) {
        // MODIFIED: In middle game, focus heavily on completing objectives
        
        if (numPossibleRoutes > 0) {
            // Make sure the index is valid
            if (possibleRoutes[0] >= 0 && possibleRoutes[0] < state->nbTracks) {
                int objectiveProgress = calculateObjectiveProgress(state, possibleRoutes[0]);
                
                // If this route helps with objectives, always take it
                if (objectiveProgress > 0) {
                    shouldClaimRoute = true;
                    printf("Middle game: claiming route for objective progress (%d)\n", objectiveProgress);
                }
                // Otherwise, evaluate other factors
                else {
                    int bestRouteUtility = enhancedEvaluateRouteUtility(state, possibleRoutes[0]);
                    
                    // Check if this route blocks opponent
                    bool isBlockingRoute = false;
                    int from = state->routes[possibleRoutes[0]].from;
                    int to = state->routes[possibleRoutes[0]].to;
                    
                    // Vérification des limites
                    int fromInterest = (from < MAX_CITIES) ? opponentCitiesOfInterest[from] : 0;
                    int toInterest = (to < MAX_CITIES) ? opponentCitiesOfInterest[to] : 0;
                    
                    if (fromInterest + toInterest > 3) {
                        isBlockingRoute = true;
                    }
                    
                    // Decide whether to claim based on utility or blocking value
                    if (bestRouteUtility > 30 || isBlockingRoute) {
                        shouldClaimRoute = true;
                        if (isBlockingRoute) {
                            printf("Middle game: blocking opponent's potential route\n");
                        } else {
                            printf("Middle game: claiming high-value route\n");
                        }
                    } else {
                        printf("Middle game: no valuable routes, drawing cards\n");
                    }
                }
            }
        }
        
        // If all objectives are complete, consider drawing more (unless in late game)
        if (incompleteObjectives == 0 && state->nbObjectives < 5 && !shouldClaimRoute) {
            moveData->action = DRAW_OBJECTIVES;
            printf("Middle game: all objectives complete, drawing more\n");
            return 1;
        }
    }
    // LATE GAME STRATEGY
    else if (phase == PHASE_LATE) {
        // MODIFIED: In late game, top priority is completing remaining objectives
        
        if (numPossibleRoutes > 0) {
            // Make sure the index is valid
            if (possibleRoutes[0] >= 0 && possibleRoutes[0] < state->nbTracks) {
                int objectiveProgress = calculateObjectiveProgress(state, possibleRoutes[0]);
                int length = state->routes[possibleRoutes[0]].length;
                
                // In late game, prioritize completing objectives over long routes
                if (objectiveProgress > 0) {
                    shouldClaimRoute = true;
                    printf("Late game: claiming route for objective progress (%d)\n", objectiveProgress);
                }
                // If no objective progress, take long routes for points
                else if (length >= 4) {
                    shouldClaimRoute = true;
                    printf("Late game: claiming long route for points\n");
                }
                // Otherwise claim if we have excess cards
                else if (state->nbCards > 8) {
                    shouldClaimRoute = true;
                    printf("Late game: using excess cards to claim route\n");
                }
            }
        }
    }
    // FINAL PHASE STRATEGY
    else if (phase == PHASE_FINAL) {
        // MODIFIED: In final phase, desperately try to complete objectives
       if (numPossibleRoutes > 0) {
           // D'abord, trouver les objectifs les plus proches d'être complétés
           int bestObjectiveIndex = -1;
           int leastMissingRoutes = INT_MAX;
           
           // Pour chaque objectif non complété
           for (int i = 0; i < state->nbObjectives; i++) {
               if (isObjectiveCompleted(state, state->objectives[i])) {
                   continue;  // Ignorer les objectifs déjà complétés
               }
               
               int objFrom = state->objectives[i].from;
               int objTo = state->objectives[i].to;
               
               // Vérification des limites
               if (objFrom < 0 || objFrom >= state->nbCities || 
                   objTo < 0 || objTo >= state->nbCities) {
                   continue;
               }
               
               // Trouver le chemin
               int path[MAX_CITIES];
               int pathLength = 0;
               if (findShortestPath(state, objFrom, objTo, path, &pathLength) > 0) {
                   int missingRoutes = 0;
                   
                   // Compter les routes manquantes
                   for (int j = 0; j < pathLength - 1; j++) {
                       int cityA = path[j];
                       int cityB = path[j+1];
                       bool routeFound = false;
                       
                       // Vérification des limites
                       if (cityA < 0 || cityA >= state->nbCities || 
                           cityB < 0 || cityB >= state->nbCities) {
                           continue;
                       }
                       
                       // Chercher si nous avons déjà cette route
                       for (int k = 0; k < state->nbTracks; k++) {
                           if (((state->routes[k].from == cityA && state->routes[k].to == cityB) ||
                                (state->routes[k].from == cityB && state->routes[k].to == cityA)) &&
                               state->routes[k].owner == 1) {  // Route nous appartenant
                               routeFound = true;
                               break;
                           }
                       }
                       
                       if (!routeFound) {
                           missingRoutes++;
                       }
                   }
                   
                   // Si cet objectif est plus proche d'être complété
                   if (missingRoutes < leastMissingRoutes && missingRoutes > 0) {
                       leastMissingRoutes = missingRoutes;
                       bestObjectiveIndex = i;
                   }
               }
           }
           
           // Si nous avons trouvé un objectif proche d'être complété
           if (bestObjectiveIndex >= 0 && leastMissingRoutes <= 2) {  // Au plus 2 routes manquantes
               int objFrom = state->objectives[bestObjectiveIndex].from;
               int objTo = state->objectives[bestObjectiveIndex].to;
               int objScore = state->objectives[bestObjectiveIndex].score;
               
               printf("Final phase: prioritizing objective %d (from %d to %d, score %d) with only %d missing routes\n",
                      bestObjectiveIndex + 1, objFrom, objTo, objScore, leastMissingRoutes);
               
               // Trouver les routes manquantes
               int path[MAX_CITIES];
               int pathLength = 0;
               findShortestPath(state, objFrom, objTo, path, &pathLength);
               
               // Pour chaque segment du chemin
               for (int j = 0; j < pathLength - 1; j++) {
                   int cityA = path[j];
                   int cityB = path[j+1];
                   bool routeClaimed = false;
                   
                   // Vérification des limites
                   if (cityA < 0 || cityA >= state->nbCities || 
                       cityB < 0 || cityB >= state->nbCities) {
                       continue;
                   }
                   
                   // Vérifier si cette route est déjà prise
                   for (int k = 0; k < state->nbTracks; k++) {
                       if (((state->routes[k].from == cityA && state->routes[k].to == cityB) ||
                            (state->routes[k].from == cityB && state->routes[k].to == cityA)) &&
                           state->routes[k].owner == 1) {
                           routeClaimed = true;
                           break;
                       }
                   }
                   
                   // Si cette route n'est pas encore prise, chercher si elle est disponible
                   if (!routeClaimed) {
                       // Chercher cette route parmi les routes possibles
                       for (int r = 0; r < numPossibleRoutes; r++) {
                           int routeIndex = possibleRoutes[r];
                           
                           if (routeIndex >= 0 && routeIndex < state->nbTracks) {
                               int from = state->routes[routeIndex].from;
                               int to = state->routes[routeIndex].to;
                               
                               if ((from == cityA && to == cityB) || (from == cityB && to == cityA)) {
                                   // Mettre cette route en première position
                                   int tempRoute = possibleRoutes[0];
                                   CardColor tempColor = possibleColors[0];
                                   int tempLoco = possibleLocomotives[0];
                                   
                                   possibleRoutes[0] = possibleRoutes[r];
                                   possibleColors[0] = possibleColors[r];
                                   possibleLocomotives[0] = possibleLocomotives[r];
                                   
                                   possibleRoutes[r] = tempRoute;
                                   possibleColors[r] = tempColor;
                                   possibleLocomotives[r] = tempLoco;
                                   
                                   printf("Found missing route for objective %d: from %d to %d\n",
                                          bestObjectiveIndex + 1, from, to);
                                   break;
                               }
                           }
                       }
                   }
               }
           }
           else {
               printf("Final phase: no objectives close to completion, maximizing points\n");
           }
           
           shouldClaimRoute = true;
       }
   }
   
   // If we decided to claim a route
   if (numPossibleRoutes > 0 && shouldClaimRoute) {
       // Make sure the index is valid
       if (possibleRoutes[0] >= 0 && possibleRoutes[0] < state->nbTracks) {
           // Take the highest utility route (first after sorting)
           int routeIndex = possibleRoutes[0];
           
           // CORRECTION: Vérifier que les villes sont valides
           int from = state->routes[routeIndex].from;
           int to = state->routes[routeIndex].to;
           
           if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
               printf("ERROR: Invalid cities in route %d: from %d to %d\n", routeIndex, from, to);
               // Choisir une action alternative
               moveData->action = DRAW_BLIND_CARD;
               printf("Strategy decided: draw blind card (invalid route)\n");
               return 1;
           }
           
           CardColor color = possibleColors[0];
           int nbLocomotives = possibleLocomotives[0];
           
           // CORRECTION: Vérifier que la couleur est valide
           if (color < 0 || color > 9) {
               printf("ERROR: Invalid color %d for route %d\n", color, routeIndex);
               // Choisir une action alternative
               moveData->action = DRAW_BLIND_CARD;
               printf("Strategy decided: draw blind card (invalid color)\n");
               return 1;
           }
           
           // CORRECTION: Vérifier que nous avons assez de cartes
           int colorCards = state->nbCardsByColor[color];
           int locomotives = state->nbCardsByColor[LOCOMOTIVE];
           int length = state->routes[routeIndex].length;
           
           if (color == LOCOMOTIVE) {
               if (locomotives < length) {
                   printf("ERROR: Not enough locomotives for route %d (need %d, have %d)\n", 
                         routeIndex, length, locomotives);
                   // Choisir une action alternative
                   moveData->action = DRAW_BLIND_CARD;
                   printf("Strategy decided: draw blind card (not enough locomotives)\n");
                   return 1;
               }
           } else {
               if (colorCards + locomotives < length) {
                   printf("ERROR: Not enough cards for route %d (need %d, have %d %s and %d locomotives)\n", 
                         routeIndex, length, colorCards, 
                         (color < 10) ? (const char*[]){
                             "None", "Purple", "White", "Blue", "Yellow", 
                             "Orange", "Black", "Red", "Green", "Locomotive"
                         }[color] : "Unknown",
                         locomotives);
                   // Choisir une action alternative
                   moveData->action = DRAW_BLIND_CARD;
                   printf("Strategy decided: draw blind card (not enough cards)\n");
                   return 1;
               }
               
               // CORRECTION: Vérifier que le nombre de locomotives est valide
               if (nbLocomotives > locomotives) {
                   printf("ERROR: Invalid locomotive count for route %d (need %d, have %d)\n", 
                         routeIndex, nbLocomotives, locomotives);
                   // Réajuster le nombre de locomotives
                   if (locomotives > 0) {
                       nbLocomotives = (length - colorCards > 0) ? 
                           (length - colorCards < locomotives ? length - colorCards : locomotives) : 0;
                       printf("Adjusted locomotive count to %d\n", nbLocomotives);
                   } else {
                       // Pas de locomotives disponibles
                       // Choisir une action alternative
                       moveData->action = DRAW_BLIND_CARD;
                       printf("Strategy decided: draw blind card (not enough locomotives)\n");
                       return 1;
                   }
               }
           }
           
           // CORRECTION: Vérifier que la route est de la bonne couleur
           CardColor routeColor = state->routes[routeIndex].color;
           CardColor routeSecondColor = state->routes[routeIndex].secondColor;
           
           if (routeColor != LOCOMOTIVE && color != LOCOMOTIVE && color != routeColor && 
               (routeSecondColor == NONE || color != routeSecondColor)) {
               printf("ERROR: Invalid color %d for route %d (route colors: %d, %d)\n", 
                     color, routeIndex, routeColor, routeSecondColor);
               // Choisir une action alternative
               moveData->action = DRAW_BLIND_CARD;
               printf("Strategy decided: draw blind card (invalid color for route)\n");
               return 1;
           }
           
           // Prepare the action
           moveData->action = CLAIM_ROUTE;
           moveData->claimRoute.from = from;
           moveData->claimRoute.to = to;
           moveData->claimRoute.color = color;
           moveData->claimRoute.nbLocomotives = nbLocomotives;
           
           printf("Strategy decided: claim route from %d to %d with color %d and %d locomotives\n",
                  moveData->claimRoute.from, moveData->claimRoute.to, 
                  moveData->claimRoute.color, moveData->claimRoute.nbLocomotives);
           
           return 1;
       } else {
           printf("ERROR: Invalid route index after prioritization\n");
           // Fall through to card drawing
       }
   }
   
   // If we haven't made a decision yet, consider drawing cards or objectives
   
   // MODIFICATION: Draw objectives if completion rate is low
   float objectiveCompletionRate = (state->nbObjectives > 0) ? 
                                  (float)completedObjectives / state->nbObjectives : 0;
   
   if (state->nbObjectives < 3 || (objectiveCompletionRate < 0.5 && state->nbObjectives < 5 && phase != PHASE_FINAL)) {
       moveData->action = DRAW_OBJECTIVES;
       printf("Strategy decided: draw new objectives (completion rate: %.2f)\n", objectiveCompletionRate);
       return 1;
   }
   
   // Otherwise, draw cards strategically
   
   // First priority: Locomotive (always valuable)
   for (int i = 0; i < 5; i++) {
       if (state->visibleCards[i] == LOCOMOTIVE) {
           moveData->action = DRAW_CARD;
           moveData->drawCard = LOCOMOTIVE;
           printf("Strategy decided: draw visible locomotive card\n");
           return 1;
       }
   }
   
   // Use strategic card drawing
   int cardIndex = strategicCardDrawing(state);

   // Safety checks for card drawing
   if (cardIndex >= 0 && cardIndex < 5 && 
       state->visibleCards[cardIndex] != NONE && 
       state->visibleCards[cardIndex] >= 0 && 
       state->visibleCards[cardIndex] < 10) {
       
       // Draw the selected visible card
       moveData->action = DRAW_CARD;
       moveData->drawCard = state->visibleCards[cardIndex];
       
       // Fixed string formatting for card name display
       const char* cardNames[] = {"None", "Purple", "White", "Blue", "Yellow", 
                              "Orange", "Black", "Red", "Green", "Locomotive"};
       
       printf("Strategy decided: draw visible %s card strategically\n", 
              cardNames[moveData->drawCard]);
   } else {
       // Draw blind card
       moveData->action = DRAW_BLIND_CARD;
       printf("Strategy decided: draw blind card (better than visible options or invalid card index)\n");
   }

   return 1;
}
/**
 * Enhanced updateAfterOpponentMove that tracks opponent behavior
 */
void enhancedUpdateAfterOpponentMove(GameState* state, MoveData* moveData) {
    // First call the original function to handle basic state updates
    updateAfterOpponentMove(state, moveData);
    
    // Then add enhanced opponent modeling
    if (moveData->action == CLAIM_ROUTE) {
        int from = moveData->claimRoute.from;
        int to = moveData->claimRoute.to;
        
        // Safety check for valid cities
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            printf("WARNING: Invalid city indices in opponent's move: %d, %d\n", from, to);
            return;
        }
        
        // Update our opponent model with this move
        updateOpponentObjectiveModel(state, from, to);
        
        // Log opponent behavior
        printf("Opponent claimed route from %d to %d - analyzing their strategy\n", from, to);
        
        // Predict if they're close to completing objectives
        checkOpponentObjectiveProgress(state);
    }
}

/* 
 * The main strategy interface functions
 */

// Implement chooseObjectivesStrategy to call the improved version
void chooseObjectivesStrategy(GameState* state, Objective* objectives, bool* chooseObjectives) {
    printf("Using advanced objective selection strategy\n");
    improvedObjectiveEvaluation(state, objectives, chooseObjectives);
}

// Implement the three core strategy functions to call the super advanced version
int basicStrategy(GameState* state, MoveData* moveData) {
    // Call the super advanced strategy
    return superAdvancedStrategy(state, moveData);
}

int dijkstraStrategy(GameState* state, MoveData* moveData) {
    // Call the super advanced strategy
    return superAdvancedStrategy(state, moveData);
}

int advancedStrategy(GameState* state, MoveData* moveData) {
    // Call the super advanced strategy
    return superAdvancedStrategy(state, moveData);
}

// Add these function implementations to the end of your strategy.c file

/**
 * Implementation of findShortestPath
 * This should be at the end of strategy.c or in rules.c
 */
int findShortestPath(GameState* state, int start, int end, int* path, int* pathLength) {
    // Vérification des paramètres
    if (!state || !path || !pathLength || start < 0 || start >= state->nbCities || 
        end < 0 || end >= state->nbCities) {
        printf("ERROR: Invalid parameters in findShortestPath\n");
        return -1;
    }
    
    // Distances depuis le départ
    int dist[MAX_CITIES];
    // Précédents dans le chemin
    int prev[MAX_CITIES];
    // Nœuds non visités
    int unvisited[MAX_CITIES];
    
    // Initialisation
    for (int i = 0; i < state->nbCities; i++) {
        dist[i] = INT_MAX;
        prev[i] = -1;
        unvisited[i] = 1;  // 1 = non visité
    }
    
    dist[start] = 0;  // Distance de la source à elle-même = 0
    
    // Implémentation de l'algorithme de Dijkstra
    for (int count = 0; count < state->nbCities; count++) {
        // Trouver le nœud non visité avec la plus petite distance
        int u = -1;
        int minDist = INT_MAX;
        
        for (int i = 0; i < state->nbCities; i++) {
            if (unvisited[i] && dist[i] < minDist) {
                minDist = dist[i];
                u = i;
            }
        }
        
        // Si nous ne trouvons pas de nœud accessible, arrêter
        if (u == -1 || dist[u] == INT_MAX) {
            break;
        }
        
        // Marquer comme visité
        unvisited[u] = 0;
        
        // Si nous avons atteint la destination, on peut s'arrêter
        if (u == end) {
            break;
        }
        
        // Parcourir toutes les routes
        for (int i = 0; i < state->nbTracks; i++) {
            // Ne considérer que les routes non prises par l'adversaire
            if (state->routes[i].owner == 2) {
                continue;
            }
            
            int from = state->routes[i].from;
            int to = state->routes[i].to;
            int length = state->routes[i].length;
            
            // Si cette route part de u
            if (from == u || to == u) {
                int v = (from == u) ? to : from;  // Autre extrémité
                
                // Calculer nouvelle distance
                int newDist = dist[u] + length;
                
                // Si c'est mieux que la distance actuelle
                if (newDist < dist[v]) {
                    dist[v] = newDist;
                    prev[v] = u;
                }
            }
        }
    }
    
    // Vérifier si un chemin a été trouvé
    if (prev[end] == -1 && start != end) {
        printf("No path found from %d to %d\n", start, end);
        return -1;
    }
    
    // Reconstruire le chemin (en ordre inverse)
    int tempPath[MAX_CITIES];
    int tempIndex = 0;
    int current = end;
    
    while (current != -1 && tempIndex < MAX_CITIES) {
        tempPath[tempIndex++] = current;
        if (current == start) break;
        current = prev[current];
    }
    
    // Inverser le chemin pour qu'il soit dans le bon ordre (start -> end)
    *pathLength = tempIndex;
    for (int i = 0; i < tempIndex; i++) {
        path[i] = tempPath[tempIndex - 1 - i];
    }
    
    // Retourner la distance totale
    return dist[end];
}

/**
 * Implementation of isRouteInPath
 */
int isRouteInPath(int from, int to, int* path, int pathLength) {
    for (int i = 0; i < pathLength - 1; i++) {
        if ((path[i] == from && path[i+1] == to) ||
            (path[i] == to && path[i+1] == from)) {
            return 1;
        }
    }
    return 0;
}

/**
 * Implementation of evaluateRouteUtility
 */
int evaluateRouteUtility(GameState* state, int routeIndex) {
    if (routeIndex < 0 || routeIndex >= state->nbTracks) {
        printf("ERROR: Invalid route index %d in evaluateRouteUtility\n", routeIndex);
        return 0;
    }
    
    int from = state->routes[routeIndex].from;
    int to = state->routes[routeIndex].to;
    int length = state->routes[routeIndex].length;
    
    // Score de base: les points que la route rapporte
    // Table de correspondance longueur -> points
    int pointsByLength[] = {0, 1, 2, 4, 7, 10, 15};
    int baseScore = (length >= 0 && length <= 6) ? pointsByLength[length] : 0;
    
    // Bonus pour les routes qui font partie du chemin le plus court pour un objectif
    int objectiveBonus = 0;
    
    for (int i = 0; i < state->nbObjectives; i++) {
        // Si l'objectif est déjà complété, on ignore
        if (isObjectiveCompleted(state, state->objectives[i])) {
            continue;
        }
        
        int objFrom = state->objectives[i].from;
        int objTo = state->objectives[i].to;
        int objScore = state->objectives[i].score;
        
        // Trouver le chemin le plus court pour cet objectif
        int path[MAX_CITIES];
        int pathLength = 0;
        
        if (findShortestPath(state, objFrom, objTo, path, &pathLength) >= 0) {
            // Vérifier si la route fait partie du chemin
            if (isRouteInPath(from, to, path, pathLength)) {
                // Bonus basé sur le score de l'objectif et la rareté du chemin
                objectiveBonus += objScore * 2;
                
                // Bonus supplémentaire si c'est une route critique (peu d'alternatives)
                // Plus la route est longue, plus elle est difficile à remplacer
                objectiveBonus += length * 3;
            }
        }
    }
    
    // Pénalité pour utiliser des wagons quand il en reste peu
    int wagonPenalty = 0;
    if (state->wagonsLeft < 15) {
        wagonPenalty = length * (15 - state->wagonsLeft) / 2;
    }
    
    // Score final
    int finalScore = baseScore + objectiveBonus - wagonPenalty;
    
    return finalScore;
}

/**
 * Check if opponent appears to be close to completing objectives
 * This function analyzes the current state to predict opponent objectives
 */
void checkOpponentObjectiveProgress(GameState* state) {
    // For each possible city pair, check if opponent has a path
    // Limit the search to cities with high interest to prevent excessive computation
    int citiesChecked = 0;
    const int MAX_CITIES_TO_CHECK = 15;  // Reasonably limit cities to check
    
    for (int i = 0; i < state->nbCities && i < MAX_CITIES && citiesChecked < MAX_CITIES_TO_CHECK; i++) {
        // Only check cities with significant interest
        if (i >= MAX_CITIES || opponentCitiesOfInterest[i] < 2) continue;
        citiesChecked++;
        
        int pairsChecked = 0;
        const int MAX_PAIRS_PER_CITY = 5;  // Limit pairs per city
        
        for (int j = i+1; j < state->nbCities && j < MAX_CITIES && pairsChecked < MAX_PAIRS_PER_CITY; j++) {
            // Only check cities with significant interest
            if (j >= MAX_CITIES || opponentCitiesOfInterest[j] < 2) continue;
            pairsChecked++;
            
            // Only process if both cities have interest and are valid indices
            if (i < MAX_CITIES && j < MAX_CITIES && 
                opponentCitiesOfInterest[i] > 0 && opponentCitiesOfInterest[j] > 0) {
                
                // Check if opponent has a path connecting these cities
                // To do this, we'll temporarily ignore our routes
                
                // Save original owners of routes we'll modify
                int modifiedRoutes[MAX_ROUTES];  // Store indices
                int modifiedCount = 0;
                
                // Count how many of our routes we need to temporarily modify
                for (int k = 0; k < state->nbTracks && modifiedCount < MAX_ROUTES - 1; k++) {
                    if (state->routes[k].owner == 1) {  // Our routes
                        modifiedRoutes[modifiedCount++] = k;
                        state->routes[k].owner = 3;  // Temporary value
                    }
                }
                
                // Check if a path exists for opponent
                int path[MAX_CITIES];
                int pathLength = 0;
                int distance = findShortestPath(state, i, j, path, &pathLength);
                
                // Restore our routes to original state
                for (int k = 0; k < modifiedCount; k++) {
                    int idx = modifiedRoutes[k];
                    if (idx >= 0 && idx < state->nbTracks) {
                        state->routes[idx].owner = 1;  // Restore to our ownership
                    }
                }
                
                // If opponent has a path and both cities have high interest
                if (distance > 0 && opponentCitiesOfInterest[i] >= 2 && opponentCitiesOfInterest[j] >= 2) {
                    printf("POTENTIAL OPPONENT OBJECTIVE: Cities %d and %d appear to be connected\n", i, j);
                    
                    // Increase interest in these cities for blocking
                    if (i < MAX_CITIES) opponentCitiesOfInterest[i] += 2;
                    if (j < MAX_CITIES) opponentCitiesOfInterest[j] += 2;
                    
                    // Also increase interest in cities along this path
                    for (int k = 0; k < pathLength && k < MAX_CITIES; k++) {
                        int cityIdx = path[k];
                        if (cityIdx >= 0 && cityIdx < MAX_CITIES) {
                            opponentCitiesOfInterest[cityIdx] += 1;
                        }
                    }
                }
            }
        }
    }
}