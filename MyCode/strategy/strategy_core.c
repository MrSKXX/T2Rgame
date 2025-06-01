/**
 * strategy_core.c
 * Interface principale du système de stratégie - VERSION FINALE
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
    
    // Log simple du mouvement final
    switch (moveData->action) {
        case CLAIM_ROUTE:
            printf("Taking route %d->%d\n", moveData->claimRoute.from, moveData->claimRoute.to);
            break;
        case DRAW_CARD:
        case DRAW_BLIND_CARD:
        default:
            break;
    }
    
    return 1;
}

int decideNextMove(GameState* state, MoveData* moveData) {
    return safeAdvancedStrategy(state, moveData);
}

// Stratégie principale optimisée
int superAdvancedStrategy(GameState* state, MoveData* moveData) {
    static int consecutiveDraws = 0;
    
    // Analyse de l'état du jeu et détermination de la phase
    int phase = determineGamePhase(state);
    
    // Incrémenter compteur de tours
    state->turnCount++;
    
    // Analyse du réseau existant pour une planification stratégique
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
   
    // Identifier le profil de l'adversaire (analyse silencieuse)
    if (state->turnCount % 5 == 0) {
        updateOpponentProfile(state);
    }
    
    // Déterminer la priorité stratégique
    StrategicPriority priority = determinePriority(state, phase, criticalRoutes, 
                                                   criticalRouteCount, consecutiveDraws);
    
    // Forcer prise de route après trop de pioches consécutives
    if (consecutiveDraws >= 4 && hasCriticalRoutesToClaim) {
        priority = BUILD_NETWORK;
    }
    
    // Exécuter la décision selon la priorité
    return executePriority(state, moveData, priority, criticalRoutes, criticalRouteCount, &consecutiveDraws);
}