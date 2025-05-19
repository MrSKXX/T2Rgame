// en general dans cette partie on a des fonctions qui permettent de manipuler les données de jeu

// premierement on initialise l'etat du jeu en fonction des données de jeu
// on utilise memset pour initialiser la structure gameState à 0
// on initialise les variables de gameState avec les données de jeu
// on initialise la matrice de connectivité des villes à 0
// on parse les données de routes et on les stocke dans la structure gameState

// ensuite on a une fonction addCardToHand qui permet de mettre à jour l'état du jeu après avoir reçu une carte
// on vérifie si on peut ajouter une carte à la main
// on ajoute la carte à la main et on met à jour le nombre de cartes de cette couleur

// on a une fonction removeCardsForRoute qui permet de mettre à jour l'état du jeu après avoir joué des cartes pour prendre une route
// on vérifie les paramètres
// on vérifie si on a assez de cartes pour prendre la route
// on enlève les locomotives demandées
// on enlève les cartes de la couleur demandée
// on enlève les locomotives si on a utilisé des locomotives pour la couleur
// on met à jour les compteurs
// on met à jour le nombre de wagons


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gamestate.h"

void initGameState(GameState* state, GameData* gameData) {
    memset(state, 0, sizeof(GameState));
    state->nbCities = gameData->nbCities;
    state->nbTracks = gameData->nbTracks;
    state->nbCards = 4; 
    state->nbObjectives = 0;
    state->nbClaimedRoutes = 0;
    state->lastTurn = 0;
    state->wagonsLeft = 45; 
    state->opponentWagonsLeft = 45;
    state->opponentCardCount = 4; 
    state->opponentObjectiveCount = 0;
    state->turnCount = 0;
    // Les tableaux sont déjà initialisés à zéro grâce au memset de la structure complète
    
    // Initialise la matrice de connectivité
    for (int i = 0; i < MAX_CITIES; i++) {
        for (int j = 0; j < MAX_CITIES; j++) {
            state->cityConnected[i][j] = 0;
        }
    }
    
    // Parse les données de routes
    int* trackData = gameData->trackData;
    for (int i = 0; i < state->nbTracks; i++) {
        int from = trackData[i*5];
        int to = trackData[i*5 + 1];
        int length = trackData[i*5 + 2];
        CardColor color = (CardColor)trackData[i*5 + 3];
        CardColor secondColor = (CardColor)trackData[i*5 + 4];
        
        Route route;
        route.from = from;
        route.to = to;
        route.length = length;
        route.color = color;
        route.secondColor = secondColor;
        route.owner = 0; 
        state->routes[i] = route;
    }
    
    // Initialise les cartes visibles update it later
    memset(state->visibleCards, 0, sizeof(state->visibleCards));
    
    printf("GameState initialized with %d cities and %d tracks\n", state->nbCities, state->nbTracks);
}

// Met à jour l'état du jeu après avoir reçu une carte
void addCardToHand(GameState* state, CardColor card) {
    if (!state) {
        printf("ERROR: Null state in addCardToHand\n");
        return;
    }
    
    // Vérifier si on peut ajouter une carte (limite de 50)
    if (state->nbCards >= 50) {
        printf("WARNING: Cannot add more cards, hand is full (50 max)!\n");
        return;
    }
    
    // Ajouter la carte
    state->cards[state->nbCards++] = card;
    state->nbCardsByColor[card]++;
    
    printf("Added card to hand: ");
    const char* cardNames[] = {"None", "Purple", "White", "Blue", "Yellow", 
                              "Orange", "Black", "Red", "Green", "Locomotive"};
    printf("%s\n", cardNames[card]);
    
    // Afficher un récapitulatif des cartes en main
    printf("Current hand: %d cards total\n", state->nbCards);
}




/*
 * removeCardsForRoute
 * 
 * Cette fonction gère le retrait complexe des cartes utilisées pour prendre une route.
 * Elle doit gérer plusieurs cas spéciaux et maintenir la cohérence de l'état du jeu.
 * 
 * Pourquoi cette fonction est complexe :
 * - Elle doit gérer l'utilisation de locomotives (cartes joker)
 * - Elle doit compacter le tableau de cartes après retrait
 * - Elle doit maintenir à jour tous les compteurs (nbCards, nbCardsByColor, wagonsLeft)
 * 
 * Étapes principales :
 * 1) Retire d'abord les locomotives explicitement demandées
 * 2) Retire ensuite les cartes de la couleur demandée
 * 3) Utilise des locomotives supplémentaires si nécessaire
 * 4) Compacte le tableau de cartes pour éliminer les "trous"
 * 5) Met à jour tous les compteurs associés
 */


// Met à jour l'état du jeu après avoir joué des cartes pour prendre une route
// Extrait montrant la fonction removeCardsForRoute corrigée
void removeCardsForRoute(GameState* state, CardColor color, int length, int nbLocomotives) {
    extern void debugPrint(int level, const char* format, ...);  // Déclaration externe
    
    // Vérification des paramètres
    if (!state || length <= 0) {
        printf("ERROR: Invalid parameters in removeCardsForRoute\n");
        return;
    }
    
    // Vérification que nous avons assez de cartes
    if (state->nbCardsByColor[color] + state->nbCardsByColor[LOCOMOTIVE] < length) {
        printf("ERROR: Not enough cards to remove in removeCardsForRoute\n");
        return;
    }
    
    const char* cardNames[] = {"None", "Purple", "White", "Blue", "Yellow", 
                              "Orange", "Black", "Red", "Green", "Locomotive"};
    
    printf("Removing cards for route: %d %s cards and %d locomotives\n", 
           length - nbLocomotives, 
           (color < 10 && color >= 0) ? cardNames[color] : "Unknown",
           nbLocomotives);
    
    // Mettre à jour directement les compteurs
    state->nbCardsByColor[color] -= (length - nbLocomotives);
    state->nbCardsByColor[LOCOMOTIVE] -= nbLocomotives;
    state->nbCards -= length;
    
    // Vérification de sécurité pour éviter les valeurs négatives
    if (state->nbCardsByColor[color] < 0) {
        printf("WARNING: Negative card count corrected for color %d\n", color);
        state->nbCardsByColor[color] = 0;
    }
    if (state->nbCardsByColor[LOCOMOTIVE] < 0) {
        printf("WARNING: Negative locomotive count corrected\n");
        state->nbCardsByColor[LOCOMOTIVE] = 0;
    }
    if (state->nbCards < 0) {
        printf("WARNING: Negative total card count corrected\n");
        state->nbCards = 0;
    }
    
    // Mettre à jour le nombre de wagons
    state->wagonsLeft -= length;
    
    printf("Removed %d cards (%d %s and %d locomotives) for claiming route\n", 
           length, length - nbLocomotives, 
           (color < 10 && color >= 0) ? cardNames[color] : "Unknown",
           nbLocomotives);
}

/*
 * addClaimedRoute
 * 
 * Cette fonction est cruciale car elle met à jour l'état du jeu après la prise d'une route.
 * Elle fait plus que simplement marquer une route comme prise - elle met à jour la connectivité.
 * 
 * Pourquoi c'est important :
 * - Elle maintient à jour la liste des routes prises
 * - Elle déclenche la mise à jour de la connectivité des villes
 * - Cette mise à jour permet de vérifier si des objectifs sont complétés
 * 
 * Note : Cette fonction est appelée à la fois par l'IA et le mode manuel,
 * c'est pourquoi il est important d'utiliser cette fonction plutôt que de
 * mettre à jour l'état directement.
 */


// Met à jour l'état du jeu après avoir pris une route
void addClaimedRoute(GameState* state, int from, int to) {
    if (!state || from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
        printf("ERREUR CRITIQUE: Paramètres invalides dans addClaimedRoute (%d -> %d)\n", from, to);
        return;
    }
    // Trouve l'index de la route
    int routeIndex = -1;
    for (int i = 0; i < state->nbTracks; i++) {
        if ((state->routes[i].from == from && state->routes[i].to == to) ||
            (state->routes[i].from == to && state->routes[i].to == from)) {
            routeIndex = i;
            break;
        }
    }
    
    if (routeIndex != -1) {
        // VALIDATION SUPPLÉMENTAIRE
        if (state->routes[routeIndex].owner != 0) {
            printf("ERREUR: Tentative de prendre une route déjà possédée (owner: %d)\n", 
                   state->routes[routeIndex].owner);
            return;
        }
        
        // Marque la route comme prise par nous
        state->routes[routeIndex].owner = 1;
        // Ajoute à notre liste de routes prises
        if (state->nbClaimedRoutes < MAX_ROUTES) {
            state->claimedRoutes[state->nbClaimedRoutes++] = routeIndex;
            printf("Added route from %d to %d to our claimed routes\n", from, to);
        } else {
            printf("ERREUR: Impossible d'ajouter plus de routes (maximum atteint)\n");
        }
        
        // Met à jour la connectivité
        updateCityConnectivity(state);
    } else {
        printf("WARNING: Could not find route from %d to %d\n", from, to);
    }
}




/*
 * updateAfterOpponentMove
 * 
 * Cette fonction met à jour notre état interne après une action de l'adversaire.
 * C'est essentiel pour suivre l'évolution du jeu et adapter notre stratégie.
 * 
 * Actions traitées :
 * - Prise de route par l'adversaire
 * - Pioche de cartes
 * - Choix d'objectifs
 * 
 * Points importants :
 * - Met à jour le nombre de wagons restants de l'adversaire
 * - Vérifie si c'est le dernier tour (≤ 2 wagons)
 * - Suit le nombre de cartes et d'objectifs de l'adversaire
 */


// Met à jour l'état du jeu après une action de l'adversaire
// Met à jour l'état du jeu après une action de l'adversaire
// Met à jour l'état du jeu après une action de l'adversaire
void updateAfterOpponentMove(GameState* state, MoveData* moveData) {
    if (!state || !moveData) {
        printf("ERROR: NULL parameters in updateAfterOpponentMove\n");
        return;
    }

    switch (moveData->action) {
        case CLAIM_ROUTE:
            // L'adversaire a pris une route
            {
                int from = moveData->claimRoute.from;
                int to = moveData->claimRoute.to;
                
                // Trouve l'index de la route
                int routeIndex = -1;
                for (int i = 0; i < state->nbTracks; i++) {
                    if ((state->routes[i].from == from && state->routes[i].to == to) ||
                        (state->routes[i].from == to && state->routes[i].to == from)) {
                        routeIndex = i;
                        break;
                    }
                }
                
                if (routeIndex != -1) {
                    // Marque la route comme prise par l'adversaire
                    state->routes[routeIndex].owner = 2;
                    
                    // Réduit le nombre de wagons de l'adversaire
                    state->opponentWagonsLeft -= state->routes[routeIndex].length;
                    
                    printf("ATTENTION: Adversaire a pris la route %d à %d (route #%d, longueur %d)\n", 
                           from, to, routeIndex, state->routes[routeIndex].length);
                    
                    // Vérifie si c'est le dernier tour
                    if (state->opponentWagonsLeft <= 2) {
                        state->lastTurn = 1;
                        printf("LAST TURN: Opponent has <= 2 wagons left\n");
                    }
                } else {
                    printf("WARNING: Could not find route claimed by opponent from %d to %d\n", from, to);
                }
            }
            break;
            
        case DRAW_CARD:
        case DRAW_BLIND_CARD:
            // L'adversaire a pioché une carte
            state->opponentCardCount++;
            break;
            
        case CHOOSE_OBJECTIVES:
            // L'adversaire a choisi des objectifs
            {
                int keptObjectives = 0;
                for (int i = 0; i < 3; i++) {
                    if (moveData->chooseObjectives[i]) {
                        keptObjectives++;
                    }
                }
                state->opponentObjectiveCount += keptObjectives;
                printf("Opponent kept %d objectives\n", keptObjectives);
            }
            break;

        case DRAW_OBJECTIVES:
            // L'adversaire pioche des objectifs - pas de mise à jour d'état pour l'instant
            // car nous ne savons pas combien il en gardera jusqu'à ce qu'il choisisse
            printf("Opponent is drawing objective cards\n");
            break;
            
        default:
            printf("WARNING: Unknown action %d in updateAfterOpponentMove\n", moveData->action);
            break;
    

            
    }
    if (moveData->action == CLAIM_ROUTE) {
        // Also update opponent model for enhanced strategy
        updateOpponentObjectiveModel(state, moveData->claimRoute.from, moveData->claimRoute.to);
    }
}


/* 
 * updateCityConnectivity
 * 
 * Cette fonction utilise l'algorithme de Floyd-Warshall pour calculer la connectivité entre toutes les paires de villes.
 * 
 * Pourquoi utiliser Floyd-Warshall ?
 * - Il trouve si un chemin existe entre TOUTES les paires de villes en une seule exécution
 * - Efficace pour un petit nombre de villes (comme dans Ticket to Ride)
 * - Permet de vérifier facilement si un objectif est complété
 * 
 * Comment ça fonctionne ?
 * 1) On réinitialise la matrice de connectivité
 * 2) On marque les connexions directes (routes que nous possédons)
 * 3) On utilise Floyd-Warshall pour trouver toutes les connexions indirectes
 *    - Pour chaque ville k, on vérifie si elle peut servir d'intermédiaire entre i et j
 *    - Si i→k et k→j existent, alors i→j existe aussi
 * 
 * Complexité: O(V³) où V est le nombre de villes
 */

// Met à jour la matrice de connectivité des villes
void updateCityConnectivity(GameState* state) {
    // Vérifier que l'état est valide
    if (!state) {
        printf("Error: Game state is NULL in updateCityConnectivity\n");
        return;
    }
    
    // Vérifier que nbCities est dans des limites raisonnables
    if (state->nbCities <= 0 || state->nbCities > MAX_CITIES) {
        printf("Warning: Invalid number of cities: %d\n", state->nbCities);
        return;
    }
    
    // Réinitialise la matrice
    for (int i = 0; i < state->nbCities; i++) {
        for (int j = 0; j < state->nbCities; j++) {
            state->cityConnected[i][j] = 0;
        }
    }
    
    // Pour chaque route que nous possédons
    for (int i = 0; i < state->nbClaimedRoutes; i++) {
        int routeIndex = state->claimedRoutes[i];
        
        // Vérifier que l'index de route est valide
        if (routeIndex < 0 || routeIndex >= state->nbTracks) {
            printf("Warning: Invalid route index %d in updateCityConnectivity\n", routeIndex);
            continue;
        }
        
        int from = state->routes[routeIndex].from;
        int to = state->routes[routeIndex].to;
        
        // Vérifier que les indices de villes sont valides
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            printf("Warning: Invalid city indices (%d, %d) in route %d\n", from, to, routeIndex);
            continue;
        }
        
        // Marque les deux villes comme connectées directement
        state->cityConnected[from][to] = 1;
        state->cityConnected[to][from] = 1;
    }
    
    // Algorithme de Floyd-Warshall pour trouver toutes les connectivités transitives
    for (int k = 0; k < state->nbCities; k++) {
        for (int i = 0; i < state->nbCities; i++) {
            for (int j = 0; j < state->nbCities; j++) {
                if (state->cityConnected[i][k] && state->cityConnected[k][j]) {
                    state->cityConnected[i][j] = 1;
                }
            }
        }
    }
}



// Ajoute des objectifs à notre main
void addObjectives(GameState* state, Objective* objectives, int count) {
    for (int i = 0; i < count && state->nbObjectives < MAX_OBJECTIVES; i++) {
        state->objectives[state->nbObjectives++] = objectives[i];
        printf("Added objective: From %d to %d, score %d\n", 
               objectives[i].from, objectives[i].to, objectives[i].score);
    }
}

// Affiche l'état du jeu actuel
void printGameState(GameState* state) {
    if (!state) {
        printf("ERROR: Cannot print NULL game state\n");
        return;
    }
    
    printf("\n--- GAME STATE ---\n");
    
    // Affiche les informations générales
    printf("Cities: %d, Tracks: %d\n", state->nbCities, state->nbTracks);
    
    // Affiche les cartes en main
    printf("Cards in hand (%d):\n", state->nbCards);
    const char* cardNames[] = {"None", "Purple", "White", "Blue", "Yellow", 
                              "Orange", "Black", "Red", "Green", "Locomotive"};
    
    for (int i = 0; i < 10; i++) {
        if (state->nbCardsByColor[i] > 0) {
            printf("  %s: %d\n", cardNames[i], state->nbCardsByColor[i]);
        }
    }
    
    // Affiche les objectifs
    printf("Objectives (%d):\n", state->nbObjectives);
    for (int i = 0; i < state->nbObjectives; i++) {
        printf("  %d. From %d to %d, score %d", 
               i+1, state->objectives[i].from, state->objectives[i].to, state->objectives[i].score);
        
        // Vérifie si l'objectif est complété (avec vérification de validité)
        int from = state->objectives[i].from;
        int to = state->objectives[i].to;
        
        if (from >= 0 && from < state->nbCities && to >= 0 && to < state->nbCities) {
            if (state->cityConnected[from][to]) {
                printf(" [COMPLETED]");
            }
        } else {
            printf(" [INVALID CITIES]");
        }
        printf("\n");
    }
    
    // Affiche les routes prises
    printf("Claimed routes (%d):\n", state->nbClaimedRoutes);
    for (int i = 0; i < state->nbClaimedRoutes; i++) {
        int routeIndex = state->claimedRoutes[i];
        if (routeIndex >= 0 && routeIndex < state->nbTracks) {
            printf("  %d. From %d to %d, length %d, color %s\n", 
                   i+1, state->routes[routeIndex].from, state->routes[routeIndex].to, 
                   state->routes[routeIndex].length, 
                   (state->routes[routeIndex].color < 10) ? cardNames[state->routes[routeIndex].color] : "Invalid");
        } else {
            printf("  %d. Invalid route index: %d\n", i+1, routeIndex);
        }
    }
    
    printf("Wagons left: %d\n", state->wagonsLeft);
    printf("Opponent wagons left: %d\n", state->opponentWagonsLeft);
    printf("Visible cards:\n");
    for (int i = 0; i < 5; i++) {
        if (state->visibleCards[i] >= 0 && state->visibleCards[i] < 10) {
            printf("  %d. %s\n", i+1, cardNames[state->visibleCards[i]]);
        } else {
            printf("  %d. Invalid card: %d\n", i+1, state->visibleCards[i]);
        }
    }
    printf("------------------\n\n");
}

// Cette fonction doit être ajoutée dans gamestate.c 

/**
 * Affiche une représentation visuelle de la matrice de connectivité
 * Utile pour déboguer et vérifier les connexions entre villes
 */
void printConnectivityMatrix(GameState* state) {
    if (!state) {
        printf("ERROR: NULL state in printConnectivityMatrix\n");
        return;
    }

    printf("\n=== CONNECTIVITY MATRIX ===\n");
    
    // Nombre maximum de villes à afficher pour la lisibilité
    int maxCitiesToShow = 10;
    if (state->nbCities < maxCitiesToShow) {
        maxCitiesToShow = state->nbCities;
    }
    
    // En-tête des colonnes
    printf("    ");
    for (int j = 0; j < maxCitiesToShow; j++) {
        printf("%2d ", j);
    }
    printf("\n");
    
    // Ligne de séparation
    printf("   ");
    for (int j = 0; j < maxCitiesToShow; j++) {
        printf("---");
    }
    printf("\n");
    
    // Contenu de la matrice
    for (int i = 0; i < maxCitiesToShow; i++) {
        printf("%2d | ", i);
        for (int j = 0; j < maxCitiesToShow; j++) {
            if (i < state->nbCities && j < state->nbCities) {
                printf("%2d ", state->cityConnected[i][j]);
            } else {
                printf(" - "); // Pour les indices hors limites
            }
        }
        printf("\n");
    }
    
    // Informations supplémentaires
    int connectedPairs = 0;
    for (int i = 0; i < state->nbCities; i++) {
        for (int j = i+1; j < state->nbCities; j++) {
            if (state->cityConnected[i][j]) {
                connectedPairs++;
            }
        }
    }
    
    printf("\nTotal connected city pairs: %d out of %d possible pairs\n", 
           connectedPairs, (state->nbCities * (state->nbCities - 1)) / 2);
    
    // Pour chaque objectif, vérifier s'il est complété
    printf("\nObjective connectivity status:\n");
    for (int i = 0; i < state->nbObjectives; i++) {
        if (i < 0 || i >= MAX_OBJECTIVES) continue;
        
        int from = state->objectives[i].from;
        int to = state->objectives[i].to;
        
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            printf("  Objective %d: Invalid cities\n", i+1);
            continue;
        }
        
        bool connected = state->cityConnected[from][to];
        printf("  Objective %d: From %d to %d - %s\n", 
               i+1, from, to, connected ? "CONNECTED" : "not connected");
    }
    
    printf("=========================\n\n");
}
void analyzeExistingNetwork(GameState* state, int* cityConnectivity) {
    // Initialiser le tableau à 0
    for (int i = 0; i < state->nbCities; i++) {
        cityConnectivity[i] = 0;
    }
    
    // Compter les connexions pour chaque ville
    for (int i = 0; i < state->nbClaimedRoutes; i++) {
        int routeIndex = state->claimedRoutes[i];
        if (routeIndex >= 0 && routeIndex < state->nbTracks) {
            int from = state->routes[routeIndex].from;
            int to = state->routes[routeIndex].to;
            
            if (from >= 0 && from < state->nbCities) {
                cityConnectivity[from]++;
            }
            if (to >= 0 && to < state->nbCities) {
                cityConnectivity[to]++;
            }
        }
    }
    
    printf("Analyse du réseau existant:\n");
    for (int i = 0; i < state->nbCities; i++) {
        if (cityConnectivity[i] > 0) {
            printf("  Ville %d: %d connexions\n", i, cityConnectivity[i]);
        }
    }
}

void findMissingConnections(GameState* state, int* cityConnectivity, MissingConnection* missingConnections, int* count) {
    *count = 0;
    
    // Pour chaque objectif non complété
    for (int i = 0; i < state->nbObjectives; i++) {
        if (!isObjectiveCompleted(state, state->objectives[i])) {
            int objFrom = state->objectives[i].from;
            int objTo = state->objectives[i].to;
            int objScore = state->objectives[i].score;
            
            // Trouver toutes les villes avec au moins 2 connexions (hubs)
            for (int city = 0; city < state->nbCities; city++) {
                if (cityConnectivity[city] >= 2) {
                    // Vérifier si un hub est déjà connecté à une extrémité de l'objectif
                    if (state->cityConnected[city][objFrom] || state->cityConnected[city][objTo]) {
                        // Chercher la connexion manquante
                        int targetCity = state->cityConnected[city][objFrom] ? objTo : objFrom;
                        
                        // Vérifier s'il est possible de connecter ce hub à l'autre extrémité
                        int path[MAX_CITIES];
                        int pathLength = 0;
                        if (!state->cityConnected[city][targetCity] && 
                            findShortestPath(state, city, targetCity, path, &pathLength) > 0) {
                            
                            // Cette connexion manquante aiderait à compléter l'objectif
                            if (*count < MAX_CITIES) {
                                missingConnections[*count].city = city;
                                missingConnections[*count].connectionsNeeded = pathLength - 1;
                                // Priorité basée sur le score de l'objectif et l'effort nécessaire
                                missingConnections[*count].priority = 
                                    (objScore * 100) / missingConnections[*count].connectionsNeeded;
                                (*count)++;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Trier par priorité
    for (int i = 0; i < *count - 1; i++) {
        for (int j = 0; j < *count - i - 1; j++) {
            if (missingConnections[j].priority < missingConnections[j+1].priority) {
                MissingConnection temp = missingConnections[j];
                missingConnections[j] = missingConnections[j+1];
                missingConnections[j+1] = temp;
            }
        }
    }
    
    printf("Connexions manquantes identifiées:\n");
    for (int i = 0; i < *count && i < 5; i++) {
        printf("  Ville %d: %d connexions nécessaires, priorité %d\n", 
               missingConnections[i].city, 
               missingConnections[i].connectionsNeeded, 
               missingConnections[i].priority);
    }
}