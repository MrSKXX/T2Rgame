#include "debug.h"
#include "gamestate.h"
#include "rules.h"
#include "strategy/strategy.h"

void debugLog(int level, const char* format, ...) {
    if (level <= DEBUG_LEVEL) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
    }
}

void debugObjectives(GameState* state) {
    if (!state) {
        printf("ERROR: NULL state in debugObjectives\n");
        return;
    }
    
    printf("\n=== ANALYSE DÉTAILLÉE DES OBJECTIFS ===\n");
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (i < 0 || i >= MAX_OBJECTIVES) continue;
        
        int from = state->objectives[i].from;
        int to = state->objectives[i].to;
        int score = state->objectives[i].score;
        
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            printf("Objectif %d: INVALIDE - From %d to %d, score %d\n", i+1, from, to, score);
            continue;
        }
        
        bool completed = isObjectiveCompleted(state, state->objectives[i]);
        
        printf("Objectif %d: From %d to %d, score %d %s\n", 
               i+1, from, to, score, completed ? "[COMPLÉTÉ]" : "");
        
        if (completed) continue;
        
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findShortestPath(state, from, to, path, &pathLength);
        
        if (distance > 0 && pathLength > 0) {
            printf("  Chemin trouvé, longueur %d: ", pathLength);
            for (int j = 0; j < pathLength && j < MAX_CITIES; j++) {
                printf("%d ", path[j]);
            }
            printf("\n");
            
            int routesNeeded = 0;
            int routesOwned = 0;
            int routesBlocked = 0;
            
            printf("  Analyse des routes:\n");
            for (int j = 0; j < pathLength - 1; j++) {
                int cityA = path[j];
                int cityB = path[j+1];
                
                if (cityA < 0 || cityA >= state->nbCities || 
                    cityB < 0 || cityB >= state->nbCities) continue;
                
                bool routeFound = false;
                for (int k = 0; k < state->nbTracks; k++) {
                    if ((state->routes[k].from == cityA && state->routes[k].to == cityB) ||
                        (state->routes[k].from == cityB && state->routes[k].to == cityA)) {
                        
                        routeFound = true;
                        printf("    %d->%d: ", cityA, cityB);
                        
                        if (state->routes[k].owner == 0) {
                            printf("Disponible (longueur %d, couleur %d)\n", 
                                  state->routes[k].length, state->routes[k].color);
                            routesNeeded++;
                        } else if (state->routes[k].owner == 1) {
                            printf("Déjà prise par nous\n");
                            routesOwned++;
                        } else if (state->routes[k].owner == 2) {
                            printf("BLOQUÉE par l'adversaire!\n");
                            routesBlocked++;
                        }
                        
                        break;
                    }
                }
                
                if (!routeFound) {
                    printf("    %d->%d: Aucune route trouvée!\n", cityA, cityB);
                }
            }
            
            printf("  Résumé: %d routes nécessaires, %d déjà prises, %d bloquées\n", 
                  routesNeeded, routesOwned, routesBlocked);
            
            if (routesBlocked > 0) {
                printf("  ATTENTION: Objectif partiellement bloqué par l'adversaire!\n");
            } else if (routesNeeded == 0) {
                printf("  Objectif en cours de complétion, toutes les routes acquises.\n");
            } else {
                printf("  Action nécessaire: Prendre %d routes pour compléter cet objectif.\n", 
                      routesNeeded);
            }
        } else {
            printf("  ERREUR: Aucun chemin trouvé pour cet objectif!\n");
        }
    }
    
    printf("=========================================\n\n");
}

void debugRoute(GameState* state, int from, int to, CardColor color, int nbLocomotives) {
    if (!state) {
        printf("ERROR: NULL state in debugRoute\n");
        return;
    }
    
    printf("\n=== ANALYSE DÉTAILLÉE DE LA ROUTE %d->%d ===\n", from, to);
    
    int routeIndex = -1;
    for (int i = 0; i < state->nbTracks; i++) {
        if ((state->routes[i].from == from && state->routes[i].to == to) ||
            (state->routes[i].from == to && state->routes[i].to == from)) {
            routeIndex = i;
            break;
        }
    }
    
    if (routeIndex == -1) {
        printf("ERREUR: Route non trouvée!\n");
        return;
    }
    
    Route* route = &state->routes[routeIndex];
    printf("Route #%d: De %d à %d\n", routeIndex, route->from, route->to);
    printf("Longueur: %d\n", route->length);
    printf("Couleur: %d\n", route->color);
    
    if (route->secondColor != NONE) {
        printf("Seconde couleur: %d\n", route->secondColor);
    }
    
    printf("Propriétaire: %d (0=Personne, 1=Nous, 2=Adversaire)\n", route->owner);
    
    printf("\nCouleur choisie pour prendre la route: %d\n", color);
    printf("Nombre de locomotives: %d\n", nbLocomotives);
    
    printf("\nVérification de validité:\n");
    
    bool colorValid = false;
    if (route->color == LOCOMOTIVE) {
        colorValid = true;
        printf("OK: Route grise, n'importe quelle couleur est valide\n");
    } else if (color == route->color || 
              (route->secondColor != NONE && color == route->secondColor) ||
              color == LOCOMOTIVE) {
        colorValid = true;
        printf("OK: Couleur valide pour cette route\n");
    } else {
        printf("ERREUR: Couleur invalide! La route accepte %d", route->color);
        
        if (route->secondColor != NONE) {
            printf(" ou %d", route->secondColor);
        }
        printf(", mais %d a été choisi\n", color);
    }
    
    int colorCards = state->nbCardsByColor[color];
    int locomotives = state->nbCardsByColor[LOCOMOTIVE];
    printf("\nCartes disponibles:\n");
    printf("- Couleur %d: %d\n", color, colorCards);
    printf("- Locomotives: %d\n", locomotives);
    
    if (color == LOCOMOTIVE) {
        if (locomotives >= route->length) {
            printf("OK: Assez de locomotives pour prendre la route\n");
        } else {
            printf("ERREUR: Pas assez de locomotives (%d nécessaires, %d disponibles)\n", 
                  route->length, locomotives);
        }
    } else {
        int colorCardsNeeded = route->length - nbLocomotives;
        if (colorCards >= colorCardsNeeded && locomotives >= nbLocomotives) {
            printf("OK: Assez de cartes (%d de couleur %d + %d locomotives)\n", 
                  colorCardsNeeded, color, nbLocomotives);
        } else {
            if (colorCards < colorCardsNeeded) {
                printf("ERREUR: Pas assez de cartes de couleur %d (%d nécessaires, %d disponibles)\n", 
                      color, colorCardsNeeded, colorCards);
            }
            if (locomotives < nbLocomotives) {
                printf("ERREUR: Pas assez de locomotives (%d nécessaires, %d disponibles)\n", 
                      nbLocomotives, locomotives);
            }
        }
    }
    
    if ((from == 17 && to == 22) || (from == 22 && to == 17)) {
        printf("\nATTENTION: Route spéciale Kansas City (17) - Saint Louis (22)\n");
        printf("Pour cette route, seules les couleurs BLUE (3), PURPLE (1) ou LOCOMOTIVE (9) sont autorisées\n");
        if (color != 3 && color != 1 && color != 9) {
            printf("ERREUR: Couleur %d non autorisée pour cette route spéciale!\n", color);
        } else {
            printf("OK: Couleur autorisée pour cette route spéciale\n");
        }
    }
    
    if ((from == 31 && to == 32) || (from == 32 && to == 31)) {
        printf("\nATTENTION: Route spéciale New York (31) - Washington (32)\n");
        printf("Pour cette route, seules les couleurs BLACK (6), ORANGE (5) ou LOCOMOTIVE (9) sont autorisées\n");
        if (color != 6 && color != 5 && color != 9) {
            printf("ERREUR: Couleur %d non autorisée pour cette route spéciale!\n", color);
        } else {
            printf("OK: Couleur autorisée pour cette route spéciale\n");
        }
    }
    
    printf("=========================================\n\n");
}