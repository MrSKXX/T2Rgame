/**
 * strategy_ultra_simple.c
 * Stratégie ultra-simple : 1 objectif, chemin le plus court, route par route
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "strategy_simple.h"
#include "rules.h"
#include "gamestate.h"

// Variables globales pour suivre notre objectif actuel
static int currentObjectiveIndex = -1;
static int currentPath[MAX_CITIES];
static int currentPathLength = 0;
static int currentStep = 0; // Quelle route on est en train de construire

// Déclarations forward
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

// Pathfinding intelligent - privilégie les routes qu'on possède déjà
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
        
        // Parcourir toutes les routes
        for (int i = 0; i < state->nbTracks; i++) {
            // IGNORER les routes prises par l'adversaire
            if (state->routes[i].owner == 2) continue;
            
            int from = state->routes[i].from;
            int to = state->routes[i].to;
            int length = state->routes[i].length;
            
            // BONUS ÉNORME pour les routes qu'on possède déjà !
            if (state->routes[i].owner == 1) {
                length = 0; // Coût ZÉRO pour nos routes !
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
    
    // Reconstruire le chemin
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

// Pathfinding avec Dijkstra - évite les routes de l'adversaire
int findShortestPath(GameState* state, int start, int end, int* path, int* pathLength) {
    // Utiliser le pathfinding intelligent qui privilégie nos routes
    return findSmartestPath(state, start, end, path, pathLength);
}

// Choisir 2 objectifs - ÉVITER l'Est, privilégier réutilisation réseau
void simpleChooseObjectives(GameState* state, Objective* objectives, unsigned char* chooseObjectives) {
    chooseObjectives[0] = 0;
    chooseObjectives[1] = 0;
    chooseObjectives[2] = 0;
    
    typedef struct {
        int index;
        int distance;
        int score;
        float efficiency;
        int isEastCoast;     // Pénalité pour objectifs Est
        int usesNetwork;     // Bonus si utilise notre réseau
        int routesNeeded;    // Nombre de nouvelles routes nécessaires
        float finalScore;    // Score final après tous les bonus/malus
    } ObjectiveEval;
    
    ObjectiveEval evals[3];
    
    printf("=== CHOOSING OBJECTIVES (avoiding East Coast) ===\n");
    
    // Villes de la côte Est difficiles (indices approximatifs selon la carte US)
    int eastCoastCities[] = {
        30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,  // Boston, New York, Washington, etc.
        20, 21, 22, 23, 24, 25, 26, 27, 28, 29      // Miami, Charleston, Atlanta, etc.
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
        
        // Détecter objectifs côte Est
        for (int j = 0; j < numEastCities; j++) {
            if (from == eastCoastCities[j] || to == eastCoastCities[j]) {
                evals[i].isEastCoast = 1;
                break;
            }
        }
        
        // Calculer distance avec pathfinding intelligent
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findSmartestPath(state, from, to, path, &pathLength);
        
        evals[i].distance = distance;
        
        if (distance > 0) {
            // Compter les routes qu'on possède déjà sur ce chemin
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
            
            // Calcul du score final
            float baseEfficiency = (float)evals[i].score / distance;
            evals[i].finalScore = baseEfficiency;
            
            // MALUS ÉNORME pour côte Est
            if (evals[i].isEastCoast) {
                evals[i].finalScore *= 0.3f; // -70% !
                printf("Obj %d: EAST COAST PENALTY (-70%%)\n", i+1);
            }
            
            // BONUS ÉNORME si utilise notre réseau
            if (evals[i].usesNetwork) {
                evals[i].finalScore *= 2.0f; // +100% !
                printf("Obj %d: NETWORK BONUS (+100%%)\n", i+1);
            }
            
            // BONUS si très peu de routes nécessaires
            if (evals[i].routesNeeded <= 1) {
                evals[i].finalScore *= 1.5f; // +50%
                printf("Obj %d: EASY COMPLETION BONUS (+50%%)\n", i+1);
            } else if (evals[i].routesNeeded <= 2) {
                evals[i].finalScore *= 1.2f; // +20%
            }
            
            // MALUS si trop de routes nécessaires
            if (evals[i].routesNeeded > 5) {
                evals[i].finalScore *= 0.5f; // -50%
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
    
    // Trier par score final
    for (int i = 0; i < 2; i++) {
        for (int j = i + 1; j < 3; j++) {
            if (evals[i].finalScore < evals[j].finalScore) {
                ObjectiveEval temp = evals[i];
                evals[i] = evals[j];
                evals[j] = temp;
            }
        }
    }
    
    // Prendre les objectifs selon leur qualité
    int chosen = 0;
    
    // Premier objectif : toujours prendre le meilleur
    if (evals[0].finalScore > 0) {
        chooseObjectives[evals[0].index] = 1;
        chosen++;
        printf("CHOSE: Objective %d (final score %.2f)\n", evals[0].index+1, evals[0].finalScore);
    }
    
    // Deuxième objectif : seulement si pas trop mauvais
    if (chosen < 2) {
        if (evals[1].finalScore > 0.2f) { // Seuil de qualité minimum
            chooseObjectives[evals[1].index] = 1;
            chosen++;
            printf("CHOSE: Objective %d (final score %.2f)\n", evals[1].index+1, evals[1].finalScore);
        } else {
            printf("REJECTED: Objective %d too difficult (score %.2f < 0.2)\n", 
                   evals[1].index+1, evals[1].finalScore);
        }
    }
    
    // Si premier tour, FORCER au moins 2 (mais avertir)
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
    
    // Si seulement 1 objectif choisi en cours de partie = OK
    if (chosen == 1 && state->nbObjectives > 0) {
        printf("Taking only 1 objective (avoiding difficult ones)\n");
    }
}

// Stratégie principale - FOCUS sur objectifs puis route la plus longue
int simpleStrategy(GameState* state, MoveData* moveData) {
    // Premier tour : prendre des objectifs
    if (state->nbObjectives == 0) {
        moveData->action = DRAW_OBJECTIVES;
        return 1;
    }
    
    // NOUVELLE LOGIQUE : Détection de fin de partie imminente
    int isEndgame = (state->lastTurn || state->wagonsLeft <= 3 || state->opponentWagonsLeft <= 3);
    int isLateGame = (state->wagonsLeft <= 8 || state->opponentWagonsLeft <= 8);
    
    if (isEndgame) {
        printf("=== ENDGAME MODE: %d wagons left ===\n", state->wagonsLeft);
        return handleEndgame(state, moveData);
    }
    
    if (isLateGame) {
        printf("=== LATE GAME: %d wagons left ===\n", state->wagonsLeft);
        // En fin de partie, être plus agressif sur les objectifs faciles
        return handleLateGame(state, moveData);
    }
    
    // Vérifier si tous nos objectifs sont complétés
    int completedCount = 0;
    int totalObjectives = state->nbObjectives;
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (isObjectiveCompleted(state, state->objectives[i])) {
            completedCount++;
        }
    }
    
    printf("Objectives status: %d/%d completed\n", completedCount, totalObjectives);
    
    // Si tous les objectifs sont complétés → construire la route la plus longue
    if (completedCount == totalObjectives) {
        printf("=== ALL OBJECTIVES COMPLETED - BUILDING LONGEST ROUTE ===\n");
        return buildLongestRoute(state, moveData);
    }
    
    // Sinon, continuer à travailler sur les objectifs
    return workOnObjectives(state, moveData);
}

// Gestion de fin de partie - focus sur la complétion immédiate
int handleEndgame(GameState* state, MoveData* moveData) {
    printf("ENDGAME: Looking for immediate completions only\n");
    
    // Chercher des objectifs complétables en 1-2 routes max
    for (int i = 0; i < state->nbObjectives; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            int objFrom = state->objectives[i].from;
            int objTo = state->objectives[i].to;
            int objScore = state->objectives[i].score;
            
            int path[MAX_CITIES];
            int pathLength = 0;
            findSmartestPath(state, objFrom, objTo, path, &pathLength);
            
            // Compter routes manquantes
            int routesNeeded = 0;
            for (int j = 0; j < pathLength - 1; j++) {
                if (getRouteOwner(state, path[j], path[j+1]) == 0) {
                    routesNeeded++;
                }
            }
            
            printf("Endgame obj %d (%d->%d): %d routes needed, score %d\n", 
                   i+1, objFrom, objTo, routesNeeded, objScore);
            
            // Si complétable immédiatement
            if (routesNeeded <= 1 && routesNeeded <= state->wagonsLeft) {
                // Essayer de compléter cet objectif
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
    
    // Si aucun objectif complétable, prendre la route la plus valuable
    printf("ENDGAME: No completable objectives, taking highest value route\n");
    return takeHighestValueRoute(state, moveData);
}

// Fin de partie tardive - plus conservateur
int handleLateGame(GameState* state, MoveData* moveData) {
    printf("LATE GAME: Prioritizing easy objectives\n");
    
    // Analyser objectifs par facilité de complétion
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
                    // Estimer wagons nécessaires (moyenne 3 par route)
                    wagonsNeeded += 3;
                }
            }
            
            // Priorité aux objectifs faisables avec nos wagons restants
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
    
    // Si aucun objectif faisable, construire réseau
    return buildLongestRoute(state, moveData);
}

// Prendre la route avec le meilleur ratio points/wagons
int takeHighestValueRoute(GameState* state, MoveData* moveData) {
    int bestRoute = -1;
    float bestValue = 0;
    
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 0) {
            int length = state->routes[i].length;
            if (length <= state->wagonsLeft && canTakeRoute(state, state->routes[i].from, state->routes[i].to, moveData)) {
                
                // Points selon la longueur
                int points = 0;
                switch (length) {
                    case 1: points = 1; break;
                    case 2: points = 2; break;
                    case 3: points = 4; break;
                    case 4: points = 7; break;
                    case 5: points = 10; break;
                    case 6: points = 15; break;
                }
                
                float value = (float)points / length; // Points par wagon
                
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
    
    // Dernier recours
    moveData->action = DRAW_BLIND_CARD;
    return 1;
}

// Travailler sur les objectifs non complétés - FOCUS sur complétion
int workOnObjectives(GameState* state, MoveData* moveData) {
    // NOUVELLE STRATÉGIE: Focus absolu sur l'objectif le plus proche
    
    // Compter nos cartes pour détecter accumulation
    int totalCards = 0;
    for (int i = PURPLE; i <= LOCOMOTIVE; i++) {
        totalCards += state->nbCardsByColor[i];
    }
    
    // Si trop de cartes, forcer complétion d'objectif ou déblocage
    if (totalCards > 25) {
        printf("Many cards (%d), forcing objective completion\n", totalCards);
        return workOnSingleObjective(state, moveData);
    }
    
    // Sinon, utiliser l'analyse globale normale mais avec focus sur complétion
    return analyzeAllObjectivesAndAct(state, moveData);
}

// NOUVELLE FONCTION : Analyse globale de tous les objectifs avec déblocage
int analyzeAllObjectivesAndAct(GameState* state, MoveData* moveData) {
    printf("=== GLOBAL OBJECTIVE ANALYSIS ===\n");
    
    // Compter nos cartes totales
    int totalCards = 0;
    for (int i = PURPLE; i <= LOCOMOTIVE; i++) {
        totalCards += state->nbCardsByColor[i];
    }
    
    printf("Total cards: %d\n", totalCards);
    
    // LOGIQUE DE DÉBLOCAGE : Si trop de cartes (>40), forcer l'action
    if (totalCards > 40) {
        printf("=== EMERGENCY UNBLOCK: %d cards! ===\n", totalCards);
        return emergencyUnblock(state, moveData);
    }
    
    // Collecter tous les objectifs non complétés
    typedef struct {
        int index;
        int from;
        int to;
        int score;
        int path[MAX_CITIES];
        int pathLength;
        int routesNeeded;
        int routesOwned;
        int blocked;  // Si objectif semble bloqué
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
                // Compter routes possédées vs nécessaires
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
                
                // Détecter si objectif semble bloqué (pas de progrès possible)
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
                // Aucun chemin trouvé = objectif bloqué
                objectives[objectiveCount].blocked = 1;
                blockedObjectives++;
                objectiveCount++;
                printf("Obj %d (%d->%d): BLOCKED - no path found\n",
                       i+1, objectives[objectiveCount-1].from, objectives[objectiveCount-1].to);
            }
        }
    }
    
    // Si tous les objectifs sont bloqués ou si on a trop de cartes, débloquer
    if ((blockedObjectives >= objectiveCount && objectiveCount > 0) || totalCards > 30) {
        printf("=== MANY OBJECTIVES BLOCKED OR TOO MANY CARDS ===\n");
        return alternativeStrategy(state, moveData, objectives, objectiveCount);
    }
    
    if (objectiveCount == 0) {
        return buildLongestRoute(state, moveData);
    }
    
    // Analyser toutes les routes manquantes et leur utilité (logique existante)
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
    
    // Pour chaque route libre sur le plateau
    for (int r = 0; r < state->nbTracks; r++) {
        if (state->routes[r].owner == 0) {
            int routeFrom = state->routes[r].from;
            int routeTo = state->routes[r].to;
            int routeLength = state->routes[r].length;
            
            int usefulCount = 0;
            int totalValue = 0;
            
            // Vérifier combien d'objectifs non bloqués cette route aide
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
    
    // Trier les routes par priorité
    for (int i = 0; i < routeAnalysisCount - 1; i++) {
        for (int j = 0; j < routeAnalysisCount - i - 1; j++) {
            if (routeAnalysis[j].priority < routeAnalysis[j+1].priority) {
                RouteAnalysis temp = routeAnalysis[j];
                routeAnalysis[j] = routeAnalysis[j+1];
                routeAnalysis[j+1] = temp;
            }
        }
    }
    
    // Essayer de prendre la route la plus utile
    for (int i = 0; i < routeAnalysisCount && i < 5; i++) {
        int from = routeAnalysis[i].from;
        int to = routeAnalysis[i].to;
        
        if (canTakeRoute(state, from, to, moveData)) {
            printf("*** TAKING OPTIMAL ROUTE %d->%d ***\n", from, to);
            return 1;
        }
    }
    
    // Si aucune route optimale possible, essayer stratégie alternative
    if (totalCards > 25) {
        printf("Many cards but no optimal routes, trying alternative strategy\n");
        return alternativeStrategy(state, moveData, objectives, objectiveCount);
    }
    
    // Sinon piocher pour la meilleure route
    if (routeAnalysisCount > 0) {
        int from = routeAnalysis[0].from;
        int to = routeAnalysis[0].to;
        return drawCardsForRoute(state, from, to, moveData);
    }
    
    return workOnSingleObjective(state, moveData);
}

// NOUVELLE FONCTION : Déblocage d'urgence quand trop de cartes
int emergencyUnblock(GameState* state, MoveData* moveData) {
    printf("EMERGENCY: Force taking any good route with %d cards\n", 
           state->nbCardsByColor[0] + state->nbCardsByColor[1] + state->nbCardsByColor[2] + 
           state->nbCardsByColor[3] + state->nbCardsByColor[4] + state->nbCardsByColor[5] + 
           state->nbCardsByColor[6] + state->nbCardsByColor[7] + state->nbCardsByColor[8] + 
           state->nbCardsByColor[9]);
    
    // 1. Essayer de prendre n'importe quelle route longue (5-6)
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
    
    // 2. Essayer de prendre n'importe quelle route connectée au réseau
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 0) {
            int from = state->routes[i].from;
            int to = state->routes[i].to;
            
            // Vérifier si connectée à notre réseau
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
    
    // 3. Prendre n'importe quelle route possible (dernière chance)
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
    
    // 4. Si vraiment rien possible, piocher aveugle
    printf("EMERGENCY: Cannot take any route, drawing blind\n");
    moveData->action = DRAW_BLIND_CARD;
    return 1;
}

// NOUVELLE FONCTION : Stratégie alternative quand objectifs bloqués
int alternativeStrategy(GameState* state, MoveData* moveData, void* objData, int objectiveCount) {
    printf("=== ALTERNATIVE STRATEGY ===\n");
    
    // 1. Essayer de chercher des chemins alternatifs pour objectifs bloqués
    for (int i = 0; i < state->nbObjectives; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            int objFrom = state->objectives[i].from;
            int objTo = state->objectives[i].to;
            
            printf("Trying alternative path for objective %d->%d\n", objFrom, objTo);
            
            // Essayer de trouver un chemin alternatif en "bannissant" une route bloquée
            if (findAlternativePath(state, objFrom, objTo, moveData)) {
                printf("Found alternative path for objective %d->%d\n", objFrom, objTo);
                return 1;
            }
        }
    }
    
    // 2. Focus sur l'extension du réseau existant pour route la plus longue
    printf("No alternative paths, extending network for longest route\n");
    return buildLongestRoute(state, moveData);
}

// NOUVELLE FONCTION : Chercher un chemin alternatif
int findAlternativePath(GameState* state, int from, int to, MoveData* moveData) {
    // Temporairement "bannir" les routes les plus encombrées et recalculer
    // Cette fonction essaie différentes approches pour contourner les blocages
    
    // Approche simple : chercher via des hubs différents
    int hubs[] = {5, 10, 15, 20, 25}; // Villes importantes (indices approximatifs)
    int numHubs = sizeof(hubs) / sizeof(hubs[0]);
    
    for (int h = 0; h < numHubs; h++) {
        int hub = hubs[h];
        if (hub >= state->nbCities || hub == from || hub == to) continue;
        
        // Essayer chemin via ce hub
        int path1[MAX_CITIES], path2[MAX_CITIES];
        int pathLength1, pathLength2;
        
        int dist1 = findSmartestPath(state, from, hub, path1, &pathLength1);
        int dist2 = findSmartestPath(state, hub, to, path2, &pathLength2);
        
        if (dist1 > 0 && dist2 > 0) {
            // Vérifier si on peut prendre une route sur ce chemin alternatif
            
            // Essayer path1 (from -> hub)
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
            
            // Essayer path2 (hub -> to)
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
    
    return 0; // Aucun chemin alternatif trouvé
}

// NOUVELLE FONCTION : Trouve l'objectif le plus proche de complétion
int findNearestCompletionObjective(GameState* state) {
    int bestObjective = -1;
    int lowestRoutesNeeded = 999;
    int highestProgress = -1;
    
    printf("=== FINDING NEAREST COMPLETION OBJECTIVE ===\n");
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (isObjectiveCompleted(state, state->objectives[i])) {
            continue; // Déjà complété
        }
        
        int objFrom = state->objectives[i].from;
        int objTo = state->objectives[i].to;
        int objScore = state->objectives[i].score;
        
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findSmartestPath(state, objFrom, objTo, path, &pathLength);
        
        if (distance <= 0) continue;
        
        // Analyser ce chemin en détail
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
                // Vérifier si on peut prendre cette route maintenant
                MoveData testMove;
                if (canTakeRoute(state, cityA, cityB, &testMove)) {
                    // Route disponible ET on a les cartes!
                }
            } else {
                routesBlocked++;
            }
        }
        
        int routesNeeded = routesAvailable; // Seulement les routes libres
        float completionProgress = (float)routesOwned / totalRoutes;
        
        printf("Obj %d (%d->%d): %d/%d owned (%.0f%%), %d needed, %d blocked, score %d\n",
               i+1, objFrom, objTo, routesOwned, totalRoutes, 
               completionProgress * 100, routesNeeded, routesBlocked, objScore);
        
        // PRIORITÉ 1: Objectifs à 1 route de complétion
        if (routesNeeded == 1 && routesBlocked == 0) {
            printf("*** PRIORITY 1: Objective %d needs only 1 route! ***\n", i+1);
            return i;
        }
        
        // PRIORITÉ 2: Objectifs à 2 routes de complétion
        if (routesNeeded == 2 && routesBlocked == 0 && routesNeeded < lowestRoutesNeeded) {
            lowestRoutesNeeded = routesNeeded;
            bestObjective = i;
            highestProgress = routesOwned;
        }
        
        // PRIORITÉ 3: Objectifs avec le plus de progrès (mais pas bloqués)
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

// FONCTION MODIFIÉE : Focus absolu sur UN objectif à la fois
int workOnSingleObjective(GameState* state, MoveData* moveData) {
    // NOUVEAU: Toujours choisir l'objectif le plus proche de complétion
    int bestObjective = findNearestCompletionObjective(state);
    
    if (bestObjective < 0) {
        return buildLongestRoute(state, moveData);
    }
    
    // Forcer le focus sur cet objectif spécifique
    currentObjectiveIndex = bestObjective;
    
    // Vérifier si cet objectif est maintenant complété
    if (isObjectiveCompleted(state, state->objectives[currentObjectiveIndex])) {
        printf("Objective %d just completed! Looking for next one.\n", currentObjectiveIndex + 1);
        currentObjectiveIndex = -1;
        return workOnSingleObjective(state, moveData);
    }
    
    // Calculer le chemin pour cet objectif spécifique
    int objFrom = state->objectives[currentObjectiveIndex].from;
    int objTo = state->objectives[currentObjectiveIndex].to;
    
    int distance = findSmartestPath(state, objFrom, objTo, currentPath, &currentPathLength);
    
    if (distance <= 0) {
        printf("No path found for priority objective %d->%d!\n", objFrom, objTo);
        currentObjectiveIndex = -1;
        return workOnSingleObjective(state, moveData);
    }
    
    printf("=== FOCUS MODE: Working on objective %d->%d ===\n", objFrom, objTo);
    
    // Analyser et afficher le chemin avec priorités
    printf("Priority path: ");
    for (int i = 0; i < currentPathLength; i++) {
        printf("%d", currentPath[i]);
        if (i < currentPathLength - 1) {
            int owner = getRouteOwner(state, currentPath[i], currentPath[i+1]);
            if (owner == 1) {
                printf("-[✓]->");
            } else if (owner == 0) {
                printf("-[!]->");  // Route nécessaire!
            } else {
                printf("-[X]->");  // Bloquée
            }
        }
    }
    printf("\n");
    
    // Trouver la PREMIÈRE route manquante et la prendre absolument
    for (int i = 0; i < currentPathLength - 1; i++) {
        int cityA = currentPath[i];
        int cityB = currentPath[i + 1];
        
        int routeOwner = getRouteOwner(state, cityA, cityB);
        
        if (routeOwner == 0) { // Route libre - c'est ça qu'il faut!
            printf("*** MUST TAKE: Route %d->%d for objective completion ***\n", cityA, cityB);
            
            if (canTakeRoute(state, cityA, cityB, moveData)) {
                printf("Taking critical route %d->%d\n", cityA, cityB);
                return 1;
            } else {
                // Pas assez de cartes - piocher UNIQUEMENT pour cette route
                printf("Need cards specifically for route %d->%d\n", cityA, cityB);
                return drawCardsForRouteAggressively(state, cityA, cityB, moveData);
            }
        } else if (routeOwner == 1) {
            // Route déjà possédée, continuer
            continue;
        } else {
            // Route bloquée par adversaire - recalculer immédiatement
            printf("Route %d->%d blocked! Recalculating for objective %d\n", 
                   cityA, cityB, currentObjectiveIndex + 1);
            return workOnSingleObjective(state, moveData);
        }
    }
    
    // Si on arrive ici, l'objectif devrait être complété
    printf("Objective path seems complete\n");
    return workOnSingleObjective(state, moveData);
}

// NOUVELLE FONCTION : Pioche agressive pour une route spécifique
int drawCardsForRouteAggressively(GameState* state, int from, int to, MoveData* moveData) {
    // Analyser exactement quelles cartes il nous faut pour cette route
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
    
    // Calculer exactement ce qui manque
    int have = 0;
    int haveLocomotives = state->nbCardsByColor[LOCOMOTIVE];
    
    if (routeColor == LOCOMOTIVE) {
        // Route grise - compter nos meilleures couleurs
        int bestColorCount = 0;
        for (int c = PURPLE; c <= GREEN; c++) {
            if (state->nbCardsByColor[c] > bestColorCount) {
                bestColorCount = state->nbCardsByColor[c];
            }
        }
        have = bestColorCount;
        printf("Gray route: best color has %d cards, %d locomotives\n", have, haveLocomotives);
    } else {
        // Route colorée
        have = state->nbCardsByColor[routeColor];
        printf("Colored route: have %d of color %d, %d locomotives\n", have, routeColor, haveLocomotives);
    }
    
    int totalHave = have + haveLocomotives;
    int stillNeed = routeLength - totalHave;
    
    printf("Total available: %d, still need: %d\n", totalHave, stillNeed);
    
    // PRIORITÉ ABSOLUE: prendre les cartes exactes nécessaires
    
    // 1. Locomotives visibles (toujours utiles)
    for (int i = 0; i < 5; i++) {
        if (state->visibleCards[i] == LOCOMOTIVE) {
            moveData->action = DRAW_CARD;
            moveData->drawCard = LOCOMOTIVE;
            printf("Taking locomotive for critical route\n");
            return 1;
        }
    }
    
    // 2. Couleur exacte si route colorée
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
    
    // 3. Pour route grise, prendre n'importe quelle couleur
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
    
    // 4. Pioche aveugle en dernier recours
    moveData->action = DRAW_BLIND_CARD;
    printf("Drawing blind for critical route\n");
    return 1;
}

// Construire la route la plus longue possible en se connectant au réseau existant
int buildLongestRoute(GameState* state, MoveData* moveData) {
    printf("=== BUILDING LONGEST ROUTE ===\n");
    
    // Si on a trop de cartes (>15), essayer de piocher de nouveaux objectifs FACILES
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
    
    // Identifier toutes nos villes connectées (extrémités de notre réseau)
    int networkCities[MAX_CITIES];
    int networkCityCount = 0;
    
    // Trouver toutes les villes de notre réseau
    for (int i = 0; i < state->nbClaimedRoutes; i++) {
        int routeIndex = state->claimedRoutes[i];
        if (routeIndex >= 0 && routeIndex < state->nbTracks) {
            int from = state->routes[routeIndex].from;
            int to = state->routes[routeIndex].to;
            
            // Ajouter les villes à notre liste si pas déjà présentes
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
    
    // Chercher la meilleure route à ajouter pour étendre notre réseau
    int bestRouteIndex = -1;
    int bestScore = 0;
    
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 0) { // Route libre
            int from = state->routes[i].from;
            int to = state->routes[i].to;
            int length = state->routes[i].length;
            
            // Vérifier si cette route se connecte à notre réseau
            int connectsToNetwork = 0;
            for (int j = 0; j < networkCityCount; j++) {
                if (networkCities[j] == from || networkCities[j] == to) {
                    connectsToNetwork = 1;
                    break;
                }
            }
            
            if (connectsToNetwork && canTakeRoute(state, from, to, moveData)) {
                // Score = longueur + bonus pour routes très longues
                int score = length * 10;
                if (length >= 5) score += 100; // Bonus routes 5-6
                if (length >= 4) score += 50;  // Bonus routes 4+
                if (length >= 3) score += 25;  // Bonus routes 3+
                
                printf("Route %d->%d: length %d, score %d, connects to network\n", 
                       from, to, length, score);
                
                if (score > bestScore) {
                    bestScore = score;
                    bestRouteIndex = i;
                }
            }
        }
    }
    
    // Prendre la meilleure route connectée
    if (bestRouteIndex >= 0) {
        int from = state->routes[bestRouteIndex].from;
        int to = state->routes[bestRouteIndex].to;
        int length = state->routes[bestRouteIndex].length;
        
        printf("Extending network with route %d->%d (length %d)\n", from, to, length);
        return canTakeRoute(state, from, to, moveData);
    }
    
    // Si aucune route connectée, prendre la plus longue route disponible
    printf("No connected routes, taking longest available route\n");
    return takeAnyGoodRoute(state, moveData);
}

// Obtenir le propriétaire d'une route
int getRouteOwner(GameState* state, int from, int to) {
    for (int i = 0; i < state->nbTracks; i++) {
        if ((state->routes[i].from == from && state->routes[i].to == to) ||
            (state->routes[i].from == to && state->routes[i].to == from)) {
            return state->routes[i].owner;
        }
    }
    return -1; // Route n'existe pas
}

// Vérifier si on peut prendre une route
int canTakeRoute(GameState* state, int from, int to, MoveData* moveData) {
    // Trouver la route
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
    
    // Vérifier wagons
    if (state->wagonsLeft < length) return 0;
    
    CardColor bestColor = NONE;
    int nbLocomotives = 0;
    
    // Route grise (n'importe quelle couleur)
    if (routeColor == LOCOMOTIVE) {
        // Chercher la couleur qu'on a le plus (éviter de gaspiller les locomotives)
        int maxCards = 0;
        for (int c = PURPLE; c <= GREEN; c++) {
            if (state->nbCardsByColor[c] >= length && state->nbCardsByColor[c] > maxCards) {
                maxCards = state->nbCardsByColor[c];
                bestColor = c;
                nbLocomotives = 0;
            }
        }
        
        // Si pas assez d'une couleur pure, utiliser couleur + locomotives
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
        
        // Dernier recours : locomotives pures
        if (bestColor == NONE && state->nbCardsByColor[LOCOMOTIVE] >= length) {
            bestColor = LOCOMOTIVE;
            nbLocomotives = length;
        }
    }
    // Route colorée spécifique
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
    
    // Configurer le mouvement
    moveData->action = CLAIM_ROUTE;
    moveData->claimRoute.from = from;
    moveData->claimRoute.to = to;
    moveData->claimRoute.color = bestColor;
    moveData->claimRoute.nbLocomotives = nbLocomotives;
    
    return 1;
}

// Piocher des cartes spécifiquement pour une route
int drawCardsForRoute(GameState* state, int from, int to, MoveData* moveData) {
    // Trouver la route et sa couleur
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
    
    // Toujours prendre une locomotive si visible
    for (int i = 0; i < 5; i++) {
        if (state->visibleCards[i] == LOCOMOTIVE) {
            moveData->action = DRAW_CARD;
            moveData->drawCard = LOCOMOTIVE;
            printf("Taking locomotive\n");
            return 1;
        }
    }
    
    // Route grise : prendre n'importe quelle couleur visible
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
    // Route colorée : prendre la couleur spécifique si visible
    else {
        for (int i = 0; i < 5; i++) {
            if (state->visibleCards[i] == routeColor) {
                moveData->action = DRAW_CARD;
                moveData->drawCard = routeColor;
                printf("Taking %d for colored route\n", routeColor);
                return 1;
            }
        }
        
        // Si pas visible, prendre n'importe quelle couleur utile
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
    
    // Pioche aveugle
    moveData->action = DRAW_BLIND_CARD;
    printf("Drawing blind card\n");
    return 1;
}

// Prendre n'importe quelle bonne route si tous les objectifs sont complétés
int takeAnyGoodRoute(GameState* state, MoveData* moveData) {
    printf("Looking for any good route...\n");
    
    // Chercher la route la plus longue qu'on peut prendre
    int bestLength = 0;
    int bestFrom = -1, bestTo = -1;
    
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 0) { // Route libre
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
    
    // Sinon piocher
    moveData->action = DRAW_BLIND_CARD;
    return 1;
}

// Point d'entrée principal
int decideNextMove(GameState* state, MoveData* moveData) {
    if (!state || !moveData) {
        printf("ERROR: NULL parameters\n");
        return 0;
    }
    
    return simpleStrategy(state, moveData);
}

// Interface pour choix d'objectifs
void chooseObjectivesStrategy(GameState* state, Objective* objectives, unsigned char* chooseObjectives) {
    simpleChooseObjectives(state, objectives, chooseObjectives);
}