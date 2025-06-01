/**
 * objective_analysis.c
 * Analyse et gestion des objectifs 
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include "strategy.h"
#include "../rules.h"      
#include "../gamestate.h"  

// Évalue combien une route aide à compléter nos objectifs
int calculateObjectiveProgress(GameState* state, int routeIndex) {
    if (!state || routeIndex < 0 || routeIndex >= state->nbTracks) {
        return 0;
    }

    int from = state->routes[routeIndex].from;
    int to = state->routes[routeIndex].to;
    int progress = 0;
    
    if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
        return 0;
    }
    
    const int objectiveMultiplier = 200;
    
    // Vérifier chaque objectif non complété
    for (int i = 0; i < state->nbObjectives; i++) {
        if (i < 0 || i >= MAX_OBJECTIVES) continue;
        
        if (isObjectiveCompleted(state, state->objectives[i])) {
            continue;
        }
        
        int objFrom = state->objectives[i].from;
        int objTo = state->objectives[i].to;
        int objScore = state->objectives[i].score;
        
        if (objFrom < 0 || objFrom >= state->nbCities || objTo < 0 || objTo >= state->nbCities) {
            continue;
        }
        
        // Connexion directe priorité importante
        if ((from == objFrom && to == objTo) || (from == objTo && to == objFrom)) {
            progress += objScore * objectiveMultiplier * 5;
            continue;
        }
        
        // Combien de routes manquent pour cet objectif
        int remainingRoutes = countRemainingRoutesForObjective(state, i);
        
        // Objectif presque complété priorité élevée
        if (remainingRoutes == 1) {
            int finalRouteBonus = objScore * objectiveMultiplier * 3;
            
            // Vérifier si cette route est la dernière nécessaire
            int path[MAX_CITIES];
            int pathLength = 0;
            if (findShortestPath(state, objFrom, objTo, path, &pathLength) > 0) {
                for (int j = 0; j < pathLength - 1; j++) {
                    if ((path[j] == from && path[j+1] == to) || (path[j] == to && path[j+1] == from)) {
                        progress += finalRouteBonus;
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
            // Vérifier si cette route est sur le chemin
            for (int j = 0; j < pathLength - 1; j++) {
                if ((path[j] == from && path[j+1] == to) || (path[j] == to && path[j+1] == from)) {
                    progress += objScore * objectiveMultiplier;
                    
                    // Vérifier si c'est un pont critique
                    int originalOwner = state->routes[routeIndex].owner;
                    state->routes[routeIndex].owner = 2; // Simuler blocage
                    
                    int altPath[MAX_CITIES];
                    int altPathLength = 0;
                    int altDistance = findShortestPath(state, objFrom, objTo, altPath, &altPathLength);
                    
                    state->routes[routeIndex].owner = originalOwner; // Restaurer
                    
                    if (altDistance <= 0) {
                        progress += objScore * objectiveMultiplier; // Double bonus
                    }
                    
                    break;
                }
            }
        }
    }
    
    return progress;
}

// Identifie les routes critiques pour compléter des objectifs
void identifyCriticalRoutes(GameState* state, CriticalRoute* criticalRoutes, int* count) {
    *count = 0;
    
    // Pour chaque objectif non complété
    for (int i = 0; i < state->nbObjectives; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            int objFrom = state->objectives[i].from;
            int objTo = state->objectives[i].to;
            int objScore = state->objectives[i].score;
            
            int path[MAX_CITIES];
            int pathLength = 0;
            if (findShortestPath(state, objFrom, objTo, path, &pathLength) > 0) {
                // Analyser ce chemin pour trouver les segments manquants
                for (int j = 0; j < pathLength - 1; j++) {
                    int cityA = path[j];
                    int cityB = path[j+1];
                    
                    // Trouver si cette route est déjà prise ou disponible
                    int routeOwnerValue = -1;
                    int routeIndex = -1;
                    
                    for (int k = 0; k < state->nbTracks; k++) {
                        if ((state->routes[k].from == cityA && state->routes[k].to == cityB) ||
                            (state->routes[k].from == cityB && state->routes[k].to == cityA)) {
                            
                            routeOwnerValue = state->routes[k].owner;
                            routeIndex = k;
                            break;
                        }
                    }
                    
                    // Si route disponible, c'est un segment critique
                    if (routeOwnerValue == 0 && routeIndex >= 0) {
                        // Vérifier si nous avons assez de cartes
                        CardColor bestColor;
                        int nbLocomotives;
                        bool hasCards = haveEnoughCards(state, cityA, cityB, &bestColor, &nbLocomotives);
                        
                        if (hasCards && bestColor != NONE) {
                            // Vérifier une dernière fois si la couleur est correcte
                            CardColor routeColor = state->routes[routeIndex].color;
                            CardColor routeSecondColor = state->routes[routeIndex].secondColor;
                            
                            if (routeColor != LOCOMOTIVE) {  // Si ce n'est pas une route grise
                                if (bestColor != routeColor && bestColor != routeSecondColor && bestColor != LOCOMOTIVE) {
                                    hasCards = false;  // Marquer comme n'ayant pas assez de cartes
                                }
                            }
                        }
                        
                        // Calculer la priorité
                        int routesOwned = 0;
                        int routesAvailable = 0;
                        for (int k = 0; k < pathLength - 1; k++) {
                            int fromCity = path[k];
                            int toCity = path[k+1];
                            int rOwner = routeOwner(state, fromCity, toCity);
                            
                            if (rOwner == 1) routesOwned++;
                            else if (rOwner == 0) routesAvailable++;
                        }
                        
                        // Priorité très élevée si c'est le seul segment manquant
                        int priority = objScore * 100;
                        if (routesAvailable == 1 && routesOwned > 0) {
                            priority = objScore * 1000; // Priorité extrême
                        }
                        
                        // Si nous n'avons pas assez de cartes, réduire la priorité
                        if (!hasCards) {
                            priority /= 10;
                        }
                        
                        // Ajouter à la liste des routes critiques
                        if (*count < MAX_OBJECTIVES * 2) {
                            criticalRoutes[*count].from = cityA;
                            criticalRoutes[*count].to = cityB;
                            criticalRoutes[*count].objectiveIndex = i;
                            criticalRoutes[*count].priority = priority;
                            criticalRoutes[*count].color = bestColor;
                            criticalRoutes[*count].nbLocomotives = nbLocomotives;
                            criticalRoutes[*count].hasEnoughCards = hasCards;
                            (*count)++;
                        }
                    }
                }
            }
        }
    }
    
    // Trier par priorité (du plus au moins prioritaire)
    for (int i = 0; i < *count - 1; i++) {
        for (int j = 0; j < *count - i - 1; j++) {
            if (criticalRoutes[j].priority < criticalRoutes[j+1].priority) {
                CriticalRoute temp = criticalRoutes[j];
                criticalRoutes[j] = criticalRoutes[j+1];
                criticalRoutes[j+1] = temp;
            }
        }
    }
}

// Analyse détaillée des chemins pour les objectifs 
void checkObjectivesPaths(GameState* state) {
    if (!state) {
        return;
    }
    
    // L'analyse continue en arrière-plan sans polluer la sortie
    for (int i = 0; i < state->nbObjectives; i++) {
        if (i < 0 || i >= MAX_OBJECTIVES) {
            continue;
        }
        
        int from = state->objectives[i].from;
        int to = state->objectives[i].to;
        
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            continue;
        }
        
        bool completed = isObjectiveCompleted(state, state->objectives[i]);
        
        if (completed) {
            continue;
        }
        
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findShortestPath(state, from, to, path, &pathLength);

        if (distance > 0 && pathLength > 0) {
            int availableRoutes = 0;
            int claimedRoutes = 0;
            int blockedRoutes = 0;
            
            for (int j = 0; j < pathLength - 1 && j < MAX_CITIES - 1; j++) {
                int cityA = path[j];
                int cityB = path[j+1];
                
                if (cityA < 0 || cityA >= state->nbCities || cityB < 0 || cityB >= state->nbCities) {
                    continue;
                }
                
                for (int k = 0; k < state->nbTracks; k++) {
                    if ((state->routes[k].from == cityA && state->routes[k].to == cityB) ||
                        (state->routes[k].from == cityB && state->routes[k].to == cityA)) {
                        
                        if (state->routes[k].owner == 0) {
                            availableRoutes++;
                        } else if (state->routes[k].owner == 1) {
                            claimedRoutes++;
                        } else {
                            blockedRoutes++;
                        }
                        break;
                    }
                }
            }
            
            if (blockedRoutes > 0) {
                printf("WARNING: Objective %d blocked by opponent\n", i+1);
            }
        }
    }
}

// Fonction de choix des objectifs optimisée 
void improvedObjectiveEvaluation(GameState* state, Objective* objectives, bool* chooseObjectives) {
    if (!state || !objectives || !chooseObjectives) {
        return;
    }
    
    // Initialiser tous les choix à false
    for (int i = 0; i < 3; i++) {
        chooseObjectives[i] = false;
    }
    
    // Déterminer si c'est le premier tour
    bool isFirstTurn = (state->nbObjectives == 0);
    int phase = determineGamePhase(state);
    
    // Structure simple pour évaluer chaque objectif
    typedef struct {
        int index;
        int score;
        bool feasible;
        int difficulty;  // 1=facile, 5=très difficile
        float efficiency; // points par wagon nécessaire
    } ObjectiveScore;
    
    ObjectiveScore evalResults[3];
    
    // Évaluer chaque objectif
    for (int i = 0; i < 3; i++) {
        evalResults[i].index = i;
        evalResults[i].score = objectives[i].score;
        evalResults[i].feasible = false;
        evalResults[i].difficulty = 5;
        evalResults[i].efficiency = 0.0f;
        
        int from = objectives[i].from;
        int to = objectives[i].to;
        int points = objectives[i].score;
        
        // Vérifier la validité des villes
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            continue;
        }
        
        // Trouver le chemin le plus court
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findShortestPath(state, from, to, path, &pathLength);
        
        if (distance <= 0) {
            continue;
        }
        
        // Analyser la faisabilité
        int routesNeeded = 0;
        int routesBlocked = 0;
        int totalWagons = 0;
        bool hasBlockedRoutes = false;
        
        for (int j = 0; j < pathLength - 1; j++) {
            int cityA = path[j];
            int cityB = path[j + 1];
            
            bool routeFound = false;
            for (int k = 0; k < state->nbTracks; k++) {
                if ((state->routes[k].from == cityA && state->routes[k].to == cityB) ||
                    (state->routes[k].from == cityB && state->routes[k].to == cityA)) {
                    
                    routeFound = true;
                    totalWagons += state->routes[k].length;
                    
                    if (state->routes[k].owner == 0) {
                        routesNeeded++;
                    } else if (state->routes[k].owner == 2) {
                        routesBlocked++;
                        hasBlockedRoutes = true;
                    }
                    break;
                }
            }
            
            if (!routeFound) {
                hasBlockedRoutes = true;
            }
        }
        
        // Rejeter si des routes sont bloquées
        if (hasBlockedRoutes) {
            continue;
        }
        
        // Rejeter si trop long
        if (distance > 12) {
            continue;
        }
        
        // Rejeter si pas assez de wagons
        if (totalWagons > state->wagonsLeft + 5) { // Marge de 5 wagons
            continue;
        }
        
        // Objectif faisable !
        evalResults[i].feasible = true;
        
        // Calculer la difficulté (1=facile, 5=difficile)
        evalResults[i].difficulty = 1;
        
        if (routesNeeded > 3) evalResults[i].difficulty++;
        if (distance > 8) evalResults[i].difficulty++;
        if (totalWagons > 15) evalResults[i].difficulty++;
        if (points < 8) evalResults[i].difficulty++; // Objectifs de faible valeur = moins prioritaires
        
        // Calculer l'efficacité (points par wagon)
        if (totalWagons > 0) {
            evalResults[i].efficiency = (float)points / totalWagons;
        }
    }
    
    // Trier par qualité (efficacité d'abord, puis difficulté)
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2 - i; j++) {
            bool swap = false;
            
            // D'abord, préférer les objectifs faisables
            if (evalResults[j].feasible && !evalResults[j+1].feasible) {
                swap = false;
            } else if (!evalResults[j].feasible && evalResults[j+1].feasible) {
                swap = true;
            }
            // Si même faisabilité, comparer l'efficacité
            else if (evalResults[j].feasible == evalResults[j+1].feasible) {
                if (evalResults[j].efficiency < evalResults[j+1].efficiency) {
                    swap = true;
                } else if (evalResults[j].efficiency == evalResults[j+1].efficiency &&
                          evalResults[j].difficulty > evalResults[j+1].difficulty) {
                    swap = true;
                }
            }
            
            if (swap) {
                ObjectiveScore temp = evalResults[j];
                evalResults[j] = evalResults[j+1];
                evalResults[j+1] = temp;
            }
        }
    }
    
    // Stratégie de sélection selon la phase
    if (isFirstTurn) {
        // Premier tour : OBLIGATOIREMENT au moins 2 objectifs
        
        // Prendre les 2 meilleurs s'ils sont faisables
        if (evalResults[0].feasible) {
            chooseObjectives[evalResults[0].index] = true;
        }
        
        if (evalResults[1].feasible) {
            chooseObjectives[evalResults[1].index] = true;
        }
        
        // Si moins de 2 sélectionnés, forcer le choix
        int selected = 0;
        for (int i = 0; i < 3; i++) {
            if (chooseObjectives[i]) selected++;
        }
        
        if (selected < 2) {
            chooseObjectives[evalResults[0].index] = true;
            chooseObjectives[evalResults[1].index] = true;
        }
        
        // Troisième objectif seulement s'il est excellent
        if (evalResults[2].feasible && evalResults[2].efficiency > 0.6f && evalResults[2].difficulty <= 2) {
            chooseObjectives[evalResults[2].index] = true;
        }
    }
    else {
        // Tours suivants : plus sélectif
        int maxObjectifs = 1;
        float seuilEfficacite = 0.4f;
        
        if (phase == PHASE_EARLY) {
            maxObjectifs = 2;
            seuilEfficacite = 0.3f;
        } else if (phase == PHASE_MIDDLE) {
            maxObjectifs = 1;
            seuilEfficacite = 0.5f;
        } else {
            maxObjectifs = 1;
            seuilEfficacite = 0.7f;
        }
        
        int selected = 0;
        for (int i = 0; i < 3 && selected < maxObjectifs; i++) {
            if (evalResults[i].feasible && 
                evalResults[i].efficiency >= seuilEfficacite && 
                evalResults[i].difficulty <= 3) {
                
                chooseObjectives[evalResults[i].index] = true;
                selected++;
            }
        }
        
        // Garantir au moins 1 objectif
        if (selected == 0 && evalResults[0].feasible) {
            chooseObjectives[evalResults[0].index] = true;
        }
    }
    
    // Seul message de résumé
    int finalCount = 0;
    for (int i = 0; i < 3; i++) {
        if (chooseObjectives[i]) {
            finalCount++;
        }
    }
    printf("Selected %d objectives\n", finalCount);
}

// Interface publique pour les stratégies
void chooseObjectivesStrategy(GameState* state, Objective* objectives, bool* chooseObjectives) {
    improvedObjectiveEvaluation(state, objectives, chooseObjectives);
}

// Trouve le meilleur objectif restant à compléter
int findBestRemainingObjective(GameState* state) {
    int bestObjective = -1;
    int bestScore = -1;
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            int objScore = state->objectives[i].score;
            int remainingRoutes = countRemainingRoutesForObjective(state, i);
            
            // Calculer un rapport valeur/effort
            if (remainingRoutes > 0 && remainingRoutes <= state->wagonsLeft) {
                int score = (objScore * 100) / remainingRoutes;
                
                // Bonus si nous sommes proches de compléter
                if (remainingRoutes <= 2) {
                    score += 200;
                }
                
                if (score > bestScore) {
                    bestScore = score;
                    bestObjective = i;
                }
            }
        }
    }
    
    return bestObjective;
}

// Force la complétion d'un objectif critique en fin de partie
bool forceCompleteCriticalObjective(GameState* state, MoveData* moveData) {
    // Trouver l'objectif le plus près de complétion
    int bestObjective = -1;
    int minRemainingRoutes = INT_MAX;
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            int remainingRoutes = countRemainingRoutesForObjective(state, i);
            if (remainingRoutes >= 0 && remainingRoutes < minRemainingRoutes && 
                remainingRoutes <= state->wagonsLeft) {
                minRemainingRoutes = remainingRoutes;
                bestObjective = i;
            }
        }
    }
    
    if (bestObjective < 0 || minRemainingRoutes > state->wagonsLeft) {
        return false;
    }
    
    // Trouver le chemin pour cet objectif
    int objFrom = state->objectives[bestObjective].from;
    int objTo = state->objectives[bestObjective].to;
    
    int path[MAX_CITIES];
    int pathLength = 0;
    findShortestPath(state, objFrom, objTo, path, &pathLength);
    
    // Chercher la première route disponible sur ce chemin
    for (int i = 0; i < pathLength - 1; i++) {
        int cityA = path[i];
        int cityB = path[i+1];
        
        // Trouver l'index de cette route
        for (int j = 0; j < state->nbTracks; j++) {
            if (((state->routes[j].from == cityA && state->routes[j].to == cityB) ||
                 (state->routes[j].from == cityB && state->routes[j].to == cityA)) &&
                state->routes[j].owner == 0) {
                
                // Vérifier si nous pouvons la prendre
                extern CardColor determineOptimalColor(GameState* state, int routeIndex);
                CardColor color = determineOptimalColor(state, j);
                int nbLoco;
                
                extern int canClaimRoute(GameState* state, int from, int to, CardColor color, int* nbLocomotives);
                if (color != NONE && canClaimRoute(state, cityA, cityB, color, &nbLoco)) {
                    // Vérification finale de sécurité pour la couleur
                    int routeIndex = findRouteIndex(state, cityA, cityB);
                    if (routeIndex >= 0) {
                        CardColor routeColor = state->routes[routeIndex].color;
                        CardColor routeSecondColor = state->routes[routeIndex].secondColor;
                        
                        if (routeColor != LOCOMOTIVE) {  // Route non grise
                            if (color != routeColor && color != routeSecondColor && color != LOCOMOTIVE) {
                                return false;
                            }
                        }
                    }
                    
                    // On peut prendre cette route!
                    moveData->action = CLAIM_ROUTE;
                    moveData->claimRoute.from = cityA;
                    moveData->claimRoute.to = cityB;
                    moveData->claimRoute.color = color;
                    moveData->claimRoute.nbLocomotives = nbLoco;
                    
                    printf("Critical objective route: %d->%d\n", cityA, cityB);
                    
                    return true;
                }
            }
        }
    }
    
    return false;
}

// Vérifie si nous avons assez de cartes pour prendre une route
bool haveEnoughCards(GameState* state, int from, int to, CardColor* bestColor, int* nbLocomotives) {
    // Trouver l'index de la route
    int routeIndex = findRouteIndex(state, from, to);
    
    if (routeIndex == -1) {
        *bestColor = NONE;
        *nbLocomotives = 0;
        return false;
    }
    
    int length = state->routes[routeIndex].length;
    CardColor routeColor = state->routes[routeIndex].color;
    CardColor routeSecondColor = state->routes[routeIndex].secondColor;
    
    // Route de couleur spécifique
    if (routeColor != LOCOMOTIVE) {
        // Vérifier si on a assez de cartes de la couleur principale
        int colorCards = state->nbCardsByColor[routeColor];
        
        if (colorCards >= length) {
            *bestColor = routeColor;
            *nbLocomotives = 0;
            return true;
        }
        
        // Sinon, compléter avec des locomotives
        int locos = state->nbCardsByColor[LOCOMOTIVE];
        
        if (colorCards + locos >= length) {
            *bestColor = routeColor;
            *nbLocomotives = length - colorCards;
            return true;
        }
        
        // Si route avec couleur alternative
        if (routeSecondColor != NONE) {
            int secondColorCards = state->nbCardsByColor[routeSecondColor];
            
            if (secondColorCards >= length) {
                *bestColor = routeSecondColor;
                *nbLocomotives = 0;
                return true;
            }
            
            if (secondColorCards + locos >= length) {
                *bestColor = routeSecondColor;
                *nbLocomotives = length - secondColorCards;
                return true;
            }
        }
        
        // Si on a assez de locomotives seules
        if (locos >= length) {
            *bestColor = LOCOMOTIVE;
            *nbLocomotives = length;
            return true;
        }
        
        // Pas assez de cartes
        *bestColor = NONE;
        *nbLocomotives = 0;
        return false;
    }
    
    // Route grise (LOCOMOTIVE)
    else {
        // Chercher la meilleure couleur dont on a assez de cartes
        int bestColorCards = 0;
        CardColor bestAvailableColor = NONE;
        
        for (int c = 1; c < 9; c++) {
            int colorCards = state->nbCardsByColor[c];
            
            if (colorCards > bestColorCards) {
                bestColorCards = colorCards;
                bestAvailableColor = c;
            }
        }
        
        if (bestColorCards >= length) {
            *bestColor = bestAvailableColor;
            *nbLocomotives = 0;
            return true;
        }
        
        // Essayer de compléter avec des locomotives
        int locos = state->nbCardsByColor[LOCOMOTIVE];
        
        if (bestColorCards + locos >= length) {
            *bestColor = bestAvailableColor;
            *nbLocomotives = length - bestColorCards;
            return true;
        }
        
        // Si on a assez de locomotives seules
        if (locos >= length) {
            *bestColor = LOCOMOTIVE;
            *nbLocomotives = length;
            return true;
        }
        
        // Pas assez de cartes
        *bestColor = NONE;
        *nbLocomotives = 0;
        return false;
    }
}

// Compte les routes restantes pour compléter un objectif
int countRemainingRoutesForObjective(GameState* state, int objectiveIndex) {
    if (!state || objectiveIndex < 0 || objectiveIndex >= state->nbObjectives) {
        return -1;
    }
    
    int objFrom = state->objectives[objectiveIndex].from;
    int objTo = state->objectives[objectiveIndex].to;
    
    if (objFrom < 0 || objFrom >= state->nbCities || objTo < 0 || objTo >= state->nbCities) {
        return -1;
    }
    
    if (isObjectiveCompleted(state, state->objectives[objectiveIndex])) {
        return 0;
    }
    
    int path[MAX_CITIES];
    int pathLength = 0;
    int distance = findShortestPath(state, objFrom, objTo, path, &pathLength);
    
    if (distance <= 0 || pathLength <= 0) {
        return -1;
    }
    
    int segmentsOwned = 0;
    int totalSegments = pathLength - 1;
    
    for (int i = 0; i < pathLength - 1; i++) {
        int cityA = path[i];
        int cityB = path[i+1];
        
        if (cityA < 0 || cityA >= state->nbCities || cityB < 0 || cityB >= state->nbCities) {
            continue;
        }
        
        for (int j = 0; j < state->nbTracks; j++) {
            if (((state->routes[j].from == cityA && state->routes[j].to == cityB) ||
                 (state->routes[j].from == cityB && state->routes[j].to == cityA)) &&
                state->routes[j].owner == 1) {
                segmentsOwned++;
                break;
            }
        }
    }
    
    return totalSegments - segmentsOwned;
}