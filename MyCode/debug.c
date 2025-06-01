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
    if (DEBUG_LEVEL < 2) {
        return; // Sortie silencieuse si pas en mode debug verbeux
    }
    
    if (!state) {
        printf("ERROR: NULL state in debugObjectives\n");
        return;
    }
    
    printf("\n=== OBJECTIVES ANALYSIS ===\n");
    
    for (int i = 0; i < state->nbObjectives; i++) {
        if (i < 0 || i >= MAX_OBJECTIVES) continue;
        
        int from = state->objectives[i].from;
        int to = state->objectives[i].to;
        int score = state->objectives[i].score;
        
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            printf("Objective %d: INVALID - From %d to %d, score %d\n", i+1, from, to, score);
            continue;
        }
        
        bool completed = isObjectiveCompleted(state, state->objectives[i]);
        
        printf("Objective %d: From %d to %d, score %d %s\n", 
               i+1, from, to, score, completed ? "[COMPLETED]" : "");
        
        if (completed) continue;
        
        int path[MAX_CITIES];
        int pathLength = 0;
        int distance = findShortestPath(state, from, to, path, &pathLength);
        
        if (distance > 0 && pathLength > 0) {
            printf("  Path found, length %d: ", pathLength);
            for (int j = 0; j < pathLength && j < MAX_CITIES; j++) {
                printf("%d ", path[j]);
            }
            printf("\n");
            
            int routesNeeded = 0;
            int routesOwned = 0;
            int routesBlocked = 0;
            
            printf("  Route analysis:\n");
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
                            printf("Available (length %d, color %d)\n", 
                                  state->routes[k].length, state->routes[k].color);
                            routesNeeded++;
                        } else if (state->routes[k].owner == 1) {
                            printf("Already taken by us\n");
                            routesOwned++;
                        } else if (state->routes[k].owner == 2) {
                            printf("BLOCKED by opponent!\n");
                            routesBlocked++;
                        }
                        
                        break;
                    }
                }
                
                if (!routeFound) {
                    printf("    %d->%d: No route found!\n", cityA, cityB);
                }
            }
            
            printf("  Summary: %d routes needed, %d already taken, %d blocked\n", 
                  routesNeeded, routesOwned, routesBlocked);
            
            if (routesBlocked > 0) {
                printf("  WARNING: Objective partially blocked by opponent!\n");
            } else if (routesNeeded == 0) {
                printf("  Objective in progress, all routes acquired.\n");
            } else {
                printf("  Action needed: Take %d routes to complete this objective.\n", 
                      routesNeeded);
            }
        } else {
            printf("  ERROR: No path found for this objective!\n");
        }
    }
    
    printf("=============================\n\n");
}

void debugRoute(GameState* state, int from, int to, CardColor color, int nbLocomotives) {
    if (DEBUG_LEVEL < 2) {
        return; // Sortie silencieuse si pas en mode debug verbeux
    }
    
    if (!state) {
        printf("ERROR: NULL state in debugRoute\n");
        return;
    }
    
    printf("\n=== ROUTE ANALYSIS %d->%d ===\n", from, to);
    
    int routeIndex = -1;
    for (int i = 0; i < state->nbTracks; i++) {
        if ((state->routes[i].from == from && state->routes[i].to == to) ||
            (state->routes[i].from == to && state->routes[i].to == from)) {
            routeIndex = i;
            break;
        }
    }
    
    if (routeIndex == -1) {
        printf("ERROR: Route not found!\n");
        return;
    }
    
    Route* route = &state->routes[routeIndex];
    printf("Route #%d: From %d to %d\n", routeIndex, route->from, route->to);
    printf("Length: %d\n", route->length);
    printf("Color: %d\n", route->color);
    
    if (route->secondColor != NONE) {
        printf("Second color: %d\n", route->secondColor);
    }
    
    printf("Owner: %d (0=None, 1=Us, 2=Opponent)\n", route->owner);
    
    printf("\nChosen color for taking route: %d\n", color);
    printf("Number of locomotives: %d\n", nbLocomotives);
    
    printf("\nValidity check:\n");
    
    bool colorValid = false;
    if (route->color == LOCOMOTIVE) {
        colorValid = true;
        printf("OK: Gray route, any color is valid\n");
    } else if (color == route->color || 
              (route->secondColor != NONE && color == route->secondColor) ||
              color == LOCOMOTIVE) {
        colorValid = true;
        printf("OK: Valid color for this route\n");
    } else {
        printf("ERROR: Invalid color! Route accepts %d", route->color);
        
        if (route->secondColor != NONE) {
            printf(" or %d", route->secondColor);
        }
        printf(", but %d was chosen\n", color);
    }
    
    int colorCards = state->nbCardsByColor[color];
    int locomotives = state->nbCardsByColor[LOCOMOTIVE];
    printf("\nAvailable cards:\n");
    printf("- Color %d: %d\n", color, colorCards);
    printf("- Locomotives: %d\n", locomotives);
    
    if (color == LOCOMOTIVE) {
        if (locomotives >= route->length) {
            printf("OK: Enough locomotives to take the route\n");
        } else {
            printf("ERROR: Not enough locomotives (%d needed, %d available)\n", 
                  route->length, locomotives);
        }
    } else {
        int colorCardsNeeded = route->length - nbLocomotives;
        if (colorCards >= colorCardsNeeded && locomotives >= nbLocomotives) {
            printf("OK: Enough cards (%d color %d + %d locomotives)\n", 
                  colorCardsNeeded, color, nbLocomotives);
        } else {
            if (colorCards < colorCardsNeeded) {
                printf("ERROR: Not enough color %d cards (%d needed, %d available)\n", 
                      color, colorCardsNeeded, colorCards);
            }
            if (locomotives < nbLocomotives) {
                printf("ERROR: Not enough locomotives (%d needed, %d available)\n", 
                      nbLocomotives, locomotives);
            }
        }
    }
    
    // Vérifications spéciales pour certaines routes
    if ((from == 17 && to == 22) || (from == 22 && to == 17)) {
        printf("\nWARNING: Special route Kansas City (17) - Saint Louis (22)\n");
        printf("For this route, only BLUE (3), PURPLE (1) or LOCOMOTIVE (9) colors are allowed\n");
        if (color != 3 && color != 1 && color != 9) {
            printf("ERROR: Color %d not allowed for this special route!\n", color);
        } else {
            printf("OK: Color allowed for this special route\n");
        }
    }
    
    if ((from == 31 && to == 32) || (from == 32 && to == 31)) {
        printf("\nWARNING: Special route New York (31) - Washington (32)\n");
        printf("For this route, only BLACK (6), ORANGE (5) or LOCOMOTIVE (9) colors are allowed\n");
        if (color != 6 && color != 5 && color != 9) {
            printf("ERROR: Color %d not allowed for this special route!\n", color);
        } else {
            printf("OK: Color allowed for this special route\n");
        }
    }
    
    printf("===============================\n\n");
}