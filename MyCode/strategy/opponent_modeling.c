/**
 * opponent_modeling.c
 * Modélisation et prédiction du comportement de l'adversaire - VERSION CORRIGÉE
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "strategy.h"
#include "../rules.h"      
#include "../gamestate.h"  


// Variables globales pour le modèle de l'adversaire
int opponentCitiesOfInterest[MAX_CITIES] = {0};
OpponentProfile currentOpponentProfile = OPPONENT_UNKNOWN;

// Identifie le profil stratégique de l'adversaire
OpponentProfile identifyOpponentProfile(GameState* state) {
    // Analyse des actions adverses pour déterminer son profil
    float routeRatio = 0.0;
    if (state->turnCount > 0) {
        routeRatio = (float)(state->nbTracks - state->nbClaimedRoutes - 
                    (state->opponentWagonsLeft * 5 / 45)) / state->turnCount;
    }
    
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
                    // CHANGEMENT: Utiliser findShortestPath pour modéliser les objectifs adverses
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
            
            // CHANGEMENT: Utiliser findShortestPath pour identifier les routes clés
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

// Met à jour le profil de l'adversaire
void updateOpponentProfile(GameState* state) {
    currentOpponentProfile = identifyOpponentProfile(state);
    
    switch (currentOpponentProfile) {
        case OPPONENT_AGGRESSIVE:
            printf("Profil adversaire: AGRESSIF - Prend beaucoup de routes courtes\n");
            break;
        case OPPONENT_HOARDER:
            printf("Profil adversaire: ACCUMULATEUR - Accumule des cartes\n");
            break;
        case OPPONENT_OBJECTIVE:
            printf("Profil adversaire: OBJECTIFS - Se concentre sur les objectifs\n");
            break;
        case OPPONENT_BLOCKER:
            printf("Profil adversaire: BLOQUEUR - Essaie de nous bloquer\n");
            break;
        default:
            printf("Profil adversaire: INCONNU\n");
            break;
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
                    
                    // CHANGEMENT: Utiliser findShortestPath pour vérifier la crédibilité
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
        // CHANGEMENT: Utiliser findShortestPath pour identifier les routes à bloquer
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
                // CHANGEMENT: Utiliser findShortestPath pour vérifier la complétion
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

// Analyse les patterns de comportement de l'adversaire
void analyzeOpponentPatterns(GameState* state) {
    if (!state) return;
    
    printf("=== ANALYSE DES PATTERNS ADVERSAIRES ===\n");
    
    // Analyser la vitesse de jeu
    if (state->turnCount > 5) {
        float routesPerTurn = 0;
        int opponentRoutes = 0;
        
        for (int i = 0; i < state->nbTracks; i++) {
            if (state->routes[i].owner == 2) {
                opponentRoutes++;
            }
        }
        
        routesPerTurn = (float)opponentRoutes / (state->turnCount / 2);
        
        printf("Adversaire - Routes par tour: %.2f\n", routesPerTurn);
        
        if (routesPerTurn > 0.8) {
            printf("  -> Stratégie AGGRESSIVE détectée\n");
        } else if (routesPerTurn < 0.3) {
            printf("  -> Stratégie d'ACCUMULATION détectée\n");
        } else {
            printf("  -> Stratégie ÉQUILIBRÉE détectée\n");
        }
    }
    
    // Analyser les préférences de longueur de routes
    int shortRoutes = 0, mediumRoutes = 0, longRoutes = 0;
    
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 2) {
            int length = state->routes[i].length;
            if (length <= 2) shortRoutes++;
            else if (length <= 4) mediumRoutes++;
            else longRoutes++;
        }
    }
    
    printf("Préférences de routes - Courtes: %d, Moyennes: %d, Longues: %d\n", 
           shortRoutes, mediumRoutes, longRoutes);
    
    // Analyser la connectivité du réseau adverse
    int connectedComponents = 0;
    bool visited[MAX_CITIES] = {false};
    
    for (int i = 0; i < state->nbCities; i++) {
        if (!visited[i]) {
            bool hasOpponentRoute = false;
            
            // Vérifier si cette ville fait partie du réseau adverse
            for (int j = 0; j < state->nbTracks; j++) {
                if ((state->routes[j].from == i || state->routes[j].to == i) && 
                    state->routes[j].owner == 2) {
                    hasOpponentRoute = true;
                    break;
                }
            }
            
            if (hasOpponentRoute) {
                connectedComponents++;
                // Marquer toutes les villes connectées comme visitées (BFS simple)
                // Cette implémentation est simplifiée pour l'exemple
                visited[i] = true;
            }
        }
    }
    
    printf("Composantes connectées du réseau adverse: %d\n", connectedComponents);
    
    if (connectedComponents > 3) {
        printf("  -> Réseau FRAGMENTÉ - possibles objectifs multiples\n");
    } else if (connectedComponents == 1) {
        printf("  -> Réseau UNIFIÉ - stratégie cohérente\n");
    }
    
    printf("==========================================\n");
}

// Détecte la stratégie principale de l'adversaire
void detectOpponentStrategy(GameState* state) {
    if (!state || state->turnCount < 3) return;
    
    printf("=== DÉTECTION DE STRATÉGIE ADVERSE ===\n");
    
    int totalOpponentRoutes = 0;
    int totalLength = 0;
    
    for (int i = 0; i < state->nbTracks; i++) {
        if (state->routes[i].owner == 2) {
            totalOpponentRoutes++;
            totalLength += state->routes[i].length;
        }
    }
    
    float avgLength = totalOpponentRoutes > 0 ? (float)totalLength / totalOpponentRoutes : 0;
    
    // Ratio cartes/routes
    float cardToRouteRatio = totalOpponentRoutes > 0 ? 
                            (float)state->opponentCardCount / totalOpponentRoutes : 
                            (float)state->opponentCardCount;
    
    printf("Statistiques adversaire:\n");
    printf("  Routes prises: %d\n", totalOpponentRoutes);
    printf("  Longueur moyenne: %.2f\n", avgLength);
    printf("  Ratio cartes/routes: %.2f\n", cardToRouteRatio);
    printf("  Objectifs estimés: %d\n", state->opponentObjectiveCount);
    
    // Détection de stratégie basée sur les métriques
    if (avgLength >= 4.0 && cardToRouteRatio < 3.0) {
        printf("STRATÉGIE DÉTECTÉE: ROUTES LONGUES - Vise les gros points\n");
        printf("  Contre-stratégie: Bloquer les routes longues disponibles\n");
    }
    else if (totalOpponentRoutes > state->turnCount / 3 && avgLength < 3.0) {
        printf("STRATÉGIE DÉTECTÉE: EXPANSION RAPIDE - Prend beaucoup de routes courtes\n");
        printf("  Contre-stratégie: Accélérer notre propre expansion\n");
    }
    else if (cardToRouteRatio > 5.0 && totalOpponentRoutes < 3) {
        printf("STRATÉGIE DÉTECTÉE: ACCUMULATION - Stocke des cartes\n");
        printf("  Contre-stratégie: Prendre les routes clés rapidement\n");
    }
    else if (state->opponentObjectiveCount > 4) {
        printf("STRATÉGIE DÉTECTÉE: OBJECTIFS MULTIPLES - Collectionne les objectifs\n");
        printf("  Contre-stratégie: Bloquer les connexions probables\n");
    }
    else {
        printf("STRATÉGIE DÉTECTÉE: ÉQUILIBRÉE - Pas de pattern clair\n");
        printf("  Contre-stratégie: Continuer notre stratégie principale\n");
    }
    
    // Recommandations tactiques
    printf("\nRecommandations tactiques:\n");
    
    if (currentOpponentProfile == OPPONENT_AGGRESSIVE) {
        printf("  - Prioriser les routes longues avant qu'il ne les bloque\n");
        printf("  - Éviter la compétition directe sur les routes courtes\n");
    }
    else if (currentOpponentProfile == OPPONENT_HOARDER) {
        printf("  - Prendre les routes critiques rapidement\n");
        printf("  - Ne pas laisser les bonnes opportunités\n");
    }
    else if (currentOpponentProfile == OPPONENT_BLOCKER) {
        printf("  - Avoir des plans de routes alternatives\n");
        printf("  - Prioriser les objectifs les moins évidents\n");
    }
    
    printf("=====================================\n");
}