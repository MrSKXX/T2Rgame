#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "strategy.h"
#include "rules.h"
#include "gamestate.h"

static int currentObjectiveIndex = -1;
static int currentPath[MAX_CITIES];
static int currentPathLength = 0;

int findBestObjective(GameState* state);
int canTakeRoute(GameState* state, int from, int to, MoveData* moveData);
int drawCardsForRoute(GameState* state, int from, int to, MoveData* moveData);
int drawBestCard(GameState* state, MoveData* moveData);
void simpleChooseObjectives(GameState* state, Objective* objectives, unsigned char* chooseObjectives);
int getRouteOwner(GameState* state, int from, int to);
int workOnObjectives(GameState* state, MoveData* moveData);
int workOnSingleObjective(GameState* state, MoveData* moveData);
int analyzeAllObjectivesAndAct(GameState* state, MoveData* moveData);
int buildLongestRoute(GameState* state, MoveData* moveData);
int takeAnyGoodRoute(GameState* state, MoveData* moveData);
int handleEndgame(GameState* state, MoveData* moveData);
int handleLateGame(GameState* state, MoveData* moveData);
int takeHighestValueRoute(GameState* state, MoveData* moveData);
int emergencyUnblock(GameState* state, MoveData* moveData);
int alternativeStrategy(GameState* state, MoveData* moveData, void* objData, int objectiveCount);
int findAlternativePath(GameState* state, int from, int to, MoveData* moveData);
int findNearestCompletionObjective(GameState* state);
int drawCardsForRouteAggressively(GameState* state, int from, int to, MoveData* moveData);

int findSmartestPath(GameState* state, int start, int end, int* path, int* pathLength) {
    if (!state || !path || !pathLength || start < 0 || start >= state->nbCities || 
        end < 0 || end >= state->nbCities) {
        return -1;
    }
    
    int dist[MAX_CITIES];
    int prev[MAX_CITIES];
    int visited[MAX_CITIES];
    
    for (int i = 0; i < state->nbCities; i++) {
        dist[i] = 999999;
        prev[i] = -1;
        visited[i] = 0;
    }
    
    dist[start] = 0;
    
    for (int count = 0; count < state->nbCities; count++) {
        int u = -1;
        int minDist = 999999;
        
        for (int i = 0; i < state->nbCities; i++) {
            if (!visited[i] && dist[i] < minDist) {
                minDist = dist[i];
                u = i;
            }
        }
        
        if (u == -1 || dist[u] == 999999) break;
        
        visited[u] = 1;
        if (u == end) break;
        
        for (int i = 0; i < state->nbTracks; i++) {
            if (state->routes[i].owner == 2) continue;
            
            int from = state->routes[i].from;
            int to = state->routes[i].to;
            int length = state->routes[i].length;
            
            if (state->routes[i].owner == 1) {
                length = 0;
            }
            
            if (from == u || to == u) {
                int v = (from == u) ? to : from;
                int newDist = dist[u] + length;
                
                if (newDist < dist[v]) {
                    dist[v] = newDist;
                    prev[v] = u;
                }
            }
        }
    }
    
    if (prev[end] == -1 && start != end) return -1;
    
    int tempPath[MAX_CITIES];
    int tempIndex = 0;
    int current = end;
    
    while (current != -1 && tempIndex < MAX_CITIES) {
        tempPath[tempIndex++] = current;
        if (current == start) break;
        current = prev[current];
    }
    
    *pathLength = tempIndex;
    for (int i = 0; i < tempIndex; i++) {
        path[i] = tempPath[tempIndex - 1 - i];
    }
    
    return dist[end];
}

int findShortestPath(GameState* state, int start, int end, int* path, int* pathLength) {
    return findSmartestPath(state, start, end, path, pathLength);
}

void simpleChooseObjectives(GameState* state, Objective* objectives, unsigned char* chooseObjectives) {
    chooseObjectives[0] = 0;
    chooseObjectives[1] = 0;
    chooseObjectives[2] = 0;
    
    typedef struct {
        int index;
        int distance;
        int score;
        float efficiency;
        int isEastCoast;
        int usesNetwork;
        int routesNeeded;
        float finalScore;
    } ObjectiveEval;
    
    ObjectiveEval evals[3];
    
    printf("=== CHOOSING OBJECTIVES (avoiding East Coast) ===\n");
    
    int eastCoastCities[] = {
        30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        20, 21, 22, 23, 24, 25, 26, 27, 28, 29
    };
    int numEastCities = sizeof(eastCoastCities) / sizeof(eastCoastCities[0]);
    
    for (int i = 0; i < 3; i++) {
        evals[i].index = i;
        evals[i].score = objectives[i].score;
        evals[i].isEastCoast = 0;
        evals[i].usesNetwork = 0;
        evals[i].routesNeeded = 999;
        
        int from = objectives[i].from;
        int to = objectives[i].to;
        
        for (int j = 0; j < numEastCities; j++) {
            if (from == eastCoastCities[j] || to == eastCoastCities[j]) {
                evals[i].isEastCoast = 1;
                break;
            }
        }
        
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findSmartestPath(state, from, to, path, &pathLength);
        
        evals[i].distance = distance;
        
        if (distance > 0) {
            int routesOwned = 0;
            int totalRoutes = pathLength - 1;
            
            for (int j = 0; j < pathLength - 1; j++) {
                int cityA = path[j];
                int cityB = path[j + 1];
                int owner = getRouteOwner(state, cityA, cityB);
                if (owner == 1) {
                    routesOwned++;
                }
            }
            
            evals[i].routesNeeded = totalRoutes - routesOwned;
            evals[i].usesNetwork = (routesOwned > 0) ? 1 : 0;
            
            float baseEfficiency = (float)evals[i].score / distance;
            evals[i].finalScore = baseEfficiency;
            
            if (evals[i].isEastCoast) {
                evals[i].finalScore *= 0.3f;
                printf("Obj %d: EAST COAST PENALTY (-70%%)\n", i+1);
            }
            
            if (evals[i].usesNetwork) {
                evals[i].finalScore *= 2.0f;
                printf("Obj %d: NETWORK BONUS (+100%%)\n", i+1);
            }
            
            if (evals[i].routesNeeded <= 1) {
                evals[i].finalScore *= 1.5f;
                printf("Obj %d: EASY COMPLETION BONUS (+50%%)\n", i+1);
            } else if (evals[i].routesNeeded <= 2) {
                evals[i].finalScore *= 1.2f;
            }
            
            if (evals[i].routesNeeded > 5) {
                evals[i].finalScore *= 0.5f;
                printf("Obj %d: TOO MANY ROUTES PENALTY (-50%%)\n", i+1);
            }
            
            evals[i].efficiency = baseEfficiency;
        } else {
            evals[i].finalScore = 0;
            evals[i].efficiency = 0;
        }
        
        printf("Obj %d: %d->%d, score %d, dist %d, routes needed %d, east %s, network %s, final %.2f\n", 
               i+1, from, to, evals[i].score, distance, evals[i].routesNeeded,
               evals[i].isEastCoast ? "YES" : "NO",
               evals[i].usesNetwork ? "YES" : "NO",
               evals[i].finalScore);
    }
    
    for (int i = 0; i < 2; i++) {
        for (int j = i + 1; j < 3; j++) {
            if (evals[i].finalScore < evals[j].finalScore) {
                ObjectiveEval temp = evals[i];
                evals[i] = evals[j];
                evals[j] = temp;
            }
        }
    }
    
    int chosen = 0;
    
    if (evals[0].finalScore > 0) {
        chooseObjectives[evals[0].index] = 1;
        chosen++;
        printf("CHOSE: Objective %d (final score %.2f)\n", evals[0].index+1, evals[0].finalScore);
    }
    
    if (chosen < 2) {
        if (evals[1].finalScore > 0.2f) {
            chooseObjectives[evals[1].index] = 1;
            chosen++;
            printf("CHOSE: Objective %d (final score %.2f)\n", evals[1].index+1, evals[1].finalScore);
        } else {
            printf("REJECTED: Objective %d too difficult (score %.2f < 0.2)\n", 
                   evals[1].index+1, evals[1].finalScore);
        }
    }
    
    if (chosen < 2 && state->nbObjectives == 0) {
        for (int i = 0; i < 3; i++) {
            if (!chooseObjectives[i]) {
                chooseObjectives[i] = 1;
                chosen++;
                printf("FORCED (first turn): Objective %d (poor choice but mandatory)\n", i+1);
                if (chosen >= 2) break;
            }
        }
    }
    
    if (chosen == 1 && state->nbObjectives > 0) {
        printf("Taking only 1 objective (avoiding difficult ones)\n");
    }
}

int simpleStrategy(GameState* state, MoveData* moveData) {
    if (state->nbObjectives == 0) {
        moveData->action = DRAW_OBJECTIVES;
        return 1;
    }
    if (isAntiAdversaireMode(state)) {
        printf("=== ANTI-ADVERSAIRE MODE ACTIVATED ===\n");
        return handleAntiAdversaire(state, moveData);
    }

    int isEndgame = (state->lastTurn || state->wagonsLeft <= 3 || state->opponentWagonsLeft <= 3);
    int isLateGame = (state->wagonsLeft <= 8 || state->opponentWagonsLeft <= 8);
    
    if (isEndgame) {
        printf("=== ENDGAME MODE: %d wagons left ===\n", state->wagonsLeft);
        return handleEndgame(state, moveData);
    }
    
    if (isLateGame) {
        printf("=== LATE GAME: %d wagons left ===\n", state->wagonsLeft);
        return handleLateGame(state, moveData);
    }
    
    int completedCount = 0;
    int totalObjectives = state->nbObjectives;
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (isObjectiveCompleted(state, state->objectives[i])) {
            completedCount++;
        }
    }
    
    printf("Objectives status: %d/%d completed\n", completedCount, totalObjectives);
    
    if (completedCount == totalObjectives) {
        printf("=== ALL OBJECTIVES COMPLETED - BUILDING LONGEST ROUTE ===\n");
        return buildLongestRoute(state, moveData);
    }
    
    return workOnObjectives(state, moveData);
}

int handleEndgame(GameState* state, MoveData* moveData) {
    printf("ENDGAME: Looking for immediate completions only\n");
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            int objFrom = state->objectives[i].from;
            int objTo = state->objectives[i].to;
            int objScore = state->objectives[i].score;
            
            int path[MAX_CITIES];
            int pathLength = 0;
            findSmartestPath(state, objFrom, objTo, path, &pathLength);
            
            int routesNeeded = 0;
            for (int j = 0; j < pathLength - 1; j++) {
                if (getRouteOwner(state, path[j], path[j+1]) == 0) {
                    routesNeeded++;
                }
            }
            
            printf("Endgame obj %d (%d->%d): %d routes needed, score %d\n", 
                   i+1, objFrom, objTo, routesNeeded, objScore);
            
            if (routesNeeded <= 1 && routesNeeded <= state->wagonsLeft) {
                for (int j = 0; j < pathLength - 1; j++) {
                    int cityA = path[j];
                    int cityB = path[j+1];
                    
                    if (getRouteOwner(state, cityA, cityB) == 0) {
                        if (canTakeRoute(state, cityA, cityB, moveData)) {
                            printf("ENDGAME: Completing objective %d with route %d->%d\n", 
                                   i+1, cityA, cityB);
                            return 1;
                        }
                    }
                }
            }
        }
    }
    
    printf("ENDGAME: No completable objectives, taking highest value route\n");
    return takeHighestValueRoute(state, moveData);
}

int handleLateGame(GameState* state, MoveData* moveData) {
    printf("LATE GAME: Prioritizing easy objectives\n");
    
    int bestObjective = -1;
    int lowestCost = 999;
    int highestValue = 0;
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            int objFrom = state->objectives[i].from;
            int objTo = state->objectives[i].to;
            int objScore = state->objectives[i].score;
            
            int path[MAX_CITIES];
            int pathLength = 0;
            findSmartestPath(state, objFrom, objTo, path, &pathLength);
            
            int routesNeeded = 0;
            int wagonsNeeded = 0;
            
            for (int j = 0; j < pathLength - 1; j++) {
                if (getRouteOwner(state, path[j], path[j+1]) == 0) {
                    routesNeeded++;
                    wagonsNeeded += 3;
                }
            }
            
            if (wagonsNeeded <= state->wagonsLeft) {
                float efficiency = (float)objScore / wagonsNeeded;
                if (efficiency > (float)highestValue / (lowestCost + 1)) {
                    bestObjective = i;
                    lowestCost = wagonsNeeded;
                    highestValue = objScore;
                }
            }
        }
    }
    
    if (bestObjective >= 0) {
        printf("LATE GAME: Focusing on objective %d (cost %d wagons)\n", 
               bestObjective + 1, lowestCost);
        currentObjectiveIndex = bestObjective;
        return workOnSingleObjective(state, moveData);
    }
    
    return buildLongestRoute(state, moveData);
}

int takeHighestValueRoute(GameState* state, MoveData* moveData) {
    int bestRoute = -1;
    float bestValue = 0;
    
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 0) {
            int length = state->routes[i].length;
            if (length <= state->wagonsLeft && canTakeRoute(state, state->routes[i].from, state->routes[i].to, moveData)) {
                
                int points = 0;
                switch (length) {
                    case 1: points = 1; break;
                    case 2: points = 2; break;
                    case 3: points = 4; break;
                    case 4: points = 7; break;
                    case 5: points = 10; break;
                    case 6: points = 15; break;
                }
                
                float value = (float)points / length;
                
                if (value > bestValue) {
                    bestValue = value;
                    bestRoute = i;
                }
            }
        }
    }
    
    if (bestRoute >= 0) {
        int from = state->routes[bestRoute].from;
        int to = state->routes[bestRoute].to;
        printf("Taking highest value route %d->%d (value %.2f)\n", from, to, bestValue);
        return canTakeRoute(state, from, to, moveData);
    }
    
    moveData->action = DRAW_BLIND_CARD;
    return 1;
}

int workOnObjectives(GameState* state, MoveData* moveData) {
    int totalCards = 0;
    for (int i = PURPLE; i <= LOCOMOTIVE; i++) {
        totalCards += state->nbCardsByColor[i];
    }
    
    if (totalCards > 25) {
        printf("Many cards (%d), forcing objective completion\n", totalCards);
        return workOnSingleObjective(state, moveData);
    }
    
    return analyzeAllObjectivesAndAct(state, moveData);
}

int analyzeAllObjectivesAndAct(GameState* state, MoveData* moveData) {
    printf("=== GLOBAL OBJECTIVE ANALYSIS ===\n");
    
    int totalCards = 0;
    for (int i = PURPLE; i <= LOCOMOTIVE; i++) {
        totalCards += state->nbCardsByColor[i];
    }
    
    printf("Total cards: %d\n", totalCards);
    
    if (totalCards > 40) {
        printf("=== EMERGENCY UNBLOCK: %d cards! ===\n", totalCards);
        return emergencyUnblock(state, moveData);
    }
    
    typedef struct {
        int index;
        int from;
        int to;
        int score;
        int path[MAX_CITIES];
        int pathLength;
        int routesNeeded;
        int routesOwned;
        int blocked;
    } ObjectiveInfo;
    
    ObjectiveInfo objectives[MAX_OBJECTIVES];
    int objectiveCount = 0;
    int blockedObjectives = 0;
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            objectives[objectiveCount].index = i;
            objectives[objectiveCount].from = state->objectives[i].from;
            objectives[objectiveCount].to = state->objectives[i].to;
            objectives[objectiveCount].score = state->objectives[i].score;
            objectives[objectiveCount].blocked = 0;
            
            int distance = findSmartestPath(state, objectives[objectiveCount].from, 
                                          objectives[objectiveCount].to,
                                          objectives[objectiveCount].path,
                                          &objectives[objectiveCount].pathLength);
            
            if (distance > 0) {
                int owned = 0, total = objectives[objectiveCount].pathLength - 1;
                int freeRoutes = 0;
                
                for (int j = 0; j < objectives[objectiveCount].pathLength - 1; j++) {
                    int cityA = objectives[objectiveCount].path[j];
                    int cityB = objectives[objectiveCount].path[j + 1];
                    int owner = getRouteOwner(state, cityA, cityB);
                    
                    if (owner == 1) {
                        owned++;
                    } else if (owner == 0) {
                        freeRoutes++;
                    }
                }
                
                objectives[objectiveCount].routesOwned = owned;
                objectives[objectiveCount].routesNeeded = total - owned;
                
                if (freeRoutes == 0 && owned < total) {
                    objectives[objectiveCount].blocked = 1;
                    blockedObjectives++;
                    printf("Obj %d (%d->%d): BLOCKED - no free routes\n",
                           i+1, objectives[objectiveCount].from, objectives[objectiveCount].to);
                } else {
                    printf("Obj %d (%d->%d): %d/%d routes owned, %d needed, %d free\n",
                           i+1, objectives[objectiveCount].from, objectives[objectiveCount].to,
                           owned, total, total - owned, freeRoutes);
                }
                
                objectiveCount++;
            } else {
                objectives[objectiveCount].blocked = 1;
                blockedObjectives++;
                objectiveCount++;
                printf("Obj %d (%d->%d): BLOCKED - no path found\n",
                       i+1, objectives[objectiveCount-1].from, objectives[objectiveCount-1].to);
            }
        }
    }
    
    if ((blockedObjectives >= objectiveCount && objectiveCount > 0) || totalCards > 30) {
        printf("=== MANY OBJECTIVES BLOCKED OR TOO MANY CARDS ===\n");
        return alternativeStrategy(state, moveData, objectives, objectiveCount);
    }
    
    if (objectiveCount == 0) {
        return buildLongestRoute(state, moveData);
    }
    
    typedef struct {
        int from;
        int to;
        int usefulForObjectives;
        int totalObjectiveValue;
        int routeLength;
        int priority;
    } RouteAnalysis;
    
    RouteAnalysis routeAnalysis[MAX_ROUTES];
    int routeAnalysisCount = 0;
    
    for (int r = 0; r < state->nbTracks; r++) {
        if (state->routes[r].owner == 0) {
            int routeFrom = state->routes[r].from;
            int routeTo = state->routes[r].to;
            int routeLength = state->routes[r].length;
            
            int usefulCount = 0;
            int totalValue = 0;
            
            for (int obj = 0; obj < objectiveCount; obj++) {
                if (!objectives[obj].blocked) {
                    for (int p = 0; p < objectives[obj].pathLength - 1; p++) {
                        int pathFrom = objectives[obj].path[p];
                        int pathTo = objectives[obj].path[p + 1];
                        
                        if ((pathFrom == routeFrom && pathTo == routeTo) ||
                            (pathFrom == routeTo && pathTo == routeFrom)) {
                            usefulCount++;
                            totalValue += objectives[obj].score;
                            break;
                        }
                    }
                }
            }
            
            if (usefulCount > 0) {
                routeAnalysis[routeAnalysisCount].from = routeFrom;
                routeAnalysis[routeAnalysisCount].to = routeTo;
                routeAnalysis[routeAnalysisCount].usefulForObjectives = usefulCount;
                routeAnalysis[routeAnalysisCount].totalObjectiveValue = totalValue;
                routeAnalysis[routeAnalysisCount].routeLength = routeLength;
                
                routeAnalysis[routeAnalysisCount].priority = totalValue;
                if (usefulCount > 1) {
                    routeAnalysis[routeAnalysisCount].priority += usefulCount * 50;
                }
                
                printf("Route %d->%d: helps %d objectives, value %d, priority %d\n",
                       routeFrom, routeTo, usefulCount, totalValue, 
                       routeAnalysis[routeAnalysisCount].priority);
                
                routeAnalysisCount++;
            }
        }
    }
    
    for (int i = 0; i < routeAnalysisCount - 1; i++) {
        for (int j = 0; j < routeAnalysisCount - i - 1; j++) {
            if (routeAnalysis[j].priority < routeAnalysis[j+1].priority) {
                RouteAnalysis temp = routeAnalysis[j];
                routeAnalysis[j] = routeAnalysis[j+1];
                routeAnalysis[j+1] = temp;
            }
        }
    }
    
    for (int i = 0; i < routeAnalysisCount && i < 5; i++) {
        int from = routeAnalysis[i].from;
        int to = routeAnalysis[i].to;
        
        if (canTakeRoute(state, from, to, moveData)) {
            printf("*** TAKING OPTIMAL ROUTE %d->%d ***\n", from, to);
            return 1;
        }
    }
    
    if (totalCards > 25) {
        printf("Many cards but no optimal routes, trying alternative strategy\n");
        return alternativeStrategy(state, moveData, objectives, objectiveCount);
    }
    
    if (routeAnalysisCount > 0) {
        int from = routeAnalysis[0].from;
        int to = routeAnalysis[0].to;
        return drawCardsForRoute(state, from, to, moveData);
    }
    
    return workOnSingleObjective(state, moveData);
}

int emergencyUnblock(GameState* state, MoveData* moveData) {
    printf("EMERGENCY: Force taking any good route with many cards\n");
    
    for (int length = 6; length >= 5; length--) {
        for (int i = 0; i < state->nbTracks; i++) {
            if (state->routes[i].owner == 0 && state->routes[i].length == length) {
                int from = state->routes[i].from;
                int to = state->routes[i].to;
                
                if (canTakeRoute(state, from, to, moveData)) {
                    printf("EMERGENCY: Taking long route %d->%d (length %d)\n", from, to, length);
                    return 1;
                }
            }
        }
    }
    
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 0) {
            int from = state->routes[i].from;
            int to = state->routes[i].to;
            
            int connectsToNetwork = 0;
            for (int j = 0; j < state->nbClaimedRoutes; j++) {
                int routeIndex = state->claimedRoutes[j];
                if (routeIndex >= 0 && routeIndex < state->nbTracks) {
                    if (state->routes[routeIndex].from == from || state->routes[routeIndex].to == from ||
                        state->routes[routeIndex].from == to || state->routes[routeIndex].to == to) {
                        connectsToNetwork = 1;
                        break;
                    }
                }
            }
            
            if (connectsToNetwork && canTakeRoute(state, from, to, moveData)) {
                printf("EMERGENCY: Taking network route %d->%d\n", from, to);
                return 1;
            }
        }
    }
    
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 0) {
            int from = state->routes[i].from;
            int to = state->routes[i].to;
            
            if (canTakeRoute(state, from, to, moveData)) {
                printf("EMERGENCY: Taking any route %d->%d\n", from, to);
                return 1;
            }
        }
    }
    
    printf("EMERGENCY: Cannot take any route, drawing blind\n");
    moveData->action = DRAW_BLIND_CARD;
    return 1;
}

int alternativeStrategy(GameState* state, MoveData* moveData, void* objData, int objectiveCount) {
    (void)objData;
    (void)objectiveCount;
    
    printf("=== ALTERNATIVE STRATEGY ===\n");
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            int objFrom = state->objectives[i].from;
            int objTo = state->objectives[i].to;
            
            printf("Trying alternative path for objective %d->%d\n", objFrom, objTo);
            
            if (findAlternativePath(state, objFrom, objTo, moveData)) {
                printf("Found alternative path for objective %d->%d\n", objFrom, objTo);
                return 1;
            }
        }
    }
    
    printf("No alternative paths, extending network for longest route\n");
    return buildLongestRoute(state, moveData);
}

int findAlternativePath(GameState* state, int from, int to, MoveData* moveData) {
    int hubs[] = {5, 10, 15, 20, 25};
    int numHubs = sizeof(hubs) / sizeof(hubs[0]);
    
    for (int h = 0; h < numHubs; h++) {
        int hub = hubs[h];
        if (hub >= state->nbCities || hub == from || hub == to) continue;
        
        int path1[MAX_CITIES], path2[MAX_CITIES];
        int pathLength1, pathLength2;
        
        int dist1 = findSmartestPath(state, from, hub, path1, &pathLength1);
        int dist2 = findSmartestPath(state, hub, to, path2, &pathLength2);
        
        if (dist1 > 0 && dist2 > 0) {
            for (int i = 0; i < pathLength1 - 1; i++) {
                int cityA = path1[i];
                int cityB = path1[i + 1];
                
                if (getRouteOwner(state, cityA, cityB) == 0) {
                    if (canTakeRoute(state, cityA, cityB, moveData)) {
                        printf("Alternative route via hub %d: taking %d->%d\n", hub, cityA, cityB);
                        return 1;
                    }
                }
            }
            
            for (int i = 0; i < pathLength2 - 1; i++) {
                int cityA = path2[i];
                int cityB = path2[i + 1];
                
                if (getRouteOwner(state, cityA, cityB) == 0) {
                    if (canTakeRoute(state, cityA, cityB, moveData)) {
                        printf("Alternative route via hub %d: taking %d->%d\n", hub, cityA, cityB);
                        return 1;
                    }
                }
            }
        }
    }
    
    return 0;
}

int findNearestCompletionObjective(GameState* state) {
    int bestObjective = -1;
    int lowestRoutesNeeded = 999;
    int highestProgress = -1;
    
    printf("=== FINDING NEAREST COMPLETION OBJECTIVE ===\n");
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (isObjectiveCompleted(state, state->objectives[i])) {
            continue;
        }
        
        int objFrom = state->objectives[i].from;
        int objTo = state->objectives[i].to;
        int objScore = state->objectives[i].score;
        
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findSmartestPath(state, objFrom, objTo, path, &pathLength);
        
        if (distance <= 0) continue;
        
        int routesOwned = 0;
        int routesAvailable = 0;
        int routesBlocked = 0;
        int totalRoutes = pathLength - 1;
        
        for (int j = 0; j < pathLength - 1; j++) {
            int cityA = path[j];
            int cityB = path[j + 1];
            int owner = getRouteOwner(state, cityA, cityB);
            
            if (owner == 1) {
                routesOwned++;
            } else if (owner == 0) {
                routesAvailable++;
                MoveData testMove;
                if (canTakeRoute(state, cityA, cityB, &testMove)) {
                }
            } else {
                routesBlocked++;
            }
        }
        
        int routesNeeded = routesAvailable;
        float completionProgress = (float)routesOwned / totalRoutes;
        
        printf("Obj %d (%d->%d): %d/%d owned (%.0f%%), %d needed, %d blocked, score %d\n",
               i+1, objFrom, objTo, routesOwned, totalRoutes, 
               completionProgress * 100, routesNeeded, routesBlocked, objScore);
        
        if (routesNeeded == 1 && routesBlocked == 0) {
            printf("*** PRIORITY 1: Objective %d needs only 1 route! ***\n", i+1);
            return i;
        }
        
        if (routesNeeded == 2 && routesBlocked == 0 && routesNeeded < lowestRoutesNeeded) {
            lowestRoutesNeeded = routesNeeded;
            bestObjective = i;
            highestProgress = routesOwned;
        }
        
        if (routesBlocked == 0 && routesOwned > highestProgress && routesNeeded <= 4) {
            if (routesNeeded < lowestRoutesNeeded || 
                (routesNeeded == lowestRoutesNeeded && routesOwned > highestProgress)) {
                lowestRoutesNeeded = routesNeeded;
                bestObjective = i;
                highestProgress = routesOwned;
            }
        }
    }
    
    if (bestObjective >= 0) {
        printf("*** SELECTED: Objective %d (needs %d routes, has %d owned) ***\n", 
               bestObjective + 1, lowestRoutesNeeded, highestProgress);
    } else {
        printf("No viable objective found\n");
    }
    
    return bestObjective;
}

int workOnSingleObjective(GameState* state, MoveData* moveData) {
    int bestObjective = findNearestCompletionObjective(state);
    
    if (bestObjective < 0) {
        return buildLongestRoute(state, moveData);
    }
    
    currentObjectiveIndex = bestObjective;
    
    if (isObjectiveCompleted(state, state->objectives[currentObjectiveIndex])) {
        printf("Objective %d just completed! Looking for next one.\n", currentObjectiveIndex + 1);
        currentObjectiveIndex = -1;
        return workOnSingleObjective(state, moveData);
    }
    
    int objFrom = state->objectives[currentObjectiveIndex].from;
    int objTo = state->objectives[currentObjectiveIndex].to;
    
    int distance = findSmartestPath(state, objFrom, objTo, currentPath, &currentPathLength);
    
    if (distance <= 0) {
        printf("No path found for priority objective %d->%d!\n", objFrom, objTo);
        currentObjectiveIndex = -1;
        return workOnSingleObjective(state, moveData);
    }
    
    printf("=== FOCUS MODE: Working on objective %d->%d ===\n", objFrom, objTo);
    
    printf("Priority path: ");
    for (int i = 0; i < currentPathLength; i++) {
        printf("%d", currentPath[i]);
        if (i < currentPathLength - 1) {
            int owner = getRouteOwner(state, currentPath[i], currentPath[i+1]);
            if (owner == 1) {
                printf("-[✓]->");
            } else if (owner == 0) {
                printf("-[!]->");
            } else {
                printf("-[X]->");
            }
        }
    }
    printf("\n");
    
    for (int i = 0; i < currentPathLength - 1; i++) {
        int cityA = currentPath[i];
        int cityB = currentPath[i + 1];
        
        int routeOwner = getRouteOwner(state, cityA, cityB);
        
        if (routeOwner == 0) {
            printf("*** MUST TAKE: Route %d->%d for objective completion ***\n", cityA, cityB);
            
            if (canTakeRoute(state, cityA, cityB, moveData)) {
                printf("Taking critical route %d->%d\n", cityA, cityB);
                return 1;
            } else {
                printf("Need cards specifically for route %d->%d\n", cityA, cityB);
                return drawCardsForRouteAggressively(state, cityA, cityB, moveData);
            }
        } else if (routeOwner == 1) {
            continue;
        } else {
            printf("Route %d->%d blocked! Recalculating for objective %d\n", 
                   cityA, cityB, currentObjectiveIndex + 1);
            return workOnSingleObjective(state, moveData);
        }
    }
    
    printf("Objective path seems complete\n");
    return workOnSingleObjective(state, moveData);
}

int drawCardsForRouteAggressively(GameState* state, int from, int to, MoveData* moveData) {
    int routeIndex = -1;
    for (int i = 0; i < state->nbTracks; i++) {
        if ((state->routes[i].from == from && state->routes[i].to == to) ||
            (state->routes[i].from == to && state->routes[i].to == from)) {
            routeIndex = i;
            break;
        }
    }
    
    if (routeIndex < 0) {
        moveData->action = DRAW_BLIND_CARD;
        return 1;
    }
    
    CardColor routeColor = state->routes[routeIndex].color;
    int routeLength = state->routes[routeIndex].length;
    
    printf("AGGRESSIVE DRAW: Need %d cards for route %d->%d (color %d)\n", 
           routeLength, from, to, routeColor);
    
    int have = 0;
    int haveLocomotives = state->nbCardsByColor[LOCOMOTIVE];
    
    if (routeColor == LOCOMOTIVE) {
        int bestColorCount = 0;
        for (int c = PURPLE; c <= GREEN; c++) {
            if (state->nbCardsByColor[c] > bestColorCount) {
                bestColorCount = state->nbCardsByColor[c];
            }
        }
        have = bestColorCount;
        printf("Gray route: best color has %d cards, %d locomotives\n", have, haveLocomotives);
    } else {
        have = state->nbCardsByColor[routeColor];
        printf("Colored route: have %d of color %d, %d locomotives\n", have, routeColor, haveLocomotives);
    }
    
    int totalHave = have + haveLocomotives;
    int stillNeed = routeLength - totalHave;
    
    printf("Total available: %d, still need: %d\n", totalHave, stillNeed);
    
    for (int i = 0; i < 5; i++) {
        if (state->visibleCards[i] == LOCOMOTIVE) {
            moveData->action = DRAW_CARD;
            moveData->drawCard = LOCOMOTIVE;
            printf("Taking locomotive for critical route\n");
            return 1;
        }
    }
    
    if (routeColor != LOCOMOTIVE) {
        for (int i = 0; i < 5; i++) {
            if (state->visibleCards[i] == routeColor) {
                moveData->action = DRAW_CARD;
                moveData->drawCard = routeColor;
                printf("Taking exact color %d for critical route\n", routeColor);
                return 1;
            }
        }
    }
    
    if (routeColor == LOCOMOTIVE) {
        for (int i = 0; i < 5; i++) {
            CardColor card = state->visibleCards[i];
            if (card != NONE && card != LOCOMOTIVE) {
                moveData->action = DRAW_CARD;
                moveData->drawCard = card;
                printf("Taking color %d for gray critical route\n", card);
                return 1;
            }
        }
    }
    
    moveData->action = DRAW_BLIND_CARD;
    printf("Drawing blind for critical route\n");
    return 1;
}

int buildLongestRoute(GameState* state, MoveData* moveData) {
    printf("=== BUILDING LONGEST ROUTE ===\n");
    
    int totalCards = 0;
    for (int i = PURPLE; i <= LOCOMOTIVE; i++) {
        totalCards += state->nbCardsByColor[i];
    }
    
    if (totalCards > 15 && state->nbObjectives < 5) {
        printf("Too many cards (%d), trying to get easy objectives...\n", totalCards);
        moveData->action = DRAW_OBJECTIVES;
        return 1;
    }
    
    printf("Building longest route with %d cards...\n", totalCards);
    
    int networkCities[MAX_CITIES];
    int networkCityCount = 0;
    
    for (int i = 0; i < state->nbClaimedRoutes; i++) {
        int routeIndex = state->claimedRoutes[i];
        if (routeIndex >= 0 && routeIndex < state->nbTracks) {
            int from = state->routes[routeIndex].from;
            int to = state->routes[routeIndex].to;
            
            int fromFound = 0, toFound = 0;
            for (int j = 0; j < networkCityCount; j++) {
                if (networkCities[j] == from) fromFound = 1;
                if (networkCities[j] == to) toFound = 1;
            }
            if (!fromFound && networkCityCount < MAX_CITIES) {
                networkCities[networkCityCount++] = from;
            }
            if (!toFound && networkCityCount < MAX_CITIES) {
                networkCities[networkCityCount++] = to;
            }
        }
    }
    
    printf("Network has %d cities\n", networkCityCount);
    
    int bestRouteIndex = -1;
    int bestScore = 0;
    
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 0) {
            int from = state->routes[i].from;
            int to = state->routes[i].to;
            int length = state->routes[i].length;
            
            int connectsToNetwork = 0;
            for (int j = 0; j < networkCityCount; j++) {
                if (networkCities[j] == from || networkCities[j] == to) {
                    connectsToNetwork = 1;
                    break;
                }
            }
            
            if (connectsToNetwork && canTakeRoute(state, from, to, moveData)) {
                int score = length * 10;
                if (length >= 5) score += 100;
                if (length >= 4) score += 50;
                if (length >= 3) score += 25;
                
                printf("Route %d->%d: length %d, score %d, connects to network\n", 
                       from, to, length, score);
                
                if (score > bestScore) {
                    bestScore = score;
                    bestRouteIndex = i;
                }
            }
        }
    }
    
    if (bestRouteIndex >= 0) {
        int from = state->routes[bestRouteIndex].from;
        int to = state->routes[bestRouteIndex].to;
        int length = state->routes[bestRouteIndex].length;
        
        printf("Extending network with route %d->%d (length %d)\n", from, to, length);
        return canTakeRoute(state, from, to, moveData);
    }
    
    printf("No connected routes, taking longest available route\n");
    return takeAnyGoodRoute(state, moveData);
}

int getRouteOwner(GameState* state, int from, int to) {
    for (int i = 0; i < state->nbTracks; i++) {
        if ((state->routes[i].from == from && state->routes[i].to == to) ||
            (state->routes[i].from == to && state->routes[i].to == from)) {
            return state->routes[i].owner;
        }
    }
    return -1;
}

int canTakeRoute(GameState* state, int from, int to, MoveData* moveData) {
    int routeIndex = -1;
    for (int i = 0; i < state->nbTracks; i++) {
        if ((state->routes[i].from == from && state->routes[i].to == to) ||
            (state->routes[i].from == to && state->routes[i].to == from)) {
            if (state->routes[i].owner == 0) {
                routeIndex = i;
                break;
            }
        }
    }
    
    if (routeIndex < 0) return 0;
    
    int length = state->routes[routeIndex].length;
    CardColor routeColor = state->routes[routeIndex].color;
    
    if (state->wagonsLeft < length) return 0;
    
    CardColor bestColor = NONE;
    int nbLocomotives = 0;
    
    if (routeColor == LOCOMOTIVE) {
        int maxCards = 0;
        for (int c = PURPLE; c <= GREEN; c++) {
            if (state->nbCardsByColor[c] >= length && state->nbCardsByColor[c] > maxCards) {
                maxCards = state->nbCardsByColor[c];
                bestColor = c;
                nbLocomotives = 0;
            }
        }
        
        if (bestColor == NONE) {
            for (int c = PURPLE; c <= GREEN; c++) {
                int total = state->nbCardsByColor[c] + state->nbCardsByColor[LOCOMOTIVE];
                if (total >= length) {
                    bestColor = c;
                    nbLocomotives = length - state->nbCardsByColor[c];
                    break;
                }
            }
        }
        
        if (bestColor == NONE && state->nbCardsByColor[LOCOMOTIVE] >= length) {
            bestColor = LOCOMOTIVE;
            nbLocomotives = length;
        }
    }
    else {
        if (state->nbCardsByColor[routeColor] >= length) {
            bestColor = routeColor;
            nbLocomotives = 0;
        } else if (state->nbCardsByColor[routeColor] + state->nbCardsByColor[LOCOMOTIVE] >= length) {
            bestColor = routeColor;
            nbLocomotives = length - state->nbCardsByColor[routeColor];
        } else if (state->nbCardsByColor[LOCOMOTIVE] >= length) {
            bestColor = LOCOMOTIVE;
            nbLocomotives = length;
        }
    }
    
    if (bestColor == NONE) {
        printf("Not enough cards: need %d, route color %d\n", length, routeColor);
        return 0;
    }
    
    moveData->action = CLAIM_ROUTE;
    moveData->claimRoute.from = from;
    moveData->claimRoute.to = to;
    moveData->claimRoute.color = bestColor;
    moveData->claimRoute.nbLocomotives = nbLocomotives;
    
    return 1;
}

int drawCardsForRoute(GameState* state, int from, int to, MoveData* moveData) {
    CardColor routeColor = NONE;
    int routeLength = 0;
    
    for (int i = 0; i < state->nbTracks; i++) {
        if ((state->routes[i].from == from && state->routes[i].to == to) ||
            (state->routes[i].from == to && state->routes[i].to == from)) {
            routeColor = state->routes[i].color;
            routeLength = state->routes[i].length;
            break;
        }
    }
    
    if (routeColor == NONE) {
        moveData->action = DRAW_BLIND_CARD;
        return 1;
    }
    
    printf("Need cards for route: color %d, length %d\n", routeColor, routeLength);
    
    for (int i = 0; i < 5; i++) {
        if (state->visibleCards[i] == LOCOMOTIVE) {
            moveData->action = DRAW_CARD;
            moveData->drawCard = LOCOMOTIVE;
            printf("Taking locomotive\n");
            return 1;
        }
    }
    
    if (routeColor == LOCOMOTIVE) {
        for (int i = 0; i < 5; i++) {
            CardColor card = state->visibleCards[i];
            if (card != NONE && card != LOCOMOTIVE) {
                moveData->action = DRAW_CARD;
                moveData->drawCard = card;
                printf("Taking %d for gray route\n", card);
                return 1;
            }
        }
    }
    else {
        for (int i = 0; i < 5; i++) {
            if (state->visibleCards[i] == routeColor) {
                moveData->action = DRAW_CARD;
                moveData->drawCard = routeColor;
                printf("Taking %d for colored route\n", routeColor);
                return 1;
            }
        }
        
        for (int i = 0; i < 5; i++) {
            CardColor card = state->visibleCards[i];
            if (card != NONE && card != LOCOMOTIVE) {
                moveData->action = DRAW_CARD;
                moveData->drawCard = card;
                printf("Taking %d as backup\n", card);
                return 1;
            }
        }
    }
    
    moveData->action = DRAW_BLIND_CARD;
    printf("Drawing blind card\n");
    return 1;
}

int takeAnyGoodRoute(GameState* state, MoveData* moveData) {
    printf("Looking for any good route...\n");
    
    int bestLength = 0;
    int bestFrom = -1, bestTo = -1;
    
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 0) {
            int from = state->routes[i].from;
            int to = state->routes[i].to;
            int length = state->routes[i].length;
            
            if (length > bestLength && canTakeRoute(state, from, to, moveData)) {
                bestLength = length;
                bestFrom = from;
                bestTo = to;
            }
        }
    }
    
    if (bestFrom >= 0) {
        return canTakeRoute(state, bestFrom, bestTo, moveData);
    }
    
    moveData->action = DRAW_BLIND_CARD;
    return 1;
}

int decideNextMove(GameState* state, MoveData* moveData) {
    if (!state || !moveData) {
        printf("ERROR: NULL parameters\n");
        return 0;
    }
    
    return simpleStrategy(state, moveData);
}

void chooseObjectivesStrategy(GameState* state, Objective* objectives, unsigned char* chooseObjectives) {
    simpleChooseObjectives(state, objectives, chooseObjectives);
}

// Détecter si on doit passer en mode anti-adversaire
int isAntiAdversaireMode(GameState* state) {
    // Si l'adversaire va finir bientôt ET on a trop de cartes
    int adversaireProcheFin = (state->opponentWagonsLeft <= 5);
    int tropDeCartes = (state->nbCards > 15);
    int dernierTour = state->lastTurn;
    
    return (adversaireProcheFin && tropDeCartes) || dernierTour;
}

// Stratégie anti-adversaire : déposer les cartes rapidement
int handleAntiAdversaire(GameState* state, MoveData* moveData) {
    printf("Anti-adversaire: Opp wagons=%d, our cards=%d\n", 
           state->opponentWagonsLeft, state->nbCards);
    
    // 1. Essayer de compléter un objectif rapidement
    int quickObjective = findQuickestObjective(state);
    if (quickObjective >= 0) {
        printf("Quick objective completion attempt\n");
        return workOnSpecificObjective(state, moveData, quickObjective);
    }
    
    // 2. Étendre notre réseau existant (routes longues)
    if (buildFromExistingNetwork(state, moveData)) {
        return 1;
    }
    
    // 3. Prendre n'importe quelle route profitable
    if (takeAnyProfitableRoute(state, moveData)) {
        return 1;
    }
    
    // 4. En dernier recours, piocher
    moveData->action = DRAW_BLIND_CARD;
    return 1;
}

// Trouver l'objectif le plus rapide à compléter
int findQuickestObjective(GameState* state) {
    int bestObjective = -1;
    int lowestCost = 999;
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (isObjectiveCompleted(state, state->objectives[i])) {
            continue;
        }
        
        int objFrom = state->objectives[i].from;
        int objTo = state->objectives[i].to;
        
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findSmartestPath(state, objFrom, objTo, path, &pathLength);
        
        if (distance > 0) {
            int routesNeeded = 0;
            int wagonsNeeded = 0;
            
            for (int j = 0; j < pathLength - 1; j++) {
                if (getRouteOwner(state, path[j], path[j+1]) == 0) {
                    routesNeeded++;
                    wagonsNeeded += 2; // Estimation moyenne
                }
            }
            
            if (wagonsNeeded <= state->wagonsLeft && wagonsNeeded < lowestCost) {
                lowestCost = wagonsNeeded;
                bestObjective = i;
            }
        }
    }
    
    return bestObjective;
}

// Construire à partir du réseau existant
int buildFromExistingNetwork(GameState* state, MoveData* moveData) {
    // Trouver les villes de notre réseau
    int networkCities[MAX_CITIES];
    int networkCityCount = 0;
    
    for (int i = 0; i < state->nbClaimedRoutes; i++) {
        int routeIndex = state->claimedRoutes[i];
        if (routeIndex >= 0 && routeIndex < state->nbTracks) {
            int from = state->routes[routeIndex].from;
            int to = state->routes[routeIndex].to;
            
            // Ajouter les villes au réseau
            int fromFound = 0, toFound = 0;
            for (int j = 0; j < networkCityCount; j++) {
                if (networkCities[j] == from) fromFound = 1;
                if (networkCities[j] == to) toFound = 1;
            }
            if (!fromFound && networkCityCount < MAX_CITIES) {
                networkCities[networkCityCount++] = from;
            }
            if (!toFound && networkCityCount < MAX_CITIES) {
                networkCities[networkCityCount++] = to;
            }
        }
    }
    
    // Trouver les meilleures extensions
    int bestRoute = -1;
    int bestValue = 0;
    
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 0) {
            int from = state->routes[i].from;
            int to = state->routes[i].to;
            int length = state->routes[i].length;
            
            // Vérifier si ça connecte à notre réseau
            int connectsToNetwork = 0;
            for (int j = 0; j < networkCityCount; j++) {
                if (networkCities[j] == from || networkCities[j] == to) {
                    connectsToNetwork = 1;
                    break;
                }
            }
            
            if (connectsToNetwork && canTakeRoute(state, from, to, moveData)) {
                int value = length * 10; // Privilégier les routes longues
                if (length >= 5) value += 50;
                
                if (value > bestValue) {
                    bestValue = value;
                    bestRoute = i;
                }
            }
        }
    }
    
    if (bestRoute >= 0) {
        int from = state->routes[bestRoute].from;
        int to = state->routes[bestRoute].to;
        printf("Anti-adversaire: Extending network %d->%d\n", from, to);
        return canTakeRoute(state, from, to, moveData);
    }
    
    return 0;
}

// Prendre n'importe quelle route profitable
int takeAnyProfitableRoute(GameState* state, MoveData* moveData) {
    int bestRoute = -1;
    int bestValue = 0;
    
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 0) {
            int from = state->routes[i].from;
            int to = state->routes[i].to;
            int length = state->routes[i].length;
            
            if (length >= 4 && canTakeRoute(state, from, to, moveData)) {
                int value = length;
                if (value > bestValue) {
                    bestValue = value;
                    bestRoute = i;
                }
            }
        }
    }
    
    if (bestRoute >= 0) {
        int from = state->routes[bestRoute].from;
        int to = state->routes[bestRoute].to;
        printf("Anti-adversaire: Taking profitable route %d->%d\n", from, to);
        return canTakeRoute(state, from, to, moveData);
    }
    
    return 0;
}

// Travailler sur un objectif spécifique
int workOnSpecificObjective(GameState* state, MoveData* moveData, int objectiveIndex) {
    int objFrom = state->objectives[objectiveIndex].from;
    int objTo = state->objectives[objectiveIndex].to;
    
    int path[MAX_CITIES];
    int pathLength = 0;
    findSmartestPath(state, objFrom, objTo, path, &pathLength);
    
    for (int i = 0; i < pathLength - 1; i++) {
        int cityA = path[i];
        int cityB = path[i + 1];
        
        if (getRouteOwner(state, cityA, cityB) == 0) {
            if (canTakeRoute(state, cityA, cityB, moveData)) {
                printf("Anti-adversaire: Quick objective route %d->%d\n", cityA, cityB);
                return 1;
            }
        }
    }
    
    return 0;
}