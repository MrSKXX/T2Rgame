/**
 * pathfinding.c
 * Algorithmes de recherche de chemin et analyse de connectivité - VERSION CORRIGÉE
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include "strategy.h"
#include "../rules.h"      
#include "../gamestate.h"  


// Variables globales du cache
PathCacheEntry pathCache[PATH_CACHE_SIZE];
int cacheEntries = 0;
int cacheTimestamp = 0;

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

// CHANGEMENT MAJEUR: findSmartestPath maintenant privilégie fortement le chemin direct
// NOUVEAU findSmartestPath - Version simplifiée et fiable
// Remplacez la fonction existante dans pathfinding.c par celle-ci

int findSmartestPath(GameState* state, int start, int end, int* path, int* pathLength) {
    if (!state || !path || !pathLength || start < 0 || start >= state->nbCities || 
        end < 0 || end >= state->nbCities) {
        printf("ERREUR: Paramètres invalides dans findSmartestPath\n");
        return -1;
    }
    
    // ÉTAPE 1: Toujours commencer par le chemin direct
    int directDistance = findShortestPath(state, start, end, path, pathLength);
    
    if (directDistance <= 0) {
        printf("Aucun chemin direct trouvé de %d à %d\n", start, end);
        return directDistance;
    }
    
    printf("Chemin direct trouvé: %d -> %d, distance %d, %d segments\n", 
           start, end, directDistance, *pathLength - 1);
    
    // ÉTAPE 2: Vérifier si le chemin direct est déjà optimal
    int phase = determineGamePhase(state);
    
    // En début de partie OU si chemin court OU si peu de routes possédées : utiliser direct
    if (phase == PHASE_EARLY || directDistance <= 4 || state->nbClaimedRoutes < 3) {
        printf("Utilisation du chemin direct (phase=%d, distance=%d, routes=%d)\n", 
               phase, directDistance, state->nbClaimedRoutes);
        return directDistance;
    }
    
    // ÉTAPE 3: Calculer combien de routes nous possédons déjà sur le chemin direct
    int routesOwnedOnPath = 0;
    int totalRoutesOnPath = *pathLength - 1;
    
    for (int i = 0; i < *pathLength - 1; i++) {
        int cityA = path[i];
        int cityB = path[i + 1];
        
        for (int j = 0; j < state->nbTracks; j++) {
            if (((state->routes[j].from == cityA && state->routes[j].to == cityB) ||
                 (state->routes[j].from == cityB && state->routes[j].to == cityA)) &&
                state->routes[j].owner == 1) {
                routesOwnedOnPath++;
                break;
            }
        }
    }
    
    float ownedRatio = (totalRoutesOnPath > 0) ? (float)routesOwnedOnPath / totalRoutesOnPath : 0;
    
    printf("Chemin direct: %d/%d routes déjà possédées (%.0f%%)\n", 
           routesOwnedOnPath, totalRoutesOnPath, ownedRatio * 100);
    
    // ÉTAPE 4: Si nous possédons déjà 50%+ du chemin direct, l'utiliser
    if (ownedRatio >= 0.5) {
        printf("Chemin direct optimal (%.0f%% déjà possédé)\n", ownedRatio * 100);
        return directDistance;
    }
    
    // ÉTAPE 5: Chercher UN SEUL chemin alternatif via notre meilleur hub
    int bestHub = -1;
    int bestHubConnections = 0;
    
    // Trouver notre meilleur hub (ville avec le plus de connexions)
    for (int city = 0; city < state->nbCities; city++) {
        if (city == start || city == end) continue;
        
        int connections = 0;
        for (int i = 0; i < state->nbClaimedRoutes; i++) {
            int routeIndex = state->claimedRoutes[i];
            if (routeIndex >= 0 && routeIndex < state->nbTracks) {
                if (state->routes[routeIndex].from == city || 
                    state->routes[routeIndex].to == city) {
                    connections++;
                }
            }
        }
        
        if (connections > bestHubConnections && connections >= 2) {
            bestHubConnections = connections;
            bestHub = city;
        }
    }
    
    // ÉTAPE 6: Si nous avons un bon hub, tester un chemin via ce hub
    if (bestHub >= 0) {
        int path1[MAX_CITIES], path2[MAX_CITIES];
        int pathLength1 = 0, pathLength2 = 0;
        
        int dist1 = findShortestPath(state, start, bestHub, path1, &pathLength1);
        int dist2 = findShortestPath(state, bestHub, end, path2, &pathLength2);
        
        if (dist1 > 0 && dist2 > 0) {
            int totalAlternativeDistance = dist1 + dist2;
            
            printf("Chemin alternatif via hub %d: distance %d (%d+%d)\n", 
                   bestHub, totalAlternativeDistance, dist1, dist2);
            
            // SEULEMENT utiliser l'alternatif s'il est vraiment meilleur
            // Critères stricts: distance égale OU plus courte
            if (totalAlternativeDistance <= directDistance) {
                // Compter routes possédées sur chemin alternatif
                int altRoutesOwned = 0;
                int altTotalRoutes = (pathLength1 - 1) + (pathLength2 - 1);
                
                // Vérifier path1 (start -> hub)
                for (int i = 0; i < pathLength1 - 1; i++) {
                    int cityA = path1[i];
                    int cityB = path1[i + 1];
                    
                    for (int j = 0; j < state->nbTracks; j++) {
                        if (((state->routes[j].from == cityA && state->routes[j].to == cityB) ||
                             (state->routes[j].from == cityB && state->routes[j].to == cityA)) &&
                            state->routes[j].owner == 1) {
                            altRoutesOwned++;
                            break;
                        }
                    }
                }
                
                // Vérifier path2 (hub -> end)
                for (int i = 0; i < pathLength2 - 1; i++) {
                    int cityA = path2[i];
                    int cityB = path2[i + 1];
                    
                    for (int j = 0; j < state->nbTracks; j++) {
                        if (((state->routes[j].from == cityA && state->routes[j].to == cityB) ||
                             (state->routes[j].from == cityB && state->routes[j].to == cityA)) &&
                            state->routes[j].owner == 1) {
                            altRoutesOwned++;
                            break;
                        }
                    }
                }
                
                float altOwnedRatio = (altTotalRoutes > 0) ? (float)altRoutesOwned / altTotalRoutes : 0;
                
                printf("Chemin alternatif: %d/%d routes possédées (%.0f%%)\n", 
                       altRoutesOwned, altTotalRoutes, altOwnedRatio * 100);
                
                // Utiliser l'alternatif SEULEMENT si clairement meilleur
                if (altOwnedRatio > ownedRatio + 0.2) { // Au moins 20% de mieux
                    // Construire le chemin complet
                    *pathLength = 0;
                    
                    // Ajouter path1 (start -> hub)
                    for (int i = 0; i < pathLength1; i++) {
                        path[(*pathLength)++] = path1[i];
                    }
                    
                    // Ajouter path2 (hub -> end), en évitant de dupliquer le hub
                    for (int i = 1; i < pathLength2; i++) {
                        path[(*pathLength)++] = path2[i];
                    }
                    
                    printf("CHEMIN ALTERNATIF CHOISI: via hub %d (%.0f%% vs %.0f%% possédé)\n", 
                           bestHub, altOwnedRatio * 100, ownedRatio * 100);
                    
                    return totalAlternativeDistance;
                }
            }
        }
    }
    
    // ÉTAPE 7: Par défaut, utiliser le chemin direct
    printf("CHEMIN DIRECT MAINTENU (meilleure option)\n");
    return directDistance;
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

// Invalide le cache de pathfinding
void invalidatePathCache(void) {
    cacheTimestamp++;
    if (cacheTimestamp < 0) {
        // Éviter le débordement
        cacheTimestamp = 0;
        cacheEntries = 0;
    }
    printf("Cache de pathfinding invalidé (timestamp: %d)\n", cacheTimestamp);
}

// Met à jour le timestamp du cache
void updateCacheTimestamp(void) {
    cacheTimestamp++;
}