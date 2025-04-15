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
    
    printf("Found %d possible routes to claim\n", numPossibleRoutes);
    
    // Si nous pouvons prendre des routes
    if (numPossibleRoutes > 0) {
        // Trier les routes par leur utilité pour compléter les objectifs
        sortRoutesByUtility(state, possibleRoutes, possibleColors, possibleLocomotives, numPossibleRoutes);
        
        // Vérifier si nous avons des objectifs en cours
        if (state->nbObjectives == 0) {
            // Si nous n'avons pas d'objectifs, en piocher d'abord
            moveData->action = DRAW_OBJECTIVES;
            printf("Strategy decided: draw new objectives (we have none)\n");
            return 1;
        }
        
        // Nous allons presque toujours prendre une route si possible
        bool shouldClaimRoute = true;
        
        // Sauf dans certains cas spécifiques :
        
        // 1. Si nous sommes au tout début et avons très peu de cartes, piocher d'abord
        if (state->turnCount < 3 && state->nbCards < 6) {
            // Vérifier si la meilleure route a une utilité élevée
            int bestRouteUtility = evaluateRouteUtility(state, possibleRoutes[0]);
            
            // Si l'utilité est faible, mieux vaut piocher
            if (bestRouteUtility < 5) {
                shouldClaimRoute = false;
                printf("Early game - few cards and low utility route - drawing cards first\n");
            }
        }
        
        // 2. Si nous avons beaucoup d'objectifs non complétés et peu de cartes, piocher pour avoir plus d'options
        int completedObjectives = 0;
        for (int i = 0; i < state->nbObjectives; i++) {
            if (isObjectiveCompleted(state, state->objectives[i])) {
                completedObjectives++;
            }
        }
        
        int incompleteObjectives = state->nbObjectives - completedObjectives;
        
        // Si beaucoup d'objectifs incomplets et peu de cartes, privilégier la pioche
        if (incompleteObjectives > 2 && state->nbCards < 4) {
            shouldClaimRoute = false;
            printf("Many incomplete objectives (%d) and few cards - drawing first\n", incompleteObjectives);
        }
        
        // Si nous décidons de prendre une route
        if (shouldClaimRoute) {
            // Prendre la route la plus utile (première après le tri)
            int routeIndex = possibleRoutes[0];
            int from = state->routes[routeIndex].from;
            int to = state->routes[routeIndex].to;
            CardColor color = possibleColors[0];
            int nbLocomotives = possibleLocomotives[0];
            
            // Vérification que la couleur est valide pour cette route
            CardColor routeColor = state->routes[routeIndex].color;
            CardColor routeSecondColor = state->routes[routeIndex].secondColor;
            
            // Pour les routes grises (LOCOMOTIVE), toute couleur est valide
            // Pour les routes colorées, vérifier que la couleur choisie correspond
            if (routeColor != LOCOMOTIVE && routeSecondColor != LOCOMOTIVE && 
                color != routeColor && color != routeSecondColor && color != LOCOMOTIVE) {
                printf("WARNING: Color mismatch for route %d. Expected %d or %d, got %d\n", 
                      routeIndex, routeColor, routeSecondColor, color);
                // Corriger la couleur si nécessaire
                if (state->nbCardsByColor[routeColor] >= state->routes[routeIndex].length - nbLocomotives) {
                    color = routeColor;
                } else if (routeSecondColor != NONE && 
                         state->nbCardsByColor[routeSecondColor] >= state->routes[routeIndex].length - nbLocomotives) {
                    color = routeSecondColor;
                }
            }
            
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
        }
    }
    // Si nous n'avons pas assez d'objectifs, en piocher
    if (state->nbObjectives < 3 && !state->lastTurn) {
        moveData->action = DRAW_OBJECTIVES;
        printf("Strategy decided: draw new objectives (we have only %d)\n", state->nbObjectives);
        return 1;
    }
    
    // Analyser les couleurs nécessaires pour nos objectifs
    int colorNeeds[10] = {0};  // Combien nous avons besoin de chaque couleur
    
    // Pour chaque objectif non complété
    for (int i = 0; i < state->nbObjectives; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            int objFrom = state->objectives[i].from;
            int objTo = state->objectives[i].to;
            
            // Trouver le chemin le plus court
            int path[MAX_CITIES];
            int pathLength = 0;
            
            if (findShortestPath(state, objFrom, objTo, path, &pathLength) >= 0) {
                // Analyser les routes dans ce chemin
                for (int j = 0; j < pathLength - 1; j++) {
                    int from = path[j];
                    int to = path[j+1];
                    
                    // Trouver la route correspondante
                    for (int k = 0; k < state->nbTracks; k++) {
                        if ((state->routes[k].from == from && state->routes[k].to == to) ||
                            (state->routes[k].from == to && state->routes[k].to == from)) {
                            
                            // Si la route n'est pas déjà prise
                            if (state->routes[k].owner == 0) {
                                // Ajouter la couleur à nos besoins
                                CardColor routeColor = state->routes[k].color;
                                if (routeColor != NONE && routeColor != LOCOMOTIVE) {
                                    colorNeeds[routeColor]++;
                                }
                                
                                CardColor routeSecondColor = state->routes[k].secondColor;
                                if (routeSecondColor != NONE && routeSecondColor != LOCOMOTIVE) {
                                    colorNeeds[routeSecondColor]++;
                                }
                                
                                // Si c'est une route grise, les locomotives sont utiles
                                if (routeColor == LOCOMOTIVE) {
                                    colorNeeds[LOCOMOTIVE]++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Piocher des cartes en fonction des besoins pour les objectifs
    
    // Priorité 1: Locomotive visible (toujours utile)
    for (int i = 0; i < 5; i++) {
        if (state->visibleCards[i] == LOCOMOTIVE) {
            moveData->action = DRAW_CARD;
            moveData->drawCard = LOCOMOTIVE;
            printf("Strategy decided: draw visible locomotive card\n");
            return 1;
        }
    }
    
    // Priorité 2: Carte de couleur dont nous avons besoin pour les objectifs
    CardColor bestVisibleCard = NONE;
    int bestScore = -1;
    
    for (int i = 0; i < 5; i++) {
        CardColor card = state->visibleCards[i];
        if (card != NONE && card != LOCOMOTIVE) {
            // Score basé sur combien nous avons besoin de cette couleur et combien nous en avons déjà
            int score = colorNeeds[card] * 2;
            
            // Bonus si nous avons déjà des cartes de cette couleur (pour compléter un set)
            if (state->nbCardsByColor[card] > 0) {
                score += state->nbCardsByColor[card];
            }
            
            if (score > bestScore) {
                bestScore = score;
                bestVisibleCard = card;
            }
        }
    }
    
    // Si on a trouvé une bonne carte visible
    if (bestVisibleCard != NONE && bestScore > 0) {
        moveData->action = DRAW_CARD;
        moveData->drawCard = bestVisibleCard;
        printf("Strategy decided: draw visible %s card (utility score: %d)\n", 
              (bestVisibleCard < 10) ? (const char*[]){"None", "Purple", "White", "Blue", "Yellow", 
                                                     "Orange", "Black", "Red", "Green", "Locomotive"}[bestVisibleCard] : "Unknown", 
              bestScore);
        return 1;
    }
    
    // Si aucune carte visible n'est particulièrement utile,
    // décider entre carte visible quelconque ou carte aveugle
    
    // Vérifier s'il y a des cartes visibles non-locomotives
    CardColor visibleCard = NONE;
    for (int i = 0; i < 5; i++) {
        if (state->visibleCards[i] != NONE && state->visibleCards[i] != LOCOMOTIVE) {
            visibleCard = state->visibleCards[i];
            break;
        }
    }
    
    // Si on a trouvé une carte visible quelconque
    if (visibleCard != NONE) {
        // Pioche aveugle si aucune couleur visible n'est utile et qu'on a beaucoup de besoins variés
        int totalNeeds = 0;
        for (int i = 1; i < 10; i++) {
            totalNeeds += colorNeeds[i];
        }
        
        if (totalNeeds > 5 && colorNeeds[visibleCard] == 0) {
            moveData->action = DRAW_BLIND_CARD;
            printf("Strategy decided: draw blind card (hoping for a needed color)\n");
        } else {
            moveData->action = DRAW_CARD;
            moveData->drawCard = visibleCard;
            printf("Strategy decided: draw visible %s card\n", 
                  (visibleCard < 10) ? (const char*[]){"None", "Purple", "White", "Blue", "Yellow", 
                                                     "Orange", "Black", "Red", "Green", "Locomotive"}[visibleCard] : "Unknown");
        }
    } else {
        // Pas de carte visible, pioche aveugle
        moveData->action = DRAW_BLIND_CARD;
        printf("Strategy decided: draw blind card (no visible cards)\n");
    }
    
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







void chooseObjectivesStrategy(GameState* state, Objective* objectives, bool* chooseObjectives) {
    printf("Evaluating objectives to choose:\n");
    
    // Initialiser tous les choix à false
    for (int i = 0; i < 3; i++) {
        chooseObjectives[i] = false;
    }
    
    // Calculer un score pour chaque objectif
    int scores[3];
    
    for (int i = 0; i < 3; i++) {
        int from = objectives[i].from;
        int to = objectives[i].to;
        int value = objectives[i].score;
        
        // Trouver le chemin le plus court pour cet objectif
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findShortestPath(state, from, to, path, &pathLength);
        
        printf("Objective %d: From %d to %d, score %d\n", i+1, from, to, value);
        
        if (distance < 0) {
            // Pas de chemin trouvé, objectif impossible à réaliser
            scores[i] = -1000;
            printf("  - No path found, impossible to complete\n");
            continue;
        }
        
        // Le score est basé sur le rapport valeur/distance
        // Plus ce rapport est élevé, plus l'objectif est intéressant
        float ratio = (float)value / (float)distance;
        
        // Bonus pour les objectifs qui partagent des routes avec des objectifs existants
        int sharedRouteBonus = 0;
        
        // Vérifier si cet objectif partage des routes avec des objectifs existants
        for (int j = 0; j < state->nbObjectives; j++) {
            int existingFrom = state->objectives[j].from;
            int existingTo = state->objectives[j].to;
            
            // Trouver le chemin pour cet objectif existant
            int existingPath[MAX_CITIES];
            int existingPathLength = 0;
            
            if (findShortestPath(state, existingFrom, existingTo, existingPath, &existingPathLength) >= 0) {
                // Compter combien de routes sont partagées
                int sharedRoutes = 0;
                
                for (int p1 = 0; p1 < pathLength - 1; p1++) {
                    for (int p2 = 0; p2 < existingPathLength - 1; p2++) {
                        // Si les segments de route sont identiques
                        if ((path[p1] == existingPath[p2] && path[p1+1] == existingPath[p2+1]) ||
                            (path[p1] == existingPath[p2+1] && path[p1+1] == existingPath[p2])) {
                            sharedRoutes++;
                        }
                    }
                }
                
                sharedRouteBonus += sharedRoutes * 10;
            }
        }
        
        // Bonus pour les objectifs qui se complètent entre eux
        int complementaryBonus = 0;
        
        for (int j = 0; j < 3; j++) {
            if (i != j) {
                int otherFrom = objectives[j].from;
                int otherTo = objectives[j].to;
                
                // Trouver le chemin pour cet autre objectif
                int otherPath[MAX_CITIES];
                int otherPathLength = 0;
                
                if (findShortestPath(state, otherFrom, otherTo, otherPath, &otherPathLength) >= 0) {
                    // Compter combien de routes sont partagées
                    int sharedRoutes = 0;
                    
                    for (int p1 = 0; p1 < pathLength - 1; p1++) {
                        for (int p2 = 0; p2 < otherPathLength - 1; p2++) {
                            // Si les segments de route sont identiques
                            if ((path[p1] == otherPath[p2] && path[p1+1] == otherPath[p2+1]) ||
                                (path[p1] == otherPath[p2+1] && path[p1+1] == otherPath[p2])) {
                                sharedRoutes++;
                            }
                        }
                    }
                    
                    complementaryBonus += sharedRoutes * 10;
                }
            }
        }
        
        // Malus pour les objectifs trop longs (plus difficiles à compléter)
        int lengthPenalty = 0;
        if (distance > 15) {
            lengthPenalty = (distance - 15) * 5;
        }
        
        // Score final pour cet objectif
        scores[i] = (int)(ratio * 100) + sharedRouteBonus + complementaryBonus - lengthPenalty;
        
        printf("  - Path length: %d, Ratio: %.2f, Shared bonus: %d, Complementary: %d, Penalty: %d\n",
               distance, ratio, sharedRouteBonus, complementaryBonus, lengthPenalty);
        printf("  - Final score: %d\n", scores[i]);
    }
    
    // Déterminer combien d'objectifs prendre (minimum 1, maximum 3)
    // Variables pour le tri
    int sortedIndices[3] = {0, 1, 2};
    
    // Tri à bulles simple pour classer les objectifs par score
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2 - i; j++) {
            if (scores[sortedIndices[j]] < scores[sortedIndices[j+1]]) {
                int temp = sortedIndices[j];
                sortedIndices[j] = sortedIndices[j+1];
                sortedIndices[j+1] = temp;
            }
        }
    }
    
    // Décider combien d'objectifs prendre
    int numToChoose = 0;
    
    // Premier objectif toujours pris s'il est réalisable
    if (scores[sortedIndices[0]] > 0) {
        chooseObjectives[sortedIndices[0]] = true;
        numToChoose++;
    }
    
    // Deuxième objectif pris s'il a un score décent
    if (scores[sortedIndices[1]] > 50) {
        chooseObjectives[sortedIndices[1]] = true;
        numToChoose++;
    }
    
    // Troisième objectif pris s'il a un bon score ou si nous sommes en début de partie
    if (scores[sortedIndices[2]] > 80 || (state->nbObjectives == 0 && scores[sortedIndices[2]] > 0)) {
        chooseObjectives[sortedIndices[2]] = true;
        numToChoose++;
    }
    
    // S'assurer qu'on prend au moins un objectif
    if (numToChoose == 0) {
        // Prendre le moins mauvais
        chooseObjectives[sortedIndices[0]] = true;
        numToChoose = 1;
    }
    
    printf("Choosing %d objectives: ", numToChoose);
    for (int i = 0; i < 3; i++) {
        if (chooseObjectives[i]) {
            printf("%d ", i+1);
        }
    }
    printf("\n");
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


int findShortestPath(GameState* state, int start, int end, int* path, int* pathLength) {
    // Vérification des paramètres
    if (!state || !path || !pathLength || start < 0 || start >= state->nbCities || 
        end < 0 || end >= state->nbCities) {
        printf("ERROR: Invalid parameters in findShortestPath\n");
        return -1;
    }
    
    // Distances depuis le départ
    int dist[MAX_CITIES];
    // Précédents dans le chemin
    int prev[MAX_CITIES];
    // Nœuds non visités
    int unvisited[MAX_CITIES];
    
    // Initialisation
    for (int i = 0; i < state->nbCities; i++) {
        dist[i] = INT_MAX;
        prev[i] = -1;
        unvisited[i] = 1;  // 1 = non visité
    }
    
    dist[start] = 0;  // Distance de la source à elle-même = 0
    
    // Implémentation de l'algorithme de Dijkstra
    for (int count = 0; count < state->nbCities; count++) {
        // Trouver le nœud non visité avec la plus petite distance
        int u = -1;
        int minDist = INT_MAX;
        
        for (int i = 0; i < state->nbCities; i++) {
            if (unvisited[i] && dist[i] < minDist) {
                minDist = dist[i];
                u = i;
            }
        }
        
        // Si nous ne trouvons pas de nœud accessible, arrêter
        if (u == -1 || dist[u] == INT_MAX) {
            break;
        }
        
        // Marquer comme visité
        unvisited[u] = 0;
        
        // Si nous avons atteint la destination, on peut s'arrêter
        if (u == end) {
            break;
        }
        
        // Parcourir toutes les routes
        for (int i = 0; i < state->nbTracks; i++) {
            // Ne considérer que les routes non prises par l'adversaire
            if (state->routes[i].owner == 2) {
                continue;
            }
            
            int from = state->routes[i].from;
            int to = state->routes[i].to;
            int length = state->routes[i].length;
            
            // Si cette route part de u
            if (from == u || to == u) {
                int v = (from == u) ? to : from;  // Autre extrémité
                
                // Calculer nouvelle distance
                int newDist = dist[u] + length;
                
                // Si c'est mieux que la distance actuelle
                if (newDist < dist[v]) {
                    dist[v] = newDist;
                    prev[v] = u;
                }
            }
        }
    }
    
    // Vérifier si un chemin a été trouvé
    if (prev[end] == -1 && start != end) {
        printf("No path found from %d to %d\n", start, end);
        return -1;
    }
    
    // Reconstruire le chemin (en ordre inverse)
    int tempPath[MAX_CITIES];
    int tempIndex = 0;
    int current = end;
    
    while (current != -1 && tempIndex < MAX_CITIES) {
        tempPath[tempIndex++] = current;
        if (current == start) break;
        current = prev[current];
    }
    
    // Inverser le chemin pour qu'il soit dans le bon ordre (start -> end)
    *pathLength = tempIndex;
    for (int i = 0; i < tempIndex; i++) {
        path[i] = tempPath[tempIndex - 1 - i];
    }
    
    // Retourner la distance totale
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

// Évalue l'utilité d'une route par rapport aux objectifs
int evaluateRouteUtility(GameState* state, int routeIndex) {
    int from = state->routes[routeIndex].from;
    int to = state->routes[routeIndex].to;
    int length = state->routes[routeIndex].length;
    
    // Score de base: les points que la route rapporte
    // Table de correspondance longueur -> points
    int pointsByLength[] = {0, 1, 2, 4, 7, 10, 15};
    int baseScore = (length >= 0 && length <= 6) ? pointsByLength[length] : 0;
    
    // Bonus pour les routes qui font partie du chemin le plus court pour un objectif
    int objectiveBonus = 0;
    
    for (int i = 0; i < state->nbObjectives; i++) {
        // Si l'objectif est déjà complété, on ignore
        if (isObjectiveCompleted(state, state->objectives[i])) {
            continue;
        }
        
        int objFrom = state->objectives[i].from;
        int objTo = state->objectives[i].to;
        int objScore = state->objectives[i].score;
        
        // Trouver le chemin le plus court pour cet objectif
        int path[MAX_CITIES];
        int pathLength = 0;
        
        if (findShortestPath(state, objFrom, objTo, path, &pathLength) >= 0) {
            // Vérifier si la route fait partie du chemin
            if (isRouteInPath(from, to, path, pathLength)) {
                // Bonus basé sur le score de l'objectif et la rareté du chemin
                objectiveBonus += objScore * 2;
                
                // Bonus supplémentaire si c'est une route critique (peu d'alternatives)
                // Plus la route est longue, plus elle est difficile à remplacer
                objectiveBonus += length * 3;
            }
        }
    }
    
    // Pénalité pour utiliser des wagons quand il en reste peu
    int wagonPenalty = 0;
    if (state->wagonsLeft < 15) {
        wagonPenalty = length * (15 - state->wagonsLeft) / 2;
    }
    
    // Score final
    int finalScore = baseScore + objectiveBonus - wagonPenalty;
    
    return finalScore;
}

// Trie les routes possibles par ordre d'utilité décroissante
void sortRoutesByUtility(GameState* state, int* possibleRoutes, CardColor* possibleColors, 
    int* possibleLocomotives, int numPossibleRoutes) {
// Vérification pour éviter un dépassement de mémoire
if (numPossibleRoutes > MAX_ROUTES) {
printf("WARNING: Too many possible routes (%d), limiting to %d\n", 
numPossibleRoutes, MAX_ROUTES);
numPossibleRoutes = MAX_ROUTES;
}

// Calculer les scores d'utilité pour chaque route
int utilityScores[MAX_ROUTES];

for (int i = 0; i < numPossibleRoutes; i++) {
utilityScores[i] = evaluateRouteUtility(state, possibleRoutes[i]);
}

// Tri à bulles simple (pour un petit nombre de routes)
for (int i = 0; i < numPossibleRoutes - 1; i++) {
for (int j = 0; j < numPossibleRoutes - i - 1; j++) {
if (utilityScores[j] < utilityScores[j+1]) {
// Échanger les routes
int tempRoute = possibleRoutes[j];
possibleRoutes[j] = possibleRoutes[j+1];
possibleRoutes[j+1] = tempRoute;

// Échanger les couleurs
CardColor tempColor = possibleColors[j];
possibleColors[j] = possibleColors[j+1];
possibleColors[j+1] = tempColor;

// Échanger les nombres de locomotives
int tempLoco = possibleLocomotives[j];
possibleLocomotives[j] = possibleLocomotives[j+1];
possibleLocomotives[j+1] = tempLoco;

// Échanger les scores d'utilité
int tempScore = utilityScores[j];
utilityScores[j] = utilityScores[j+1];
utilityScores[j+1] = tempScore;
}
}
}

// Afficher les routes triées par utilité pour le débogage
printf("Routes sorted by utility:\n");
for (int i = 0; i < numPossibleRoutes; i++) {
int routeIndex = possibleRoutes[i];
printf("  %d. From %d to %d, utility: %d\n", 
i+1, state->routes[routeIndex].from, state->routes[routeIndex].to, utilityScores[i]);
}
}