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
        
        *consecutiveDraws = 0;
        return 1;
    }
    
    // Si aucune route possible, chercher une locomotive visible
    for (int i = 0; i < 5; i++) {
        if (state->visibleCards[i] == LOCOMOTIVE) {
            moveData->action = DRAW_CARD;
            moveData->drawCard = LOCOMOTIVE;
            (*consecutiveDraws)++;
            return 1;
        }
    }
    
    // Dernier recours : pioche aveugle
    moveData->action = DRAW_BLIND_CARD;
    (*consecutiveDraws)++;
    return 1;
}

// Gestion de la stratégie de fin de partie
int handleEndgameStrategy(GameState* state, MoveData* moveData) {
    bool isEndgame = (state->lastTurn || state->wagonsLeft <= 5 || state->opponentWagonsLeft <= 2);
    
    if (!isEndgame) {
        return 0;
    }
    
    // Compléter un objectif en 1 coup si possible
    for (int i = 0; i < state->nbObjectives; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            int routesRestantes = countRemainingRoutesForObjective(state, i);
            
            if (routesRestantes == 1) {
                if (forceCompleteCriticalObjective(state, moveData)) {
                    return 1;
                }
            }
        }
    }
    
    // Prendre la route la plus valuable possible
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
                    if (isObjectiveCompleted(state, state->objectives[j])) {
                        points += state->objectives[j].score;
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
            
            printf("Endgame route: %d->%d (%d points)\n",
                   moveData->claimRoute.from, moveData->claimRoute.to, bestValue);
            
            return 1;
        }
    }
    
    return 0;
}

// Fonction principale d'exécution des priorités
int executePriority(GameState* state, MoveData* moveData, StrategicPriority priority, 
                   CriticalRoute* criticalRoutes, int criticalRouteCount, int* consecutiveDraws) {
    
    if (!state || !moveData || !consecutiveDraws) {
        printf("ERROR: NULL parameters in executePriority\n");
        moveData->action = DRAW_BLIND_CARD;
        return 1;
    }
    
    // Essayer d'abord la logique de fin de partie
    if (state->lastTurn || state->wagonsLeft <= 3) {
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
            result = executeCompleteObjectives(state, moveData, criticalRoutes, criticalRouteCount, consecutiveDraws);
            break;
            
        case BLOCK_OPPONENT:
            result = executeBlockOpponent(state, moveData, consecutiveDraws);
            break;
            
        case BUILD_NETWORK:
            result = executeBuildNetwork(state, moveData, consecutiveDraws);
            break;
            
        case DRAW_CARDS:
            result = executeDrawCards(state, moveData, consecutiveDraws);
            break;
            
        default:
            moveData->action = DRAW_BLIND_CARD;
            (*consecutiveDraws)++;
            return 1;
    }
    
    // Si l'exécution a échoué, fallback simple
    if (result != 1) {
        moveData->action = DRAW_BLIND_CARD;
        (*consecutiveDraws)++;
        return 1;
    }
    
    // Validation simple du mouvement
    if (moveData->action == CLAIM_ROUTE) {
        int from = moveData->claimRoute.from;
        int to = moveData->claimRoute.to;
        
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
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
    
    // Prendre les routes critiques pour compléter des objectifs
    if (criticalRouteCount > 0) {
        for (int i = 0; i < criticalRouteCount; i++) {
            if (criticalRoutes[i].hasEnoughCards) {
                moveData->action = CLAIM_ROUTE;
                moveData->claimRoute.from = criticalRoutes[i].from;
                moveData->claimRoute.to = criticalRoutes[i].to;
                
                // Vérification couleur
                if (criticalRoutes[i].color < 1 || criticalRoutes[i].color > 9) {
                    criticalRoutes[i].color = 6;  // BLACK par défaut
                }
                moveData->claimRoute.color = criticalRoutes[i].color;
                moveData->claimRoute.nbLocomotives = criticalRoutes[i].nbLocomotives;
                
                // Validation finale
                if (!validateRouteMove(state, moveData)) {
                    correctInvalidMove(state, moveData);
                }
                
                *consecutiveDraws = 0;
                return 1;
            }
        }
        
        // Si nous avons des routes critiques mais pas assez de cartes, piocher
        for (int i = 0; i < criticalRouteCount; i++) {
            if (!criticalRoutes[i].hasEnoughCards) {
                CardColor neededColor = NONE;
                
                // Trouver la couleur de la route
                int routeIndex = findRouteIndex(state, criticalRoutes[i].from, criticalRoutes[i].to);
                if (routeIndex >= 0) {
                    neededColor = state->routes[routeIndex].color;
                }
                
                // Si c'est une route grise, chercher notre meilleure couleur
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
                        (*consecutiveDraws)++;
                        return 1;
                    }
                }
                
                // Si pas visible, piocher aveugle
                moveData->action = DRAW_BLIND_CARD;
                (*consecutiveDraws)++;
                return 1;
            }
        }
    }
    
    // Fin de partie - force complétion d'objectif critique
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
            return executeDrawCards(state, moveData, consecutiveDraws);
        }
        
        // Seuil minimal
        if (bestScore < 20 && phase == PHASE_EARLY && *consecutiveDraws < 4 && !state->lastTurn) {
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
            
            printf("Blocking route %d->%d\n", from, to);
            
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
            
            // Table de points modifiée pour favoriser les routes longues
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
        
        // Vérifier que l'index est valide
        if (bestRouteIndex < 0 || bestRouteIndex >= numPossibleRoutes) {
            bestRouteIndex = 0;
        }
        
        // Seuil minimal
        if (bestScore < 20 && phase == PHASE_EARLY && *consecutiveDraws < 4 && !state->lastTurn) {
            return executeDrawCards(state, moveData, consecutiveDraws);
        }
        
        // Vérifier que les routes existent et sont valides
        if (possibleRoutes[bestRouteIndex] < 0 || possibleRoutes[bestRouteIndex] >= state->nbTracks) {
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
            (*consecutiveDraws)++;
            return 1;
        }
    }
    
    // Utiliser la pioche stratégique
    int bestCardIndex = strategicCardDrawing(state);
    
    if (bestCardIndex >= 0 && bestCardIndex < 5) {
        moveData->action = DRAW_CARD;
        moveData->drawCard = state->visibleCards[bestCardIndex];
        (*consecutiveDraws)++;
        return 1;
    }
    
    // Sinon, piocher une carte aveugle
    moveData->action = DRAW_BLIND_CARD;
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
        return false;
    }
    
    // Vérification de la couleur
    if (color < PURPLE || color > LOCOMOTIVE) {
        return false;
    }
    
    // Vérifier que la route existe
    int routeIndex = findRouteIndex(state, from, to);
    if (routeIndex < 0) {
        return false;
    }
    
    // Vérifier que la route n'est pas déjà prise
    if (state->routes[routeIndex].owner != 0) {
        return false;
    }
    
    // Vérifier la validité de la couleur pour cette route
    CardColor routeColor = state->routes[routeIndex].color;
    CardColor routeSecondColor = state->routes[routeIndex].secondColor;
    
    if (routeColor != LOCOMOTIVE) {  // Route non grise
        if (color != routeColor && color != routeSecondColor && color != LOCOMOTIVE) {
            return false;
        }
    }
    
    // Vérifier que nous avons assez de cartes
    int nbLoco;
    if (!canClaimRoute(state, from, to, color, &nbLoco)) {
        return false;
    }
    
    return true;
}

// Corrige un mouvement invalide
void correctInvalidMove(GameState* state, MoveData* moveData) {
    (void)state; // Éviter le warning de paramètre non utilisé
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
        printf("Decision: %s (route %d->%d, score %d)\n", decision, from, to, score);
    } else {
        printf("Decision: %s (score %d)\n", decision, score);
    }
}

int executeCompleteObjectivesWrapper(GameState* state, MoveData* moveData, int* consecutiveDraws) {
    // Créer des variables dummy pour les paramètres manquants
    CriticalRoute dummyCriticalRoutes[10];
    int dummyCriticalRouteCount = 0;
    
    return executeCompleteObjectives(state, moveData, dummyCriticalRoutes, dummyCriticalRouteCount, consecutiveDraws);
}