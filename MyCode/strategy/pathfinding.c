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
int findSmartestPath(GameState* state, int start, int end, int* path, int* pathLength) {
    // TOUJOURS commencer par le chemin direct
    int directDistance = findShortestPath(state, start, end, path, pathLength);
    
    // Si pas de chemin direct ou erreur, retourner immédiatement
    if (directDistance <= 0) {
        return directDistance;
    }
    
    // CHANGEMENT: Être beaucoup plus restrictif sur quand utiliser le réseau existant
    int phase = determineGamePhase(state);
    
    // En début de partie OU si peu de routes OU si le chemin direct est court: utiliser le chemin direct
    if (phase == PHASE_EARLY || state->nbClaimedRoutes < 3 || directDistance <= 4) {
        printf("Chemin direct privilégié: phase=%d, routes=%d, distance=%d\n", 
               phase, state->nbClaimedRoutes, directDistance);
        return directDistance;
    }
    
    // Sauvegarder le chemin direct
    int directPath[MAX_CITIES];
    int directPathLength = *pathLength;
    memcpy(directPath, path, directPathLength * sizeof(int));
    
    // D'abord, vérifions si nous avons déjà construit un réseau qui se rapproche de la cible
    int cityConnectivity[MAX_CITIES] = {0};
    analyzeExistingNetwork(state, cityConnectivity);
    
    // CHANGEMENT: Seuil plus élevé pour considérer un hub
    int hubs[MAX_CITIES];
    int hubCount = 0;
    for (int i = 0; i < state->nbCities; i++) {
        if (cityConnectivity[i] >= 2) {  // Garder seuil à 2 mais être plus strict sur les bonus
            hubs[hubCount++] = i;
        }
    }
    
    // Variables pour le meilleur chemin alternatif
    int bestAlternativeDistance = INT_MAX;
    int bestAlternativePath[MAX_CITIES];
    int bestAlternativePathLength = 0;
    
    // Essayer des chemins via nos hubs existants, mais avec critères stricts
    for (int i = 0; i < hubCount; i++) {
        int hub = hubs[i];
        
        // Éviter les hubs qui sont déjà sur le chemin direct
        bool hubOnDirectPath = false;
        for (int j = 0; j < directPathLength; j++) {
            if (directPath[j] == hub) {
                hubOnDirectPath = true;
                break;
            }
        }
        
        if (hubOnDirectPath) {
            continue; // Pas la peine d'essayer ce hub
        }
        
        int path1[MAX_CITIES], path2[MAX_CITIES];
        int pathLength1 = 0, pathLength2 = 0;
        
        // Trouver un chemin du début au hub, puis du hub à la fin
        int distance1 = findShortestPath(state, start, hub, path1, &pathLength1);
        int distance2 = findShortestPath(state, hub, end, path2, &pathLength2);
        
        if (distance1 > 0 && distance2 > 0) {
            // Calculer le chemin total
            int totalDistance = distance1 + distance2;
            
            // CHANGEMENT: Critères beaucoup plus stricts pour accepter un chemin alternatif
            if (totalDistance >= directDistance + 3) {
                continue; // Trop long, ignorer
            }
            
            // Compter combien de routes nous possédons déjà sur ce chemin
            int routesOwned = 0;
            int totalRoutes = (pathLength1 - 1) + (pathLength2 - 1);
            
            for (int j = 0; j < pathLength1 - 1; j++) {
                int cityA = path1[j];
                int cityB = path1[j+1];
                
                for (int k = 0; k < state->nbTracks; k++) {
                    if (((state->routes[k].from == cityA && state->routes[k].to == cityB) ||
                         (state->routes[k].from == cityB && state->routes[k].to == cityA)) &&
                        state->routes[k].owner == 1) {
                        routesOwned++;
                        break;
                    }
                }
            }
            
            for (int j = 0; j < pathLength2 - 1; j++) {
                int cityA = path2[j];
                int cityB = path2[j+1];
                
                for (int k = 0; k < state->nbTracks; k++) {
                    if (((state->routes[k].from == cityA && state->routes[k].to == cityB) ||
                         (state->routes[k].from == cityB && state->routes[k].to == cityA)) &&
                        state->routes[k].owner == 1) {
                        routesOwned++;
                        break;
                    }
                }
            }
            
            // CHANGEMENT: Bonus très modéré et seulement si vraiment avantageux
            float ownedRatio = (totalRoutes > 0) ? (float)routesOwned / totalRoutes : 0;
            
            // Seulement accepter si:
            // 1. Nous possédons au moins 60% des routes ET
            // 2. Le chemin alternatif n'est pas plus long que le direct ET
            // 3. Nous économisons au moins 1 route
            if (ownedRatio >= 0.6 && totalDistance <= directDistance && routesOwned >= 2) {
                // Petit bonus seulement (pas -2 par route comme avant)
                totalDistance -= 1;
                
                if (totalDistance < bestAlternativeDistance) {
                    bestAlternativeDistance = totalDistance;
                    bestAlternativePathLength = 0;
                    
                    // Construire le chemin complet
                    for (int j = 0; j < pathLength1; j++) {
                        bestAlternativePath[bestAlternativePathLength++] = path1[j];
                    }
                    // Éviter de dupliquer le hub
                    for (int j = 1; j < pathLength2; j++) {
                        bestAlternativePath[bestAlternativePathLength++] = path2[j];
                    }
                    
                    printf("Chemin alternatif via hub %d trouvé: %d segments, %.0f%% déjà possédés\n", 
                           hub, bestAlternativePathLength - 1, ownedRatio * 100);
                }
            }
        }
    }
    
    // CHANGEMENT: Seulement utiliser le chemin alternatif s'il est vraiment meilleur
    if (bestAlternativeDistance < directDistance) {
        memcpy(path, bestAlternativePath, bestAlternativePathLength * sizeof(int));
        *pathLength = bestAlternativePathLength;
        printf("Chemin alternatif sélectionné (distance %d vs %d direct)\n", 
               bestAlternativeDistance, directDistance);
        return bestAlternativeDistance;
    }
    
    // Par défaut, toujours retourner le chemin direct
    memcpy(path, directPath, directPathLength * sizeof(int));
    *pathLength = directPathLength;
    printf("Chemin direct maintenu (distance %d)\n", directDistance);
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