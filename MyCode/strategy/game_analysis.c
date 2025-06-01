/**
 * game_analysis.c
 * Analyse de l'état du jeu et détermination des priorités stratégiques - VERSION NETTOYÉE
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "strategy.h"
#include "../rules.h"      
#include "../gamestate.h"  

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
    
    // Premier tour - objectifs obligatoires
    if (state->nbObjectives == 0) {
        return DRAW_CARDS;
    }
    
    // Fin de partie imminente
    if (state->lastTurn || state->wagonsLeft <= 3 || state->opponentWagonsLeft <= 2) {
        // Vérifier si on peut compléter un objectif rapidement
        for (int i = 0; i < state->nbObjectives; i++) {
            if (!isObjectiveCompleted(state, state->objectives[i])) {
                int routesRestantes = countRemainingRoutesForObjective(state, i);
                if (routesRestantes == 1) {
                    return COMPLETE_OBJECTIVES;
                }
            }
        }
        return BUILD_NETWORK;
    }
    
    // Compter objectifs complétés vs incomplets
    int completedObjectives = 0;
    int incompleteObjectives = 0;
    int incompleteObjectiveValue = 0;
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (isObjectiveCompleted(state, state->objectives[i])) {
            completedObjectives++;
        } else {
            incompleteObjectives++;
            incompleteObjectiveValue += state->objectives[i].score;
        }
    }
    
    // Compter nos cartes totales
    int totalCards = 0;
    int maxColorCards = 0;
    for (int i = 1; i < 10; i++) {
        totalCards += state->nbCardsByColor[i];
        if (state->nbCardsByColor[i] > maxColorCards) {
            maxColorCards = state->nbCardsByColor[i];
        }
    }
    
    // Routes critiques disponibles
    int criticalRoutesReady = 0;
    for (int i = 0; i < criticalRouteCount; i++) {
        if (criticalRoutes[i].hasEnoughCards) {
            criticalRoutesReady++;
        }
    }
    
    // LOGIQUE DE DÉCISION SIMPLIFIÉE
    
    // Routes critiques prêtes = priorité absolue
    if (criticalRoutesReady > 0) {
        return COMPLETE_OBJECTIVES;
    }
    
    // Début de partie avec peu de cartes = accumuler
    if (phase == PHASE_EARLY && totalCards < 8) {
        return DRAW_CARDS;
    }
    
    // Beaucoup de cartes = construire
    if (totalCards >= 12 || maxColorCards >= 6) {
        return BUILD_NETWORK;
    }
    
    // Objectifs incomplets à haute valeur
    if (incompleteObjectiveValue >= 15 && phase >= PHASE_MIDDLE) {
        return COMPLETE_OBJECTIVES;
    }
    
    // Tous objectifs complétés
    if (incompleteObjectives == 0) {
        if (state->nbObjectives < 3 && phase < PHASE_LATE) {
            return DRAW_CARDS;
        } else {
            return BUILD_NETWORK;
        }
    }
    
    // Trop de pioches consécutives = forcer action
    if (consecutiveDraws >= 4) {
        return BUILD_NETWORK;
    }
    
    // Milieu/fin de partie avec objectifs incomplets
    if (phase >= PHASE_MIDDLE && incompleteObjectives > 0) {
        // Vérifier si des objectifs sont "presque" complétables
        int nearlyCompleteObjectives = 0;
        for (int i = 0; i < state->nbObjectives; i++) {
            if (!isObjectiveCompleted(state, state->objectives[i])) {
                int routesNeeded = countRemainingRoutesForObjective(state, i);
                if (routesNeeded >= 0 && routesNeeded <= 2) {
                    nearlyCompleteObjectives++;
                }
            }
        }
        
        if (nearlyCompleteObjectives > 0) {
            return COMPLETE_OBJECTIVES;
        }
    }
    
    // Défaut selon la phase
    if (phase == PHASE_EARLY) {
        return DRAW_CARDS;
    } else if (totalCards >= 8) {
        return BUILD_NETWORK;
    } else {
        return DRAW_CARDS;
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

// SUPPRESSION COMPLÈTE de identifyAndPrioritizeBottlenecks() 
// Cette fonction utilisait les structures MapRegion supprimées
// et les constantes MAX_CENTRAL_CITIES, MAX_HUB_CONNECTIONS

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
    
    // Priorité aux objectifs
    for (int i = 0; i < state->nbObjectives && count > 0; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            int path[MAX_CITIES];
            int pathLength = 0;
            int objFrom = state->objectives[i].from;
            int objTo = state->objectives[i].to;
            
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
}