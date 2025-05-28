/**
 * game_analysis.c
 * Analyse de l'état du jeu et détermination des priorités stratégiques - VERSION CORRIGÉE
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "strategy.h"
#include "../rules.h"      
#include "../gamestate.h"  


// Régions stratégiques de la carte
static MapRegion regions[] = {
    {{31, 32, 33, 34, 30, 27, 28, 29}, 8, 85}, // Est
    {{19, 20, 21, 22, 18, 23}, 6, 90},         // Centre
    {{0, 1, 2, 8, 9, 10}, 6, 80},              // Ouest
    {{24, 25, 26, 35, 23, 16, 15, 14}, 8, 75}, // Sud
    {{3, 4, 5, 6, 7, 11, 12, 13}, 8, 70}       // Nord
};
static const int NUM_REGIONS = 5;

// Détermine la phase de jeu actuelle pour adapter la stratégie
int determineGamePhase(GameState* state) {
    // Évaluation basée sur les wagons restants et le tour de jeu
    if (state->turnCount < 5 || state->wagonsLeft > 35) {
        return PHASE_EARLY;
    } 
    else if (state->wagonsLeft < 12 || state->lastTurn) {
        return PHASE_FINAL;
    }
    else if (state->wagonsLeft < 25) {
        return PHASE_LATE;
    }
    else {
        return PHASE_MIDDLE;
    }
}

// Détermination des priorités stratégiques
StrategicPriority determinePriority(GameState* state, int phase, CriticalRoute* criticalRoutes, 
                                   int criticalRouteCount, int consecutiveDraws) {
    
    // 1. Cas spécial: Premier tour - piocher des objectifs
    if (state->nbObjectives == 0) {
        printf("Priorité: PIOCHER DES OBJECTIFS (premier tour)\n");
        return DRAW_CARDS;
    }
    
    // 2. Analyse des objectifs
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
    
    printf("Objectifs: %d complétés, %d incomplets, valeur restante: %d\n",
           completedObjectives, incompleteObjectives, incompleteObjectiveValue);
    
    // 3. Statistiques sur cartes disponibles
    int totalCards = 0;
    int maxSameColorCards = 0;
    int colorWithMostCards = 0;
    for (int i = 1; i < 10; i++) {
        totalCards += state->nbCardsByColor[i];
        if (state->nbCardsByColor[i] > maxSameColorCards) {
            maxSameColorCards = state->nbCardsByColor[i];
            colorWithMostCards = i;
        }
    }
    
    // 4. Déterminer la priorité selon les conditions
    
    // 4.1 Stratégie d'accumulation en début de partie
    if (phase == PHASE_EARLY && state->turnCount < 6 && totalCards < 8) {
        printf("Priorité: ACCUMULER DES CARTES (phase initiale, seulement %d cartes)\n", totalCards);
        return DRAW_CARDS;
    }
    
    // 4.2 Fin de partie avec objectifs incomplets - priorité absolue
    if ((phase == PHASE_LATE || phase == PHASE_FINAL) && incompleteObjectives > 0) {
        printf("URGENCE FIN DE PARTIE: Prioriser complétion des %d objectifs incomplets!\n", 
               incompleteObjectives);
        return COMPLETE_OBJECTIVES;
    }
    
    // 4.3 Dernier tour - maximiser points
    if (state->lastTurn) {
        printf("DERNIER TOUR: Utiliser nos ressources restantes!\n");
        return BUILD_NETWORK;
    }
    
    // 4.4 Analyse des objectifs critiques
    if ((phase == PHASE_LATE || phase == PHASE_FINAL) && incompleteObjectives > 0) {
        for (int i = 0; i < state->nbObjectives; i++) {
            if (!isObjectiveCompleted(state, state->objectives[i])) {
                extern int countRemainingRoutesForObjective(GameState* state, int objectiveIndex);
                int remainingRoutes = countRemainingRoutesForObjective(state, i);
                
                if (remainingRoutes >= 0 && remainingRoutes <= 2) {
                    printf("URGENCE: Objectif %d proche de la complétion! (reste %d routes)\n", 
                           i+1, remainingRoutes);
                    return COMPLETE_OBJECTIVES;
                }
            }
        }
    }
    
    // 4.5 Objectifs à haute valeur
    if (incompleteObjectiveValue > 15 && phase >= PHASE_MIDDLE) {
        printf("PRIORITÉ CRITIQUE: Objectifs incomplets de grande valeur (%d points)\n", 
               incompleteObjectiveValue);
        return COMPLETE_OBJECTIVES;
    }
    
    // 4.6 Tous objectifs complétés
    if (incompleteObjectives == 0) {
        if (state->nbObjectives < 3 && phase < PHASE_LATE) {
            printf("Priorité: PIOCHER DES OBJECTIFS (tous complétés)\n");
            return DRAW_CARDS;
        } else {
            printf("Tous objectifs complétés: Prioriser les routes longues\n");
            return BUILD_NETWORK;
        }
    }
    
    // 4.7 Analyse plus fine pour autres cas
    float completionRate = (float)completedObjectives / state->nbObjectives;
    
    if (completionRate > 0.7 && state->nbObjectives <= 3 && phase != PHASE_FINAL) {
        printf("Priorité: PIOCHER DES OBJECTIFS (bon taux de complétion: %.2f)\n", completionRate);
        return DRAW_CARDS;
    }
    else if (completionRate < 0.3 && state->wagonsLeft < 30) {
        printf("Priorité: COMPLÉTER OBJECTIFS (faible taux de complétion: %.2f)\n", completionRate);
        return COMPLETE_OBJECTIVES;
    }
    else {
        // Comparer scores estimés
        int ourEstimatedScore = calculateScore(state);
        int estimatedOpponentScore = estimateOpponentScore(state);
        
        printf("Score estimé: Nous = %d, Adversaire = %d\n", ourEstimatedScore, estimatedOpponentScore);
        
        if (ourEstimatedScore < estimatedOpponentScore && phase != PHASE_EARLY) {
            printf("Priorité: CONSTRUIRE RÉSEAU (nous sommes en retard)\n");
            return BUILD_NETWORK;
        }
        else if (incompleteObjectiveValue > 20) {
            printf("Priorité: COMPLÉTER OBJECTIFS (valeur élevée: %d)\n", incompleteObjectiveValue);
            return COMPLETE_OBJECTIVES;
        }
        else {
            // Équilibrer entre objectifs et routes
            if (maxSameColorCards >= 4) {
                printf("Priorité: CONSTRUIRE RÉSEAU (beaucoup de cartes: %d)\n", maxSameColorCards);
                return BUILD_NETWORK;
            } else if (state->turnCount % 3 == 0) {
                printf("Priorité: CONSTRUIRE RÉSEAU (alternance)\n");
                return BUILD_NETWORK;
            } else {
                printf("Priorité: COMPLÉTER OBJECTIFS (alternance)\n");
                return COMPLETE_OBJECTIVES;
            }
        }
    }
}

// Évalue l'utilité générale d'une route
int evaluateRouteUtility(GameState* state, int routeIndex) {
    if (routeIndex < 0 || routeIndex >= state->nbTracks) {
        return 0;
    }
    
    int from = state->routes[routeIndex].from;
    int to = state->routes[routeIndex].to;
    int length = state->routes[routeIndex].length;
    
    int pointsByLength[] = {0, 1, 2, 4, 7, 10, 15};
    int baseScore = (length >= 0 && length <= 6) ? pointsByLength[length] : 0;
    
    // Bonus pour routes sur chemin d'objectif
    int objectiveBonus = 0;
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (isObjectiveCompleted(state, state->objectives[i])) {
            continue;
        }
        
        int objFrom = state->objectives[i].from;
        int objTo = state->objectives[i].to;
        int objScore = state->objectives[i].score;
        
        int path[MAX_CITIES];
        int pathLength = 0;
        
        // CHANGEMENT: Utiliser findShortestPath pour évaluer l'utilité
        if (findShortestPath(state, objFrom, objTo, path, &pathLength) >= 0) {
            extern int isRouteInPath(int from, int to, int* path, int pathLength);
            if (isRouteInPath(from, to, path, pathLength)) {
                objectiveBonus += objScore * 2;
                objectiveBonus += length * 3;
            }
        }
    }
    
    // Pénalité pour utiliser des wagons quand il en reste peu
    int wagonPenalty = 0;
    if (state->wagonsLeft < 15) {
        wagonPenalty = length * (15 - state->wagonsLeft) / 2;
    }
    
    // Bonus pour les routes qui connectent à notre réseau existant
    int networkBonus = 0;
    int cityConnectivity[MAX_CITIES] = {0};
    analyzeExistingNetwork(state, cityConnectivity);

    // Si l'une des extrémités est un hub (ville avec plusieurs connexions)
    if (cityConnectivity[from] >= 2 || cityConnectivity[to] >= 2) {
        networkBonus += 50;
        
        // Bonus supplémentaire si cette route fait partie d'un objectif non complété
        for (int i = 0; i < state->nbObjectives; i++) {
            if (!isObjectiveCompleted(state, state->objectives[i])) {
                int objFrom = state->objectives[i].from;
                int objTo = state->objectives[i].to;
                
                if ((from == objFrom || from == objTo || to == objFrom || to == objTo) &&
                    (cityConnectivity[from] >= 2 || cityConnectivity[to] >= 2)) {
                    networkBonus += 100 * state->objectives[i].score;
                    printf("BONUS MAJEUR: Route %d->%d connecte un hub à un objectif!\n", from, to);
                }
            }
        }
    }
    // Bonus pour les villes qui sont presque des hubs (1 connexion)
    else if (cityConnectivity[from] == 1 || cityConnectivity[to] == 1) {
        networkBonus += 25;
    }

    return baseScore + objectiveBonus - wagonPenalty + networkBonus;
}

// Évaluation avancée de l'utilité d'une route
int enhancedEvaluateRouteUtility(GameState* state, int routeIndex) {
    if (routeIndex < 0 || routeIndex >= state->nbTracks) {
        printf("ERROR: Invalid route index %d in enhancedEvaluateRouteUtility\n", routeIndex);
        return 0;
    }

    int utility = evaluateRouteUtility(state, routeIndex);
    int from = state->routes[routeIndex].from;
    int to = state->routes[routeIndex].to;
    int length = state->routes[routeIndex].length;
    
    // Bonus pour connexion à notre réseau existant
    for (int i = 0; i < state->nbClaimedRoutes; i++) {
        int claimedRouteIndex = state->claimedRoutes[i];
        if (claimedRouteIndex >= 0 && claimedRouteIndex < state->nbTracks) {
            if (state->routes[claimedRouteIndex].from == from || 
                state->routes[claimedRouteIndex].to == from ||
                state->routes[claimedRouteIndex].from == to || 
                state->routes[claimedRouteIndex].to == to) {
                utility += 30;
                break;
            }
        }
    }
    
    // Bonus stratégique pour les routes longues
    if (length >= 5) {
        utility += length * 100;
    } else if (length >= 4) {
        utility += length * 50;
    } else if (length >= 3) {
        utility += length * 20;
    }
    
    // Pénalité pour routes inutiles en fin de partie
    int phase = determineGamePhase(state);
    if ((phase == PHASE_LATE || phase == PHASE_FINAL)) {
        extern int calculateObjectiveProgress(GameState* state, int routeIndex);
        if (calculateObjectiveProgress(state, routeIndex) == 0) {
            utility -= 50;
        }
    }
    
    // Bonus pour routes bloquant l'adversaire
    if (from < MAX_CITIES && to < MAX_CITIES &&
        (opponentCitiesOfInterest[from] > 8 || opponentCitiesOfInterest[to] > 8)) {
        utility += 35;
    }
    
    return utility;
}

// Évalue l'efficacité d'utilisation de nos cartes
int evaluateCardEfficiency(GameState* state, int routeIndex) {
    if (routeIndex < 0 || routeIndex >= state->nbTracks) {
        return 0;
    }

    CardColor routeColor = state->routes[routeIndex].color;
    int length = state->routes[routeIndex].length;
    

    if (routeColor != LOCOMOTIVE && length <= 2 && state->nbCardsByColor[LOCOMOTIVE] > 0) {
        return 50;  // Pénaliser utilisation de locomotives pour routes courtes
    }

    // Pour les routes grises, trouver la couleur la plus efficace
    if (routeColor == LOCOMOTIVE) {
        int bestEfficiency = 0;
        for (int c = 1; c < 9; c++) {  // Ignorer NONE et LOCOMOTIVE
            if (state->nbCardsByColor[c] > 0) {
                int cardsNeeded = length;
                int cardsAvailable = state->nbCardsByColor[c];
                
                if (cardsNeeded <= cardsAvailable) {
                    int efficiency = 150;
                    if (efficiency > bestEfficiency) {
                        bestEfficiency = efficiency;
                    }
                } else {
                    int locosNeeded = cardsNeeded - cardsAvailable;
                    if (locosNeeded <= state->nbCardsByColor[LOCOMOTIVE]) {
                        int efficiency = 100 - (locosNeeded * 10);
                        if (efficiency > bestEfficiency) {
                            bestEfficiency = efficiency;
                        }
                    }
                }
            }
        }
        return bestEfficiency;
    } 
    // Pour les routes colorées
    else {
        int cardsNeeded = length;
        int cardsAvailable = state->nbCardsByColor[routeColor];
        
        if (cardsNeeded <= cardsAvailable) {
            return 150;
        } else {
            int locosNeeded = cardsNeeded - cardsAvailable;
            if (locosNeeded <= state->nbCardsByColor[LOCOMOTIVE]) {
                return 100 - (locosNeeded * 10);
            }
        }
    }
    
    return 0;
}

// Identifie les routes centrales stratégiques
void identifyAndPrioritizeBottlenecks(GameState* state, int* prioritizedRoutes, int* count) {
    *count = 0;
    
    // 1. Identifier les "hubs" (villes avec plusieurs connexions)
    int hubCities[MAX_CITIES] = {0};
    int hubCount = 0;
    
    for (int i = 0; i < state->nbCities && hubCount < MAX_CENTRAL_CITIES; i++) {
        int connections = 0;
        
        for (int j = 0; j < state->nbTracks; j++) {
            if (state->routes[j].from == i || state->routes[j].to == i) {
                connections++;
            }
        }
        
        if (connections >= MAX_HUB_CONNECTIONS) {
            hubCities[hubCount++] = i;
        }
    }
    
    // 2. Identifier les routes entre hubs (goulots d'étranglement)
    for (int i = 0; i < hubCount && *count < MAX_ROUTES; i++) {
        for (int j = i+1; j < hubCount && *count < MAX_ROUTES; j++) {
            int cityA = hubCities[i];
            int cityB = hubCities[j];
            
            // Vérifier s'il existe une route directe
            for (int r = 0; r < state->nbTracks; r++) {
                if (((state->routes[r].from == cityA && state->routes[r].to == cityB) ||
                     (state->routes[r].from == cityB && state->routes[r].to == cityA)) &&
                    state->routes[r].owner == 0) {
                    // C'est une route bottleneck
                    prioritizedRoutes[(*count)++] = r;
                }
            }
        }
    }
    
    // 3. Ajouter les routes entre régions stratégiques
    for (int r1 = 0; r1 < NUM_REGIONS && *count < MAX_ROUTES; r1++) {
        for (int r2 = r1+1; r2 < NUM_REGIONS && *count < MAX_ROUTES; r2++) {
            // Pour chaque paire de villes entre deux régions
            for (int c1 = 0; c1 < regions[r1].cityCount && *count < MAX_ROUTES; c1++) {
                for (int c2 = 0; c2 < regions[r2].cityCount && *count < MAX_ROUTES; c2++) {
                    int cityA = regions[r1].cities[c1];
                    int cityB = regions[r2].cities[c2];
                    
                    // Vérifier routes directes
                    for (int r = 0; r < state->nbTracks; r++) {
                        if (((state->routes[r].from == cityA && state->routes[r].to == cityB) ||
                             (state->routes[r].from == cityB && state->routes[r].to == cityA)) &&
                            state->routes[r].owner == 0) {
                            // Ajouter seulement si pas déjà dans la liste
                            bool alreadyAdded = false;
                            for (int k = 0; k < *count; k++) {
                                if (prioritizedRoutes[k] == r) {
                                    alreadyAdded = true;
                                    break;
                                }
                            }
                            
                            if (!alreadyAdded) {
                                prioritizedRoutes[(*count)++] = r;
                            }
                        }
                    }
                }
            }
        }
    }
}

// Cherche la meilleure route à prendre en fin de partie
int evaluateEndgameScore(GameState* state, int routeIndex) {
    if (routeIndex < 0 || routeIndex >= state->nbTracks) {
        return -1000;
    }

    // Simuler la prise de route
    int originalOwner = state->routes[routeIndex].owner;
    state->routes[routeIndex].owner = 1;
    
    // Mettre à jour la connectivité
    updateCityConnectivity(state);
    
    // Calculer le score avec cette route
    int score = calculateScore(state);
    
    // Restaurer l'état original
    state->routes[routeIndex].owner = originalOwner;
    updateCityConnectivity(state);
    
    return score;
}

// Planifie les prochaines routes à prendre
void planNextRoutes(GameState* state, int* routesPlan, int count) {
    // Initialiser le plan
    for (int i = 0; i < count; i++) {
        routesPlan[i] = -1;
    }
    
    // 1. Priorité aux objectifs
    for (int i = 0; i < state->nbObjectives && count > 0; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            int path[MAX_CITIES];
            int pathLength = 0;
            int objFrom = state->objectives[i].from;
            int objTo = state->objectives[i].to;
            
            // CHANGEMENT: Utiliser findShortestPath pour planifier les routes
            if (findShortestPath(state, objFrom, objTo, path, &pathLength) > 0) {
                for (int j = 0; j < pathLength - 1 && count > 0; j++) {
                    int cityA = path[j];
                    int cityB = path[j+1];
                    
                    // Vérifier si cette route est libre
                    for (int r = 0; r < state->nbTracks; r++) {
                        if (((state->routes[r].from == cityA && state->routes[r].to == cityB) ||
                             (state->routes[r].from == cityB && state->routes[r].to == cityA)) &&
                            state->routes[r].owner == 0) {
                            
                            // Vérifier que cette route n'est pas déjà dans le plan
                            bool alreadyPlanned = false;
                            for (int p = 0; p < count; p++) {
                                if (routesPlan[p] == r) {
                                    alreadyPlanned = true;
                                    break;
                                }
                            }
                            
                            if (!alreadyPlanned) {
                                // Ajouter au plan
                                routesPlan[--count] = r;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // 2. Si plan incomplet, ajouter routes stratégiques
    if (count > 0) {
        int strategicRoutes[MAX_ROUTES];
        int routeCount = 0;
        identifyAndPrioritizeBottlenecks(state, strategicRoutes, &routeCount);
        
        for (int i = 0; i < routeCount && count > 0; i++) {
            // Vérifier que cette route n'est pas déjà dans le plan
            bool alreadyPlanned = false;
            for (int p = 0; p < count; p++) {
                if (routesPlan[p] == strategicRoutes[i]) {
                    alreadyPlanned = true;
                    break;
                }
            }
            
            if (!alreadyPlanned) {
                routesPlan[--count] = strategicRoutes[i];
            }
        }
    }
}