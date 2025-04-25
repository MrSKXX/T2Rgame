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
    // Safety check
    if (routeIndex < 0 || routeIndex >= state->nbTracks) {
        printf("ERROR: Invalid route index %d in calculateObjectiveProgress\n", routeIndex);
        return 0;
    }

    int from = state->routes[routeIndex].from;
    int to = state->routes[routeIndex].to;
    int progress = 0;
    
    // Check each incomplete objective
    for (int i = 0; i < state->nbObjectives; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            int objFrom = state->objectives[i].from;
            int objTo = state->objectives[i].to;
            int objScore = state->objectives[i].score;
            
            // Find optimal path for this objective
            int path[MAX_CITIES];
            int pathLength = 0;
            
            if (findShortestPath(state, objFrom, objTo, path, &pathLength) >= 0) {
                // Check if our route is on this path
                for (int j = 0; j < pathLength - 1; j++) {
                    if ((path[j] == from && path[j+1] == to) ||
                        (path[j] == to && path[j+1] == from)) {
                        progress += objScore;
                        break;
                    }
                }
            }
        }
    }
    
    return progress;
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
    // Analyze what colors we need most
    int colorNeeds[10] = {0};
    
    // For each incomplete objective
    for (int i = 0; i < state->nbObjectives; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            int objFrom = state->objectives[i].from;
            int objTo = state->objectives[i].to;
            
            // Find the optimal path
            int path[MAX_CITIES];
            int pathLength = 0;
            
            if (findShortestPath(state, objFrom, objTo, path, &pathLength) >= 0) {
                // Check each segment of the path
                for (int j = 0; j < pathLength - 1; j++) {
                    int pathFrom = path[j];
                    int pathTo = path[j+1];
                    
                    // Find the route for this segment
                    for (int k = 0; k < state->nbTracks; k++) {
                        if (((state->routes[k].from == pathFrom && state->routes[k].to == pathTo) ||
                             (state->routes[k].from == pathTo && state->routes[k].to == pathFrom)) &&
                            state->routes[k].owner == 0) {  // Unclaimed route
                            
                            CardColor routeColor = state->routes[k].color;
                            if (routeColor != LOCOMOTIVE) {
                                colorNeeds[routeColor] += state->routes[k].length;
                            } else {
                                // Gray route - need any color or locomotives
                                colorNeeds[LOCOMOTIVE] += 1;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Find best visible card based on needs
    int bestCardIndex = -1;
    int bestCardScore = 0;
    
    for (int i = 0; i < 5; i++) {
        CardColor card = state->visibleCards[i];
        // Safety check - ensure the card color is valid
        if (card != NONE && card >= 0 && card < 10) {
            int score = 0;
            
            // Locomotives are always valuable
            if (card == LOCOMOTIVE) {
                score = 100;
            } 
            // Score other cards based on need
            else {
                score = colorNeeds[card] * 5;
                
                // Bonus if we already have some of this color
                if (state->nbCardsByColor[card] > 0) {
                    score += state->nbCardsByColor[card] * 3;
                }
            }
            
            if (score > bestCardScore) {
                bestCardScore = score;
                bestCardIndex = i;
            }
        }
    }
    
    // If no good visible card and blind draw has potential to be better
    int totalNeeds = 0;
    for (int c = 1; c < 10; c++) {
        totalNeeds += colorNeeds[c];
    }
    
    // If we need many different colors, blind draw might be better
    if (totalNeeds > 20 && bestCardScore < 30) {
        return -1;  // Recommend blind draw
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
    
    // Safety check - ensure arrays are valid
    if (!blockingRoutes || !blockingPriorities) {
        printf("ERROR: Invalid arrays in findCriticalBlockingRoutes\n");
        return 0;
    }
    
    // For each unclaimed route
    for (int i = 0; i < state->nbTracks && count < MAX_BLOCKING_ROUTES; i++) {
        if (state->routes[i].owner == 0) {
            int from = state->routes[i].from;
            int to = state->routes[i].to;
            
            // Skip if we don't have enough wagons
            if (state->routes[i].length > state->wagonsLeft) {
                continue;
            }
            
            // Skip if these cities are out of bounds for our interest array
            if (from >= MAX_CITIES || to >= MAX_CITIES) {
                continue;
            }
            
            int blockingValue = 0;
            
            // 1. Route connects cities the opponent seems interested in
            if (opponentCitiesOfInterest[from] > 0 && opponentCitiesOfInterest[to] > 0) {
                blockingValue += opponentCitiesOfInterest[from] + opponentCitiesOfInterest[to];
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
                
                for (int j = 0; j < state->nbCities && j < MAX_CITIES; j++) {
                    if (opponentCitiesOfInterest[j] <= 0) continue;
                    
                    for (int k = j+1; k < state->nbCities && k < MAX_CITIES; k++) {
                        if (opponentCitiesOfInterest[k] <= 0) continue;
                        
                        int interest = opponentCitiesOfInterest[j] + opponentCitiesOfInterest[k];
                        
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
                    
                    if (cityA < 0 || cityB < 0) continue;  // Skip invalid pairs
                    
                    int path[MAX_CITIES];
                    int pathLength = 0;
                    
                    int distance = findShortestPath(state, cityA, cityB, path, &pathLength);
                    
                    if (distance > 0 && pathLength < MAX_CITIES) {
                        for (int s = 0; s < pathLength - 1; s++) {
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
    if (count < MAX_ROUTES) {
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
    // This function enhances the objective selection strategy by:
    // 1. Analyzing how objectives can be completed together (shared routes)
    // 2. Planning early-game routes to maximize objective completion
    // 3. Considering opponent's likely moves
    
    printf("Advanced objective evaluation:\n");
    
    // Initialize choices to false
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
        
        // Analyze path complexity
        int routesNeeded = 0;
        int routesOwnedByUs = 0;
        int routesOwnedByOpponent = 0;
        int routesAvailable = 0;
        int totalLength = 0;
        
        for (int j = 0; j < pathLength - 1; j++) {
            int pathFrom = path[j];
            int pathTo = path[j+1];
            
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
                        // If opponent owns a route, this path is blocked
                        scores[i] = -500;
                        printf("  - Path blocked by opponent\n");
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
        
        for (int j = 0; j < pathLength - 1; j++) {
            int pathFrom = path[j];
            int pathTo = path[j+1];
            
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
        
        // Check synergy with existing objectives
        for (int j = 0; j < state->nbObjectives; j++) {
            int existingFrom = state->objectives[j].from;
            int existingTo = state->objectives[j].to;
            
            // Shared endpoints
            if (from == existingFrom || from == existingTo || to == existingFrom || to == existingTo) {
                synergyScore += 20;
            }
            
            // Shared routes
            int existingPath[MAX_CITIES];
            int existingPathLength = 0;
            
            if (findShortestPath(state, existingFrom, existingTo, existingPath, &existingPathLength) >= 0) {
                int sharedRoutes = 0;
                
                for (int p1 = 0; p1 < pathLength - 1 && p1 < MAX_CITIES - 1; p1++) {
                    for (int p2 = 0; p2 < existingPathLength - 1 && p2 < MAX_CITIES - 1; p2++) {
                        if ((path[p1] == existingPath[p2] && path[p1+1] == existingPath[p2+1]) ||
                            (path[p1] == existingPath[p2+1] && path[p1+1] == existingPath[p2])) {
                            sharedRoutes++;
                        }
                    }
                }
                
                synergyScore += sharedRoutes * 15;
            }
        }
        
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
        
        // Calculate final score
        scores[i] = baseScore + (completionProgress * 100) + cardMatchScore + synergyScore - competitionPenalty - difficultyPenalty;
        
        // Log components for debugging
        printf("  - Base score (points/length): %.1f\n", baseScore);
        printf("  - Completion: %.1f%%\n", completionProgress * 100);
        printf("  - Card matching: %.1f\n", cardMatchScore);
        printf("  - Synergy: %.1f\n", synergyScore);
        printf("  - Competition: -%.1f\n", competitionPenalty);
        printf("  - Difficulty: -%.1f\n", difficultyPenalty);
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
    if (scores[sortedIndices[0]] > 0) {
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
        
        if (scores[sortedIndices[2]] > 60 || (state->nbObjectives == 0 && scores[sortedIndices[2]] > 0)) {
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
        
        if (scores[sortedIndices[2]] > 120) {
            chooseObjectives[sortedIndices[2]] = true;
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
    
    // Make sure we take at least one objective
    if (numToChoose == 0 && scores[sortedIndices[0]] > -100) {
        chooseObjectives[sortedIndices[0]] = true;
        numToChoose = 1;
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

// Safety check - limit the number of routes to process
const int MAX_ROUTES_TO_SORT = 100;
if (numPossibleRoutes > MAX_ROUTES_TO_SORT) {
printf("WARNING: Too many possible routes (%d), limiting to %d\n", 
numPossibleRoutes, MAX_ROUTES_TO_SORT);
numPossibleRoutes = MAX_ROUTES_TO_SORT;
}

// Create a copy of the arrays to avoid modifying the originals
int routes[MAX_ROUTES_TO_SORT];
CardColor colors[MAX_ROUTES_TO_SORT];
int locomotives[MAX_ROUTES_TO_SORT];

// Safety check - ensure arrays are valid
if (!possibleRoutes || !possibleColors || !possibleLocomotives) {
printf("ERROR: Invalid arrays in advancedRoutePrioritization\n");
return;
}

// Create a copy of the input arrays to avoid modifying originals during sorting
for (int i = 0; i < numPossibleRoutes && i < MAX_ROUTES_TO_SORT; i++) {
routes[i] = possibleRoutes[i];
colors[i] = possibleColors[i];
locomotives[i] = possibleLocomotives[i];
}

// We'll score routes based on multiple factors
int scores[MAX_ROUTES_TO_SORT];
// Initialize all scores to 0
for (int i = 0; i < MAX_ROUTES_TO_SORT; i++) {
scores[i] = 0;
}

// 1. Find critical blocking routes - limit to a reasonable number
int blockingRoutes[MAX_ROUTES_TO_SORT];
int blockingPriorities[MAX_ROUTES_TO_SORT];
int numBlockingRoutes = findCriticalBlockingRoutes(state, blockingRoutes, blockingPriorities);

// Limit blocking routes to process
if (numBlockingRoutes > 20) {
numBlockingRoutes = 20;
}

// 2. Evaluate each route's utility
for (int i = 0; i < numPossibleRoutes; i++) {
int routeIndex = routes[i];

// Skip invalid routes
if (routeIndex < 0 || routeIndex >= state->nbTracks) {
printf("WARNING: Invalid route index %d in advancedRoutePrioritization\n", routeIndex);
scores[i] = -1000; // Very low score to place at the end of sorting
continue;
}

// Start with the basic utility
int baseScore = enhancedEvaluateRouteUtility(state, routeIndex);
scores[i] = baseScore;

// Check if this is a blocking route - but limit the search
for (int j = 0; j < numBlockingRoutes; j++) {
if (blockingRoutes[j] == routeIndex) {
// Add blocking value to score
scores[i] += blockingPriorities[j];
// No need to print this again
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
if (!isObjectiveCompleted(state, state->objectives[j])) {
// Find if this route starts a path to the objective
int from = state->routes[routeIndex].from;
int to = state->routes[routeIndex].to;

// Safely compare with explicit casting to avoid signed/unsigned comparison warnings
unsigned int objFrom = state->objectives[j].from;
unsigned int objTo = state->objectives[j].to;

if ((int)objFrom == from || (int)objTo == from ||
(int)objFrom == to || (int)objTo == to) {
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

// Sort routes by score - using a safer bubble sort with limited iterations
for (int i = 0; i < numPossibleRoutes - 1 && i < 50; i++) {  // Limit sorting passes to 50
for (int j = 0; j < numPossibleRoutes - i - 1 && j < 50; j++) {  // Limit comparisons too
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

// Print top routes - but limit to 5 to prevent log spam
printf("Routes sorted by advanced priority:\n");
int routesToShow = numPossibleRoutes < 5 ? numPossibleRoutes : 5;

// Track the routes we've shown to prevent duplicates
int shownFrom[5] = {-1, -1, -1, -1, -1};
int shownTo[5] = {-1, -1, -1, -1, -1};
int uniqueRoutesShown = 0;

for (int i = 0; i < numPossibleRoutes && uniqueRoutesShown < routesToShow; i++) {
int routeIndex = routes[i];

// Ensure the route is valid before printing
if (routeIndex >= 0 && routeIndex < state->nbTracks) {
int from = state->routes[routeIndex].from;
int to = state->routes[routeIndex].to;

// Check if we've already shown this route (by endpoints)
bool isDuplicate = false;
for (int j = 0; j < uniqueRoutesShown; j++) {
if ((shownFrom[j] == from && shownTo[j] == to) ||
(shownFrom[j] == to && shownTo[j] == from)) {
isDuplicate = true;
break;
}
}

// Only show if it's not a duplicate
if (!isDuplicate && uniqueRoutesShown < 5) {
printf("  %d. From %d to %d, score: %d\n", 
uniqueRoutesShown+1, from, to, scores[i]);

// Record this route as shown
shownFrom[uniqueRoutesShown] = from;
shownTo[uniqueRoutesShown] = to;
uniqueRoutesShown++;
}
}
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
    
    // Determine the current game phase
    int phase = determineGamePhase(state);
    printf("Current game phase: %d\n", phase);
    
    // Increment turn counter
    state->turnCount++;
    
    // Find possible routes
    int possibleRoutes[MAX_ROUTES] = {0};
    CardColor possibleColors[MAX_ROUTES] = {0};
    int possibleLocomotives[MAX_ROUTES] = {0};
    
    int numPossibleRoutes = findPossibleRoutes(state, possibleRoutes, possibleColors, possibleLocomotives);
    printf("Found %d possible routes to claim\n", numPossibleRoutes);
    
    // Safety check - don't exceed array bounds
    if (numPossibleRoutes > MAX_ROUTES - 1) {
        printf("WARNING: Too many possible routes (%d), limiting to %d\n", 
              numPossibleRoutes, MAX_ROUTES - 1);
        numPossibleRoutes = MAX_ROUTES - 1;
    }
    
    // No objectives? Draw some first
    if (state->nbObjectives == 0) {
        moveData->action = DRAW_OBJECTIVES;
        printf("Strategy decided: draw new objectives (we have none)\n");
        return 1;
    }
    
    // Check objective completion status
    int completedObjectives = 0;
    for (int i = 0; i < state->nbObjectives; i++) {
        if (isObjectiveCompleted(state, state->objectives[i])) {
            completedObjectives++;
        }
    }
    int incompleteObjectives = state->nbObjectives - completedObjectives;
    
    // Use advanced route prioritization
    if (numPossibleRoutes > 0) {
        advancedRoutePrioritization(state, possibleRoutes, possibleColors, possibleLocomotives, numPossibleRoutes);
    }
    
    // Decision making based on game phase
    bool shouldClaimRoute = false;
    
    // EARLY GAME STRATEGY
    if (phase == PHASE_EARLY) {
        // In early game, balance between drawing cards and claiming strategic routes
        
        // If we have few cards, draw more unless there's a high-value route
        if (state->nbCards < 7 && numPossibleRoutes > 0) {
            // Make sure the index is valid
            if (possibleRoutes[0] >= 0 && possibleRoutes[0] < state->nbTracks) {
                int bestRouteUtility = enhancedEvaluateRouteUtility(state, possibleRoutes[0]);
                
                // Only claim if it's a very good route
                if (bestRouteUtility > 60) {
                    shouldClaimRoute = true;
                    printf("Early game: claiming high-value route (utility: %d)\n", bestRouteUtility);
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
        // More focus on completing objectives and blocking opponent
        
        if (numPossibleRoutes > 0) {
            // Make sure the index is valid
            if (possibleRoutes[0] >= 0 && possibleRoutes[0] < state->nbTracks) {
                int bestRouteIndex = possibleRoutes[0];
                int bestRouteUtility = enhancedEvaluateRouteUtility(state, bestRouteIndex);
                
                // Check if this route helps with objectives
                int objectiveProgress = calculateObjectiveProgress(state, bestRouteIndex);
                
                // Check if this route blocks opponent
                bool isBlockingRoute = false;
                int from = state->routes[bestRouteIndex].from;
                int to = state->routes[bestRouteIndex].to;
                
                if (from < MAX_CITIES && to < MAX_CITIES && 
                    opponentCitiesOfInterest[from] + opponentCitiesOfInterest[to] > 3) {
                    isBlockingRoute = true;
                }
                
                // Decide whether to claim based on utility, objective progress, or blocking value
                if (bestRouteUtility > 30 || objectiveProgress > 0 || isBlockingRoute) {
                    shouldClaimRoute = true;
                    if (isBlockingRoute) {
                        printf("Middle game: blocking opponent's potential route\n");
                    } else if (objectiveProgress > 0) {
                        printf("Middle game: claiming route for objective progress\n");
                    } else {
                        printf("Middle game: claiming high-value route\n");
                    }
                } else {
                    printf("Middle game: no valuable routes, drawing cards\n");
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
        // Prioritize completing objectives and high-point routes
        
        if (numPossibleRoutes > 0) {
            // Make sure the index is valid
            if (possibleRoutes[0] >= 0 && possibleRoutes[0] < state->nbTracks) {
                int bestRouteIndex = possibleRoutes[0];
                int objectiveProgress = calculateObjectiveProgress(state, bestRouteIndex);
                int length = state->routes[bestRouteIndex].length;
                
                // In late game, almost always claim if it helps with objectives
                if (objectiveProgress > 0 || length >= 4) {
                    shouldClaimRoute = true;
                    if (objectiveProgress > 0) {
                        printf("Late game: claiming route for objective progress\n");
                    } else {
                        printf("Late game: claiming long route for points\n");
                    }
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
        // Maximize points - almost always claim routes if possible
        if (numPossibleRoutes > 0) {
            shouldClaimRoute = true;
            printf("Final phase: maximizing points by claiming route\n");
        }
    }
    
    // If we decided to claim a route
    if (numPossibleRoutes > 0 && shouldClaimRoute) {
        // Make sure the index is valid
        if (possibleRoutes[0] >= 0 && possibleRoutes[0] < state->nbTracks) {
            // Take the highest utility route (first after sorting)
            int routeIndex = possibleRoutes[0];
            int from = state->routes[routeIndex].from;
            int to = state->routes[routeIndex].to;
            CardColor color = possibleColors[0];
            int nbLocomotives = possibleLocomotives[0];
            
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
    
    // Draw objectives if we have few and it's not the final phase
    if (state->nbObjectives < 3 && phase != PHASE_FINAL) {
        moveData->action = DRAW_OBJECTIVES;
        printf("Strategy decided: draw new objectives (we have only %d)\n", state->nbObjectives);
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