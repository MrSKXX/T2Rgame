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
    // Activer le débogage avancé pour aider à identifier les problèmes
    debugObjectives(state);
    
    // Ajout d'une trace supplémentaire avant de prendre des décisions
    debugPrint(1, "Analyse de l'état du jeu pour prendre une décision");
    printf("Cartes en main: ");
    for (int i = 1; i < 10; i++) {
        if (state->nbCardsByColor[i] > 0) {
            printf("%s:%d ", 
                  (i < 10) ? (const char*[]){
                      "None", "Purple", "White", "Blue", "Yellow", 
                      "Orange", "Black", "Red", "Green", "Locomotive"
                  }[i] : "Unknown", 
                  state->nbCardsByColor[i]);
        }
    }
    printf("\n");
    
    // Continuer avec la stratégie choisie
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
                utility += 30;  // Augmenté de 10 à 30
                break;
            }
        }
    }
    
    // Bonus pour les routes longues (plus de points)
    if (length >= 5) {
        utility += length * 100;  // Bonus massif pour les routes très longues
    } else if (length >= 4) {
        utility += length * 50;   // Bonus important pour les routes longues
    } else if (length >= 3) {
        utility += length * 20;   // Bonus modéré pour les routes moyennes
    }
    
    return utility;
}

// Calculate how much a route helps with objective completion
/**
 * Calculate how much a route helps with objective completion
 */
// Dans calculateObjectiveProgress
/**
 * Calculate how much a route helps with objective completion
 */
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
    
    // Multiplicateur plus raisonnable pour les routes d'objectifs
    const int OBJECTIVE_MULTIPLIER = 200;
    
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
        
        // Connexion directe - priorité importante (mais pas excessive)
        if ((from == objFrom && to == objTo) || (from == objTo && to == objFrom)) {
            int directBonus = objScore * OBJECTIVE_MULTIPLIER * 5;
            progress += directBonus;
            printf("!!! CONNEXION DIRECTE pour objectif %d !!! Bonus: %d\n", i+1, directBonus);
            continue;
        }
        
        // Compter routes restantes pour cet objectif
        int remainingRoutes = countRemainingRoutesForObjective(state, i);
        
        // Objectif presque complété - priorité élevée (mais pas excessive)
        if (remainingRoutes == 1) {
            int finalRouteBonus = objScore * OBJECTIVE_MULTIPLIER * 3;
            
            // Vérifier si cette route est la dernière nécessaire
            int path[MAX_CITIES];
            int pathLength = 0;
            if (findShortestPath(state, objFrom, objTo, path, &pathLength) > 0) {
                for (int j = 0; j < pathLength - 1; j++) {
                    if ((path[j] == from && path[j+1] == to) || (path[j] == to && path[j+1] == from)) {
                        progress += finalRouteBonus;
                        printf("!!! DERNIÈRE ROUTE pour objectif %d !!! Bonus: %d\n", i+1, finalRouteBonus);
                        break;
                    }
                }
            }
        }
        
        // Route normale sur un chemin d'objectif
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findShortestPath(state, objFrom, objTo, path, &pathLength);
        
        if (distance > 0 && pathLength > 0) {
            // Vérifier si notre route est sur ce chemin
            for (int j = 0; j < pathLength - 1; j++) {
                if ((path[j] == from && path[j+1] == to) || (path[j] == to && path[j+1] == from)) {
                    // Bonus de base
                    progress += objScore * OBJECTIVE_MULTIPLIER;
                    printf("Route %d -> %d est sur le chemin de l'objectif %d\n", from, to, i+1);
                    
                    // Vérifier si c'est un pont critique (pas d'alternative)
                    // Temporairement marquer cette route comme prise par l'adversaire
                    int originalOwner = state->routes[routeIndex].owner;
                    state->routes[routeIndex].owner = 2;
                    
                    // Vérifier s'il existe encore un chemin
                    int altPath[MAX_CITIES];
                    int altPathLength = 0;
                    int altDistance = findShortestPath(state, objFrom, objTo, altPath, &altPathLength);
                    
                    // Restaurer l'état original
                    state->routes[routeIndex].owner = originalOwner;
                    
                    // Si aucun chemin alternatif n'existe, c'est un pont critique
                    if (altDistance <= 0) {
                        progress += objScore * OBJECTIVE_MULTIPLIER;  // Double le bonus
                        printf("Route %d -> %d est un PONT CRITIQUE pour objectif %d\n", from, to, i+1);
                    }
                    
                    break;
                }
            }
        }
    }
    
    return progress;
}

/**
 * Helper function: Count how many routes remain to complete an objective
 */
/**
 * Helper function: Count how many routes remain to complete an objective
 */
/**
 * Helper function: Count how many routes remain to complete an objective
 */
int countRemainingRoutesForObjective(GameState* state, int objectiveIndex) {
    if (!state || objectiveIndex < 0 || objectiveIndex >= state->nbObjectives) {
        return -1;  // Invalid params
    }
    
    int objFrom = state->objectives[objectiveIndex].from;
    int objTo = state->objectives[objectiveIndex].to;
    
    // Safety check
    if (objFrom < 0 || objFrom >= state->nbCities || objTo < 0 || objTo >= state->nbCities) {
        return -1;
    }
    
    // If already completed, no routes needed
    if (isObjectiveCompleted(state, state->objectives[objectiveIndex])) {
        return 0;
    }
    
    // Find the shortest path
    int path[MAX_CITIES];
    int pathLength = 0;
    int distance = findShortestPath(state, objFrom, objTo, path, &pathLength);
    
    if (distance <= 0 || pathLength <= 0) {
        return -1;  // No path exists
    }
    
    // Count how many segments of the path we already own
    int segmentsOwned = 0;
    int totalSegments = pathLength - 1;
    
    for (int i = 0; i < pathLength - 1; i++) {
        int cityA = path[i];
        int cityB = path[i+1];
        
        // Safety check
        if (cityA < 0 || cityA >= state->nbCities || cityB < 0 || cityB >= state->nbCities) {
            continue;
        }
        
        // Check if we already own a route between these cities
        for (int j = 0; j < state->nbTracks; j++) {
            if (((state->routes[j].from == cityA && state->routes[j].to == cityB) ||
                 (state->routes[j].from == cityB && state->routes[j].to == cityA)) &&
                state->routes[j].owner == 1) {  // We own it
                segmentsOwned++;
                break;
            }
        }
    }
    
    return totalSegments - segmentsOwned;
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
    

 if (routeColor != LOCOMOTIVE && length <= 2 && state->nbCardsByColor[LOCOMOTIVE] > 0) {
        // Pénalisation importante si on veut utiliser des locomotives pour des routes courtes
        return 50;  // Valeur réduite par rapport au match parfait (150)
    }


    // For gray routes, find the most efficient color to use
    if (routeColor == LOCOMOTIVE) {
        int bestEfficiency = 0;
        for (int c = 1; c < 9; c++) {  // Skip NONE and LOCOMOTIVE
            if (state->nbCardsByColor[c] > 0) {
                // Calculate efficiency as cards used / cards available
                int cardsNeeded = length;
                int cardsAvailable = state->nbCardsByColor[c];
                
                if (cardsNeeded <= cardsAvailable) {
                    // Perfect match - augmenté pour favoriser l'utilisation des cartes
                    int efficiency = 150;
                    if (efficiency > bestEfficiency) {
                        bestEfficiency = efficiency;
                    }
                } else {
                    // Need to supplement with locomotives
                    int locosNeeded = cardsNeeded - cardsAvailable;
                    if (locosNeeded <= state->nbCardsByColor[LOCOMOTIVE]) {
                        // Calculate efficiency (lower is better)
                        int efficiency = 100 - (locosNeeded * 10);  // Pénalité réduite
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
            // Perfect match - augmenté pour favoriser l'utilisation des cartes
            return 150;
        } else {
            // Need to supplement with locomotives
            int locosNeeded = cardsNeeded - cardsAvailable;
            if (locosNeeded <= state->nbCardsByColor[LOCOMOTIVE]) {
                // Calculate efficiency (lower is better)
                return 100 - (locosNeeded * 10);  // Pénalité réduite
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

// Amélioration massive de updateOpponentObjectiveModel
// Amélioration massive de updateOpponentObjectiveModel
void updateOpponentObjectiveModel(GameState* state, int from, int to) {
    // Safety checks
    if (from < 0 || from >= MAX_CITIES || to < 0 || to >= MAX_CITIES) {
        printf("ERROR: Invalid city indices in updateOpponentObjectiveModel: %d, %d\n", from, to);
        return;
    }
    
    static int opponentCityVisits[MAX_CITIES] = {0};  // Compteur de connexions par ville
    static int opponentLikelyObjectives[MAX_CITIES][MAX_CITIES] = {0};  // Score de probabilité par paire de villes
    static int opponentConsecutiveRoutesInDirection[MAX_CITIES] = {0}; // Détection de direction intentionnelle
    
    // Mettre à jour le compteur de visites
    opponentCityVisits[from]++;
    opponentCityVisits[to]++;
    
    // Villes avec plusieurs connexions sont probablement des objectifs
    if (opponentCityVisits[from] >= 2) {
        printf("ADVERSAIRE: La ville %d semble être importante (connexions: %d)\n", 
               from, opponentCityVisits[from]);
    }
    if (opponentCityVisits[to] >= 2) {
        printf("ADVERSAIRE: La ville %d semble être importante (connexions: %d)\n", 
               to, opponentCityVisits[to]);
    }
    
    // Détecter les patterns de routes consécutives dans une direction
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 2) {  // Route de l'adversaire
            // Si cette route est connectée à celle qu'il vient de prendre
            if (state->routes[i].from == from || state->routes[i].from == to || 
                state->routes[i].to == from || state->routes[i].to == to) {
                
                // Identifier les villes à l'autre extrémité de cette route existante
                int otherCity = -1;
                if (state->routes[i].from == from || state->routes[i].from == to) {
                    otherCity = state->routes[i].to;
                } else {
                    otherCity = state->routes[i].from;
                }
                
                if (otherCity != from && otherCity != to && otherCity >= 0 && otherCity < MAX_CITIES) {
                    opponentConsecutiveRoutesInDirection[otherCity]++;
                    
                    if (opponentConsecutiveRoutesInDirection[otherCity] >= 2) {
                        printf("TRAJECTOIRE ADVERSAIRE DÉTECTÉE: Direction vers la ville %d\n", otherCity);
                        // Augmenter massivement la probabilité d'objectif dans cette direction
                        for (int dest = 0; dest < state->nbCities && dest < MAX_CITIES; dest++) {
                            if (dest != otherCity) {
                                // Calculer distance approximative
                                int dx = abs(dest - otherCity) % 10;  // Approximation grossière
                                int distance = 10 - dx;  // Plus c'est loin, plus c'est probable
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
    // Attribuer des scores élevés aux villes qui semblent être des extrémités
    for (int i = 0; i < state->nbCities && i < MAX_CITIES; i++) {
        if (opponentCityVisits[i] >= 2) {
            for (int j = i+1; j < state->nbCities && j < MAX_CITIES; j++) {
                if (opponentCityVisits[j] >= 2) {
                    // Calculer distance
                    int path[MAX_CITIES];
                    int pathLength = 0;
                    int distance = findShortestPath(state, i, j, path, &pathLength);
                    
                    if (distance > 0) {
                        // Les distances moyennes (5-8) sont les plus probables pour des objectifs
                        int distanceScore = 0;
                        if (distance >= 4 && distance <= 9) {
                            distanceScore = 10;  // Bon candidat pour un objectif
                        } else if (distance >= 2 && distance <= 12) {
                            distanceScore = 5;   // Candidat possible
                        }
                        
                        // Vérifier s'il y a des routes de l'adversaire sur ce chemin
                        int opponentRoutesOnPath = 0;
                        for (int k = 0; k < pathLength - 1; k++) {
                            int cityA = path[k];
                            int cityB = path[k + 1];
                            
                            for (int r = 0; r < state->nbTracks; r++) {
                                if (((state->routes[r].from == cityA && state->routes[r].to == cityB) ||
                                     (state->routes[r].from == cityB && state->routes[r].to == cityA)) &&
                                    state->routes[r].owner == 2) {  // Route de l'adversaire
                                    opponentRoutesOnPath++;
                                    break;
                                }
                            }
                        }
                        
                        // Si l'adversaire a pris plusieurs routes sur ce chemin, c'est probablement un objectif
                        if (opponentRoutesOnPath >= 2) {
                            opponentLikelyObjectives[i][j] += 30 * opponentRoutesOnPath;
                            opponentLikelyObjectives[j][i] += 30 * opponentRoutesOnPath;
                            
                            printf("OBJECTIF ADVERSE PROBABLE: %d -> %d (score: %d, routes: %d)\n", 
                                   i, j, opponentLikelyObjectives[i][j], opponentRoutesOnPath);
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
    printf("Objectifs probables de l'adversaire:\n");
    int threshold = 30;  // Seuil plus élevé pour être plus sélectif
    int count = 0;
    int topObjectives[5][2];  // Stocker les 5 meilleurs objectifs
    int topScores[5] = {0};
    
    for (int i = 0; i < state->nbCities && i < MAX_CITIES; i++) {
        for (int j = i+1; j < state->nbCities && j < MAX_CITIES; j++) {
            if (opponentLikelyObjectives[i][j] > threshold) {
                // Insérer dans notre top 5
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
                if (count <= 5) {  // Limiter à 5 dans les logs
                    printf("  %d -> %d: score %d\n", i, j, opponentLikelyObjectives[i][j]);
                }
            }
        }
    }
    
    // Mettre à jour le tableau global des points d'intérêt
    // Réinitialiser les valeurs pour ne pas qu'elles augmentent indéfiniment
    for (int i = 0; i < MAX_CITIES; i++) {
        opponentCitiesOfInterest[i] = 0;
    }
    
    // Attribuer des points d'intérêt aux villes des objectifs probables
    for (int i = 0; i < 5; i++) {
        if (topScores[i] > 0) {
            int city1 = topObjectives[i][0];
            int city2 = topObjectives[i][1];
            if (city1 >= 0 && city1 < MAX_CITIES) {
                opponentCitiesOfInterest[city1] += (5-i) * 2;  // Plus de points pour les meilleurs
            }
            if (city2 >= 0 && city2 < MAX_CITIES) {
                opponentCitiesOfInterest[city2] += (5-i) * 2;
            }
            
            // Trouver les routes clés pour cet objectif
            int path[MAX_CITIES];
            int pathLength = 0;
            if (findShortestPath(state, city1, city2, path, &pathLength) > 0) {
                // Identifier les routes critiques sur ce chemin
                for (int j = 0; j < pathLength - 1; j++) {
                    int pathFrom = path[j];
                    int pathTo = path[j+1];
                    
                    // Augmenter l'intérêt pour les villes intermédiaires cruciales
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

/**
 * Identifies critical routes that should be claimed to block the opponent
 */
/**
 * Identifies critical routes that should be claimed to block the opponent
 * This version is optimized to prevent excessive memory use and logging
 */
// Fonction pour identifier les routes critiques à bloquer
/**
 * Fonction pour identifier les routes critiques à bloquer
 */
int findCriticalRoutesToBlock(GameState* state, int* routesToBlock, int* blockingPriorities) {
    int count = 0;
    const int MAX_BLOCKING_ROUTES = 10;  // Remplacé MAX_ROUTES par MAX_BLOCKING_ROUTES
    
    // Initialiser
    for (int i = 0; i < MAX_BLOCKING_ROUTES; i++) {
        routesToBlock[i] = -1;
        blockingPriorities[i] = 0;
    }
    
    // Analyse des objectifs probables de l'adversaire (basée sur updateOpponentObjectiveModel)
    int topObjectives[5][2] = {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};
    int topScores[5] = {0};
    
    // Trouver les 5 objectifs les plus probables de l'adversaire
    for (int i = 0; i < state->nbCities && i < MAX_CITIES; i++) {
        if (opponentCitiesOfInterest[i] > 0) {
            for (int j = i+1; j < state->nbCities && j < MAX_CITIES; j++) {
                if (opponentCitiesOfInterest[j] > 0) {
                    // Score basé sur l'intérêt combiné
                    int score = opponentCitiesOfInterest[i] + opponentCitiesOfInterest[j];
                    
                    // Vérifier si cet objectif est crédible
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
                                    state->routes[r].owner == 2) {  // Route de l'adversaire
                                    opponentRoutes++;
                                    break;
                                }
                            }
                        }
                        
                        // Augmenter le score si l'adversaire a déjà des routes sur ce chemin
                        score += opponentRoutes * 5;
                        
                        // Insérer dans notre top 5 si score suffisant
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
        
        printf("Analyse des routes à bloquer pour l'objectif probable %d -> %d (score: %d)\n", 
               from, to, topScores[i]);
        
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
                        state->routes[r].owner == 0) {  // Route libre
                        
                        // Vérifier s'il existe un chemin alternatif sans cette route
                        bool isBottleneck = true;
                        
                        // Temporairement marquer cette route comme prise par l'adversaire
                        int originalOwner = state->routes[r].owner;
                        state->routes[r].owner = 2;  // Simuler que l'adversaire l'a prise
                        
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
                            // Calculer la priorité en fonction de la position dans le chemin et du score de l'objectif
                            // Réduire l'importance des scores pour éviter des priorités excessives
                            int priority = topScores[i] * (3-i);  // Réduire le multiplicateur de 5-i à 3-i
                            
                            // Bonus pour les routes courtes (plus faciles à prendre)
                            if (state->routes[r].length <= 2) {
                                priority += 10;  // Réduire de 20 à 10
                            }
                            
                            // Bonus si c'est près du début du chemin (plus stratégique)
                            if (j <= 1 || j >= pathLength - 2) {
                                priority += 10;  // Réduire de 15 à 10
                            }
                            
                            printf("Route critique à bloquer: %d -> %d (priorité: %d)\n", 
                                   cityA, cityB, priority);
                            
                            // Ajouter à notre liste de routes à bloquer
                            routesToBlock[count] = r;
                            blockingPriorities[count] = priority;
                            count++;
                            
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
/**
 * Fonction améliorée pour choisir les objectifs de manière plus intelligente
 */
void improvedObjectiveEvaluation(GameState* state, Objective* objectives, bool* chooseObjectives) {
    printf("Évaluation avancée des objectifs\n");
    
    // Vérification des paramètres
    if (!state || !objectives || !chooseObjectives) {
        printf("ERROR: Invalid parameters in improvedObjectiveEvaluation\n");
        return;
    }
    
    // Initialiser les choix à false
    for (int i = 0; i < 3; i++) {
        chooseObjectives[i] = false;
    }
    
    // DÉTECTION SPÉCIALE premier tour - au moins 2 objectifs requis
    bool isFirstTurn = (state->nbObjectives == 0 && state->nbClaimedRoutes == 0);
    
    if (isFirstTurn) {
        printf("PREMIER TOUR: Au moins 2 objectifs doivent être sélectionnés\n");
    }
    
    // Scores pour chaque objectif
    float scores[3];
    
    // Analyser chaque objectif
    for (int i = 0; i < 3; i++) {
        int from = objectives[i].from;
        int to = objectives[i].to;
        int value = objectives[i].score;
        
        // Vérification des limites
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            printf("Objectif %d: Villes invalides - De %d à %d, score %d\n", i+1, from, to, value);
            scores[i] = -1000;  // Objectif invalide
            continue;
        }
        
        printf("Objectif %d: De %d à %d, score %d\n", i+1, from, to, value);
        
        // Trouver chemin optimal
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findShortestPath(state, from, to, path, &pathLength);
        
        if (distance < 0) {
            // Aucun chemin trouvé - objectif impossible
            scores[i] = -1000;
            printf("  - Aucun chemin disponible, objectif impossible\n");
            continue;
        }
        
        // Pénalité massive pour les chemins trop longs
        if (distance > 10) {
            printf("  - ATTENTION: Chemin très long (%d) pour cet objectif\n", distance);
            scores[i] = -1000;  // Pénalité rédhibitoire
            continue;
        }
        
        // Analyser la complexité du chemin
        int routesNeeded = 0;
        int routesOwnedByUs = 0;
        int routesOwnedByOpponent = 0;
        int routesAvailable = 0;
        int totalLength = 0;
        int routesBlockedByOpponent = 0;
        
        for (int j = 0; j < pathLength - 1 && j < MAX_CITIES - 1; j++) {
            int pathFrom = path[j];
            int pathTo = path[j+1];
            
            // Vérification des limites
            if (pathFrom < 0 || pathFrom >= state->nbCities || 
                pathTo < 0 || pathTo >= state->nbCities) {
                printf("  - ATTENTION: Villes invalides dans le chemin\n");
                continue;
            }
            
            // Trouver la route entre ces villes
            bool routeFound = false;
            bool alternativeRouteExists = false;
            
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
                        routesBlockedByOpponent++;
                    }
                }
                
                // Vérifier s'il existe des routes alternatives
                else if (((state->routes[k].from == pathFrom && state->routes[k].to == pathTo) ||
                         (state->routes[k].from == pathTo && state->routes[k].to == pathFrom)) &&
                        state->routes[k].owner == 0) {
                    alternativeRouteExists = true;
                }
            }
            
            // Si une route est bloquée mais qu'une alternative existe, ne pas compter comme bloquée
            if (routesBlockedByOpponent > 0 && alternativeRouteExists) {
                routesBlockedByOpponent--;
            }
            
            if (!routeFound) {
                // Aucune route entre ces villes - ne devrait pas arriver si findShortestPath a fonctionné
                scores[i] = -1000;
                printf("  - Erreur: Le chemin contient une route inexistante\n");
            }
        }
        
        routesNeeded = routesAvailable + routesOwnedByUs;
        
        // Si des routes sont bloquées par l'adversaire, pénalité massive
        if (routesBlockedByOpponent > 0) {
            int penalty = routesBlockedByOpponent * 50;  // Pénalité extrême
            scores[i] = -penalty;
            printf("  - %d routes bloquées par l'adversaire, pénalité: -%d\n", 
                   routesBlockedByOpponent, penalty);
            continue;
        }
        
        // Score de base: rapport points/longueur (bonus pour les objectifs courts à haut score)
        float pointsPerLength = (totalLength > 0) ? (float)value / totalLength : 0;
        float baseScore = pointsPerLength * 150.0;  // Augmenté car crucial
        
        // Progrès de complétion: combien de routes nous possédons déjà
        float completionProgress = 0;
        if (routesNeeded > 0) {
            completionProgress = (float)routesOwnedByUs / routesNeeded;
        }
        
        // Bonus crucial pour les objectifs déjà partiellement complétés
        float completionBonus = completionProgress * 250.0;  // Bonus très important
        
        // Correspondance des cartes: combien nous avons de cartes qui correspondent aux routes nécessaires
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
                        // Route grise - vérifier si nous avons assez de cartes de n'importe quelle couleur
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
            cardMatchScore = (float)colorMatchCount / routesAvailable * 120.0;
        }
        
        // Synergie avec les objectifs existants
        float synergyScore = 0;
        
        // Vérifier la synergie avec les objectifs existants
        for (int j = 0; j < state->nbObjectives; j++) {
            int objFrom = state->objectives[j].from;
            int objTo = state->objectives[j].to;
            
            // Points communs avec les objectifs existants
            if (from == objFrom || from == objTo || to == objFrom || to == objTo) {
                synergyScore += 60;  // Bonus important pour la synergie
            }
            
            // Vérifier les routes communes avec les objectifs existants
            int objPath[MAX_CITIES];
            int objPathLength = 0;
            
            if (findShortestPath(state, objFrom, objTo, objPath, &objPathLength) >= 0) {
                int sharedRoutes = 0;
                
                for (int p1 = 0; p1 < pathLength - 1 && p1 < MAX_CITIES - 1; p1++) {
                    for (int p2 = 0; p2 < objPathLength - 1 && p2 < MAX_CITIES - 1; p2++) {
                        if ((path[p1] == objPath[p2] && path[p1+1] == objPath[p2+1]) ||
                            (path[p1] == objPath[p2+1] && path[p1+1] == objPath[p2])) {
                            sharedRoutes++;
                        }
                    }
                }
                
                synergyScore += sharedRoutes * 30;  // Bonus très important
            }
        }
        
        // Pénalité pour la compétition avec l'adversaire
        float competitionPenalty = 0;
        
        for (int j = 0; j < pathLength - 1 && j < MAX_CITIES - 1; j++) {
            int pathFrom = path[j];
            int pathTo = path[j+1];
            
            // Vérifier si l'adversaire est actif près de ces villes
            if (pathFrom < MAX_CITIES && pathTo < MAX_CITIES && 
                (opponentCitiesOfInterest[pathFrom] > 0 || opponentCitiesOfInterest[pathTo] > 0)) {
                // Calculer la pénalité
                int fromPenalty = (pathFrom < MAX_CITIES) ? opponentCitiesOfInterest[pathFrom] : 0;
                int toPenalty = (pathTo < MAX_CITIES) ? opponentCitiesOfInterest[pathTo] : 0;
                competitionPenalty += (fromPenalty + toPenalty) * 5;
            }
        }
        
        // Pénalité pour la difficulté: les objectifs difficiles ont des scores plus bas
        float difficultyPenalty = 0;
        if (routesNeeded > 4) {  // Seuil de difficulté
            difficultyPenalty = (routesNeeded - 3) * 40;
        }
        
        // Pénalité pour les chemins longs
        float lengthPenalty = 0;
        if (distance > 6) {  // Seuil de longueur
            lengthPenalty = (distance - 6) * 30;
        }
        
        // Bonus pour les objectifs à haute valeur
        float valueBonus = 0;
        if (value > 10) {
            valueBonus = (value - 10) * 10;
        }
        
        // Calcul du score final avec tous les composants
        scores[i] = baseScore + completionBonus + cardMatchScore + synergyScore + valueBonus
                  - competitionPenalty - difficultyPenalty - lengthPenalty;
        
        // Log des composants pour le débogage
        printf("  - Score de base (points/longueur): %.1f\n", baseScore);
        printf("  - Bonus de complétion: %.1f\n", completionBonus);
        printf("  - Correspondance des cartes: %.1f\n", cardMatchScore);
        printf("  - Synergie: %.1f\n", synergyScore);
        printf("  - Bonus de valeur: %.1f\n", valueBonus);
        printf("  - Compétition: -%.1f\n", competitionPenalty);
        printf("  - Difficulté: -%.1f\n", difficultyPenalty);
        printf("  - Pénalité de longueur: -%.1f\n", lengthPenalty);
        printf("  - SCORE FINAL: %.1f\n", scores[i]);
    }
    
    // Trier les objectifs par score
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
    
    // STRATÉGIE FINALE DE SÉLECTION
    
    int numToChoose = 0;
    
    /// Règle spéciale pour le premier tour
   if (isFirstTurn) {
       // Exigence du serveur: au moins 2 objectifs au premier tour
       chooseObjectives[sortedIndices[0]] = true;
       chooseObjectives[sortedIndices[1]] = true;
       numToChoose = 2;
       
       // Prendre le troisième objectif seulement s'il a un bon score (pas de score négatif)
       if (scores[sortedIndices[2]] > 0) {
           chooseObjectives[sortedIndices[2]] = true;
           numToChoose = 3;
       }
   }
   else {
       // Stratégie pour les tours suivants
       int phase = determineGamePhase(state);
       int currentObjectiveCount = state->nbObjectives;
       
       // Toujours prendre le meilleur objectif s'il n'est pas catastrophique
       if (scores[sortedIndices[0]] > -500) {
           chooseObjectives[sortedIndices[0]] = true;
           numToChoose++;
       }
       
       // La sélection des objectifs supplémentaires dépend de la phase et des objectifs actuels
       if (phase == PHASE_EARLY) {
           // Début de partie: plus agressif sur la prise d'objectifs
           if (currentObjectiveCount < 2) {
               // Si nous avons peu d'objectifs, on peut en prendre un second s'il est bon
               if (scores[sortedIndices[1]] > 100) {
                   chooseObjectives[sortedIndices[1]] = true;
                   numToChoose++;
               }
               
               // Un troisième seulement s'il est excellent
               if (scores[sortedIndices[2]] > 150) {
                   chooseObjectives[sortedIndices[2]] = true;
                   numToChoose++;
               }
           } else if (currentObjectiveCount < 3) {
               // Si nous avons déjà 2 objectifs, être sélectif pour le troisième
               if (scores[sortedIndices[1]] > 120) {
                   chooseObjectives[sortedIndices[1]] = true;
                   numToChoose++;
               }
           }
           // Si déjà 3+ objectifs, n'en prendre qu'un seul de plus et uniquement s'il est excellent
           else if (scores[sortedIndices[1]] > 200) {
               chooseObjectives[sortedIndices[1]] = true;
               numToChoose++;
           }
       }
       else if (phase == PHASE_MIDDLE) {
           // Milieu de partie: plus sélectif
           if (currentObjectiveCount < 3) {
               if (scores[sortedIndices[1]] > 150) {
                   chooseObjectives[sortedIndices[1]] = true;
                   numToChoose++;
               }
           }
           // Si déjà 3+ objectifs, extrêmement sélectif
           else if (scores[sortedIndices[1]] > 250) {
               chooseObjectives[sortedIndices[1]] = true;
               numToChoose++;
           }
       }
       else {
           // Fin de partie: ultra-sélectif
           // Ne prendre un second objectif que s'il est vraiment exceptionnel
           if (scores[sortedIndices[1]] > 300) {
               chooseObjectives[sortedIndices[1]] = true;
               numToChoose++;
           }
       }
       
       // LIMITATION DU NOMBRE TOTAL D'OBJECTIFS
       int maxTotalObjectives = 3 + (phase == PHASE_EARLY ? 1 : 0);  // Plus permissif en début de partie
       if (currentObjectiveCount + numToChoose > maxTotalObjectives) {
           // Réduire pour respecter la limite
           int maxNewObjectives = maxTotalObjectives - currentObjectiveCount;
           if (maxNewObjectives <= 0) {
               // Déjà atteint ou dépassé la limite, ne garder que le meilleur
               for (int i = 1; i < 3; i++) {
                   chooseObjectives[sortedIndices[i]] = false;
               }
               numToChoose = 1;
           } else {
               // Garder uniquement les N meilleurs pour respecter la limite
               for (int i = maxNewObjectives; i < 3; i++) {
                   chooseObjectives[sortedIndices[i]] = false;
               }
               numToChoose = maxNewObjectives;
           }
       }
   }
   
   // VÉRIFICATION: Au moins un objectif doit être sélectionné
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
       printf("CORRECTION D'URGENCE: Aucun objectif sélectionné! Sélection forcée de l'objectif 1\n");
   }
   
   // VÉRIFICATION FINALE POUR LE PREMIER TOUR: au moins 2 objectifs requis
   if (isFirstTurn) {
       int selectedCount = 0;
       for (int i = 0; i < 3; i++) {
           if (chooseObjectives[i]) {
               selectedCount++;
           }
       }
       
       if (selectedCount < 2) {
           printf("CORRECTION PREMIER TOUR: Moins de 2 objectifs sélectionnés, forcé à 2\n");
           // Assurer que les deux meilleurs sont choisis
           chooseObjectives[sortedIndices[0]] = true;
           chooseObjectives[sortedIndices[1]] = true;
           numToChoose = 2;
       }
   }
   
   printf("Choix de %d objectifs: ", numToChoose);
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
    
    // CORRECTION: Utiliser findCriticalRoutesToBlock au lieu de findCriticalBlockingRoutes
    int numBlockingRoutes = findCriticalRoutesToBlock(state, blockingRoutes, blockingPriorities);

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

        // Bonus pour l'utilisation efficace des cartes en main
        CardColor routeColor = state->routes[routeIndex].color;
        int length = state->routes[routeIndex].length;
        
        if (routeColor != LOCOMOTIVE && state->nbCardsByColor[routeColor] >= length) {
            // Bonus si nous avons assez de cartes pour cette route
            scores[i] += 50;  // Bonus significatif pour encourager l'utilisation des cartes
        } else if (routeColor == LOCOMOTIVE) {
            // Pour les routes grises, vérifier si nous avons assez de cartes de n'importe quelle couleur
            for (int c = 1; c < 9; c++) {
                if (state->nbCardsByColor[c] >= length) {
                    scores[i] += 40;
                    break;
                }
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
// Amélioration majeure de superAdvancedStrategy
int superAdvancedStrategy(GameState* state, MoveData* moveData) {
    printf("Stratégie avancée optimisée en cours d'exécution\n");
    
    // Ajouter un compteur de pioche consécutive
    static int consecutiveDraws = 0;
    
    // Analyse de l'état du jeu
    int phase = determineGamePhase(state);
    printf("Phase de jeu actuelle: %d\n", phase);
    
    // Incrémenter le compteur de tours
    state->turnCount++;
    
    // 1. ANALYSE DES OBJECTIFS ACTUELS
    int completedObjectives = 0;
    int incompleteObjectives = 0;
    int totalObjectiveValue = 0;
    int incompleteObjectiveValue = 0;
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (isObjectiveCompleted(state, state->objectives[i])) {
            completedObjectives++;
            totalObjectiveValue += state->objectives[i].score;
        } else {
            incompleteObjectives++;
            totalObjectiveValue += state->objectives[i].score;
            incompleteObjectiveValue += state->objectives[i].score;
        }
    }
    
    printf("Objectifs: %d complétés, %d incomplets, valeur totale: %d, valeur restante: %d\n",
          completedObjectives, incompleteObjectives, totalObjectiveValue, incompleteObjectiveValue);
    
    // Phase d'accumulation - si début de partie et pas assez de cartes pour routes longues
    int totalCards = 0;
    for (int i = 1; i < 10; i++) {
        totalCards += state->nbCardsByColor[i];
    }
    
    // Calculer des statistiques sur nos cartes
    int maxSameColorCards = 0;
    int colorWithMostCards = 0;
    for (int c = 1; c < 9; c++) {  // Hors locomotives
        if (state->nbCardsByColor[c] > maxSameColorCards) {
            maxSameColorCards = state->nbCardsByColor[c];
            colorWithMostCards = c;
        }
    }
    
    // 2. DÉTERMINER LA PRIORITÉ STRATÉGIQUE
    // Options: COMPLETE_OBJECTIVES, BLOCK_OPPONENT, BUILD_NETWORK, DRAW_CARDS
    enum StrategicPriority { COMPLETE_OBJECTIVES, BLOCK_OPPONENT, BUILD_NETWORK, DRAW_CARDS };
    enum StrategicPriority priority = COMPLETE_OBJECTIVES;  // Par défaut
    
    // 2.1 Cas spécial: Premier tour, nous devons piocher des objectifs si nous n'en avons pas
    if (state->nbObjectives == 0) {
        moveData->action = DRAW_OBJECTIVES;
        printf("Priorité: PIOCHER DES OBJECTIFS (premier tour)\n");
        return 1;
    }
    
    // Stratégie spéciale d'accumulation en début de partie
    if (phase == PHASE_EARLY && state->turnCount < 10 && totalCards < 8) {
        priority = DRAW_CARDS;
        printf("Priorité: ACCUMULER DES CARTES (phase initiale, seulement %d cartes)\n", totalCards);
    }
    // Si nous sommes en fin de partie et qu'il reste peu de wagons, prioriser les objectifs incomplets
    else if ((phase == PHASE_LATE || phase == PHASE_FINAL || state->wagonsLeft < 20) && incompleteObjectives > 0) {
        priority = COMPLETE_OBJECTIVES;
        printf("URGENCE FIN DE PARTIE: Prioriser complétion des %d objectifs incomplets!\n", 
              incompleteObjectives);
    }
    // Si l'adversaire est proche de finir (dernier tour)
    else if (state->lastTurn) {
        printf("DERNIER TOUR: Utiliser nos ressources restantes!\n");
        // Priorité à la prise de routes
        priority = BUILD_NETWORK;
    }
    // Analyse complète de l'urgence de complétion des objectifs en fin de partie
    else if (phase == PHASE_LATE || phase == PHASE_FINAL) {
        // Vérifier si nous sommes proches de compléter un objectif
        for (int i = 0; i < state->nbObjectives; i++) {
            if (!isObjectiveCompleted(state, state->objectives[i])) {
                int remainingRoutes = countRemainingRoutesForObjective(state, i);
                
                // Si nous sommes très proches de compléter un objectif, priorité absolue!
                if (remainingRoutes >= 0 && remainingRoutes <= 2) {
                    priority = COMPLETE_OBJECTIVES;
                    printf("URGENCE: Objectif %d proche de la complétion en fin de partie! (reste %d routes)\n", 
                          i+1, remainingRoutes);
                    break;
                }
            }
        }
    }
    // Si nous avons des objectifs incomplets avec une grande valeur, priorité absolue
    else if (incompleteObjectiveValue > 15 && phase >= PHASE_MIDDLE) {
        priority = COMPLETE_OBJECTIVES;
        printf("PRIORITÉ CRITIQUE: Objectifs incomplets de grande valeur (%d points) en phase %d\n", 
               incompleteObjectiveValue, phase);
    }
    // 2.2 Déterminer la priorité en fonction de la phase et des objectifs (si pas déjà décidé)
    else if (incompleteObjectives == 0) {
        // Si tous nos objectifs sont complétés, en piocher de nouveaux (seulement si pas trop)
        if (state->nbObjectives < 3) {
            moveData->action = DRAW_OBJECTIVES;
            printf("Priorité: PIOCHER DES OBJECTIFS (tous complétés)\n");
            return 1;
        } else {
            // Sinon, focus sur la construction de routes longues
            priority = BUILD_NETWORK;
            printf("Tous objectifs complétés: Prioriser les routes longues\n");
        }
    }
    else {
        // Si nous avons des objectifs incomplets, la priorité dépend de la situation
        // (sauf si déjà décidé ci-dessus)
        if (priority != COMPLETE_OBJECTIVES && priority != DRAW_CARDS) {
            // Calculer pourcentage d'objectifs complétés
            float completionRate = (float)completedObjectives / state->nbObjectives;
            
            if (completionRate > 0.7 && state->nbObjectives <= 3 && phase != PHASE_FINAL) {
                // Bon taux de complétion, on peut piocher plus d'objectifs
                moveData->action = DRAW_OBJECTIVES;
                printf("Priorité: PIOCHER DES OBJECTIFS (bon taux de complétion: %.2f)\n", completionRate);
                return 1;
            }
            else if (completionRate < 0.3 && state->wagonsLeft < 30) {
                // Mauvais taux de complétion avec peu de wagons, il faut se concentrer sur les objectifs
                priority = COMPLETE_OBJECTIVES;
                printf("Priorité: COMPLÉTER OBJECTIFS (faible taux de complétion: %.2f)\n", completionRate);
            }
            else {
                // Équilibrer entre complétion d'objectifs et construction de réseau
                // Vérifier si notre score est supérieur à l'adversaire (estimation)
                int ourEstimatedScore = calculateScore(state);
                int estimatedOpponentScore = 0;
                
                // Estimation très grossière du score adverse
                for (int i = 0; i < state->nbTracks; i++) {
                    if (state->routes[i].owner == 2) {
                        int length = state->routes[i].length;
                        int points = 0;
                        switch (length) {
                            case 1: points = 1; break;
                            case 2: points = 2; break;
                            case 3: points = 4; break;
                            case 4: points = 7; break;
                            case 5: points = 10; break;
                            case 6: points = 15; break;
                            default: points = 0;
                        }
                        estimatedOpponentScore += points;
                    }
                }
                
                // Ajouter estimation des objectifs adverses
                estimatedOpponentScore += state->opponentObjectiveCount * 8;  // Moyenne très grossière
                
                printf("Score estimé: Nous = %d, Adversaire = %d\n", ourEstimatedScore, estimatedOpponentScore);
                
                if (ourEstimatedScore < estimatedOpponentScore && phase != PHASE_EARLY) {
                    // Nous sommes en retard, priorité aux routes longues
                    priority = BUILD_NETWORK;
                    printf("Priorité: CONSTRUIRE RÉSEAU (nous sommes en retard)\n");
                }
                else if (incompleteObjectiveValue > 20) {
                    // Valeur élevée des objectifs restants, les compléter
                    priority = COMPLETE_OBJECTIVES;
                    printf("Priorité: COMPLÉTER OBJECTIFS (valeur élevée: %d)\n", incompleteObjectiveValue);
                }
                else {
                    // Équilibrer entre les deux
                    if (maxSameColorCards >= 4) {
                        // Si nous avons beaucoup de cartes d'une même couleur, essayer de les utiliser
                        priority = BUILD_NETWORK;
                        printf("Priorité: CONSTRUIRE RÉSEAU (beaucoup de cartes %s: %d)\n", 
                              (colorWithMostCards < 10) ? (const char*[]){
                                  "None", "Purple", "White", "Blue", "Yellow", 
                                  "Orange", "Black", "Red", "Green", "Locomotive"
                              }[colorWithMostCards] : "Unknown", 
                              maxSameColorCards);
                    } else if (state->turnCount % 3 == 0) {  // Alterner
                        priority = BUILD_NETWORK;
                        printf("Priorité: CONSTRUIRE RÉSEAU (alternance)\n");
                    } else {
                       priority = COMPLETE_OBJECTIVES;
                       printf("Priorité: COMPLÉTER OBJECTIFS (alternance)\n");
                   }
               }
           }
        }
    }
   
    // 3. TROUVER LES ROUTES POSSIBLES
    int possibleRoutes[MAX_ROUTES] = {-1};
    CardColor possibleColors[MAX_ROUTES] = {NONE};
    int possibleLocomotives[MAX_ROUTES] = {0};
   
    int numPossibleRoutes = findPossibleRoutes(state, possibleRoutes, possibleColors, possibleLocomotives);
    printf("Trouvé %d routes possibles à prendre\n", numPossibleRoutes);
   
    if (numPossibleRoutes <= 0) {
        // Aucune route possible, piocher des cartes
        priority = DRAW_CARDS;
        printf("Priorité modifiée: PIOCHER DES CARTES (aucune route possible)\n");
    }
   
    // Forcer la prise de route après trop de pioches consécutives
    if (consecutiveDraws >= 4 && numPossibleRoutes > 0) {
        printf("FORCE MAJEURE: Trop de pioches consécutives (%d), forcer la prise d'une route\n", consecutiveDraws);
        priority = BUILD_NETWORK;
    }
   
    // 4. PRENDRE UNE DÉCISION EN FONCTION DE LA PRIORITÉ
    switch (priority) {
        case COMPLETE_OBJECTIVES: {
            // 4.1 FOCUS SUR LA COMPLÉTION DES OBJECTIFS
            if (numPossibleRoutes > 0) {
                // 4.1.1 Trouver la meilleure route pour compléter les objectifs
                int bestRouteIndex = 0;
                int bestScore = -1;
               
                for (int i = 0; i < numPossibleRoutes; i++) {
                    int routeIndex = possibleRoutes[i];
                    if (routeIndex < 0 || routeIndex >= state->nbTracks) continue;
                   
                    int objectiveScore = calculateObjectiveProgress(state, routeIndex);
                    int length = state->routes[routeIndex].length;
                   
                    // Calculer un score global pour cette route
                    int routeScore = 0;
                   
                    // Si la route aide à compléter un objectif, lui donner beaucoup plus de valeur
                    if (objectiveScore > 0) {
                        routeScore += objectiveScore * 5;  // Multiplier par 5 au lieu de 2
                    }
                   
                    // Bonus pour les routes longues (plus de points)
                    if (length >= 5) {
                        routeScore += length * 100;  // Bonus massif pour les routes très longues
                    } 
                    else if (length >= 4) {
                        routeScore += length * 50;  // Bonus important pour les routes longues
                    }
                    else if (length >= 3) {
                        routeScore += length * 25;  // Bonus modéré pour les routes moyennes
                    }
                   
                    // Vérifier s'il s'agit d'une connexion directe pour un objectif
                    int from = state->routes[routeIndex].from;
                    int to = state->routes[routeIndex].to;
                    
                    for (int j = 0; j < state->nbObjectives; j++) {
                        if (!isObjectiveCompleted(state, state->objectives[j])) {
                            if ((state->objectives[j].from == from && state->objectives[j].to == to) ||
                                (state->objectives[j].from == to && state->objectives[j].to == from)) {
                                routeScore += 1000; // Priorité absolue
                                printf("Connexion directe trouvée pour objectif %d! Score +1000\n", j+1);
                            }
                        }
                    }
                   
                    if (routeScore > bestScore) {
                        bestScore = routeScore;
                        bestRouteIndex = i;
                    }
                }
               
                // Si la meilleure route a un score acceptable, la prendre
                // Ne pas prendre de route courte en début/milieu de partie sauf urgence
                int routeIndex = possibleRoutes[bestRouteIndex];
                int length = state->routes[routeIndex].length;
                
                if (length <= 2 && phase < PHASE_LATE && state->turnCount < 15 && consecutiveDraws < 4 && bestScore < 1000) {
                    printf("Route trop courte (longueur %d) en phase %d. Mieux vaut piocher que de gaspiller des wagons.\n", 
                           length, phase);
                    priority = DRAW_CARDS;
                    break;
                }
                
                // Seuil minimal - ne pas prendre de routes trop tôt à moins qu'elles soient excellentes
                if (bestScore < 20 && phase == PHASE_EARLY && consecutiveDraws < 4 && !state->lastTurn) {
                    printf("Toutes les routes ont un score faible (%d), continuer à piocher\n", bestScore);
                    priority = DRAW_CARDS;
                    break;
                }
                
                // Sinon, prendre la meilleure route
                if (bestScore > -10) {
                    int from = state->routes[routeIndex].from;
                    int to = state->routes[routeIndex].to;
                    CardColor color = possibleColors[bestRouteIndex];
                    int nbLocomotives = possibleLocomotives[bestRouteIndex];
                   
                    moveData->action = CLAIM_ROUTE;
                    moveData->claimRoute.from = from;
                    moveData->claimRoute.to = to;
                    moveData->claimRoute.color = color;
                    moveData->claimRoute.nbLocomotives = nbLocomotives;
                   
                    printf("Décision: Prendre route %d -> %d pour objectifs (score: %d)\n", 
                         from, to, bestScore);
                         
                    // Réinitialiser le compteur de pioches consécutives
                    consecutiveDraws = 0;
                    return 1;
                } else {
                    // Pas de route intéressante pour les objectifs, piocher des cartes
                    priority = DRAW_CARDS;
                    printf("Priorité modifiée: PIOCHER DES CARTES (pas de route utile pour objectifs)\n");
                }
            } else {
                // Pas de route possible, piocher des cartes
                priority = DRAW_CARDS;
                printf("Priorité modifiée: PIOCHER DES CARTES (aucune route possible)\n");
            }
            break;
        }
       
        case BLOCK_OPPONENT: {
            // 4.2 FOCUS SUR LE BLOCAGE DE L'ADVERSAIRE
            if (numPossibleRoutes > 0) {
                // 4.2.1 Identifier les routes critiques à bloquer
                int routesToBlock[MAX_ROUTES];
                int blockingPriorities[MAX_ROUTES];
               
                int numRoutesToBlock = findCriticalRoutesToBlock(state, routesToBlock, blockingPriorities);
                printf("Trouvé %d routes critiques à bloquer\n", numRoutesToBlock);
               
                // 4.2.2 Vérifier si l'une des routes à bloquer est parmi les routes possibles
                int bestBlockingRoute = -1;
                int bestBlockingScore = -1;
               
                for (int i = 0; i < numRoutesToBlock; i++) {
                    int blockRouteIndex = routesToBlock[i];
                    if (blockRouteIndex < 0 || blockRouteIndex >= state->nbTracks) continue;
                   
                    // Vérifier si nous pouvons prendre cette route
                    for (int j = 0; j < numPossibleRoutes; j++) {
                        if (possibleRoutes[j] == blockRouteIndex) {
                            int score = blockingPriorities[i];
                            int length = state->routes[blockRouteIndex].length;
                           
                            // Bonus pour les routes longues (plus de points)
                            if (length >= 5) {
                                score += length * 50;  // Bonus massif pour les routes très longues
                            } 
                            else if (length >= 4) {
                                score += length * 25;  // Bonus important pour les routes longues
                            }
                            else if (length >= 3) {
                                score += length * 10;  // Bonus modéré pour les routes moyennes
                            }
                           
                            if (score > bestBlockingScore) {
                                bestBlockingScore = score;
                                bestBlockingRoute = j;
                            }
                           
                            break;
                        }
                    }
                }
               
                // 4.2.3 Si nous avons trouvé une route à bloquer, la prendre
                // Réduire le seuil pour être plus souple
                if (bestBlockingRoute >= 0 && bestBlockingScore > 20) {
                    int routeIndex = possibleRoutes[bestBlockingRoute];
                    int from = state->routes[routeIndex].from;
                    int to = state->routes[routeIndex].to;
                    CardColor color = possibleColors[bestBlockingRoute];
                    int nbLocomotives = possibleLocomotives[bestBlockingRoute];
                   
                    moveData->action = CLAIM_ROUTE;
                    moveData->claimRoute.from = from;
                    moveData->claimRoute.to = to;
                    moveData->claimRoute.color = color;
                    moveData->claimRoute.nbLocomotives = nbLocomotives;
                   
                    printf("Décision: BLOQUER route %d -> %d (score: %d)\n", 
                         from, to, bestBlockingScore);
                   
                    // Réinitialiser le compteur de pioches consécutives
                    consecutiveDraws = 0;
                    return 1;
                } else {
                    // 4.2.4 Si aucune route à bloquer, passer à la construction de réseau
                    printf("Aucune route critique à bloquer, passer à la construction de réseau\n");
                    priority = BUILD_NETWORK;
                    // Continuer à l'itération suivante
                   
                    // Si nous avons déjà essayé BUILD_NETWORK, passer directement à DRAW_CARDS
                    if (priority == BLOCK_OPPONENT) {
                        priority = DRAW_CARDS;
                        printf("Priorité modifiée: PIOCHER DES CARTES (pas de blocage possible)\n");
                    }
                }
            } else {
                // Pas de route possible, piocher des cartes
                priority = DRAW_CARDS;
                printf("Priorité modifiée: PIOCHER DES CARTES (aucune route possible)\n");
            }
            break;
        }
       
        case BUILD_NETWORK: {
            // 4.3 FOCUS SUR LA CONSTRUCTION DE RÉSEAU (maximiser les points de route)
            if (numPossibleRoutes > 0) {
                // 4.3.1 Privilégier les routes longues pour maximiser les points
                int bestRouteIndex = -1;
                int bestScore = -1;
               
                for (int i = 0; i < numPossibleRoutes; i++) {
                    int routeIndex = possibleRoutes[i];
                    if (routeIndex < 0 || routeIndex >= state->nbTracks) continue;
                   
                    int length = state->routes[routeIndex].length;
                    int score = 0;
                   
                    // Table de points modifiée pour fortement favoriser les routes longues
                    switch (length) {
                        case 1: score = 1; break;
                        case 2: score = 5; break;
                        case 3: score = 20; break;  // Augmenté de 15 à 20
                        case 4: score = 50; break;  // Augmenté de 35 à 50
                        case 5: score = 100; break; // Augmenté de 60 à 100
                        case 6: score = 150; break; // Augmenté de 90 à 150
                        default: score = 0;
                    }
                    
                    // Ne pas prendre de route courte en début/milieu de partie sauf urgence
                    if (length <= 2 && phase < PHASE_LATE && state->turnCount < 15 && consecutiveDraws < 4) {
                        score -= 50; // Forte pénalité pour dissuader les routes courtes en début de partie
                    }
                   
                    // Bonus pour les routes qui connectent à notre réseau existant
                    int from = state->routes[routeIndex].from;
                    int to = state->routes[routeIndex].to;
                    bool connectsToNetwork = false;
                   
                    for (int j = 0; j < state->nbClaimedRoutes; j++) {
                        int claimedRouteIndex = state->claimedRoutes[j];
                        if (claimedRouteIndex < 0 || claimedRouteIndex >= state->nbTracks) continue;
                       
                        if (state->routes[claimedRouteIndex].from == from || 
                            state->routes[claimedRouteIndex].to == from ||
                            state->routes[claimedRouteIndex].from == to || 
                            state->routes[claimedRouteIndex].to == to) {
                            connectsToNetwork = true;
                            break;
                        }
                    }
                   
                    if (connectsToNetwork) {
                        score += 30;  // Bonus significatif pour la connexion
                    }
                    
                    // Si c'est une connexion directe pour un objectif, la prendre quand même
                    for (int j = 0; j < state->nbObjectives; j++) {
                        if (!isObjectiveCompleted(state, state->objectives[j])) {
                            if ((state->objectives[j].from == from && state->objectives[j].to == to) ||
                                (state->objectives[j].from == to && state->objectives[j].to == from)) {
                                score += 1000; // Priorité absolue
                            }
                        }
                    }
                   
                    if (score > bestScore) {
                        bestScore = score;
                        bestRouteIndex = i;
                    }
                }
                
                // Seuil minimal - ne pas prendre de routes trop tôt à moins qu'elles soient excellentes
                if (bestScore < 20 && phase == PHASE_EARLY && consecutiveDraws < 4 && !state->lastTurn) {
                    printf("Toutes les routes ont un score faible (%d), continuer à piocher\n", bestScore);
                    priority = DRAW_CARDS;
                    break;
                }
               
                // 4.3.2 Prendre la meilleure route pour le réseau
                int routeIndex = possibleRoutes[bestRouteIndex];
                int from = state->routes[routeIndex].from;
                int to = state->routes[routeIndex].to;
                CardColor color = possibleColors[bestRouteIndex];
                int nbLocomotives = possibleLocomotives[bestRouteIndex];
               
                moveData->action = CLAIM_ROUTE;
                moveData->claimRoute.from = from;
                moveData->claimRoute.to = to;
                moveData->claimRoute.color = color;
                moveData->claimRoute.nbLocomotives = nbLocomotives;
               
                printf("Décision: Construire réseau, route %d -> %d\n", from, to);
               
                // Réinitialiser le compteur de pioches consécutives
                consecutiveDraws = 0;
                return 1;
            } else {
                // Pas de route possible, piocher des cartes
                priority = DRAW_CARDS;
                printf("Priorité modifiée: PIOCHER DES CARTES (aucune route possible)\n");
            }
            break;
        }
       
        case DRAW_CARDS: {
            // 4.4 PIOCHER DES CARTES STRATÉGIQUEMENT
           
            // 4.4.1 Analyser nos besoins en cartes pour les objectifs prioritaires
            int colorNeeds[10] = {0};  // Pour chaque couleur, combien en avons-nous besoin
            bool needMoreCards = false;
           
            // Pour chaque objectif incomplet, analyser les routes nécessaires
            for (int i = 0; i < state->nbObjectives; i++) {
                if (isObjectiveCompleted(state, state->objectives[i])) continue;
               
                int objFrom = state->objectives[i].from;
                int objTo = state->objectives[i].to;
               
                // Trouver le chemin le plus court
                int path[MAX_CITIES];
                int pathLength = 0;
                if (findShortestPath(state, objFrom, objTo, path, &pathLength) > 0) {
                    // Pour chaque segment du chemin
                    for (int j = 0; j < pathLength - 1; j++) {
                        int cityA = path[j];
                        int cityB = path[j+1];
                       
                        // Trouver la route correspondante
                        for (int r = 0; r < state->nbTracks; r++) {
                            if (((state->routes[r].from == cityA && state->routes[r].to == cityB) ||
                                 (state->routes[r].from == cityB && state->routes[r].to == cityA)) &&
                                state->routes[r].owner == 0) {  // Route non prise
                               
                                CardColor routeColor = state->routes[r].color;
                                int length = state->routes[r].length;
                               
                                // Si c'est une route grise, toutes les couleurs sont possibles
                                if (routeColor == LOCOMOTIVE) {
                                    needMoreCards = true;  // Nous voulons plus de cartes pour cette route
                                } else {
                                    // Calculer combien il nous manque de cartes de cette couleur
                                    int have = state->nbCardsByColor[routeColor];
                                    int needed = length;
                                    if (have < needed) {
                                        colorNeeds[routeColor] += (needed - have);
                                        needMoreCards = true;
                                    }
                                }
                               
                                break;
                            }
                        }
                    }
                }
            }
            
            // Analyser les routes longues disponibles et leurs couleurs nécessaires
            for (int i = 0; i < state->nbTracks; i++) {
                if (state->routes[i].owner == 0 && state->routes[i].length >= 4) {
                    CardColor routeColor = state->routes[i].color;
                    if (routeColor != LOCOMOTIVE) {
                        colorNeeds[routeColor] += state->routes[i].length * 2; // Donner plus de valeur aux couleurs pour routes longues
                    }
                }
            }
           
            // 4.4.2 Si nous avons déjà assez de cartes, essayer de prendre une route
            if (!needMoreCards && numPossibleRoutes > 0) {
                printf("Nous avons déjà assez de cartes, essayer de prendre une route\n");
                priority = COMPLETE_OBJECTIVES;
                // Continuer à l'itération suivante
                break;
            }
           
            // 4.4.3 Déterminer quelle carte piocher (visible ou aveugle)
           
            // D'abord, vérifier s'il y a une locomotive visible
            for (int i = 0; i < 5; i++) {
                if (state->visibleCards[i] == LOCOMOTIVE) {
                    moveData->action = DRAW_CARD;
                    moveData->drawCard = LOCOMOTIVE;
                    printf("Décision: Piocher la locomotive visible\n");
                    // Incrémenter le compteur de pioches consécutives
                    consecutiveDraws++;
                    return 1;
                }
            }
           
            // Ensuite, chercher une carte visible qui correspond à nos besoins
            int bestCardIndex = -1;
            int bestCardValue = 0;
           
            for (int i = 0; i < 5; i++) {
                CardColor card = state->visibleCards[i];
                if (card == NONE) continue;
               
                int value = 0;
               
                // Valeur basée sur nos besoins
                if (colorNeeds[card] > 0) {
                    value += colorNeeds[card] * 10;
                }
               
                // Bonus si nous avons déjà des cartes de cette couleur
                if (state->nbCardsByColor[card] > 0) {
                    value += state->nbCardsByColor[card] * 5;
                    
                    // Bonus important si une carte nous permet de compléter une route
                    for (int r = 0; r < state->nbTracks; r++) {
                        if (state->routes[r].owner == 0) {  // Route non prise
                            CardColor routeColor = state->routes[r].color;
                            int length = state->routes[r].length;
                            
                            // Pour les routes colorées correspondant à notre carte
                            if (routeColor == card) {
                                int cardsNeeded = length - state->nbCardsByColor[card];
                                // Si nous avons presque assez de cartes pour prendre cette route
                                if (cardsNeeded == 1) {
                                    value += length * 15; // Bonus très important pour compléter une route
                                    
                                    // Bonus supplémentaire pour les routes longues
                                    if (length >= 4) {
                                        value += length * 20; // Favoriser les routes longues
                                    }
                                }
                            }
                        }
                    }
                }
               
                // Éviter d'avoir trop de cartes d'une seule couleur
                if (state->nbCardsByColor[card] > 8) {
                    value -= (state->nbCardsByColor[card] - 8) * 5;
                }
               
                if (value > bestCardValue) {
                    bestCardValue = value;
                    bestCardIndex = i;
                }
            }
           
            // Si nous avons trouvé une bonne carte visible, la piocher
            if (bestCardIndex >= 0 && bestCardValue > 5) {
                moveData->action = DRAW_CARD;
                moveData->drawCard = state->visibleCards[bestCardIndex];
                printf("Décision: Piocher la carte visible %d (valeur: %d)\n", 
                     moveData->drawCard, bestCardValue);
                // Incrémenter le compteur de pioches consécutives
                consecutiveDraws++;
                return 1;
            }
           
            // Sinon, piocher une carte aveugle
            moveData->action = DRAW_BLIND_CARD;
            printf("Décision: Piocher une carte aveugle\n");
            // Incrémenter le compteur de pioches consécutives
            consecutiveDraws++;
            return 1;
        }
       
        default:
            printf("Cas non géré, pioche par défaut\n");
            moveData->action = DRAW_BLIND_CARD;
            // Incrémenter le compteur de pioches consécutives
            consecutiveDraws++;
            return 1;
    }
   
    // Si nous avons changé de priorité, exécuter à nouveau mais en évitant les boucles infinies
    static int recursionDepth = 0;
    if (recursionDepth < 2) {  // Limiter la récursivité pour éviter les boucles infinies
        recursionDepth++;
        int result = superAdvancedStrategy(state, moveData);
        recursionDepth--;
        return result;
    }
   
    // Fallback: piocher une carte aveugle
    printf("Décision par défaut: Piocher une carte aveugle\n");
    moveData->action = DRAW_BLIND_CARD;
    // Incrémenter le compteur de pioches consécutives
    consecutiveDraws++;
    return 1;
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