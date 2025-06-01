#ifndef DEBUG_H
#define DEBUG_H

#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include "gamestate.h"

// Niveau de débogage
// 0 = aucun message de débogage
// 1 = messages importants seulement
// 2 = messages détaillés
// 3 = tous les messages (très verbeux)
#define DEBUG_LEVEL 1  // Augmenté de 0 à 1 pour voir les erreurs importantes

// Fonction utilitaire pour afficher les messages de débogage selon le niveau
void debugLog(int level, const char* format, ...);

// Nouvelle fonction pour analyser les objectifs en profondeur
void debugObjectives(GameState* state);

// Nouvelle fonction pour analyser les détails d'une route spécifique
void debugRoute(GameState* state, int from, int to, CardColor color, int nbLocomotives);

#endif // DEBUG_H