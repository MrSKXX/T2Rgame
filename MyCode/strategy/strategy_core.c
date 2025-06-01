/**
 * strategy_core.c
 * Interface principale du système de stratégie 
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include "strategy.h"
#include "../rules.h"      
#include "../gamestate.h"  

/**
 * Wrapper sécurisé pour la stratégie - valide tous les mouvements
 */
int safeAdvancedStrategy(GameState* state, MoveData* moveData) {
    if (!state || !moveData) {
        printf("ERROR: NULL parameters in strategy\n");
        return 0;
    }
    
    // Exécuter la stratégie normale
    int result = superAdvancedStrategy(state, moveData);
    
    if (result != 1) {
        // Action de secours
        moveData->action = DRAW_BLIND_CARD;
        return 1;
    }
    
    // Valider le mouvement choisi
    if (!isValidMove(state, moveData)) {
        moveData->action = DRAW_BLIND_CARD;
    }
    
    switch (moveData->action) {
        case CLAIM_ROUTE:
            printf("Taking route %d->%d\n", moveData->claimRoute.from, moveData->claimRoute.to);
            break;
        case DRAW_CARD:
            break;
        case DRAW_BLIND_CARD:
            break;
        default:
            break;
    }
    
    return 1;
}

int decideNextMove(GameState* state, StrategyType strategy, MoveData* moveData) {
    (void)strategy; 
    return safeAdvancedStrategy(state, moveData);
}

// Stratégie principale optimisée 
int superAdvancedStrategy(GameState* state, MoveData* moveData) {

    
    // Variables de suivi stratégique
    static int consecutiveDraws = 0;
    
    // 1. Analyse de l'état du jeu et détermination de la phase
    int phase = determineGamePhase(state);
    
    // Incrémenter compteur de tours
    state->turnCount++;
    
    // 2. Analyse du réseau existant pour une planification stratégique
    int cityConnectivity[MAX_CITIES] = {0};
    analyzeExistingNetwork(state, cityConnectivity);

    MissingConnection missingConnections[MAX_CITIES];
    int missingConnectionCount = 0;
    findMissingConnections(state, cityConnectivity, missingConnections, &missingConnectionCount);
    
    CriticalRoute criticalRoutes[MAX_OBJECTIVES * 2];
    int criticalRouteCount = 0;
    identifyCriticalRoutes(state, criticalRoutes, &criticalRouteCount);

    // Vérifier si nous avons des routes critiques avec assez de cartes
    bool hasCriticalRoutesToClaim = false;
    for (int i = 0; i < criticalRouteCount; i++) {
        if (criticalRoutes[i].hasEnoughCards) {
            hasCriticalRoutesToClaim = true;
            break;
        }
    }
    
    if (missingConnectionCount > 0) {

    }
   
    // 3. Identifier le profil de l'adversaire 
    if (state->turnCount % 5 == 0) {
        updateOpponentProfile(state);
    }
    
    // 4. Déterminer la priorité stratégique
    StrategicPriority priority = determinePriority(state, phase, criticalRoutes, 
                                                   criticalRouteCount, consecutiveDraws);
    
    // 5. Forcer prise de route après trop de pioches consécutives
    if (consecutiveDraws >= 4 && hasCriticalRoutesToClaim) {
        priority = BUILD_NETWORK;
    }
    
    // 6. Exécuter la décision selon la priorité
    return executePriority(state, moveData, priority, criticalRoutes, criticalRouteCount, &consecutiveDraws);
}

// Interfaces pour les anciennes stratégies (toutes redirigent vers superAdvancedStrategy)
int basicStrategy(GameState* state, MoveData* moveData) {
    return superAdvancedStrategy(state, moveData);
}

int dijkstraStrategy(GameState* state, MoveData* moveData) {
    return superAdvancedStrategy(state, moveData);
}

int advancedStrategy(GameState* state, MoveData* moveData) {
    return superAdvancedStrategy(state, moveData);
}