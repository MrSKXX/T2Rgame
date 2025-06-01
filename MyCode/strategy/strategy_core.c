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
        printf("ERREUR CRITIQUE: Paramètres NULL dans safeAdvancedStrategy\n");
        return 0;
    }
    
    // Exécuter la stratégie normale
    int result = superAdvancedStrategy(state, moveData);
    
    if (result != 1) {
        printf("ERREUR: Stratégie principale a échoué (code %d)\n", result);
        // Action de secours
        moveData->action = DRAW_BLIND_CARD;
        return 1;
    }
    
    // Valider le mouvement choisi
    if (!isValidMove(state, moveData)) {
        printf("CORRECTION AUTOMATIQUE: Mouvement invalide détecté, pioche forcée\n");
        moveData->action = DRAW_BLIND_CARD;
    }
    
    // Log du mouvement pour debug
    switch (moveData->action) {
        case CLAIM_ROUTE:
            printf("MOUVEMENT VALIDÉ: Prendre route %d->%d (couleur %d)\n",
                   moveData->claimRoute.from, moveData->claimRoute.to, 
                   moveData->claimRoute.color);
            break;
        case DRAW_CARD:
            printf("MOUVEMENT VALIDÉ: Piocher carte visible %d\n", moveData->drawCard);
            break;
        case DRAW_BLIND_CARD:
            printf("MOUVEMENT VALIDÉ: Piocher carte aveugle\n");
            break;
        default:
            printf("MOUVEMENT VALIDÉ: Action %d\n", moveData->action);
            break;
    }
    
    return 1;
}


int decideNextMove(GameState* state, StrategyType strategy, MoveData* moveData) {
    // La fonction debugObjectives est externe (dans debug.c)
    extern void debugObjectives(GameState* state);
    debugObjectives(state);
    
    printf("Cartes en main: ");
    for (int i = 1; i < 10; i++) {
        if (state->nbCardsByColor[i] > 0) {
            printf("%s:%d ", 
                  (i < 10) ? (const char*[]){
                      "None", "Purple", "White", "Blue", "Yellow", 
                      "Orange", "Black", "Red", "Green", "Locomotive"
                  }[i] : "Unknown", 
                  state->nbCardsByColor[i]);
        }
    }
    printf("\n");
    
    // Ignorer le paramètre strategy, utiliser toujours la stratégie avancée
    (void)strategy; // Éviter l'avertissement de paramètre non utilisé
    
    return safeAdvancedStrategy(state, moveData);
}

// Stratégie principale optimisée
int superAdvancedStrategy(GameState* state, MoveData* moveData) {
    printf("Stratégie avancée optimisée en cours d'exécution\n");
    
    // Variables de suivi stratégique
    static int consecutiveDraws = 0;
    
    // 1. Analyse de l'état du jeu et détermination de la phase
    int phase = determineGamePhase(state);
    printf("Phase de jeu actuelle: %d\n", phase);
    
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
            printf("DÉCISION STRATÉGIQUE: Route critique %d -> %d disponible et prenable\n", 
                criticalRoutes[i].from, criticalRoutes[i].to);
            break;
        }
    }
    
    // Vérifier s'il y a des opportunités stratégiques
    if (missingConnectionCount > 0) {
        printf("OPPORTUNITÉ DÉTECTÉE: %d connexions stratégiques manquantes identifiées!\n", 
            missingConnectionCount);
    }
   
    // 3. Identifier le profil de l'adversaire
    if (state->turnCount % 5 == 0) {
        updateOpponentProfile(state);
        printf("Profil adversaire mis à jour: %d\n", currentOpponentProfile);
    }
    
    // 4. Déterminer la priorité stratégique
    StrategicPriority priority = determinePriority(state, phase, criticalRoutes, 
                                                   criticalRouteCount, consecutiveDraws);
    
    // 5. Forcer prise de route après trop de pioches consécutives
    if (consecutiveDraws >= 4 && hasCriticalRoutesToClaim) {
        printf("FORCE MAJEURE: Trop de pioches consécutives (%d), forcer la prise d'une route\n", consecutiveDraws);
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