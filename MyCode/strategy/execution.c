/**
 * execution.c
 * Exécution des décisions stratégiques
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "strategy.h"
#include "../rules.h"      
#include "../gamestate.h"  



int emergencyFallback(GameState* state, MoveData* moveData, int* consecutiveDraws) {
    printf(" FALLBACK D'URGENCE ACTIVÉ \n");
    
    // Essayer de prendre n'importe quelle route possible
    int possibleRoutes[MAX_ROUTES];
    CardColor possibleColors[MAX_ROUTES];
    int possibleLocomotives[MAX_ROUTES];
    
    int numPossible = findPossibleRoutes(state, possibleRoutes, possibleColors, possibleLocomotives);
    
    if (numPossible > 0) {
        // Prendre la première route possible
        int routeIndex = possibleRoutes[0];
        
        moveData->action = CLAIM_ROUTE;
        moveData->claimRoute.from = state->routes[routeIndex].from;
        moveData->claimRoute.to = state->routes[routeIndex].to;
        moveData->claimRoute.color = possibleColors[0];
        moveData->claimRoute.nbLocomotives = possibleLocomotives[0];
        
        printf("FALLBACK: Prendre route %d->%d\n", 
               moveData->claimRoute.from, moveData->claimRoute.to);
        
        *consecutiveDraws = 0;
        return 1;
    }
    
    // Si aucune route possible, chercher une locomotive visible
    for (int i = 0; i < 5; i++) {
        if (state->visibleCards[i] == LOCOMOTIVE) {
            moveData->action = DRAW_CARD;
            moveData->drawCard = LOCOMOTIVE;
            
            printf("FALLBACK: Piocher locomotive visible\n");
            (*consecutiveDraws)++;
            return 1;
        }
    }
    
    // Dernier recours : pioche aveugle
    moveData->action = DRAW_BLIND_CARD;
    printf("FALLBACK: Pioche aveugle\n");
    (*consecutiveDraws)++;
    return 1;
}

/**
 * Wrapper sécurisé pour toutes les exécutions
 */
int safeExecute(int (*executeFunction)(GameState*, MoveData*, int*), 
                GameState* state, MoveData* moveData, int* consecutiveDraws,
                const char* functionName) {
    
    printf("Exécution de %s...\n", functionName);
    
    // Essayer la fonction normale
    int result = executeFunction(state, moveData, consecutiveDraws);
    
    if (result == 1) {
        // Valider le mouvement résultant
        if (isValidMove(state, moveData)) {
            printf("✓ %s: Mouvement valide généré\n", functionName);
            return 1;
        } else {
            printf("✗ %s: Mouvement invalide, fallback nécessaire\n", functionName);
        }
    } else {
        printf("✗ %s: Échec de l'exécution (code %d)\n", functionName, result);
    }
    
    // Si échec, utiliser le fallback
    return emergencyFallback(state, moveData, consecutiveDraws);
}



int handleEndgameStrategy(GameState* state, MoveData* moveData) {
    // Déterminer si c'est vraiment la fin de partie
    bool isEndgame = (state->lastTurn || state->wagonsLeft <= 5 || state->opponentWagonsLeft <= 2);
    
    if (!isEndgame) {
        return 0; // Pas de fin de partie
    }
    
    printf("=== STRATÉGIE DE FIN DE PARTIE ACTIVÉE ===\n");
    printf("Wagons restants: Nous=%d, Adversaire=%d, LastTurn=%d\n", 
           state->wagonsLeft, state->opponentWagonsLeft, state->lastTurn);
    
    // PRIORITÉ 1: Compléter un objectif en 1 coup si possible
    for (int i = 0; i < state->nbObjectives; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            int routesRestantes = countRemainingRoutesForObjective(state, i);
            
            if (routesRestantes == 1) {
                printf("OPPORTUNITÉ FIN DE PARTIE: Objectif %d complétable en 1 route!\n", i + 1);
                
                if (forceCompleteCriticalObjective(state, moveData)) {
                    printf("ACTION FIN DE PARTIE: Compléter objectif %d\n", i + 1);
                    return 1;
                }
            }
        }
    }
    
    // PRIORITÉ 2: Prendre la route la plus valuable possible
    int possibleRoutes[MAX_ROUTES];
    CardColor possibleColors[MAX_ROUTES];
    int possibleLocomotives[MAX_ROUTES];
    
    int numPossible = findPossibleRoutes(state, possibleRoutes, possibleColors, possibleLocomotives);
    
    if (numPossible > 0) {
        int bestRouteIdx = -1;
        int bestValue = 0;
        
        for (int i = 0; i < numPossible; i++) {
            int routeIndex = possibleRoutes[i];
            int length = state->routes[routeIndex].length;
            
            // Ne prendre que les routes qu'on peut se permettre
            if (length > state->wagonsLeft) {
                continue;
            }
            
            // Calculer la valeur en points
            int points = 0;
            switch (length) {
                case 1: points = 1; break;
                case 2: points = 2; break;
                case 3: points = 4; break;
                case 4: points = 7; break;
                case 5: points = 10; break;
                case 6: points = 15; break;
            }
            
            // Bonus si ça complète un objectif
            int from = state->routes[routeIndex].from;
            int to = state->routes[routeIndex].to;
            
            // Test rapide : simuler la prise de route
            int originalOwner = state->routes[routeIndex].owner;
            state->routes[routeIndex].owner = 1;
            updateCityConnectivity(state);
            
            for (int j = 0; j < state->nbObjectives; j++) {
                if (!isObjectiveCompleted(state, state->objectives[j])) {
                    // Tester si cette route complète l'objectif
                    if (isObjectiveCompleted(state, state->objectives[j])) {
                        points += state->objectives[j].score;
                        printf("BONUS OBJECTIF: Route %d->%d complète objectif %d (+%d pts)\n",
                               from, to, j + 1, state->objectives[j].score);
                    }
                }
            }
            
            // Restaurer l'état
            state->routes[routeIndex].owner = originalOwner;
            updateCityConnectivity(state);
            
            if (points > bestValue) {
                bestValue = points;
                bestRouteIdx = i;
            }
        }
        
        if (bestRouteIdx >= 0) {
            int routeIndex = possibleRoutes[bestRouteIdx];
            
            moveData->action = CLAIM_ROUTE;
            moveData->claimRoute.from = state->routes[routeIndex].from;
            moveData->claimRoute.to = state->routes[routeIndex].to;
            moveData->claimRoute.color = possibleColors[bestRouteIdx];
            moveData->claimRoute.nbLocomotives = possibleLocomotives[bestRouteIdx];
            
            printf("ACTION FIN DE PARTIE: Prendre route %d->%d pour %d points\n",
                   moveData->claimRoute.from, moveData->claimRoute.to, bestValue);
            
            return 1;
        }
    }
    
    // PRIORITÉ 3: Si rien d'autre, piocher stratégiquement
    printf("FIN DE PARTIE: Aucune route valuable disponible, pioche stratégique\n");
    return 0; // Laisser la stratégie normale gérer
}


// Fonction principale d'exécution des priorités
// REMPLACEZ COMPLÈTEMENT executePriority dans execution.c par cette version SIMPLE

int executePriority(GameState* state, MoveData* moveData, StrategicPriority priority, 
                   CriticalRoute* criticalRoutes, int criticalRouteCount, int* consecutiveDraws) {
    
    if (!state || !moveData || !consecutiveDraws) {
        printf("ERREUR: Paramètres NULL dans executePriority\n");
        moveData->action = DRAW_BLIND_CARD;
        return 1;
    }
    
    printf("=== EXÉCUTION PRIORITÉ: %d ===\n", priority);
    
    // Essayer d'abord la logique de fin de partie
    if (state->lastTurn || state->wagonsLeft <= 3) {
        printf("Fin de partie détectée, recherche de route valuable\n");
        
        int possibleRoutes[MAX_ROUTES];
        CardColor possibleColors[MAX_ROUTES]; 
        int possibleLocomotives[MAX_ROUTES];
        
        int numPossible = findPossibleRoutes(state, possibleRoutes, possibleColors, possibleLocomotives);
        
        if (numPossible > 0) {
            int routeIndex = possibleRoutes[0];
            
            moveData->action = CLAIM_ROUTE;
            moveData->claimRoute.from = state->routes[routeIndex].from;
            moveData->claimRoute.to = state->routes[routeIndex].to;
            moveData->claimRoute.color = possibleColors[0];
            moveData->claimRoute.nbLocomotives = possibleLocomotives[0];
            
            *consecutiveDraws = 0;
            return 1;
        }
    }
    
    // Exécution normale selon priorité
    int result = 0;
    
    switch (priority) {
        case COMPLETE_OBJECTIVES:
            printf("Tentative: COMPLETE_OBJECTIVES\n");
            result = executeCompleteObjectives(state, moveData, criticalRoutes, criticalRouteCount, consecutiveDraws);
            break;
            
        case BLOCK_OPPONENT:
            printf("Tentative: BLOCK_OPPONENT\n");
            result = executeBlockOpponent(state, moveData, consecutiveDraws);
            break;
            
        case BUILD_NETWORK:
            printf("Tentative: BUILD_NETWORK\n");
            result = executeBuildNetwork(state, moveData, consecutiveDraws);
            break;
            
        case DRAW_CARDS:
            printf("Tentative: DRAW_CARDS\n");
            result = executeDrawCards(state, moveData, consecutiveDraws);
            break;
            
        default:
            printf("Priorité inconnue, pioche par défaut\n");
            moveData->action = DRAW_BLIND_CARD;
            (*consecutiveDraws)++;
            return 1;
    }
    
    // Si l'exécution a échoué, fallback simple
    if (result != 1) {
        printf("Échec de l'exécution, fallback vers pioche\n");
        moveData->action = DRAW_BLIND_CARD;
        (*consecutiveDraws)++;
        return 1;
    }
    
    // Validation simple du mouvement
    if (moveData->action == CLAIM_ROUTE) {
        int from = moveData->claimRoute.from;
        int to = moveData->claimRoute.to;
        
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            printf("CORRECTION: Villes invalides, pioche forcée\n");
            moveData->action = DRAW_BLIND_CARD;
            (*consecutiveDraws)++;
        }
    }
    
    return 1;
}

// Exécution de la priorité COMPLETE_OBJECTIVES
int executeCompleteObjectives(GameState* state, MoveData* moveData, 
                             CriticalRoute* criticalRoutes, int criticalRouteCount, int* consecutiveDraws) {
    if (handleEndgameStrategy(state, moveData)) {
        *consecutiveDraws = 0;
        return 1;
    }
    // PRIORITÉ ABSOLUE: Prendre les routes critiques pour compléter des objectifs
    if (criticalRouteCount > 0) {
        for (int i = 0; i < criticalRouteCount; i++) {
            if (criticalRoutes[i].hasEnoughCards) {
                moveData->action = CLAIM_ROUTE;
                moveData->claimRoute.from = criticalRoutes[i].from;
                moveData->claimRoute.to = criticalRoutes[i].to;
                
                // Vérification de validité de la couleur
                if (criticalRoutes[i].color < 1 || criticalRoutes[i].color > 9) {
                    printf("ERREUR: Couleur invalide %d pour route critique, correction à 6 (BLACK)\n", criticalRoutes[i].color);
                    criticalRoutes[i].color = 6;  // BLACK est généralement 6
                }
                moveData->claimRoute.color = criticalRoutes[i].color;
                moveData->claimRoute.nbLocomotives = criticalRoutes[i].nbLocomotives;
                
                printf("DÉCISION CRITIQUE: Prendre route %d -> %d pour compléter objectif %d (priorité: %d)\n", 
                      criticalRoutes[i].from, criticalRoutes[i].to, 
                      criticalRoutes[i].objectiveIndex + 1, criticalRoutes[i].priority);
                
                // Validation finale
                if (!validateRouteMove(state, moveData)) {
                    correctInvalidMove(state, moveData);
                }
                
                *consecutiveDraws = 0;
                return 1;
            }
        }
        
        // Si nous avons des routes critiques mais pas assez de cartes,
        // nous devrions piocher des cartes de la bonne couleur
        for (int i = 0; i < criticalRouteCount; i++) {
            if (!criticalRoutes[i].hasEnoughCards) {
                CardColor neededColor = NONE;
                
                // Trouver la couleur de la route
                int routeIndex = findRouteIndex(state, criticalRoutes[i].from, criticalRoutes[i].to);
                if (routeIndex >= 0) {
                    neededColor = state->routes[routeIndex].color;
                }
                
                // Si c'est une route grise, chercher la couleur dont on a le plus de cartes
                if (neededColor == LOCOMOTIVE) {
                    int maxCards = 0;
                    for (int c = 1; c < 9; c++) {
                        if (state->nbCardsByColor[c] > maxCards) {
                            maxCards = state->nbCardsByColor[c];
                            neededColor = c;
                        }
                    }
                }
                
                // Chercher cette couleur dans les cartes visibles
                for (int j = 0; j < 5; j++) {
                    if (state->visibleCards[j] == neededColor || state->visibleCards[j] == LOCOMOTIVE) {
                        moveData->action = DRAW_CARD;
                        moveData->drawCard = state->visibleCards[j];
                        
                        printf("PIOCHE STRATÉGIQUE: Carte %d pour compléter route critique %d -> %d\n", 
                            state->visibleCards[j], criticalRoutes[i].from, criticalRoutes[i].to);
                        
                        (*consecutiveDraws)++;
                        return 1;
                    }
                }
                
                // Si la couleur n'est pas visible, piocher une carte aveugle
                printf("PIOCHE AVEUGLE STRATÉGIQUE: Pour compléter route critique %d -> %d\n", 
                    criticalRoutes[i].from, criticalRoutes[i].to);
                moveData->action = DRAW_BLIND_CARD;
                (*consecutiveDraws)++;
                return 1;
            }
        }
    }
    
    // CAS SPECIAL: Fin de partie, force complétion d'objectif critique
    int phase = determineGamePhase(state);
    if (phase == PHASE_FINAL || phase == PHASE_LATE || state->lastTurn) {
        if (forceCompleteCriticalObjective(state, moveData)) {
            *consecutiveDraws = 0;
            return 1;
        }
    }
    
    // Focus sur la complétion des objectifs avec routes disponibles
    int possibleRoutes[MAX_ROUTES] = {-1};
    CardColor possibleColors[MAX_ROUTES] = {NONE};
    int possibleLocomotives[MAX_ROUTES] = {0};
    
    int numPossibleRoutes = findPossibleRoutes(state, possibleRoutes, possibleColors, possibleLocomotives);
    
    if (numPossibleRoutes > 0) {
        int bestRouteIndex = 0;
        int bestScore = -1;
        
        for (int i = 0; i < numPossibleRoutes; i++) {
            int routeIndex = possibleRoutes[i];
            if (routeIndex < 0 || routeIndex >= state->nbTracks) continue;
            
            int objectiveScore = calculateObjectiveProgress(state, routeIndex);
            int length = state->routes[routeIndex].length;
            int routeScore = 0;
            
            if (objectiveScore > 0) {
                routeScore += objectiveScore * 5;
            }
            
            if (length >= 5) {
                routeScore += length * 100;
            } 
            else if (length >= 4) {
                routeScore += length * 50;
            }
            else if (length >= 3) {
                routeScore += length * 25;
            }
            
            // Connexion directe pour un objectif?
            int from = state->routes[routeIndex].from;
            int to = state->routes[routeIndex].to;
            
            for (int j = 0; j < state->nbObjectives; j++) {
                if (!isObjectiveCompleted(state, state->objectives[j])) {
                    int objFrom = (int)state->objectives[j].from;
                    int objTo = (int)state->objectives[j].to;
                    
                    if ((objFrom == from && objTo == to) ||
                        (objFrom == to && objTo == from)) {
                        routeScore += 1000;
                        printf("Connexion directe trouvée pour objectif %d! Score +1000\n", j+1);
                    }
                }
            }
            
            if (routeScore > bestScore) {
                bestScore = routeScore;
                bestRouteIndex = i;
            }
        }
        
        // Vérifier seuil minimal et caractéristiques de la route
        int routeIndex = possibleRoutes[bestRouteIndex];
        int length = state->routes[routeIndex].length;
        
        // Éviter routes courtes en début/milieu de partie sauf urgence
        if (length <= 2 && phase < PHASE_LATE && state->turnCount < 15 && 
            *consecutiveDraws < 4 && bestScore < 1000) {
            printf("Route trop courte (longueur %d) en phase %d. Mieux vaut piocher.\n", 
                length, phase);
            return executeDrawCards(state, moveData, consecutiveDraws);
        }
        
        // Seuil minimal
        if (bestScore < 20 && phase == PHASE_EARLY && *consecutiveDraws < 4 && !state->lastTurn) {
            printf("Toutes les routes ont un score faible (%d), continuer à piocher\n", bestScore);
            return executeDrawCards(state, moveData, consecutiveDraws);
        }
        
        // Prendre la meilleure route
        if (bestScore > 0) {
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
            
            // Validation finale
            if (!validateRouteMove(state, moveData)) {
                correctInvalidMove(state, moveData);
            }
            
            *consecutiveDraws = 0;
            return 1;
        }
    }
    
    // Pas de route utile, piocher des cartes
    return executeDrawCards(state, moveData, consecutiveDraws);
}

// Exécution de la priorité BLOCK_OPPONENT
int executeBlockOpponent(GameState* state, MoveData* moveData, int* consecutiveDraws) {
    int possibleRoutes[MAX_ROUTES] = {-1};
    CardColor possibleColors[MAX_ROUTES] = {NONE};
    int possibleLocomotives[MAX_ROUTES] = {0};
    
    int numPossibleRoutes = findPossibleRoutes(state, possibleRoutes, possibleColors, possibleLocomotives);
    
    if (numPossibleRoutes > 0) {
        // Identifier les routes critiques à bloquer
        int routesToBlock[MAX_ROUTES];
        int blockingPriorities[MAX_ROUTES];
        
        int numRoutesToBlock = findCriticalRoutesToBlock(state, routesToBlock, blockingPriorities);
        printf("Trouvé %d routes critiques à bloquer\n", numRoutesToBlock);
        
        // Vérifier si l'une des routes à bloquer est parmi les routes possibles
        int bestBlockingRoute = -1;
        int bestBlockingScore = -1;
        
        for (int i = 0; i < numRoutesToBlock; i++) {
            int blockRouteIndex = routesToBlock[i];
            if (blockRouteIndex < 0 || blockRouteIndex >= state->nbTracks) continue;
            
            for (int j = 0; j < numPossibleRoutes; j++) {
                if (possibleRoutes[j] == blockRouteIndex) {
                    int score = blockingPriorities[i];
                    int length = state->routes[blockRouteIndex].length;
                    
                    if (length >= 5) {
                        score += length * 50;
                    } 
                    else if (length >= 4) {
                        score += length * 25;
                    }
                    else if (length >= 3) {
                        score += length * 10;
                    }
                    
                    if (score > bestBlockingScore) {
                        bestBlockingScore = score;
                        bestBlockingRoute = j;
                    }
                    
                    break;
                }
            }
        }
        
        if (bestBlockingRoute >= 0 && bestBlockingScore > 40) {
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
            
            // Validation finale
            if (!validateRouteMove(state, moveData)) {
                correctInvalidMove(state, moveData);
            }
            
            *consecutiveDraws = 0;
            return 1;
        }
    }
    
    // Pas de route à bloquer, passer à la construction de réseau
    return executeBuildNetwork(state, moveData, consecutiveDraws);
}

// Exécution de la priorité BUILD_NETWORK
int executeBuildNetwork(GameState* state, MoveData* moveData, int* consecutiveDraws) {
    int possibleRoutes[MAX_ROUTES] = {-1};
    CardColor possibleColors[MAX_ROUTES] = {NONE};
    int possibleLocomotives[MAX_ROUTES] = {0};
    
    int numPossibleRoutes = findPossibleRoutes(state, possibleRoutes, possibleColors, possibleLocomotives);
    
    if (numPossibleRoutes > 0) {
        // Privilégier les routes longues pour maximiser les points
        int bestRouteIndex = 0;
        int bestScore = -1;
        int phase = determineGamePhase(state);
        
        // Calculer les scores pour chaque route possible
        for (int i = 0; i < numPossibleRoutes; i++) {
            int routeIndex = possibleRoutes[i];
            if (routeIndex < 0 || routeIndex >= state->nbTracks) continue;
            
            int length = state->routes[routeIndex].length;
            int score = 0;
            
            // Table de points modifiée pour fortement favoriser les routes longues
            switch (length) {
                case 1: score = 1; break;
                case 2: score = 5; break;
                case 3: score = 20; break;
                case 4: score = 50; break;
                case 5: score = 100; break;
                case 6: score = 150; break;
                default: score = 0;
            }
            
            // Ne pas prendre de route courte en début/milieu de partie sauf urgence
            if (length <= 2 && phase < PHASE_LATE && state->turnCount < 15 && *consecutiveDraws < 4) {
                score -= 50;
            }
            
            // Bonus pour connexion à notre réseau
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
                score += 30;
            }
            
            // Connexion directe pour un objectif?
            for (int j = 0; j < state->nbObjectives; j++) {
                if (!isObjectiveCompleted(state, state->objectives[j])) {
                    int objFrom = (int)state->objectives[j].from;
                    int objTo = (int)state->objectives[j].to;
                    
                    if ((objFrom == from && objTo == to) ||
                        (objFrom == to && objTo == from)) {
                        score += 1000;
                    }
                }
            }
            
            if (score > bestScore) {
                bestScore = score;
                bestRouteIndex = i;
            }
        }
        
        // IMPORTANT: Vérifier que l'index est valide
        if (bestRouteIndex < 0 || bestRouteIndex >= numPossibleRoutes) {
            printf("ERREUR CRITIQUE: bestRouteIndex invalide (%d), utilisation du premier (0)\n", 
                   bestRouteIndex);
            bestRouteIndex = 0;
        }
        
        // Seuil minimal
        if (bestScore < 20 && phase == PHASE_EARLY && *consecutiveDraws < 4 && !state->lastTurn) {
            printf("Toutes les routes ont un score faible (%d), continuer à piocher\n", bestScore);
            return executeDrawCards(state, moveData, consecutiveDraws);
        }
        
        // VÉRIFICATION CRITIQUE: s'assurer que les routes existent et sont valides
        if (possibleRoutes[bestRouteIndex] < 0 || possibleRoutes[bestRouteIndex] >= state->nbTracks) {
            printf("ERREUR CRITIQUE: Index de route invalide: %d\n", possibleRoutes[bestRouteIndex]);
            return executeDrawCards(state, moveData, consecutiveDraws);
        }
        
        // Prendre la meilleure route
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
        
        // Validation finale
        if (!validateRouteMove(state, moveData)) {
            correctInvalidMove(state, moveData);
        }
        
        *consecutiveDraws = 0;
        return 1;
    }
    
    // Aucune route possible, piocher des cartes
    return executeDrawCards(state, moveData, consecutiveDraws);
}

// Exécution de la priorité DRAW_CARDS
int executeDrawCards(GameState* state, MoveData* moveData, int* consecutiveDraws) {
    // D'abord, vérifier s'il y a une locomotive visible
    for (int i = 0; i < 5; i++) {
        if (state->visibleCards[i] == LOCOMOTIVE) {
            moveData->action = DRAW_CARD;
            moveData->drawCard = LOCOMOTIVE;
            printf("Décision: Piocher la locomotive visible\n");
            (*consecutiveDraws)++;
            return 1;
        }
    }
    
    // Utiliser la pioche stratégique
    int bestCardIndex = strategicCardDrawing(state);
    
    if (bestCardIndex >= 0 && bestCardIndex < 5) {
        moveData->action = DRAW_CARD;
        moveData->drawCard = state->visibleCards[bestCardIndex];
        printf("Décision: Piocher la carte visible stratégique\n");
        (*consecutiveDraws)++;
        return 1;
    }
    
    // Sinon, piocher une carte aveugle
    moveData->action = DRAW_BLIND_CARD;
    printf("Décision: Piocher une carte aveugle\n");
    (*consecutiveDraws)++;
    return 1;
}

// Valide un mouvement de route avant exécution
bool validateRouteMove(GameState* state, MoveData* moveData) {
    if (moveData->action != CLAIM_ROUTE) {
        return true; // Pas une route, pas de validation nécessaire
    }
    
    int from = moveData->claimRoute.from;
    int to = moveData->claimRoute.to;
    CardColor color = moveData->claimRoute.color;
    
    // Vérification des villes
    if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
        printf("VALIDATION: Villes invalides (%d -> %d)\n", from, to);
        return false;
    }
    
    // Vérification de la couleur
    if (color < PURPLE || color > LOCOMOTIVE) {
        printf("VALIDATION: Couleur invalide (%d)\n", color);
        return false;
    }
    
    // Vérifier que la route existe
    int routeIndex = findRouteIndex(state, from, to);
    if (routeIndex < 0) {
        printf("VALIDATION: Route inexistante (%d -> %d)\n", from, to);
        return false;
    }
    
    // Vérifier que la route n'est pas déjà prise
    if (state->routes[routeIndex].owner != 0) {
        printf("VALIDATION: Route déjà prise par %d\n", state->routes[routeIndex].owner);
        return false;
    }
    
    // Vérifier la validité de la couleur pour cette route
    CardColor routeColor = state->routes[routeIndex].color;
    CardColor routeSecondColor = state->routes[routeIndex].secondColor;
    
    if (routeColor != LOCOMOTIVE) {  // Route non grise
        if (color != routeColor && color != routeSecondColor && color != LOCOMOTIVE) {
            printf("VALIDATION: Couleur incorrecte pour cette route\n");
            return false;
        }
    }
    
    // Vérifier que nous avons assez de cartes
    int nbLoco;
    if (!canClaimRoute(state, from, to, color, &nbLoco)) {
        printf("VALIDATION: Pas assez de cartes\n");
        return false;
    }
    
    return true;
}

// Corrige un mouvement invalide
void correctInvalidMove(GameState* state, MoveData* moveData) {
    (void)state; // Éviter le warning de paramètre non utilisé
    printf("CORRECTION: Mouvement invalide détecté, correction en pioche aveugle\n");
    moveData->action = DRAW_BLIND_CARD;
}

// Vérifie si une action de route est valide
bool isValidRouteAction(GameState* state, int from, int to, CardColor color) {
    // Vérifications de base
    if (!state || from < 0 || from >= state->nbCities || 
        to < 0 || to >= state->nbCities || color < 1 || color > 9) {
        return false;
    }
    
    // Vérifier que la route existe et est libre
    int routeIndex = findRouteIndex(state, from, to);
    if (routeIndex < 0 || state->routes[routeIndex].owner != 0) {
        return false;
    }
    
    // Vérifier la couleur
    CardColor routeColor = state->routes[routeIndex].color;
    CardColor routeSecondColor = state->routes[routeIndex].secondColor;
    
    if (routeColor != LOCOMOTIVE) {
        if (color != routeColor && color != routeSecondColor && color != LOCOMOTIVE) {
            return false;
        }
    }
    
    // Vérifier les cartes
    int nbLoco;
    return canClaimRoute(state, from, to, color, &nbLoco);
}

// Log une décision pour debug
void logDecision(const char* decision, int from, int to, int score) {
    if (from >= 0 && to >= 0) {
        printf("DÉCISION: %s (route %d->%d, score %d)\n", decision, from, to, score);
    } else {
        printf("DÉCISION: %s (score %d)\n", decision, score);
    }
}
int executeCompleteObjectivesWrapper(GameState* state, MoveData* moveData, int* consecutiveDraws) {
    // Créer des variables dummy pour les paramètres manquants
    CriticalRoute dummyCriticalRoutes[10];
    int dummyCriticalRouteCount = 0;
    
    return executeCompleteObjectives(state, moveData, dummyCriticalRoutes, dummyCriticalRouteCount, consecutiveDraws);
}