/**
 * debug.h
 * Système de debug conditionnel
 */
#ifndef DEBUG_H
#define DEBUG_H

#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include "gamestate.h"

// Niveau de debug global
#define DEBUG_LEVEL 1  // 0=off, 1=errors, 2=verbose

// Fonctions de debug
void debugLog(int level, const char* format, ...);
void debugObjectives(GameState* state);  // Ne s'exécute que si DEBUG_LEVEL >= 2
void debugRoute(GameState* state, int from, int to, CardColor color, int nbLocomotives);

#endif