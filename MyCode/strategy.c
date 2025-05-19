// strategy.c
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include "strategy.h"
#include "rules.h"

// Constantes pour l'évaluation stratégique
#define STRATEGIC_OBJECTIVE_MULTIPLIER 15
#define CRITICAL_PATH_MULTIPLIER 30
#define BLOCKING_VALUE_MULTIPLIER 12
#define CARD_EFFICIENCY_MULTIPLIER 8
#define MAX_CENTRAL_CITIES 10
#define MAX_HUB_CONNECTIONS 6

// Carte des régions stratégiques (clusters de villes connexes)
typedef struct {
    int cities[12];
    int cityCount;
    int strategicValue;
} MapRegion;

// Données du modèle de l'adversaire
static int opponentCitiesOfInterest[MAX_CITIES] = {0};
static OpponentProfile currentOpponentProfile = OPPONENT_UNKNOWN;

// Régions stratégiques de la carte (version simplifiée)
static MapRegion regions[] = {
    {{31, 32, 33, 34, 30, 27, 28, 29}, 8, 85}, // Est
    {{19, 20, 21, 22, 18, 23}, 6, 90},         // Centre
    {{0, 1, 2, 8, 9, 10}, 6, 80},              // Ouest
    {{24, 25, 26, 35, 23, 16, 15, 14}, 8, 75}, // Sud
    {{3, 4, 5, 6, 7, 11, 12, 13}, 8, 70}       // Nord
};
static const int NUM_REGIONS = 5;

// Cache pour l'algorithme de pathfinding
typedef struct {
    int from;
    int to;
    int pathLength;
    int path[MAX_CITIES];
    int distance;
    int timestamp; // Pour invalidation du cache
} PathCacheEntry;

#define PATH_CACHE_SIZE 50
static PathCacheEntry pathCache[PATH_CACHE_SIZE];
static int cacheEntries = 0;
static int cacheTimestamp = 0;

// Paramètres de la personnalité du bot (ajustables)
typedef struct {
    float aggressiveness;    // 0.0-1.0: tendance à bloquer
    float objectiveFocus;    // 0.0-1.0: priorité aux objectifs
    float riskTolerance;     // 0.0-1.0: tolérance au risque
    float opportunism;       // 0.0-1.0: changement de plan si opportunité
    float territorialControl; // 0.0-1.0: contrôle territorial
} BotPersonality;

// Fonction principale de décision
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
    
    return superAdvancedStrategy(state, moveData);
}

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

// Identifie le profil stratégique de l'adversaire
OpponentProfile identifyOpponentProfile(GameState* state) {
    // Analyse des actions adverses pour déterminer son profil
    float routeRatio = 0.0;
    if (state->turnCount > 0) {
        routeRatio = (float)state->nbTracks - state->nbClaimedRoutes - 
                    (state->opponentWagonsLeft * 5 / 45) / state->turnCount;
    }
    
    int colorDiversity = 0;
    
    // Routes longues vs courtes
    float avgRouteLength = 0;
    int routeCount = 0;
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 2) {
            avgRouteLength += state->routes[i].length;
            routeCount++;
        }
    }
    
    if (routeCount > 0) {
        avgRouteLength /= routeCount;
    }
    
    // Déduction du profil
    if (routeRatio > 0.7 || avgRouteLength < 2.5) {
        return OPPONENT_AGGRESSIVE;
    }
    
    if (state->opponentCardCount > 12 && routeCount < state->turnCount / 3) {
        return OPPONENT_HOARDER;
    }
    
    if (state->opponentObjectiveCount > 3) {
        return OPPONENT_OBJECTIVE;
    }
    
    // Détection de stratégie de blocage
    int blockingMoves = 0;
    for (int i = 0; i < MAX_CITIES; i++) {
        if (opponentCitiesOfInterest[i] > 10) {
            blockingMoves++;
        }
    }
    
    if (blockingMoves > 3) {
        return OPPONENT_BLOCKER;
    }
    
    return OPPONENT_UNKNOWN;
}

// Évaluation avancée de l'utilité d'une route
int enhancedEvaluateRouteUtility(GameState* state, int routeIndex) {
    if (routeIndex < 0 || routeIndex >= state->nbTracks) {
        printf("ERROR: Invalid route index %d in enhancedEvaluateRouteUtility\n", routeIndex);
        return 0;
    }

    int utility = evaluateRouteUtility(state, routeIndex);
    int from = state->routes[routeIndex].from;
    int to = state->routes[routeIndex].to;
    int length = state->routes[routeIndex].length;
    
    // Bonus pour connexion à notre réseau existant
    for (int i = 0; i < state->nbClaimedRoutes; i++) {
        int claimedRouteIndex = state->claimedRoutes[i];
        if (claimedRouteIndex >= 0 && claimedRouteIndex < state->nbTracks) {
            if (state->routes[claimedRouteIndex].from == from || 
                state->routes[claimedRouteIndex].to == from ||
                state->routes[claimedRouteIndex].from == to || 
                state->routes[claimedRouteIndex].to == to) {
                utility += 30;
                break;
            }
        }
    }
    
    // Bonus stratégique pour les routes longues
    if (length >= 5) {
        utility += length * 100;
    } else if (length >= 4) {
        utility += length * 50;
    } else if (length >= 3) {
        utility += length * 20;
    }
    
    // Pénalité pour routes inutiles en fin de partie
    if ((determineGamePhase(state) == PHASE_LATE || determineGamePhase(state) == PHASE_FINAL) && 
        calculateObjectiveProgress(state, routeIndex) == 0) {
        utility -= 50;
    }
    
    // Bonus pour routes bloquant l'adversaire
    if (from < MAX_CITIES && to < MAX_CITIES &&
        (opponentCitiesOfInterest[from] > 8 || opponentCitiesOfInterest[to] > 8)) {
        utility += 35;
    }
    
    return utility;
}

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

// Identifie les routes centrales stratégiques
void identifyAndPrioritizeBottlenecks(GameState* state, int* prioritizedRoutes, int* count) {
    *count = 0;
    
    // 1. Identifier les "hubs" (villes avec plusieurs connexions)
    int hubCities[MAX_CITIES] = {0};
    int hubCount = 0;
    
    for (int i = 0; i < state->nbCities && hubCount < MAX_CENTRAL_CITIES; i++) {
        int connections = 0;
        
        for (int j = 0; j < state->nbTracks; j++) {
            if (state->routes[j].from == i || state->routes[j].to == i) {
                connections++;
            }
        }
        
        if (connections >= MAX_HUB_CONNECTIONS) {
            hubCities[hubCount++] = i;
        }
    }
    
    // 2. Identifier les routes entre hubs (goulots d'étranglement)
    for (int i = 0; i < hubCount && *count < MAX_ROUTES; i++) {
        for (int j = i+1; j < hubCount && *count < MAX_ROUTES; j++) {
            int cityA = hubCities[i];
            int cityB = hubCities[j];
            
            // Vérifier s'il existe une route directe
            for (int r = 0; r < state->nbTracks; r++) {
                if (((state->routes[r].from == cityA && state->routes[r].to == cityB) ||
                     (state->routes[r].from == cityB && state->routes[r].to == cityA)) &&
                    state->routes[r].owner == 0) {
                    // C'est une route bottleneck
                    prioritizedRoutes[(*count)++] = r;
                }
            }
        }
    }
    
    // 3. Ajouter les routes entre régions stratégiques
    for (int r1 = 0; r1 < NUM_REGIONS && *count < MAX_ROUTES; r1++) {
        for (int r2 = r1+1; r2 < NUM_REGIONS && *count < MAX_ROUTES; r2++) {
            // Pour chaque paire de villes entre deux régions
            for (int c1 = 0; c1 < regions[r1].cityCount && *count < MAX_ROUTES; c1++) {
                for (int c2 = 0; c2 < regions[r2].cityCount && *count < MAX_ROUTES; c2++) {
                    int cityA = regions[r1].cities[c1];
                    int cityB = regions[r2].cities[c2];
                    
                    // Vérifier routes directes
                    for (int r = 0; r < state->nbTracks; r++) {
                        if (((state->routes[r].from == cityA && state->routes[r].to == cityB) ||
                             (state->routes[r].from == cityB && state->routes[r].to == cityA)) &&
                            state->routes[r].owner == 0) {
                            // Ajouter seulement si pas déjà dans la liste
                            bool alreadyAdded = false;
                            for (int k = 0; k < *count; k++) {
                                if (prioritizedRoutes[k] == r) {
                                    alreadyAdded = true;
                                    break;
                                }
                            }
                            
                            if (!alreadyAdded) {
                                prioritizedRoutes[(*count)++] = r;
                            }
                        }
                    }
                }
            }
        }
    }
}

// Détermine la couleur optimale pour prendre une route
CardColor determineOptimalColor(GameState* state, int routeIndex) {
    if (routeIndex < 0 || routeIndex >= state->nbTracks) {
        return NONE;
    }
    
    CardColor routeColor = state->routes[routeIndex].color;
    CardColor secondColor = state->routes[routeIndex].secondColor;
    int length = state->routes[routeIndex].length;
    
    // Si c'est une route colorée (non grise)
    if (routeColor != LOCOMOTIVE) {
        if (state->nbCardsByColor[routeColor] >= length) {
            return routeColor; // Assez de cartes de cette couleur
        }
        
        // S'il y a une couleur alternative
        if (secondColor != NONE && state->nbCardsByColor[secondColor] >= length) {
            return secondColor;
        }
        
        // Si on peut compléter avec des locomotives
        if (state->nbCardsByColor[routeColor] + state->nbCardsByColor[LOCOMOTIVE] >= length) {
            return routeColor;
        }
        
        if (secondColor != NONE && 
            state->nbCardsByColor[secondColor] + state->nbCardsByColor[LOCOMOTIVE] >= length) {
            return secondColor;
        }
        
        // Dernier recours: utiliser uniquement des locomotives
        if (state->nbCardsByColor[LOCOMOTIVE] >= length) {
            return LOCOMOTIVE;
        }
        
        return NONE; // Pas assez de cartes
    }
    
    // C'est une route grise, trouver la couleur la plus efficace
    CardColor bestColor = NONE;
    int bestColorCount = 0;
    
    for (int c = 1; c < 9; c++) { // 1-8 = couleurs, 9 = locomotive
        if (state->nbCardsByColor[c] > bestColorCount) {
            bestColorCount = state->nbCardsByColor[c];
            bestColor = c;
        }
    }
    
    // Si la meilleure couleur suffit
    if (bestColorCount >= length) {
        return bestColor;
    }
    
    // Compléter avec des locomotives
    if (bestColorCount + state->nbCardsByColor[LOCOMOTIVE] >= length) {
        return bestColor;
    }
    
    // Si uniquement des locomotives suffisent
    if (state->nbCardsByColor[LOCOMOTIVE] >= length) {
        return LOCOMOTIVE;
    }
    
    return NONE; // Pas assez de cartes
}

// Évalue l'efficacité d'utilisation de nos cartes
int evaluateCardEfficiency(GameState* state, int routeIndex) {
    if (routeIndex < 0 || routeIndex >= state->nbTracks) {
        return 0;
    }

    CardColor routeColor = state->routes[routeIndex].color;
    int length = state->routes[routeIndex].length;
    

    if (routeColor != LOCOMOTIVE && length <= 2 && state->nbCardsByColor[LOCOMOTIVE] > 0) {
        return 50;  // Pénaliser utilisation de locomotives pour routes courtes
    }

    // Pour les routes grises, trouver la couleur la plus efficace
    if (routeColor == LOCOMOTIVE) {
        int bestEfficiency = 0;
        for (int c = 1; c < 9; c++) {  // Ignorer NONE et LOCOMOTIVE
            if (state->nbCardsByColor[c] > 0) {
                int cardsNeeded = length;
                int cardsAvailable = state->nbCardsByColor[c];
                
                if (cardsNeeded <= cardsAvailable) {
                    int efficiency = 150;
                    if (efficiency > bestEfficiency) {
                        bestEfficiency = efficiency;
                    }
                } else {
                    int locosNeeded = cardsNeeded - cardsAvailable;
                    if (locosNeeded <= state->nbCardsByColor[LOCOMOTIVE]) {
                        int efficiency = 100 - (locosNeeded * 10);
                        if (efficiency > bestEfficiency) {
                            bestEfficiency = efficiency;
                        }
                    }
                }
            }
        }
        return bestEfficiency;
    } 
    // Pour les routes colorées
    else {
        int cardsNeeded = length;
        int cardsAvailable = state->nbCardsByColor[routeColor];
        
        if (cardsNeeded <= cardsAvailable) {
            return 150;
        } else {
            int locosNeeded = cardsNeeded - cardsAvailable;
            if (locosNeeded <= state->nbCardsByColor[LOCOMOTIVE]) {
                return 100 - (locosNeeded * 10);
            }
        }
    }
    
    return 0;
}

// Pioche stratégique de cartes basée sur les besoins actuels
int strategicCardDrawing(GameState* state) {
    if (!state) {
        return -1;
    }
    
    // Définir les tableaux pour l'analyse
    int colorNeeds[10] = {0};
    int totalColorNeeds = 0;
    
    printf("Analyzing card needs for incomplete objectives:\n");
    
    // Déterminer les besoins de couleurs pour objectifs non complétés
    for (int i = 0; i < state->nbObjectives; i++) {
        if (i < 0 || i >= MAX_OBJECTIVES) continue;
        
        if (isObjectiveCompleted(state, state->objectives[i])) {
            continue;
        }
        
        int objFrom = state->objectives[i].from;
        int objTo = state->objectives[i].to;
        
        if (objFrom < 0 || objFrom >= state->nbCities || objTo < 0 || objTo >= state->nbCities) {
            continue;
        }
        
        printf("  Objective %d: From %d to %d\n", i+1, objFrom, objTo);
        
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findShortestPath(state, objFrom, objTo, path, &pathLength);
        
        if (distance > 0 && pathLength > 0) {
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
                            colorNeeds[routeColor] += length;
                            totalColorNeeds += length;
                        } else {
                            colorNeeds[LOCOMOTIVE] += 1;
                        }
                        
                        break;
                    }
                }
            }
        }
    }
    
    // Afficher les besoins par couleur
    printf("Color needs summary:\n");
    for (int c = 1; c < 10; c++) {
        if (colorNeeds[c] > 0) {
            printf("  %s: %d cards needed\n", 
                  (c < 10) ? (const char*[]){
                      "None", "Purple", "White", "Blue", "Yellow", 
                      "Orange", "Black", "Red", "Green", "Locomotive"
                  }[c] : "Unknown", 
                  colorNeeds[c]);
        }
    }
    
    // Trouver la meilleure carte visible selon les besoins
    int bestCardIndex = -1;
    int bestCardScore = 0;
    
    for (int i = 0; i < 5; i++) {
        CardColor card = state->visibleCards[i];
        if (card == NONE || card < 0 || card >= 10) {
            continue;
        }
        
        int score = 0;
        
        // Les locomotives sont toujours très précieuses
        if (card == LOCOMOTIVE) {
            score = 100;
            score += colorNeeds[LOCOMOTIVE] * 10;
        } 
        // Score des autres cartes basé sur les besoins
        else {
            score = colorNeeds[card] * 5;
            
            // Bonus si nous avons déjà des cartes de cette couleur
            if (state->nbCardsByColor[card] > 0) {
                score += state->nbCardsByColor[card] * 3;
                
                // Bonus si nous sommes proches de compléter une route
                for (int r = 0; r < state->nbTracks; r++) {
                    if (state->routes[r].owner == 0) {
                        CardColor routeColor = state->routes[r].color;
                        int length = state->routes[r].length;
                        
                        if (routeColor == card || routeColor == LOCOMOTIVE) {
                            int cardsNeeded = length - state->nbCardsByColor[card];
                            if (cardsNeeded > 0 && cardsNeeded <= 2) {
                                score += (3 - cardsNeeded) * 15;
                            }
                        }
                    }
                }
            }
            
            // Pénalité si nous avons trop de cartes de cette couleur (>8)
            if (state->nbCardsByColor[card] > 8) {
                score -= (state->nbCardsByColor[card] - 8) * 5;
            }
        }
        
        printf("Visible card %d: %s, score %d\n", 
              i+1, 
              (card < 10) ? (const char*[]){
                  "None", "Purple", "White", "Blue", "Yellow", 
                  "Orange", "Black", "Red", "Green", "Locomotive"
              }[card] : "Unknown", 
              score);
        
        if (score > bestCardScore) {
            bestCardScore = score;
            bestCardIndex = i;
        }
    }
    
    // Si aucune carte visible n'est bonne, suggérer une pioche aveugle
    if (bestCardIndex == -1 || bestCardScore < 20) {
        int blindScore = 40;
        
        // Plus de chances de pioche aveugle si besoins diversifiés
        int uniqueNeeds = 0;
        for (int c = 1; c < 10; c++) {
            if (colorNeeds[c] > 0) {
                uniqueNeeds++;
            }
        }
        
        if (uniqueNeeds >= 3) {
            blindScore += 20;
        }
        
        printf("Blind draw score: %d (unique color needs: %d)\n", blindScore, uniqueNeeds);
        
        if (blindScore > bestCardScore) {
            printf("Recommending blind draw (score %d > best visible card score %d)\n", 
                  blindScore, bestCardScore);
            return -1;
        }
    }
    
    if (bestCardIndex >= 0) {
        printf("Recommending visible card %d: %s (score %d)\n", 
              bestCardIndex + 1, 
              (state->visibleCards[bestCardIndex] < 10) ? (const char*[]){
                  "None", "Purple", "White", "Blue", "Yellow", 
                  "Orange", "Black", "Red", "Green", "Locomotive"
              }[state->visibleCards[bestCardIndex]] : "Unknown", 
              bestCardScore);
    } else {
        printf("No good visible card found, recommending blind draw\n");
    }
    
    return bestCardIndex;
}

// Modèle les objectifs probables de l'adversaire à partir de ses actions
void updateOpponentObjectiveModel(GameState* state, int from, int to) {
    if (from < 0 || from >= MAX_CITIES || to < 0 || to >= MAX_CITIES) {
        return;
    }
    
    static int opponentCityVisits[MAX_CITIES] = {0};
    static int opponentLikelyObjectives[MAX_CITIES][MAX_CITIES] = {0};
    static int opponentConsecutiveRoutesInDirection[MAX_CITIES] = {0};
    
    // Mettre à jour le compteur de visites
   opponentCityVisits[from]++;
   opponentCityVisits[to]++;
   
   // Villes avec plusieurs connexions sont probablement des objectifs
   if (opponentCityVisits[from] >= 2) {
       printf("ADVERSAIRE: La ville %d semble être importante (connexions: %d)\n", 
              from, opponentCityVisits[from]);
   }
   if (opponentCityVisits[to] >= 2) {
       printf("ADVERSAIRE: La ville %d semble être importante (connexions: %d)\n", 
              to, opponentCityVisits[to]);
   }
   
   // Détecter les patterns de routes consécutives dans une direction
   for (int i = 0; i < state->nbTracks; i++) {
       if (state->routes[i].owner == 2) {
           if (state->routes[i].from == from || state->routes[i].from == to || 
               state->routes[i].to == from || state->routes[i].to == to) {
               
               int otherCity = -1;
               if (state->routes[i].from == from || state->routes[i].from == to) {
                   otherCity = state->routes[i].to;
               } else {
                   otherCity = state->routes[i].from;
               }
               
               if (otherCity != from && otherCity != to && otherCity >= 0 && otherCity < MAX_CITIES) {
                   opponentConsecutiveRoutesInDirection[otherCity]++;
                   
                   if (opponentConsecutiveRoutesInDirection[otherCity] >= 2) {
                       printf("TRAJECTOIRE ADVERSAIRE DÉTECTÉE: Direction vers la ville %d\n", otherCity);
                       
                       for (int dest = 0; dest < state->nbCities && dest < MAX_CITIES; dest++) {
                           if (dest != otherCity) {
                               int dx = abs(dest - otherCity) % 10;
                               int distance = 10 - dx;
                               if (distance > 0) {
                                   opponentLikelyObjectives[otherCity][dest] += distance * 15;
                                   opponentLikelyObjectives[dest][otherCity] += distance * 15;
                               }
                           }
                       }
                   }
               }
           }
       }
   }
   
   // Mettre à jour les probabilités d'objectifs
   for (int i = 0; i < state->nbCities && i < MAX_CITIES; i++) {
       if (opponentCityVisits[i] >= 2) {
           for (int j = i+1; j < state->nbCities && j < MAX_CITIES; j++) {
               if (opponentCityVisits[j] >= 2) {
                   int path[MAX_CITIES];
                   int pathLength = 0;
                   int distance = findShortestPath(state, i, j, path, &pathLength);
                   
                   if (distance > 0) {
                       int distanceScore = 0;
                       if (distance >= 4 && distance <= 9) {
                           distanceScore = 10;
                       } else if (distance >= 2 && distance <= 12) {
                           distanceScore = 5;
                       }
                       
                       int opponentRoutesOnPath = 0;
                       for (int k = 0; k < pathLength - 1; k++) {
                           int cityA = path[k];
                           int cityB = path[k + 1];
                           
                           for (int r = 0; r < state->nbTracks; r++) {
                               if (((state->routes[r].from == cityA && state->routes[r].to == cityB) ||
                                    (state->routes[r].from == cityB && state->routes[r].to == cityA)) &&
                                   state->routes[r].owner == 2) {
                                   opponentRoutesOnPath++;
                                   break;
                               }
                           }
                       }
                       
                       if (opponentRoutesOnPath >= 2) {
                           opponentLikelyObjectives[i][j] += 30 * opponentRoutesOnPath;
                           opponentLikelyObjectives[j][i] += 30 * opponentRoutesOnPath;
                           
                           printf("OBJECTIF ADVERSE PROBABLE: %d -> %d (score: %d, routes: %d)\n", 
                                  i, j, opponentLikelyObjectives[i][j], opponentRoutesOnPath);
                       }
                       else if (distanceScore > 0) {
                           opponentLikelyObjectives[i][j] += distanceScore;
                           opponentLikelyObjectives[j][i] += distanceScore;
                       }
                   }
               }
           }
       }
   }
   
   // Analyse des objectifs probables de l'adversaire
   printf("Objectifs probables de l'adversaire:\n");
   int threshold = 30;
   int count = 0;
   int topObjectives[5][2];
   int topScores[5] = {0};
   
   for (int i = 0; i < state->nbCities && i < MAX_CITIES; i++) {
       for (int j = i+1; j < state->nbCities && j < MAX_CITIES; j++) {
           if (opponentLikelyObjectives[i][j] > threshold) {
               // Insérer dans le top 5
               for (int k = 0; k < 5; k++) {
                   if (opponentLikelyObjectives[i][j] > topScores[k]) {
                       // Décaler les éléments pour insérer
                       for (int m = 4; m > k; m--) {
                           topScores[m] = topScores[m-1];
                           topObjectives[m][0] = topObjectives[m-1][0];
                           topObjectives[m][1] = topObjectives[m-1][1];
                       }
                       // Insérer le nouvel élément
                       topScores[k] = opponentLikelyObjectives[i][j];
                       topObjectives[k][0] = i;
                       topObjectives[k][1] = j;
                       break;
                   }
               }
               
               count++;
               if (count <= 5) {
                   printf("  %d -> %d: score %d\n", i, j, opponentLikelyObjectives[i][j]);
               }
           }
       }
   }
   
   // Mettre à jour le tableau global des points d'intérêt
   for (int i = 0; i < MAX_CITIES; i++) {
       opponentCitiesOfInterest[i] = 0;
   }
   
   // Attribuer des points d'intérêt aux villes des objectifs probables
   for (int i = 0; i < 5; i++) {
       if (topScores[i] > 0) {
           int city1 = topObjectives[i][0];
           int city2 = topObjectives[i][1];
           if (city1 >= 0 && city1 < MAX_CITIES) {
               opponentCitiesOfInterest[city1] += (5-i) * 2;
           }
           if (city2 >= 0 && city2 < MAX_CITIES) {
               opponentCitiesOfInterest[city2] += (5-i) * 2;
           }
           
           // Trouver les routes clés pour cet objectif
           int path[MAX_CITIES];
           int pathLength = 0;
           if (findShortestPath(state, city1, city2, path, &pathLength) > 0) {
               // Identifier les routes critiques sur ce chemin
               for (int j = 0; j < pathLength - 1; j++) {
                   int pathFrom = path[j];
                   int pathTo = path[j+1];
                   
                   if (pathFrom >= 0 && pathFrom < MAX_CITIES) {
                       opponentCitiesOfInterest[pathFrom] += 1;
                   }
                   if (pathTo >= 0 && pathTo < MAX_CITIES) {
                       opponentCitiesOfInterest[pathTo] += 1;
                   }
               }
           }
       }
   }
}

// Identifie les routes critiques à bloquer pour l'adversaire
int findCriticalRoutesToBlock(GameState* state, int* routesToBlock, int* blockingPriorities) {
   int count = 0;
   const int MAX_BLOCKING_ROUTES = 10;
   
   // Initialiser les tableaux
   for (int i = 0; i < MAX_BLOCKING_ROUTES; i++) {
       routesToBlock[i] = -1;
       blockingPriorities[i] = 0;
   }
   
   // Analyse des objectifs probables de l'adversaire
   int topObjectives[5][2] = {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};
   int topScores[5] = {0};
   
   // Trouver les 5 objectifs les plus probables de l'adversaire
   for (int i = 0; i < state->nbCities && i < MAX_CITIES; i++) {
       if (opponentCitiesOfInterest[i] > 0) {
           for (int j = i+1; j < state->nbCities && j < MAX_CITIES; j++) {
               if (opponentCitiesOfInterest[j] > 0) {
                   // Score basé sur l'intérêt combiné
                   int score = opponentCitiesOfInterest[i] + opponentCitiesOfInterest[j];
                   
                   // Vérifier si cet objectif est crédible
                   int path[MAX_CITIES];
                   int pathLength = 0;
                   if (findShortestPath(state, i, j, path, &pathLength) > 0) {
                       // Compter combien de routes l'adversaire a déjà sur ce chemin
                       int opponentRoutes = 0;
                       for (int k = 0; k < pathLength - 1; k++) {
                           int cityA = path[k];
                           int cityB = path[k+1];
                           
                           for (int r = 0; r < state->nbTracks; r++) {
                               if (((state->routes[r].from == cityA && state->routes[r].to == cityB) ||
                                    (state->routes[r].from == cityB && state->routes[r].to == cityA)) &&
                                   state->routes[r].owner == 2) {
                                   opponentRoutes++;
                                   break;
                               }
                           }
                       }
                       
                       // Augmenter le score si l'adversaire a déjà des routes sur ce chemin
                       score += opponentRoutes * 5;
                       
                       // Insérer dans le top 5 si score suffisant
                       for (int k = 0; k < 5; k++) {
                           if (score > topScores[k]) {
                               // Décaler les éléments
                               for (int m = 4; m > k; m--) {
                                   topScores[m] = topScores[m-1];
                                   topObjectives[m][0] = topObjectives[m-1][0];
                                   topObjectives[m][1] = topObjectives[m-1][1];
                               }
                               // Insérer
                               topScores[k] = score;
                               topObjectives[k][0] = i;
                               topObjectives[k][1] = j;
                               break;
                           }
                       }
                   }
               }
           }
       }
   }
   
   // Pour chaque objectif probable, identifier les routes critiques à bloquer
   for (int i = 0; i < 5 && topScores[i] > 0; i++) {
       int from = topObjectives[i][0];
       int to = topObjectives[i][1];
       
       printf("Analyse des routes à bloquer pour l'objectif probable %d -> %d (score: %d)\n", 
              from, to, topScores[i]);
       
       int path[MAX_CITIES];
       int pathLength = 0;
       if (findShortestPath(state, from, to, path, &pathLength) > 0) {
           // Pour chaque segment du chemin
           for (int j = 0; j < pathLength - 1 && count < MAX_BLOCKING_ROUTES; j++) {
               int cityA = path[j];
               int cityB = path[j+1];
               
               // Trouver la route correspondante
               for (int r = 0; r < state->nbTracks; r++) {
                   if (((state->routes[r].from == cityA && state->routes[r].to == cityB) ||
                        (state->routes[r].from == cityB && state->routes[r].to == cityA)) &&
                       state->routes[r].owner == 0) {
                       
                       // Vérifier s'il existe un chemin alternatif sans cette route
                       bool isBottleneck = true;
                       
                       // Temporairement marquer cette route comme prise par l'adversaire
                       int originalOwner = state->routes[r].owner;
                       state->routes[r].owner = 2;
                       
                       // Vérifier s'il existe encore un chemin
                       int altPath[MAX_CITIES];
                       int altPathLength = 0;
                       int altDistance = findShortestPath(state, from, to, altPath, &altPathLength);
                       
                       // Restaurer l'état original
                       state->routes[r].owner = originalOwner;
                       
                       // Si un chemin alternatif existe, ce n'est pas un bottleneck
                       if (altDistance > 0) {
                           isBottleneck = false;
                       }
                       
                       // Si c'est un bottleneck, c'est une route critique à bloquer
                       if (isBottleneck) {
                           // Calculer la priorité en fonction de la position dans le chemin et du score de l'objectif
                           int priority = topScores[i] * (3-i);
                           
                           // Bonus pour les routes courtes (plus faciles à prendre)
                           if (state->routes[r].length <= 2) {
                               priority += 10;
                           }
                           
                           // Bonus si c'est près du début du chemin (plus stratégique)
                           if (j <= 1 || j >= pathLength - 2) {
                               priority += 10;
                           }
                           
                           printf("Route critique à bloquer: %d -> %d (priorité: %d)\n", 
                                  cityA, cityB, priority);
                           
                           // Ajouter à notre liste de routes à bloquer
                           routesToBlock[count] = r;
                           blockingPriorities[count] = priority;
                           count++;
                           
                           // Eviter les doublons
                           break;
                       }
                   }
               }
           }
       }
   }
   
   // Trier les routes par priorité (tri à bulles simple)
   for (int i = 0; i < count - 1; i++) {
       for (int j = 0; j < count - i - 1; j++) {
           if (blockingPriorities[j] < blockingPriorities[j+1]) {
               // Swap routes
               int tempRoute = routesToBlock[j];
               routesToBlock[j] = routesToBlock[j+1];
               routesToBlock[j+1] = tempRoute;
               
               // Swap priorities
               int tempPriority = blockingPriorities[j];
               blockingPriorities[j] = blockingPriorities[j+1];
               blockingPriorities[j+1] = tempPriority;
           }
       }
   }
   
   return count;
}

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

// Estime le score actuel ou potentiel de l'adversaire
int estimateOpponentScore(GameState* state) {
   int score = 0;
   
   // Points pour les routes prises par l'adversaire
   for (int i = 0; i < state->nbTracks; i++) {
       if (state->routes[i].owner == 2) {
           int length = state->routes[i].length;
           
           // Table de correspondance longueur -> points
           int pointsByLength[] = {0, 1, 2, 4, 7, 10, 15};
           if (length > 0 && length < 7) {
               score += pointsByLength[length];
           }
       }
   }
   
   // Estimation des points d'objectifs
   // En moyenne, 5-8 points par objectif complété, avec ~60% complétés
   score += state->opponentObjectiveCount * 6; 
   
   // Tenir compte de notre connaissance des objectifs probables
   for (int i = 0; i < state->nbCities && i < MAX_CITIES; i++) {
       for (int j = i+1; j < state->nbCities && j < MAX_CITIES; j++) {
           // Si nous avons identifié cet objectif comme probable
           if (opponentCitiesOfInterest[i] > 8 && opponentCitiesOfInterest[j] > 8) {
               // Vérifier si l'adversaire l'a complété
               int path[MAX_CITIES];
               int pathLength = 0;
               findShortestPath(state, i, j, path, &pathLength);
               
               bool isCompleted = true;
               for (int k = 0; k < pathLength - 1; k++) {
                   int cityA = path[k];
                   int cityB = path[k+1];
                   
                   bool hasRoute = false;
                   for (int r = 0; r < state->nbTracks; r++) {
                       if (((state->routes[r].from == cityA && state->routes[r].to == cityB) ||
                            (state->routes[r].from == cityB && state->routes[r].to == cityA)) &&
                           state->routes[r].owner == 2) {
                           hasRoute = true;
                           break;
                       }
                   }
                   
                   if (!hasRoute) {
                       isCompleted = false;
                       break;
                   }
               }
               
               if (isCompleted) {
                   // Ajouter points d'objectif estimés (8-12 points selon distance)
                   int distance = 0;
                   for (int k = 0; k < pathLength - 1; k++) {
                       int cityA = path[k];
                       int cityB = path[k+1];
                       
                       for (int r = 0; r < state->nbTracks; r++) {
                           if ((state->routes[r].from == cityA && state->routes[r].to == cityB) ||
                               (state->routes[r].from == cityB && state->routes[r].to == cityA)) {
                               distance += state->routes[r].length;
                               break;
                           }
                       }
                   }
                   
                   int objectivePoints = 8;
                   if (distance >= 10) objectivePoints = 12;
                   else if (distance >= 7) objectivePoints = 10;
                   
                   score += objectivePoints;
               }
           }
       }
   }
   
   return score;
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
               CardColor color = determineOptimalColor(state, j);
               int nbLoco;
               
               if (color != NONE && canClaimRoute(state, cityA, cityB, color, &nbLoco)) {
                   // On peut prendre cette route!
                   moveData->action = CLAIM_ROUTE;
                   moveData->claimRoute.from = cityA;
                   moveData->claimRoute.to = cityB;
                   moveData->claimRoute.color = color;
                   moveData->claimRoute.nbLocomotives = nbLoco;
                   
                   printf("OBJECTIF CRITIQUE: Prendre route %d->%d pour terminer objectif %d\n", 
                          cityA, cityB, bestObjective+1);
                   return true;
               }
           }
       }
   }
   
   return false;
}

// Planifie les prochaines routes à prendre
void planNextRoutes(GameState* state, int* routesPlan, int count) {
   // Initialiser le plan
   for (int i = 0; i < count; i++) {
       routesPlan[i] = -1;
   }
   
   // 1. Priorité aux objectifs
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
   
   // 2. Si plan incomplet, ajouter routes stratégiques
   if (count > 0) {
       int strategicRoutes[MAX_ROUTES];
       int routeCount = 0;
       identifyAndPrioritizeBottlenecks(state, strategicRoutes, &routeCount);
       
       for (int i = 0; i < routeCount && count > 0; i++) {
           // Vérifier que cette route n'est pas déjà dans le plan
           bool alreadyPlanned = false;
           for (int p = 0; p < count; p++) {
               if (routesPlan[p] == strategicRoutes[i]) {
                   alreadyPlanned = true;
                   break;
               }
           }
           
           if (!alreadyPlanned) {
               routesPlan[--count] = strategicRoutes[i];
           }
       }
   }
}

// Optimisé de Dijkstra avec cache pour le pathfinding
int findShortestPath(GameState* state, int start, int end, int* path, int* pathLength) {
   if (!state || !path || !pathLength || start < 0 || start >= state->nbCities || 
       end < 0 || end >= state->nbCities) {
       return -1;
   }
   
   // Vérifier le cache d'abord
   for (int i = 0; i < cacheEntries; i++) {
       if (pathCache[i].from == start && pathCache[i].to == end) {
           // Vérifier si le cache est à jour
           if (pathCache[i].timestamp == cacheTimestamp) {
               // Copier les résultats du cache
               *pathLength = pathCache[i].pathLength;
               for (int j = 0; j < *pathLength; j++) {
                   path[j] = pathCache[i].path[j];
               }
               return pathCache[i].distance;
           }
       }
   }
   
   // Si non trouvé dans le cache, calculer avec Dijkstra
   int dist[MAX_CITIES];
   int prev[MAX_CITIES];
   int unvisited[MAX_CITIES];
   
   for (int i = 0; i < state->nbCities; i++) {
       dist[i] = INT_MAX;
       prev[i] = -1;
       unvisited[i] = 1;  // 1 = non visité
   }
   
   dist[start] = 0;
   
   for (int count = 0; count < state->nbCities; count++) {
       int u = -1;
       int minDist = INT_MAX;
       
       for (int i = 0; i < state->nbCities; i++) {
           if (unvisited[i] && dist[i] < minDist) {
               minDist = dist[i];
               u = i;
           }
       }
       
       if (u == -1 || dist[u] == INT_MAX) {
           break;
       }
       
       unvisited[u] = 0;
       
       if (u == end) {
           break;
       }
       
       for (int i = 0; i < state->nbTracks; i++) {
           if (state->routes[i].owner == 2) {
               continue;
           }
           
           int from = state->routes[i].from;
           int to = state->routes[i].to;
           int length = state->routes[i].length;
           
           if (from == u || to == u) {
               int v = (from == u) ? to : from;
               int newDist = dist[u] + length;
               
               if (newDist < dist[v]) {
                   dist[v] = newDist;
                   prev[v] = u;
               }
           }
       }
   }
   
   if (prev[end] == -1 && start != end) {
       return -1;
   }
   
   int tempPath[MAX_CITIES];
   int tempIndex = 0;
   int current = end;
   
   while (current != -1 && tempIndex < MAX_CITIES) {
       tempPath[tempIndex++] = current;
       if (current == start) break;
       current = prev[current];
   }
   
   *pathLength = tempIndex;
   for (int i = 0; i < tempIndex; i++) {
       path[i] = tempPath[tempIndex - 1 - i];
   }
   
   // Ajouter au cache
   if (dist[end] >= 0 && *pathLength > 0) {
       int cacheIndex = cacheEntries % PATH_CACHE_SIZE;
       pathCache[cacheIndex].from = start;
       pathCache[cacheIndex].to = end;
       pathCache[cacheIndex].pathLength = *pathLength;
       pathCache[cacheIndex].distance = dist[end];
       pathCache[cacheIndex].timestamp = cacheTimestamp;
       
       for (int i = 0; i < *pathLength && i < MAX_CITIES; i++) {
           pathCache[cacheIndex].path[i] = path[i];
       }
       
       if (cacheEntries < PATH_CACHE_SIZE) {
           cacheEntries++;
       }
   }
   
   return dist[end];
}

// Vérifie si une route fait partie d'un chemin
int isRouteInPath(int from, int to, int* path, int pathLength) {
   for (int i = 0; i < pathLength - 1; i++) {
       if ((path[i] == from && path[i+1] == to) ||
           (path[i] == to && path[i+1] == from)) {
           return 1;
       }
   }
   return 0;
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
   
   return baseScore + objectiveBonus - wagonPenalty;
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
       int distance = findShortestPath(state, from, to, path, &pathLength);
       
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

// Fonction principale de stratégie optimisée
int superAdvancedStrategy(GameState* state, MoveData* moveData) {
   printf("Stratégie avancée optimisée en cours d'exécution\n");
   
   // Variables de suivi stratégique
   static int consecutiveDraws = 0;
   
   // 1. Analyse de l'état du jeu et détermination de la phase
   int phase = determineGamePhase(state);
   printf("Phase de jeu actuelle: %d\n", phase);
   
   // Incrémenter compteur de tours
   state->turnCount++;
   
   // 2. Identifier le profil de l'adversaire
   if (state->turnCount % 5 == 0) {
       currentOpponentProfile = identifyOpponentProfile(state);
       printf("Profil adversaire mis à jour: %d\n", currentOpponentProfile);
   }
   
   // 3. Analyse des objectifs
   int completedObjectives = 0;
   int incompleteObjectives = 0;
   int totalObjectiveValue = 0;
   int incompleteObjectiveValue = 0;
   
   for (int i = 0; i < state->nbObjectives; i++) {
       if (isObjectiveCompleted(state, state->objectives[i])) {
           completedObjectives++;
           totalObjectiveValue += state->objectives[i].score;
       } else {
           incompleteObjectives++;
           totalObjectiveValue += state->objectives[i].score;
           incompleteObjectiveValue += state->objectives[i].score;
       }
   }
   
   printf("Objectifs: %d complétés, %d incomplets, valeur totale: %d, valeur restante: %d\n",
         completedObjectives, incompleteObjectives, totalObjectiveValue, incompleteObjectiveValue);
   
   // 4. Statistiques sur cartes disponibles
   int totalCards = 0;
   int maxSameColorCards = 0;
   int colorWithMostCards = 0;
   for (int i = 1; i < 10; i++) {
       totalCards += state->nbCardsByColor[i];
       if (state->nbCardsByColor[i] > maxSameColorCards) {
           maxSameColorCards = state->nbCardsByColor[i];
           colorWithMostCards = i;
       }
   }
   
   // 5. Déterminer la priorité stratégique
   enum StrategicPriority { COMPLETE_OBJECTIVES, BLOCK_OPPONENT, BUILD_NETWORK, DRAW_CARDS };
   enum StrategicPriority priority = COMPLETE_OBJECTIVES;  // Par défaut
   
   // 5.1 Cas spécial: Premier tour - piocher des objectifs
   if (state->nbObjectives == 0) {
       moveData->action = DRAW_OBJECTIVES;
       printf("Priorité: PIOCHER DES OBJECTIFS (premier tour)\n");
       return 1;
   }
   
   // 5.2 Stratégie d'accumulation en début de partie
   if (phase == PHASE_EARLY && state->turnCount < 6 && totalCards < 8) {
       priority = DRAW_CARDS;
       printf("Priorité: ACCUMULER DES CARTES (phase initiale, seulement %d cartes)\n", totalCards);
   }
   // 5.3 Fin de partie avec objectifs incomplets - priorité absolue
   else if ((phase == PHASE_LATE || phase == PHASE_FINAL) && incompleteObjectives > 0) {
       priority = COMPLETE_OBJECTIVES;
       printf("URGENCE FIN DE PARTIE: Prioriser complétion des %d objectifs incomplets!\n", 
             incompleteObjectives);
   }
   // 5.4 Dernier tour - maximiser points
   else if (state->lastTurn) {
       printf("DERNIER TOUR: Utiliser nos ressources restantes!\n");
       priority = BUILD_NETWORK;
   }
   // 5.5 Analyse des objectifs critiques en fin de partie
   else if ((phase == PHASE_LATE || phase == PHASE_FINAL) && incompleteObjectives > 0) {
       // Vérifier si objectif proche de complétion
       for (int i = 0; i < state->nbObjectives; i++) {
           if (!isObjectiveCompleted(state, state->objectives[i])) {
               int remainingRoutes = countRemainingRoutesForObjective(state, i);
               
               if (remainingRoutes >= 0 && remainingRoutes <= 2) {
                   priority = COMPLETE_OBJECTIVES;
                   printf("URGENCE: Objectif %d proche de la complétion! (reste %d routes)\n", 
                         i+1, remainingRoutes);
                   break;
               }
           }
       }
   }
   // 5.6 Objectifs à haute valeur - priorité importante
   else if (incompleteObjectiveValue > 15 && phase >= PHASE_MIDDLE) {
       priority = COMPLETE_OBJECTIVES;
       printf("PRIORITÉ CRITIQUE: Objectifs incomplets de grande valeur (%d points)\n", 
              incompleteObjectiveValue);
   }
   // 5.7 Tous objectifs complétés - piocher des nouveaux ou maximiser points
   else if (incompleteObjectives == 0) {
       if (state->nbObjectives < 3 && phase < PHASE_LATE) {
           moveData->action = DRAW_OBJECTIVES;
           printf("Priorité: PIOCHER DES OBJECTIFS (tous complétés)\n");
           return 1;
       } else {
           priority = BUILD_NETWORK;
           printf("Tous objectifs complétés: Prioriser les routes longues\n");
       }
   }
   // 5.8 Autres cas - analyse plus fine
   else {
       float completionRate = (float)completedObjectives / state->nbObjectives;
       
       if (completionRate > 0.7 && state->nbObjectives <= 3 && phase != PHASE_FINAL) {
           moveData->action = DRAW_OBJECTIVES;
           printf("Priorité: PIOCHER DES OBJECTIFS (bon taux de complétion: %.2f)\n", completionRate);
           return 1;
       }
       else if (completionRate < 0.3 && state->wagonsLeft < 30) {
           priority = COMPLETE_OBJECTIVES;
           printf("Priorité: COMPLÉTER OBJECTIFS (faible taux de complétion: %.2f)\n", completionRate);
       }
       else {
           // Comparer scores estimés
           int ourEstimatedScore = calculateScore(state);
           int estimatedOpponentScore = estimateOpponentScore(state);
           
           printf("Score estimé: Nous = %d, Adversaire = %d\n", ourEstimatedScore, estimatedOpponentScore);
           
           if (ourEstimatedScore < estimatedOpponentScore && phase != PHASE_EARLY) {
               priority = BUILD_NETWORK;
               printf("Priorité: CONSTRUIRE RÉSEAU (nous sommes en retard)\n");
           }
           else if (incompleteObjectiveValue > 20) {
               priority = COMPLETE_OBJECTIVES;
               printf("Priorité: COMPLÉTER OBJECTIFS (valeur élevée: %d)\n", incompleteObjectiveValue);
           }
           else {
               // Équilibrer entre objectifs et routes
               if (maxSameColorCards >= 4) {
                   priority = BUILD_NETWORK;
                   printf("Priorité: CONSTRUIRE RÉSEAU (beaucoup de cartes %s: %d)\n", 
                         (colorWithMostCards < 10) ? (const char*[]){
                             "None", "Purple", "White", "Blue", "Yellow", 
                             "Orange", "Black", "Red", "Green", "Locomotive"
                         }[colorWithMostCards] : "Unknown", 
                         maxSameColorCards);
               } else if (state->turnCount % 3 == 0) {
                   priority = BUILD_NETWORK;
                   printf("Priorité: CONSTRUIRE RÉSEAU (alternance)\n");
               } else {
                  priority = COMPLETE_OBJECTIVES;
                  printf("Priorité: COMPLÉTER OBJECTIFS (alternance)\n");
              }
          }
       }
   }
   
   // 6. Trouver les routes possibles
   int possibleRoutes[MAX_ROUTES] = {-1};
   CardColor possibleColors[MAX_ROUTES] = {NONE};
   int possibleLocomotives[MAX_ROUTES] = {0};
   
   int numPossibleRoutes = findPossibleRoutes(state, possibleRoutes, possibleColors, possibleLocomotives);
   printf("Trouvé %d routes possibles à prendre\n", numPossibleRoutes);
   
   // 7. Forcer prise de route après trop de pioches consécutives
   if (consecutiveDraws >= 4 && numPossibleRoutes > 0) {
       printf("FORCE MAJEURE: Trop de pioches consécutives (%d), forcer la prise d'une route\n", consecutiveDraws);
       priority = BUILD_NETWORK;
   }
   
   // 8. Prendre une décision selon la priorité
   switch (priority) {
       case COMPLETE_OBJECTIVES: {
           // 8.1 CAS SPECIAL: Fin de partie, force complétion d'objectif critique
           if (phase == PHASE_FINAL || phase == PHASE_LATE || state->lastTurn) {
               if (forceCompleteCriticalObjective(state, moveData)) {
                   consecutiveDraws = 0;
                   return 1;
               }
           }
           
           // 8.2 Focus sur la complétion des objectifs
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
                           // Convertir en int pour la comparaison
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
               if (length <= 2 && phase < PHASE_LATE && state->turnCount < 15 && consecutiveDraws < 4 && bestScore < 1000) {
                   printf("Route trop courte (longueur %d) en phase %d. Mieux vaut piocher.\n", 
                          length, phase);
                   priority = DRAW_CARDS;
                   break;
               }
               
               // Seuil minimal - ne pas prendre de routes trop tôt à moins qu'elles soient excellentes
               if (bestScore < 20 && phase == PHASE_EARLY && consecutiveDraws < 4 && !state->lastTurn) {
                   printf("Toutes les routes ont un score faible (%d), continuer à piocher\n", bestScore);
                   priority = DRAW_CARDS;
                   break;
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
                   
                   consecutiveDraws = 0;
                   return 1;
               } else {
                   priority = DRAW_CARDS;
                   printf("Priorité modifiée: PIOCHER DES CARTES (pas de route utile pour objectifs)\n");
               }
           } else {
               priority = DRAW_CARDS;
               printf("Priorité modifiée: PIOCHER DES CARTES (aucune route possible)\n");
           }
           break;
       }
       
       case BLOCK_OPPONENT: {
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
                   
                   consecutiveDraws = 0;
                   return 1;
               } else {
                   printf("Aucune route critique à bloquer, passer à la construction de réseau\n");
                   priority = BUILD_NETWORK;
                   
                   if (priority == BLOCK_OPPONENT) {
                       priority = DRAW_CARDS;
                       printf("Priorité modifiée: PIOCHER DES CARTES (pas de blocage possible)\n");
                   }
               }
           } else {
               priority = DRAW_CARDS;
               printf("Priorité modifiée: PIOCHER DES CARTES (aucune route possible)\n");
           }
           break;
       }
       
       case BUILD_NETWORK: {
           if (numPossibleRoutes > 0) {
               // Privilégier les routes longues pour maximiser les points
               int bestRouteIndex = -1;
               int bestScore = -1;
               
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
                   if (length <= 2 && phase < PHASE_LATE && state->turnCount < 15 && consecutiveDraws < 4) {
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
                           // Convertir en int pour la comparaison
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
               
               // Seuil minimal - ne pas prendre de routes trop tôt à moins qu'elles soient excellentes
               if (bestScore < 20 && phase == PHASE_EARLY && consecutiveDraws < 4 && !state->lastTurn) {
                   printf("Toutes les routes ont un score faible (%d), continuer à piocher\n", bestScore);
                   priority = DRAW_CARDS;
                   break;
               }
               
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
               
               consecutiveDraws = 0;
               return 1;
           } else {
               priority = DRAW_CARDS;
               printf("Priorité modifiée: PIOCHER DES CARTES (aucune route possible)\n");
           }
           break;
       }
       
       case DRAW_CARDS: {
           // Analyse des besoins en cartes pour les objectifs prioritaires
           int colorNeeds[10] = {0};
           bool needMoreCards = false;
           
           // Pour chaque objectif incomplet, analyser les routes nécessaires
           for (int i = 0; i < state->nbObjectives; i++) {
               if (isObjectiveCompleted(state, state->objectives[i])) continue;
               
               int objFrom = state->objectives[i].from;
               int objTo = state->objectives[i].to;
               
               // Trouver le chemin le plus court
               int path[MAX_CITIES];
               int pathLength = 0;
               if (findShortestPath(state, objFrom, objTo, path, &pathLength) > 0) {
                   // Pour chaque segment du chemin
                   for (int j = 0; j < pathLength - 1; j++) {
                       int cityA = path[j];
                       int cityB = path[j+1];
                       
                       // Trouver la route correspondante
                       for (int r = 0; r < state->nbTracks; r++) {
                           if (((state->routes[r].from == cityA && state->routes[r].to == cityB) ||
                                (state->routes[r].from == cityB && state->routes[r].to == cityA)) &&
                               state->routes[r].owner == 0) {
                               
                               CardColor routeColor = state->routes[r].color;
                               int length = state->routes[r].length;
                               
                               // Si c'est une route grise, toutes les couleurs sont possibles
                               if (routeColor == LOCOMOTIVE) {
                                   needMoreCards = true;
                               } else {
                                   // Calculer combien il nous manque de cartes de cette couleur
                                   int have = state->nbCardsByColor[routeColor];
                                   int needed = length;
                                   if (have < needed) {
                                       colorNeeds[routeColor] += (needed - have);
                                       needMoreCards = true;
                                   }
                               }
                               
                               break;
                           }
                       }
                   }
               }
           }
           
           // Si nous avons déjà assez de cartes, essayer de prendre une route
           if (!needMoreCards && numPossibleRoutes > 0) {
               printf("Nous avons déjà assez de cartes, essayer de prendre une route\n");
               priority = COMPLETE_OBJECTIVES;
               break;
           }
           
           // D'abord, vérifier s'il y a une locomotive visible
           for (int i = 0; i < 5; i++) {
               if (state->visibleCards[i] == LOCOMOTIVE) {
                   moveData->action = DRAW_CARD;
                   moveData->drawCard = LOCOMOTIVE;
                   printf("Décision: Piocher la locomotive visible\n");
                   consecutiveDraws++;
                   return 1;
               }
           }
           
           // Ensuite, chercher une carte visible qui correspond à nos besoins
           int bestCardIndex = -1;
           int bestCardValue = 0;
           
           for (int i = 0; i < 5; i++) {
               CardColor card = state->visibleCards[i];
               if (card == NONE) continue;
               
               int value = 0;
               
               // Valeur basée sur nos besoins
               if (colorNeeds[card] > 0) {
                   value += colorNeeds[card] * 10;
               }
               
               // Bonus si nous avons déjà des cartes de cette couleur
               if (state->nbCardsByColor[card] > 0) {
                   value += state->nbCardsByColor[card] * 5;
                   
                   // Bonus important si une carte nous permet de compléter une route
                   for (int r = 0; r < state->nbTracks; r++) {
                       if (state->routes[r].owner == 0) {
                           CardColor routeColor = state->routes[r].color;
                           int length = state->routes[r].length;
                           
                           // Pour les routes colorées correspondant à notre carte
                           if (routeColor == card) {
                               int cardsNeeded = length - state->nbCardsByColor[card];
                               // Si nous avons presque assez de cartes pour prendre cette route
                               if (cardsNeeded == 1) {
                                   value += length * 15;
                                   
                                   // Bonus supplémentaire pour les routes longues
                                   if (length >= 4) {
                                       value += length * 20;
                                   }
                               }
                           }
                       }
                   }
               }
               
               // Éviter d'avoir trop de cartes d'une seule couleur
               if (state->nbCardsByColor[card] > 8) {
                   value -= (state->nbCardsByColor[card] - 8) * 5;
               }
               
               if (value > bestCardValue) {
                   bestCardValue = value;
                   bestCardIndex = i;
               }
           }
           
           // Si nous avons trouvé une bonne carte visible, la piocher
           if (bestCardIndex >= 0 && bestCardValue > 5) {
               moveData->action = DRAW_CARD;
               moveData->drawCard = state->visibleCards[bestCardIndex];
               printf("Décision: Piocher la carte visible %d (valeur: %d)\n", 
                    moveData->drawCard, bestCardValue);
               consecutiveDraws++;
               return 1;
           }
           
           // Sinon, piocher une carte aveugle
           moveData->action = DRAW_BLIND_CARD;
           printf("Décision: Piocher une carte aveugle\n");
           consecutiveDraws++;
           return 1;
       }
       
       default:
           printf("Cas non géré, pioche par défaut\n");
           moveData->action = DRAW_BLIND_CARD;
           consecutiveDraws++;
           return 1;
   }
   
   // Si nous avons changé de priorité, exécuter à nouveau avec limitation de récursivité
   static int recursionDepth = 0;
   if (recursionDepth < 2) {
       recursionDepth++;
       int result = superAdvancedStrategy(state, moveData);
       recursionDepth--;
       return result;
   }
   
   // Fallback: piocher une carte aveugle
   printf("Décision par défaut: Piocher une carte aveugle\n");
   moveData->action = DRAW_BLIND_CARD;
   consecutiveDraws++;
   return 1;
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
       
       // Trouver chemin optimal
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

// Interfaces publiques pour les stratégies (toutes redirigent vers superAdvancedStrategy)
void chooseObjectivesStrategy(GameState* state, Objective* objectives, bool* chooseObjectives) {
   printf("Using advanced objective selection strategy\n");
   improvedObjectiveEvaluation(state, objectives, chooseObjectives);
}

int basicStrategy(GameState* state, MoveData* moveData) {
   return superAdvancedStrategy(state, moveData);
}

int dijkstraStrategy(GameState* state, MoveData* moveData) {
   return superAdvancedStrategy(state, moveData);
}

int advancedStrategy(GameState* state, MoveData* moveData) {
   return superAdvancedStrategy(state, moveData);
}