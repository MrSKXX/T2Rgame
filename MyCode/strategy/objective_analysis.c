/**
 * objective_analysis.c
 * Analyse et gestion des objectifs - VERSION CORRIGÉE
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
        printf("ERROR: Invalid parameters in calculateObjectiveProgress\n");
        return 0;
    }

    int from = state->routes[routeIndex].from;
    int to = state->routes[routeIndex].to;
    int progress = 0;
    
    if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
        return 0;
    }
    
    // Utilisation d'une valeur de 200 directement au lieu de la constante OBJECTIVE_MULTIPLIER
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
        
        // Connexion directe - priorité importante
        if ((from == objFrom && to == objTo) || (from == objTo && to == objFrom)) {
            progress += objScore * objectiveMultiplier * 5;
            continue;
        }
        
        // Combien de routes manquent pour cet objectif
        int remainingRoutes = countRemainingRoutesForObjective(state, i);
        
        // Objectif presque complété - priorité élevée
        if (remainingRoutes == 1) {
            int finalRouteBonus = objScore * objectiveMultiplier * 3;
            
            // Vérifier si cette route est la dernière nécessaire
            int path[MAX_CITIES];
            int pathLength = 0;
            // CHANGEMENT: Utiliser findShortestPath pour l'évaluation
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
        // CHANGEMENT: Utiliser findShortestPath pour l'évaluation
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
            
            // CHANGEMENT: Utiliser findShortestPath pour identifier les routes critiques
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
                                    printf("ERREUR CRITIQUE: Couleur %d incorrecte pour route %d->%d (couleurs valides: %d", 
                                        bestColor, cityA, cityB, routeColor);
                                    
                                    if (routeSecondColor != NONE) {
                                        printf(", %d", routeSecondColor);
                                    }
                                    printf(")\n");
                                    
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
                            
                            printf("Route CRITIQUE identifiée: %d -> %d pour objectif %d, priorité %d, cartes %s\n", 
                                  cityA, cityB, i+1, priority, hasCards ? "DISPONIBLES" : "INSUFFISANTES");
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
        printf("ERROR: Null state in checkObjectivesPaths\n");
        return;
    }
    
    printf("\n=== OBJECTIVE PATHS ANALYSIS ===\n");
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (i < 0 || i >= MAX_OBJECTIVES) {
            continue;
        }
        
        int from = state->objectives[i].from;
        int to = state->objectives[i].to;
        int score = state->objectives[i].score;
        
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            printf("ERROR: Invalid cities in objective %d: from %d to %d\n", i, from, to);
            continue;
        }
        
        bool completed = isObjectiveCompleted(state, state->objectives[i]);
        
        printf("Objective %d: From %d to %d, score %d %s\n", 
             i+1, from, to, score, completed ? "[COMPLETED]" : "");
        
        if (completed) {
            continue;
        }
        
        int path[MAX_CITIES];
        int pathLength = 0;
        // CHANGEMENT: Utiliser findShortestPath pour l'analyse des objectifs
        int distance = findShortestPath(state, from, to, path, &pathLength);
        printf("  Utilisation du chemin le plus court et direct\n");   

        if (distance > 0 && pathLength > 0) {
            printf("  Path exists, length %d: ", pathLength);
            for (int j = 0; j < pathLength && j < MAX_CITIES; j++) {
                printf("%d ", path[j]);
            }
            printf("\n");
            
            printf("  Routes needed:\n");
            int availableRoutes = 0;
            int claimedRoutes = 0;
            int blockedRoutes = 0;
            
            for (int j = 0; j < pathLength - 1 && j < MAX_CITIES - 1; j++) {
                int cityA = path[j];
                int cityB = path[j+1];
                
                if (cityA < 0 || cityA >= state->nbCities || cityB < 0 || cityB >= state->nbCities) {
                    printf("    WARNING: Invalid cities in path: %d to %d\n", cityA, cityB);
                    continue;
                }
                
                bool routeFound = false;
                
                for (int k = 0; k < state->nbTracks; k++) {
                    if ((state->routes[k].from == cityA && state->routes[k].to == cityB) ||
                        (state->routes[k].from == cityB && state->routes[k].to == cityA)) {
                        
                        routeFound = true;
                        printf("    %d to %d: ", cityA, cityB);
                        
                        if (state->routes[k].owner == 0) {
                            printf("Available (length %d, color ", state->routes[k].length);
                            printf("Card color: %s\n", 
                                 (state->routes[k].color < 10) ? (const char*[]){
                                    "None", "Purple", "White", "Blue", "Yellow", 
                                    "Orange", "Black", "Red", "Green", "Locomotive"
                                 }[state->routes[k].color] : "Unknown");
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

// Fonction de choix des objectifs optimisée
// REMPLACEZ improvedObjectiveEvaluation dans objective_analysis.c par cette version

void improvedObjectiveEvaluation(GameState* state, Objective* objectives, bool* chooseObjectives) {
    printf("\n=== ÉVALUATION SIMPLIFIÉE DES OBJECTIFS ===\n");
    
    if (!state || !objectives || !chooseObjectives) {
        printf("ERREUR: Paramètres NULL dans improvedObjectiveEvaluation\n");
        return;
    }
    
    // Initialiser tous les choix à false
    for (int i = 0; i < 3; i++) {
        chooseObjectives[i] = false;
    }
    
    // Déterminer si c'est le premier tour
    bool isFirstTurn = (state->nbObjectives == 0);
    int phase = determineGamePhase(state);
    
    printf("Phase de jeu: %d, Premier tour: %s\n", phase, isFirstTurn ? "OUI" : "NON");
    
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
        
        printf("\nObjectif %d: De %d à %d pour %d points\n", i+1, from, to, points);
        
        // Vérifier la validité des villes
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            printf("  REJETÉ: Villes invalides\n");
            continue;
        }
        
        // Trouver le chemin le plus court
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findShortestPath(state, from, to, path, &pathLength);
        
        if (distance <= 0) {
            printf("  REJETÉ: Aucun chemin possible\n");
            continue;
        }
        
        printf("  Chemin trouvé: %d segments, distance totale %d\n", pathLength - 1, distance);
        
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
                    // Si owner == 1, c'est déjà notre route
                    break;
                }
            }
            
            if (!routeFound) {
                printf("  ERREUR: Route manquante entre %d et %d\n", cityA, cityB);
                hasBlockedRoutes = true;
            }
        }
        
        // Rejeter si des routes sont bloquées
        if (hasBlockedRoutes) {
            printf("  REJETÉ: %d routes bloquées par l'adversaire\n", routesBlocked);
            continue;
        }
        
        // Rejeter si trop long
        if (distance > 12) {
            printf("  REJETÉ: Chemin trop long (%d > 12)\n", distance);
            continue;
        }
        
        // Rejeter si pas assez de wagons
        if (totalWagons > state->wagonsLeft + 5) { // Marge de 5 wagons
            printf("  REJETÉ: Trop de wagons nécessaires (%d, nous avons %d)\n", 
                   totalWagons, state->wagonsLeft);
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
        
        printf("  FAISABLE: Difficulté %d/5, Efficacité %.2f pts/wagon\n", 
               evalResults[i].difficulty, evalResults[i].efficiency);
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
    printf("\nSTRATÉGIE DE SÉLECTION:\n");
    
    if (isFirstTurn) {
        // Premier tour : OBLIGATOIREMENT au moins 2 objectifs
        printf("PREMIER TOUR: Minimum 2 objectifs requis\n");
        
        // Prendre les 2 meilleurs s'ils sont faisables
        if (evalResults[0].feasible) {
            chooseObjectives[evalResults[0].index] = true;
            printf("  Sélectionné: Objectif %d (meilleur)\n", evalResults[0].index + 1);
        }
        
        if (evalResults[1].feasible) {
            chooseObjectives[evalResults[1].index] = true;
            printf("  Sélectionné: Objectif %d (deuxième)\n", evalResults[1].index + 1);
        }
        
        // Si moins de 2 sélectionnés, forcer le choix
        int selected = 0;
        for (int i = 0; i < 3; i++) {
            if (chooseObjectives[i]) selected++;
        }
        
        if (selected < 2) {
            printf("  CORRECTION: Moins de 2 objectifs, sélection forcée\n");
            chooseObjectives[evalResults[0].index] = true;
            chooseObjectives[evalResults[1].index] = true;
        }
        
        // Troisième objectif seulement s'il est excellent
        if (evalResults[2].feasible && evalResults[2].efficiency > 0.6f && evalResults[2].difficulty <= 2) {
            chooseObjectives[evalResults[2].index] = true;
            printf("  Bonus: Objectif %d (excellent)\n", evalResults[2].index + 1);
        }
    }
    else {
        // Tours suivants : plus sélectif
        printf("TOUR NORMAL: Sélection selon phase %d\n", phase);
        
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
                printf("  Sélectionné: Objectif %d (efficacité %.2f)\n", 
                       evalResults[i].index + 1, evalResults[i].efficiency);
            }
        }
        
        // Garantir au moins 1 objectif
        if (selected == 0 && evalResults[0].feasible) {
            chooseObjectives[evalResults[0].index] = true;
            printf("  Sélection forcée: Objectif %d (meilleur disponible)\n", 
                   evalResults[0].index + 1);
        }
    }
    
    // Résumé final
    int finalCount = 0;
    printf("\nRÉSULTAT FINAL:\n");
    for (int i = 0; i < 3; i++) {
        if (chooseObjectives[i]) {
            finalCount++;
            printf("  ✓ Objectif %d: %d->%d (%d points)\n", 
                   i+1, objectives[i].from, objectives[i].to, objectives[i].score);
        }
    }
    printf("Total: %d objectifs sélectionnés\n", finalCount);
    
    printf("============================================\n\n");
}

// Interface publique pour les stratégies
void chooseObjectivesStrategy(GameState* state, Objective* objectives, bool* chooseObjectives) {
    printf("Using advanced objective selection strategy\n");
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
    
    printf("PRIORITÉ ABSOLUE: Compléter objectif %d (reste %d routes, %d wagons)\n", 
           bestObjective+1, minRemainingRoutes, state->wagonsLeft);
    
    // Trouver le chemin pour cet objectif
    int objFrom = state->objectives[bestObjective].from;
    int objTo = state->objectives[bestObjective].to;
    
    int path[MAX_CITIES];
    int pathLength = 0;
    // CHANGEMENT: Utiliser findShortestPath pour forcer la complétion
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
                    // On peut prendre cette route!
                    moveData->action = CLAIM_ROUTE;
                    moveData->claimRoute.from = cityA;
                    moveData->claimRoute.to = cityB;
                    moveData->claimRoute.color = color;
                    moveData->claimRoute.nbLocomotives = nbLoco;
                    
                    printf("OBJECTIF CRITIQUE: Prendre route %d->%d pour terminer objectif %d\n", 
                           cityA, cityB, bestObjective+1);
                    
                    // Vérification finale de sécurité pour la couleur
                    int routeIndex = findRouteIndex(state, cityA, cityB);
                    if (routeIndex >= 0) {
                        CardColor routeColor = state->routes[routeIndex].color;
                        CardColor routeSecondColor = state->routes[routeIndex].secondColor;
                        
                        if (routeColor != LOCOMOTIVE) {  // Route non grise
                            if (color != routeColor && color != routeSecondColor && color != LOCOMOTIVE) {
                                printf("ERREUR FATALE: Tentative de prendre route %d->%d avec couleur incorrecte %d (valides: %d", 
                                    cityA, cityB, color, routeColor);
                                
                                if (routeSecondColor != NONE) {
                                    printf(", %d", routeSecondColor);
                                }
                                printf(")\n");
                                
                                // Annuler cette action et retourner false
                                return false;
                            }
                        }
                    }
                    
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
        printf("ERREUR: Route %d -> %d non trouvée dans haveEnoughCards\n", from, to);
        *bestColor = NONE;
        *nbLocomotives = 0;
        return false;
    }
    
    int length = state->routes[routeIndex].length;
    CardColor routeColor = state->routes[routeIndex].color;
    CardColor routeSecondColor = state->routes[routeIndex].secondColor;
    
    printf("VÉRIFICATION ROUTE %d->%d: Longueur %d, Couleur %d, SecondColor %d\n", 
           from, to, length, routeColor, routeSecondColor);
    
    // Route de couleur spécifique
    if (routeColor != LOCOMOTIVE) {
        // Vérifier si on a assez de cartes de la couleur principale
        int colorCards = state->nbCardsByColor[routeColor];
        printf("  Cartes %d disponibles: %d\n", routeColor, colorCards);
        
        if (colorCards >= length) {
            *bestColor = routeColor;
            *nbLocomotives = 0;
            return true;
        }
        
        // Sinon, compléter avec des locomotives
        int locos = state->nbCardsByColor[LOCOMOTIVE];
        printf("  Locomotives disponibles: %d\n", locos);
        
        if (colorCards + locos >= length) {
            *bestColor = routeColor;
            *nbLocomotives = length - colorCards;
            return true;
        }
        
        // Si route avec couleur alternative
        if (routeSecondColor != NONE) {
            int secondColorCards = state->nbCardsByColor[routeSecondColor];
            printf("  Cartes couleur alternative %d disponibles: %d\n", routeSecondColor, secondColorCards);
            
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
            printf("  Cartes %d disponibles: %d\n", c, colorCards);
            
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
        printf("  Locomotives disponibles: %d\n", locos);
        
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
    // CHANGEMENT: Utiliser findShortestPath pour compter les routes restantes
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

