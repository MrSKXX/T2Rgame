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

 // Extrait montrant la fonction modifiée avec gestion du débogage
/**
 * Vérifie si nous avons assez de cartes pour prendre une route
 * @param state État actuel du jeu
 * @param from Ville de départ
 * @param to Ville d'arrivée
 * @param color Couleur à utiliser
 * @param nbLocomotives Nombre de locomotives à utiliser
 * @return 1 si le coup est légal, 0 sinon
 */
int canClaimRoute(GameState* state, int from, int to, CardColor color, int* nbLocomotives) {
    extern void debugPrint(int level, const char* format, ...);  // Déclaration externe
    
    // Vérifie si l'état est correctement initialisé
    if (!state || !nbLocomotives) {
        printf("ERROR: Invalid parameters in canClaimRoute\n");
        return 0;
    }
    
    // Définir les noms des cartes pour le débogage
    const char* cardNames[] = {"None", "Purple", "White", "Blue", "Yellow", 
                              "Orange", "Black", "Red", "Green", "Locomotive"};
    
    // Initialiser le nombre de locomotives à 0
    *nbLocomotives = 0;
    
    // Vérifie si nous avons assez de wagons
    if (state->wagonsLeft <= 0) {
        debugPrint(2, "DEBUG: canClaimRoute - Not enough wagons left");
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
        debugPrint(2, "DEBUG: canClaimRoute - Route from %d to %d not found", from, to);
        return 0;
    }
    
    // Vérifie si la route est déjà prise
    if (state->routes[routeIndex].owner != 0) {
        debugPrint(2, "DEBUG: canClaimRoute - Route from %d to %d already owned by %d", 
              from, to, state->routes[routeIndex].owner);
        return 0;
    }
    
    // Vérifie si nous avons assez de wagons
    int length = state->routes[routeIndex].length;
    if (state->wagonsLeft < length) {
        debugPrint(2, "DEBUG: canClaimRoute - Not enough wagons (%d needed, %d left)", 
              length, state->wagonsLeft);
        return 0;
    }
    
    // Vérifie si la couleur est valide pour cette route
    CardColor routeColor = state->routes[routeIndex].color;
    CardColor routeSecondColor = state->routes[routeIndex].secondColor;

    // VÉRIFICATION STRICTE DE LA COULEUR DE LA ROUTE
    // Pour les routes grises (représentées par LOCOMOTIVE)
    if (routeColor == LOCOMOTIVE) {
        debugPrint(2, "DEBUG: canClaimRoute - This is a gray route, any color is valid");
        
        // Si on utilise des locomotives comme couleur principale
        if (color == LOCOMOTIVE) {
            int locomotives = state->nbCardsByColor[LOCOMOTIVE];
            if (locomotives >= length) {
                *nbLocomotives = length; // Toutes les cartes sont des locomotives
                debugPrint(2, "DEBUG: canClaimRoute - Can claim with %d locomotives", *nbLocomotives);
                return 1;
            } else {
                debugPrint(2, "DEBUG: canClaimRoute - Not enough locomotives (%d needed, %d available)", 
                      length, locomotives);
                return 0;
            }
        }
        
        // Si on utilise une couleur spécifique
        int colorCards = state->nbCardsByColor[color];
        int locomotives = state->nbCardsByColor[LOCOMOTIVE];
        
        // On peut utiliser n'importe quelle couleur pour une route grise
        if (colorCards >= length) {
            // Assez de cartes de la couleur choisie
            *nbLocomotives = 0;
            return 1;
        } else if (colorCards + locomotives >= length) {
            // On complète avec des locomotives
            *nbLocomotives = length - colorCards;
            return 1;
        } else {
            // Pas assez de cartes
            debugPrint(2, "DEBUG: canClaimRoute - Not enough cards (%d %s + %d locomotives available, %d needed)", 
                  colorCards, cardNames[color], locomotives, length);
            return 0;
        }
    }
    // Pour les routes non grises, on doit ABSOLUMENT respecter la couleur
    else {
        // Vérifier strictement que la couleur choisie est valide pour cette route
        bool validColor = false;
        
        // Soit la couleur correspond exactement à routeColor
        if (color == routeColor) {
            validColor = true;
        }
        // Soit c'est la couleur alternative (si elle existe)
        else if (routeSecondColor != NONE && color == routeSecondColor) {
            validColor = true;
        }
        // Soit nous utilisons des locomotives uniquement
        else if (color == LOCOMOTIVE) {
            validColor = true;
        }
        
        if (!validColor) {
            debugPrint(1, "ERROR: Invalid color for route %d-%d: expected %s", 
                      from, to, cardNames[routeColor]);
            
            if (routeSecondColor != NONE) {
                debugPrint(1, " or %s", cardNames[routeSecondColor]);
            }
            
            debugPrint(1, ", got %s", cardNames[color]);
            
            return 0;  // Couleur invalide, impossible de prendre la route
        }
        
        // Compte combien de cartes de la bonne couleur nous avons
        int colorCards = state->nbCardsByColor[color];
        int locomotives = state->nbCardsByColor[LOCOMOTIVE];
        
        debugPrint(2, "DEBUG: canClaimRoute - We have %d %s cards and %d locomotives, need %d cards total",
              colorCards, cardNames[color], locomotives, length);
        
        // Si on utilise des locomotives comme couleur principale
        if (color == LOCOMOTIVE) {
            if (locomotives >= length) {
                *nbLocomotives = length; // Toutes les cartes sont des locomotives
                debugPrint(2, "DEBUG: canClaimRoute - Can claim with %d locomotives", *nbLocomotives);
                return 1;
            } else {
                debugPrint(2, "DEBUG: canClaimRoute - Not enough locomotives (%d needed, %d available)", 
                      length, locomotives);
                return 0;
            }
        }
        
        // Vérification plus claire du nombre de cartes nécessaires
        // Combien de locomotives devons-nous utiliser?
        if (colorCards >= length) {
            // Assez de cartes de la bonne couleur, pas besoin de locomotives
            *nbLocomotives = 0;
            debugPrint(2, "DEBUG: canClaimRoute - Can claim with %d %s cards, no locomotives needed",
                  length, cardNames[color]);
            return 1;
        } else if (colorCards + locomotives >= length) {
            // On peut compléter avec des locomotives
            *nbLocomotives = length - colorCards;
            debugPrint(2, "DEBUG: canClaimRoute - Can claim with %d %s cards and %d locomotives",
                  colorCards, cardNames[color], *nbLocomotives);
            return 1;
        } else {
            debugPrint(2, "DEBUG: canClaimRoute - Not enough cards to claim route (%d %s + %d locomotives available, %d needed)",
                  colorCards, cardNames[color], locomotives, length);
            return 0;
        }
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


 // Extrait montrant la fonction findPossibleRoutes modifiée
/**
 * This is a modified version of the findPossibleRoutes function
 * that limits the number of routes it processes and reduces excessive logging
 */
int findPossibleRoutes(GameState* state, int* possibleRoutes, CardColor* possibleColors, int* possibleLocomotives) {
    extern void debugPrint(int level, const char* format, ...);  // Déclaration externe
    int count = 0;
    
    // Vérification des paramètres
    if (!state || !possibleRoutes || !possibleColors || !possibleLocomotives) {
        printf("ERROR: Invalid parameters in findPossibleRoutes\n");
        return 0;
    }
    
    // CORRECTION: Limite stricte pour éviter les débordements de tableau
    const int MAX_ROUTES_TO_PROCESS = 50;
    
    // Vérifier que les données d'état sont cohérentes
    int totalCards = 0;
    for (int c = 1; c < 10; c++) {
        totalCards += state->nbCardsByColor[c];
    }
    
    if (state->nbCards != totalCards) {
        debugPrint(1, "WARNING: Card count mismatch! nbCards = %d, sum of cards by color = %d", 
              state->nbCards, totalCards);
        // Correction du nombre de cartes
        state->nbCards = totalCards;
    }
    
    // Afficher le nombre total de cartes en main sans faire de log excessif
    debugPrint(2, "DEBUG: findPossibleRoutes - Total cards in hand: %d", state->nbCards);
    const char* cardNames[] = {"None", "Purple", "White", "Blue", "Yellow", 
                               "Orange", "Black", "Red", "Green", "Locomotive"};
    
    // Only log cards we actually have, not every possible color
    debugPrint(2, "DEBUG: findPossibleRoutes - Cards by color:");
    for (int c = 1; c < 10; c++) {
        if (state->nbCardsByColor[c] > 0) {
            debugPrint(2, "  - %s: %d", cardNames[c], state->nbCardsByColor[c]);
        }
    }
    
    // Afficher le nombre de wagons restants
    debugPrint(2, "DEBUG: findPossibleRoutes - Wagons left: %d", state->wagonsLeft);
    
    // CORRECTION: Vérification des limites avant de parcourir les routes
    int nbTracksToCheck = state->nbTracks;
    if (nbTracksToCheck <= 0 || nbTracksToCheck > 150) {
        printf("WARNING: Invalid number of tracks: %d, limiting to 150\n", nbTracksToCheck);
        nbTracksToCheck = (nbTracksToCheck <= 0) ? 0 : 150;
    }
    
    // Pour chaque route
    debugPrint(3, "DEBUG: Checking %d tracks for possible routes", nbTracksToCheck);
    
    for (int i = 0; i < nbTracksToCheck && count < MAX_ROUTES_TO_PROCESS; i++) {
        // Vérification de l'index
        if (i < 0 || i >= state->nbTracks) {
            printf("WARNING: Invalid track index: %d, skipping\n", i);
            continue;
        }
        
    if (state->routes[i].from < 0 || state->routes[i].from >= state->nbCities || 
        state->routes[i].to < 0 || state->routes[i].to >= state->nbCities) {
        printf("ERREUR CRITIQUE: Route %d contient des villes invalides: %d -> %d\n", 
               i, state->routes[i].from, state->routes[i].to);
        continue;
    }


        // Si la route n'est pas déjà prise
        if (state->routes[i].owner == 0) {
            int from = state->routes[i].from;
            int to = state->routes[i].to;
            
            // CORRECTION: Vérification des limites des villes
            if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
                printf("WARNING: Invalid cities in route %d: from %d to %d, skipping\n", i, from, to);
                continue;
            }
            
            CardColor routeColor = state->routes[i].color;
            CardColor routeSecondColor = state->routes[i].secondColor;
            int length = state->routes[i].length;
            
            // Skip excessive debug logging - only log at high debug levels
            if (count < 10) {  // Only log the first 10 routes being examined
                debugPrint(3, "DEBUG: Examining route %d: from %d to %d, length %d, color %s, secondColor %s", 
                      i, from, to, length, 
                      (routeColor < 10) ? cardNames[routeColor] : "Unknown",
                      (routeSecondColor < 10) ? cardNames[routeSecondColor] : "Unknown");
            }
            
            // Vérification rapide - si nous n'avons pas assez de wagons, passer à la route suivante
            if (state->wagonsLeft < length) {
                continue;
            }
            
            // CORRECTION: Liste des couleurs à vérifier avec des limites strictes
            CardColor colorsToCheck[10]; // 9 couleurs + NONE
            int numColorsToCheck = 0;
            
            // Cas spécial: route grise (routeColor == LOCOMOTIVE)
            if (routeColor == LOCOMOTIVE) {
                // Pour les routes grises, toutes les couleurs dont on a des cartes sont valides
                for (int c = 1; c < 10; c++) { // Commence à 1 pour éviter NONE
                    if (state->nbCardsByColor[c] > 0) {
                        if (numColorsToCheck < 9) { // Assure qu'on ne dépasse pas la taille du tableau
                            colorsToCheck[numColorsToCheck++] = c;
                        }
                    }
                }
            } else {
                // Pour les routes colorées, uniquement la(les) couleur(s) de la route
                // Continuation de findPossibleRoutes dans rules.c
                if (routeColor != NONE && state->nbCardsByColor[routeColor] > 0) {
                    if (numColorsToCheck < 9) {
                        colorsToCheck[numColorsToCheck++] = routeColor;
                    }
                }
                
                if (routeSecondColor != NONE && routeSecondColor != routeColor && 
                    state->nbCardsByColor[routeSecondColor] > 0) {
                    if (numColorsToCheck < 9) {
                        colorsToCheck[numColorsToCheck++] = routeSecondColor;
                    }
                }
            }
            
            // Ajouter les locomotives comme option si on en a
            if (state->nbCardsByColor[LOCOMOTIVE] > 0) {
                // Pour les routes grises ou si nous n'avons pas les couleurs de la route
                if (numColorsToCheck == 0 || routeColor == LOCOMOTIVE) {
                    if (numColorsToCheck < 9) {
                        colorsToCheck[numColorsToCheck++] = LOCOMOTIVE;
                    }
                }
            }
            
            // Si nous n'avons aucune carte utilisable pour cette route, passer à la suivante
            if (numColorsToCheck == 0) {
                continue;
            }
            
            // CORRECTION: Limiter le nombre de couleurs vérifiées pour éviter les boucles infinies
            int maxColorsToCheck = (numColorsToCheck < 5) ? numColorsToCheck : 5;
            
            // Vérifier chaque couleur possible pour cette route - limit colors checked
            for (int c = 0; c < maxColorsToCheck; c++) {
                CardColor color = colorsToCheck[c];
                
                // Vérification rapide - si nous n'avons pas assez de cartes au total, passer à la couleur suivante
                int totalAvailableCards = (color == LOCOMOTIVE) ? 
                                         state->nbCardsByColor[LOCOMOTIVE] : 
                                         state->nbCardsByColor[color] + state->nbCardsByColor[LOCOMOTIVE];
                
                if (totalAvailableCards < length) {
                    continue;
                }
                
                int nbLocomotives = 0;
                if (canClaimRoute(state, from, to, color, &nbLocomotives)) {
                    // Double vérification que nous avons assez de cartes
                    int availableColorCards = state->nbCardsByColor[color];
                    int availableLocomotives = state->nbCardsByColor[LOCOMOTIVE];
                    
                    // Si on utilise des locomotives, on vérifie qu'on en a assez
                    if (nbLocomotives <= availableLocomotives && 
                        (color == LOCOMOTIVE || (length - nbLocomotives) <= availableColorCards)) {
                        // CORRECTION: Vérifier qu'on ne dépasse pas la taille du tableau
                        if (count < MAX_ROUTES_TO_PROCESS) {
                            possibleRoutes[count] = i;
                            // Vérification de validité de la couleur
                            if (color < 1 || color > 9) {
                                printf("ERREUR DANS findPossibleRoutes: Couleur invalide %d détectée pour route %d->%d, correction à BLACK (6)\n", 
                                    color, from, to);
                                color = 6;  // BLACK est généralement 6
                            }
                            possibleColors[count] = color;
                            possibleLocomotives[count] = nbLocomotives;
                            
                            // Only log the first 20 possible routes to avoid console spam
                            if (count < 20) {
                                printf("Possible route %d: from %d to %d, color %s, length %d, with %d locomotives\n", 
                                      count, from, to, cardNames[color], length, nbLocomotives);
                            }
                            
                            count++;
                        }
                        
                        // CORRECTION: Ne pas continuer à chercher d'autres couleurs pour les routes non-grises
                        // Pour les routes grises, on veut explorer toutes les couleurs possibles
                        // Dans rules.c, fonction findPossibleRoutes

                        // Pour les routes colorées, être sûr de choisir la bonne couleur
                        if (routeColor != LOCOMOTIVE) {
                            // Pour les routes colorées, uniquement les couleurs valides
                            if (state->nbCardsByColor[routeColor] > 0) {
                                if (numColorsToCheck < 9) {
                                    colorsToCheck[numColorsToCheck++] = routeColor;
                                }
                            }
                            
                            if (routeSecondColor != NONE && routeSecondColor != routeColor && 
                                state->nbCardsByColor[routeSecondColor] > 0) {
                                if (numColorsToCheck < 9) {
                                    colorsToCheck[numColorsToCheck++] = routeSecondColor;
                                }
                            }
                            
                            // Ajouter les locomotives comme option seulement si on a assez
                            if (state->nbCardsByColor[LOCOMOTIVE] >= length) {
                                if (numColorsToCheck < 9) {
                                    colorsToCheck[numColorsToCheck++] = LOCOMOTIVE;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // CORRECTION: Marquer explicitement la fin de la liste
    if (count < MAX_ROUTES_TO_PROCESS) {
        possibleRoutes[count] = -1;  // Marquer la fin de la liste
    }
    
    // Just print a summary with the total count instead of all routes
    printf("Found %d possible routes to claim\n", count);
    
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

// Trouver l'index d'une route entre deux villes
int findRouteIndex(GameState* state, int from, int to) {
    for (int i = 0; i < state->nbTracks; i++) {
        if ((state->routes[i].from == from && state->routes[i].to == to) ||
            (state->routes[i].from == to && state->routes[i].to == from)) {
            return i;
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