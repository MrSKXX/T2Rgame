/**
 * debug.h
 * Système de debug minimaliste
 */
#ifndef DEBUG_H
#define DEBUG_H

#include <stdarg.h>
#include <stdio.h>
#include "gamestate.h"

// Debug activé seulement si nécessaire
#define DEBUG_LEVEL 0  // 0=off, 1=errors only

// Fonction de debug conditionnelle
void debugLog(int level, const char* format, ...);

#endif