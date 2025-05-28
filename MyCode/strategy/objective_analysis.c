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
void improvedObjectiveEvaluation(GameState* state, Objective* objectives, bool* chooseObjectives) {
    printf("Évaluation avancée des objectifs\n");
    
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
        
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            printf("Objectif %d: Villes invalides - De %d à %d, score %d\n", i+1, from, to, value);
            scores[i] = -1000;
            continue;
        }
        
        printf("Objectif %d: De %d à %d, score %d\n", i+1, from, to, value);
        
        // CHANGEMENT: Utiliser findShortestPath pour l'évaluation
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findShortestPath(state, from, to, path, &pathLength);
        
        if (distance < 0) {
            scores[i] = -1000;
            printf("  - Aucun chemin disponible, objectif impossible\n");
            continue;
        }
        
        // Pénalité massive pour les chemins trop longs
        if (distance > 10) {
            printf("  - ATTENTION: Chemin très long (%d) pour cet objectif\n", distance);
            scores[i] = -1000;
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
                scores[i] = -1000;
                printf("  - Erreur: Le chemin contient une route inexistante\n");
            }
        }
        
        routesNeeded = routesAvailable + routesOwnedByUs;
        
        // Si des routes sont bloquées par l'adversaire, pénalité massive
        if (routesBlockedByOpponent > 0) {
            int penalty = routesBlockedByOpponent * 50;
            scores[i] = -penalty;
            printf("  - %d routes bloquées par l'adversaire, pénalité: -%d\n", 
                   routesBlockedByOpponent, penalty);
            continue;
        }
        
        // Score de base: rapport points/longueur
        float pointsPerLength = (totalLength > 0) ? (float)value / totalLength : 0;
        float baseScore = pointsPerLength * 150.0;
        
        // Progrès de complétion
        float completionProgress = 0;
        if (routesNeeded > 0) {
            completionProgress = (float)routesOwnedByUs / routesNeeded;
        }
        
        float completionBonus = completionProgress * 250.0;
        
        // Correspondance des cartes
        float cardMatchScore = 0;
        int colorMatchCount = 0;
        
        for (int j = 0; j < pathLength - 1 && j < MAX_CITIES - 1; j++) {
            int pathFrom = path[j];
            int pathTo = path[j+1];
            
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
                synergyScore += 60;
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
                
                synergyScore += sharedRoutes * 30;
            }
        }
        
        // Pénalité pour la compétition avec l'adversaire
        float competitionPenalty = 0;
        extern int opponentCitiesOfInterest[MAX_CITIES];
        
        for (int j = 0; j < pathLength - 1 && j < MAX_CITIES - 1; j++) {
            int pathFrom = path[j];
            int pathTo = path[j+1];
            
            if (pathFrom < MAX_CITIES && pathTo < MAX_CITIES && 
                (opponentCitiesOfInterest[pathFrom] > 0 || opponentCitiesOfInterest[pathTo] > 0)) {
                
                int fromPenalty = (pathFrom < MAX_CITIES) ? opponentCitiesOfInterest[pathFrom] : 0;
                int toPenalty = (pathTo < MAX_CITIES) ? opponentCitiesOfInterest[pathTo] : 0;
                competitionPenalty += (fromPenalty + toPenalty) * 5;
            }
        }
        
        // Pénalité pour la difficulté
        float difficultyPenalty = 0;
        if (routesNeeded > 4) {
            difficultyPenalty = (routesNeeded - 3) * 40;
        }
        
        // Pénalité pour les chemins longs
        float lengthPenalty = 0;
        if (distance > 6) {
            lengthPenalty = (distance - 6) * 30;
        }
        
        // Bonus pour les objectifs à haute valeur
        float valueBonus = 0;
        if (value > 10) {
            valueBonus = (value - 10) * 10;
        }
        
        // Calcul du score final
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
    
    // STRATÉGIE DE SÉLECTION
    int numToChoose = 0;
    
    // Règle spéciale pour le premier tour
    if (isFirstTurn) {
        // Exigence du serveur: au moins 2 objectifs au premier tour
        chooseObjectives[sortedIndices[0]] = true;
        chooseObjectives[sortedIndices[1]] = true;
        numToChoose = 2;
        
        // Prendre le troisième objectif seulement s'il a un bon score
        if (scores[sortedIndices[2]] > 50) {
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
        
        // La sélection des objectifs supplémentaires dépend de la phase
        if (phase == PHASE_EARLY) {
            // Plus agressif sur la prise d'objectifs en début de partie
            if (currentObjectiveCount < 2) {
                if (scores[sortedIndices[1]] > 100) {
                    chooseObjectives[sortedIndices[1]] = true;
                    numToChoose++;
                }
                
                if (scores[sortedIndices[2]] > 150) {
                    chooseObjectives[sortedIndices[2]] = true;
                    numToChoose++;
                }
            } else if (currentObjectiveCount < 3) {
                if (scores[sortedIndices[1]] > 120) {
                    chooseObjectives[sortedIndices[1]] = true;
                    numToChoose++;
                }
            }
            else if (scores[sortedIndices[1]] > 200) {
                chooseObjectives[sortedIndices[1]] = true;
                numToChoose++;
            }
        }
        else if (phase == PHASE_MIDDLE) {
            // Plus sélectif en milieu de partie
            if (currentObjectiveCount < 3) {
                if (scores[sortedIndices[1]] > 150) {
                    chooseObjectives[sortedIndices[1]] = true;
                    numToChoose++;
                }
            }
            else if (scores[sortedIndices[1]] > 250) {
                chooseObjectives[sortedIndices[1]] = true;
                numToChoose++;
            }
        }
        else {
            // Très sélectif en fin de partie
            if (scores[sortedIndices[1]] > 300) {
                chooseObjectives[sortedIndices[1]] = true;
                numToChoose++;
            }
        }
        
        // LIMITATION DU NOMBRE TOTAL D'OBJECTIFS
        int maxTotalObjectives = 3 + (phase == PHASE_EARLY ? 1 : 0);
        if (currentObjectiveCount + numToChoose > maxTotalObjectives) {
            // Réduire pour respecter la limite
            int maxNewObjectives = maxTotalObjectives - currentObjectiveCount;
            if (maxNewObjectives <= 0) {
                // Ne garder que le meilleur
                for (int i = 1; i < 3; i++) {
                    chooseObjectives[sortedIndices[i]] = false;
                }
                numToChoose = 1;
            } else {
                // Garder les N meilleurs
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
        // Forcer la sélection du premier objectif
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

