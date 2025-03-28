#include <stdio.h>
#include <stdlib.h>
#include "rules.h"


/*
 * canClaimRoute
 * 
 * Cette fonction vérifie si un joueur peut prendre une route spécifique avec ses cartes actuelles.
 * C'est un élément central de la validation des règles du jeu.
 * 
 * Vérifications effectuées :
 * 1) La route existe-t-elle ?
 * 2) La route est-elle déjà prise par quelqu'un ?
 * 3) Avons-nous assez de wagons pour la prendre ?
 * 4) La couleur proposée est-elle valide pour cette route ?
 * 5) Avons-nous assez de cartes de cette couleur et de locomotives ?
 * 
 * Le paramètre nbLocomotives est passé par référence car la fonction calcule
 * également le nombre optimal de locomotives à utiliser et le renvoie à l'appelant.
 * 
 * C'est un exemple de fonction qui a plusieurs responsabilités :
 * - Validation des règles
 * - Optimisation (nombre de locomotives)
 */

int canClaimRoute(GameState* state, int from, int to, CardColor color, int* nbLocomotives) {
    // Vérifie si l'état est correctement initialisé
    if (!state) {
        return 0;
    }
    
    // Vérifie si nous avons assez de wagons en premier
    if (state->wagonsLeft <= 0) {
        return 0;
    }
    
    // Trouve la route
    int routeIndex = -1;
    for (int i = 0; i < state->nbTracks; i++) {
        if ((state->routes[i].from == from && state->routes[i].to == to) ||
            (state->routes[i].from == to && state->routes[i].to == from)) {
            routeIndex = i;
            break;
        }
    }
    
    if (routeIndex == -1) {
        return 0;
    }
    
    // Vérifie si la route est déjà prise
    if (state->routes[routeIndex].owner != 0) {
        return 0;
    }
    
    // Vérifie si nous avons assez de wagons
    int length = state->routes[routeIndex].length;
    if (state->wagonsLeft < length) {
        return 0;
    }
    
    // Vérifie si la couleur est valide pour cette route
    CardColor routeColor = state->routes[routeIndex].color;
    CardColor routeSecondColor = state->routes[routeIndex].secondColor;
    
    if (routeColor != color && routeSecondColor != color && color != 0) {
        return 0;
    }
    
    // Compte combien de cartes de la bonne couleur nous avons
    int colorCards = state->nbCardsByColor[color];
    int locomotives = state->nbCardsByColor[LOCOMOTIVE];
    
    // Si on utilise des locomotives, on les compte différemment
    if (color == LOCOMOTIVE) {
        if (locomotives >= length) {
            *nbLocomotives = length; // Toutes les cartes sont des locomotives
            return 1;
        } else {
            return 0;
        }
    }
    
    // Combien de locomotives devons-nous utiliser?
    if (colorCards >= length) {
        // Assez de cartes de la bonne couleur, pas besoin de locomotives
        *nbLocomotives = 0;
        return 1;
    } else if (colorCards + locomotives >= length) {
        // On peut compléter avec des locomotives
        *nbLocomotives = length - colorCards;
        return 1;
    } else {
        return 0;
    }
}



/*
 * findPossibleRoutes
 * 
 * Cette fonction stratégique identifie toutes les routes que le joueur peut prendre
 * avec ses cartes actuelles. Elle est utilisée par l'IA pour décider quelle action effectuer.
 * 
 * Comment ça fonctionne :
 * 1) Parcourt toutes les routes du plateau
 * 2) Pour chaque route libre, détermine les couleurs possibles
 * 3) Pour chaque couleur, vérifie si on peut prendre la route
 * 4) Stocke les routes possibles avec leur couleur et nombre de locomotives
 * 
 * Optimisations :
 * - Vérification rapide du nombre de wagons avant analyse approfondie
 * - Création d'une liste limitée de couleurs à vérifier (au lieu de tester les 9 couleurs)
 * - Arrêt dès qu'une couleur valide est trouvée pour une route
 * 
 * Cette fonction est complexe car elle doit être efficace - l'IA l'appelle fréquemment.
 */


int findPossibleRoutes(GameState* state, int* possibleRoutes, CardColor* possibleColors, int* possibleLocomotives) {
    int count = 0;
    
    // Vérification des paramètres
    if (!state || !possibleRoutes || !possibleColors || !possibleLocomotives) {
        printf("ERROR: Invalid parameters in findPossibleRoutes\n");
        return 0;
    }
    
    // Pour chaque route
    for (int i = 0; i < state->nbTracks; i++) {
        // Si la route n'est pas déjà prise
        if (state->routes[i].owner == 0) {
            int from = state->routes[i].from;
            int to = state->routes[i].to;
            CardColor routeColor = state->routes[i].color;
            CardColor routeSecondColor = state->routes[i].secondColor;
            int length = state->routes[i].length;
            
            // Vérification rapide - si nous n'avons pas assez de wagons, passer à la route suivante
            if (state->wagonsLeft < length) {
                continue;
            }
            
            // Définir les couleurs à vérifier (optimisation)
            CardColor colorsToCheck[3];
            int numColorsToCheck = 0;
            
            // Ajouter la couleur principale de la route si nous avons des cartes de cette couleur
            if (routeColor != NONE && state->nbCardsByColor[routeColor] > 0) {
                colorsToCheck[numColorsToCheck++] = routeColor;
            }
            
            // Ajouter la couleur secondaire de la route si différente et si nous avons des cartes
            if (routeSecondColor != NONE && routeSecondColor != routeColor && 
                state->nbCardsByColor[routeSecondColor] > 0) {
                colorsToCheck[numColorsToCheck++] = routeSecondColor;
            }
            
            // Ajouter la locomotive si nous en avons
            if (state->nbCardsByColor[LOCOMOTIVE] > 0) {
                colorsToCheck[numColorsToCheck++] = LOCOMOTIVE;
            }
            
            // Si nous n'avons aucune carte des couleurs de la route et pas de locomotive, passer à la suivante
            if (numColorsToCheck == 0) {
                continue;
            }
            
            // Vérifier chaque couleur possible pour cette route
            for (int c = 0; c < numColorsToCheck; c++) {
                CardColor color = colorsToCheck[c];
                
                // Vérification rapide - si nous n'avons pas assez de cartes au total, passer à la couleur suivante
                if (state->nbCardsByColor[color] + state->nbCardsByColor[LOCOMOTIVE] < length) {
                    continue;
                }
                
                int nbLocomotives = 0;
                if (canClaimRoute(state, from, to, color, &nbLocomotives)) {
                    // Double vérification que nous avons assez de cartes
                    int availableColorCards = state->nbCardsByColor[color];
                    int availableLocomotives = state->nbCardsByColor[LOCOMOTIVE];
                    
                    // Si on utilise des locomotives, on vérifie qu'on en a assez
                    if (nbLocomotives <= availableLocomotives && 
                        (length - nbLocomotives) <= availableColorCards) {
                        possibleRoutes[count] = i;
                        possibleColors[count] = color;
                        possibleLocomotives[count] = nbLocomotives;
                        
                        printf("Possible route %d: from %d to %d, color %d, length %d, with %d locomotives\n", 
                               count, from, to, color, length, nbLocomotives);
                        
                        count++;
                        break; // Une seule couleur par route
                    }
                }
            }
        }
    }
    
    return count;
}


// Vérifie si une couleur de carte visible peut être piochée
int canDrawVisibleCard(CardColor color) {
    // On ne peut pas piocher une locomotive visible si c'est notre seconde carte
    // Mais cette règle est gérée par le serveur, donc on retourne toujours vrai ici
    return 1;
}

// Vérifie s'il reste suffisamment de wagons pour prendre une route
int hasEnoughWagons(GameState* state, int length) {
    return state->wagonsLeft >= length;
}

/*
 * isLastTurn
 * 
 * Cette fonction détermine si nous sommes dans le dernier tour du jeu.
 * C'est important pour ajuster la stratégie de l'IA.
 * 
 * Selon les règles de Ticket to Ride, le dernier tour commence quand un joueur
 * atteint 2 wagons ou moins après avoir pris une route. Chaque joueur a alors
 * un dernier tour, y compris celui qui a déclenché la fin.
 * 
 * Cette fonction vérifie si :
 * - Le flag lastTurn a déjà été activé
 * - Nous avons 2 wagons ou moins
 * - L'adversaire a 2 wagons ou moins
 */

int isLastTurn(GameState* state) {
    return state->lastTurn || state->wagonsLeft <= 2 || state->opponentWagonsLeft <= 2;
}




// Vérifie si une route est déjà prise
int routeOwner(GameState* state, int from, int to) {
    for (int i = 0; i < state->nbTracks; i++) {
        if ((state->routes[i].from == from && state->routes[i].to == to) ||
            (state->routes[i].from == to && state->routes[i].to == from)) {
            return state->routes[i].owner;
        }
    }
    return -1; // Route non trouvée
}




/*
 * isObjectiveCompleted
 * 
 * Cette fonction vérifie si un objectif est complété en utilisant la matrice de connectivité.
 * C'est une fonction simple mais cruciale car elle détermine une grande partie du score.
 * 
 * Un objectif est considéré comme complété si le joueur a établi une connexion
 * (directe ou indirecte) entre les deux villes de l'objectif.
 * 
 * La fonction s'appuie sur la matrice de connectivité qui est mise à jour par
 * l'algorithme de Floyd-Warshall dans updateCityConnectivity().
 */
int isObjectiveCompleted(GameState* state, Objective objective) {
    return state->cityConnected[objective.from][objective.to];
}




/*
 * calculateScore
 * 
 * Cette fonction calcule le score du joueur selon les règles de Ticket to Ride.
 * Elle est utilisée pour déterminer le gagnant et évaluer la performance de l'IA.
 * 
 * Composantes du score :
 * 1) Points pour les routes prises (selon leur longueur)
 * 2) Points pour les objectifs complétés
 * 3) Pénalités pour les objectifs non complétés
 * 
 * Table de points pour les routes :
 * - Longueur 1: 1 point
 * - Longueur 2: 2 points
 * - Longueur 3: 4 points
 * - Longueur 4: 7 points
 * - Longueur 5: 10 points
 * - Longueur 6: 15 points
 * 
 * Cette fonction est un bon exemple de l'implémentation des règles de scoring du jeu.
 */


int calculateScore(GameState* state) {
    int score = 0;
    
    // Points pour les routes prises
    for (int i = 0; i < state->nbClaimedRoutes; i++) {
        int routeIndex = state->claimedRoutes[i];
        
        // Vérifier que l'index est valide pour éviter les accès mémoire incorrects
        if (routeIndex >= 0 && routeIndex < state->nbTracks) {
            int length = state->routes[routeIndex].length;
            
            // Vérifier que la longueur est valide
            if (length >= 0 && length <= 6) {
                // Table de correspondance longueur -> points
                int pointsByLength[] = {0, 1, 2, 4, 7, 10, 15};
                score += pointsByLength[length];
            } else {
                printf("Warning: Invalid route length: %d\n", length);
            }
        } else {
            printf("Warning: Invalid route index: %d\n", routeIndex);
        }
    }
    
    // Points pour les objectifs complétés
    int objectivesCompleted = 0;
    int objectivesFailed = 0;
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (isObjectiveCompleted(state, state->objectives[i])) {
            score += state->objectives[i].score;
            objectivesCompleted++;
            printf("Objective %d completed: +%d points\n", i+1, state->objectives[i].score);
        } else {
            // Pénalité pour les objectifs non complétés
            score -= state->objectives[i].score;
            objectivesFailed++;
            printf("Objective %d failed: -%d points\n", i+1, state->objectives[i].score);
        }
    }
    
    printf("Score summary: %d points from routes, %d objectives completed, %d objectives failed\n", 
           score - objectivesCompleted + objectivesFailed,
           objectivesCompleted, 
           objectivesFailed);
    
    return score;
}