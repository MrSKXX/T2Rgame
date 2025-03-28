#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "strategy.h"
#include "rules.h"


/*
 * decideNextMove
 * 
 * Cette fonction est le cerveau de l'IA - elle décide quelle action effectuer
 * en fonction de l'état actuel du jeu et de la stratégie choisie.
 * 
 * Elle sert de façade vers différentes implémentations de stratégies :
 * - basicStrategy : stratégie simple qui prend la première route disponible
 * - dijkstraStrategy : stratégie utilisant Dijkstra (non implémentée complètement)
 * - advancedStrategy : stratégie avancée combinant plusieurs approches (non implémentée)
 * 
 * Cette conception en façade permet de facilement tester et comparer différentes
 * stratégies sans modifier le reste du code.
 */

int decideNextMove(GameState* state, StrategyType strategy, MoveData* moveData) {
    switch (strategy) {
        case STRATEGY_BASIC:
            return basicStrategy(state, moveData);
        case STRATEGY_DIJKSTRA:
            return dijkstraStrategy(state, moveData);
        case STRATEGY_ADVANCED:
            return advancedStrategy(state, moveData);
        default:
            return basicStrategy(state, moveData);
    }
}


/*
 * basicStrategy
 * 
 * Cette fonction implémente une stratégie simple mais fonctionnelle pour l'IA.
 * Elle représente le minimum viable pour jouer correctement.
 * 
 * Logique de décision :
 * 1) Si possible, prendre une route (la première disponible)
 * 2) Sinon, piocher avec les priorités suivantes :
 *    a. Locomotive visible (très utile)
 *    b. Carte de couleur visible
 *    c. Tirer des objectifs si on en a peu
 *    d. Carte aveugle en dernier recours
 * 
 * Cette stratégie est "gloutonne" (greedy) - elle prend la première option
 * disponible sans planification à long terme ou optimisation globale.
 * 
 * Amélioration possible : évaluer les routes en fonction de leur utilité
 * pour compléter des objectifs, plutôt que de prendre la première disponible.
 */



int basicStrategy(GameState* state, MoveData* moveData) {
    // Tableau pour stocker les routes possibles
    int possibleRoutes[MAX_ROUTES];
    CardColor possibleColors[MAX_ROUTES];
    int possibleLocomotives[MAX_ROUTES];
    
    // Trouve les routes que nous pouvons prendre
    int numPossibleRoutes = findPossibleRoutes(state, possibleRoutes, possibleColors, possibleLocomotives);
    
    // Si nous pouvons prendre une route
    if (numPossibleRoutes > 0) {
        // Simple stratégie: prendre la première route possible
        int routeIndex = possibleRoutes[0];
        int from = state->routes[routeIndex].from;
        int to = state->routes[routeIndex].to;
        CardColor color = possibleColors[0];
        int nbLocomotives = possibleLocomotives[0];
        
        // Vérification supplémentaire que nous avons bien les cartes nécessaires
        int requiredCards = state->routes[routeIndex].length;
        int availableColorCards = state->nbCardsByColor[color];
        int availableLocomotives = state->nbCardsByColor[LOCOMOTIVE];
        
        // Debug: afficher les cartes requises vs disponibles
        printf("Route requires %d cards, we have %d %s cards and %d locomotives\n",
            requiredCards, 
            availableColorCards,
            (color < 10 && color >= 0) ? (const char*[]){"None", "Purple", "White", "Blue", "Yellow", 
                                                       "Orange", "Black", "Red", "Green", "Locomotive"}[color] : "Unknown", 
            availableLocomotives);
        
        if (availableColorCards + availableLocomotives >= requiredCards) {
            // Prépare l'action
            moveData->action = CLAIM_ROUTE;
            moveData->claimRoute.from = from;
            moveData->claimRoute.to = to;
            moveData->claimRoute.color = color;
            moveData->claimRoute.nbLocomotives = nbLocomotives;
            
            printf("Strategy decided: claim route from %d to %d with color %d and %d locomotives\n",
                  moveData->claimRoute.from, moveData->claimRoute.to, 
                  moveData->claimRoute.color, moveData->claimRoute.nbLocomotives);
            
            return 1;
        } else {
            printf("Warning: findPossibleRoutes returned a route we can't actually claim. This shouldn't happen!\n");
        }
    }
    
    // Si nous n'avons pas assez de cartes pour prendre une route,
    // on pioche une carte stratégiquement
    
    // Priorité 1: Locomotive visible (très utile)
    for (int i = 0; i < 5; i++) {
        if (state->visibleCards[i] == LOCOMOTIVE) {
            moveData->action = DRAW_CARD;
            moveData->drawCard = LOCOMOTIVE;
            printf("Strategy decided: draw visible locomotive card\n");
            return 1;
        }
    }
    
    // Priorité 2: Carte de couleur dont nous avons besoin pour nos objectifs
    // (simplification: on choisit la première carte visible qui n'est pas une locomotive)
    CardColor cardToDraw = (CardColor)-1;
    for (int i = 0; i < 5; i++) {
        if (state->visibleCards[i] != LOCOMOTIVE && state->visibleCards[i] != NONE && state->visibleCards[i] > 0) {
            cardToDraw = state->visibleCards[i];
            break;
        }
    }
    
    // Si on a trouvé une carte à piocher
    if (cardToDraw != (CardColor)-1) {
        moveData->action = DRAW_CARD;
        moveData->drawCard = cardToDraw;
        printf("Strategy decided: draw visible card of color %d\n", cardToDraw);
        return 1;
    }
    
    // Priorité 3: Tirer des objectifs si on en a peu
    if (state->nbObjectives < 2) {
        moveData->action = DRAW_OBJECTIVES;
        printf("Strategy decided: draw new objectives\n");
        return 1;
    }
    
    // Priorité 4: Si aucune carte visible n'est intéressante, on pioche une carte aveugle
    moveData->action = DRAW_BLIND_CARD;
    printf("Strategy decided: draw blind card\n");
    return 1;
}





// Stratégie Dijkstra: pas implémentée pour l'instant
int dijkstraStrategy(GameState* state, MoveData* moveData) {
    printf("Dijkstra strategy not implemented yet, using basic strategy\n");
    return basicStrategy(state, moveData);
}

// Stratégie avancée: pas implémentée pour l'instant
int advancedStrategy(GameState* state, MoveData* moveData) {
    printf("Advanced strategy not implemented yet, using basic strategy\n");
    return basicStrategy(state, moveData);
}







// Décide quels objectifs garder parmi les 3 proposés
void chooseObjectivesStrategy(GameState* state, Objective* objectives, bool* chooseObjectives) {
    // Stratégie simple: on garde tous les objectifs pour l'instant
    for (int i = 0; i < 3; i++) {
        chooseObjectives[i] = true;
    }
}

// Décide quelle carte visible piocher
int chooseCardToDraw(GameState* state, CardColor* visibleCards) {
    // Stratégie simple: prendre la première carte qui n'est pas une locomotive
    for (int i = 0; i < 5; i++) {
        if (visibleCards[i] != LOCOMOTIVE && visibleCards[i] != NONE) {
            return i;
        }
    }
    
    // Si aucune carte ne convient, on prend une locomotive si disponible
    for (int i = 0; i < 5; i++) {
        if (visibleCards[i] == LOCOMOTIVE) {
            return i;
        }
    }
    
    // Si aucune carte n'est disponible, on indique de piocher une carte aveugle
    return -1;
}


int findShortestPath(GameState* state, int start, int end, int* path) {
    // Cette fonction n'est pas encore implémentée complètement
    // Juste un squelette pour l'instant
    
    // Distances depuis le départ
    int dist[MAX_CITIES];
    // Précédents dans le chemin
    int prev[MAX_CITIES];
    // Villes déjà visitées
    int visited[MAX_CITIES];
    
    // Initialisation
    for (int i = 0; i < state->nbCities; i++) {
        dist[i] = INT_MAX;
        prev[i] = -1;
        visited[i] = 0;
    }
    
    dist[start] = 0;
    
    // TODO: Implémentation complète de Dijkstra
    
    printf("Dijkstra algorithm not fully implemented yet\n");
    
    return -1; // Pas encore implémenté
}