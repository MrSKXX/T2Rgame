#ifndef DEBUG_H
#define DEBUG_H

#include <stdarg.h>
#include <stdio.h>

// Niveau de débogage
// 0 = aucun message de débogage
// 1 = messages importants seulement
// 2 = messages détaillés
// 3 = tous les messages (très verbeux)
#define DEBUG_LEVEL 0

// Fonction utilitaire pour afficher les messages de débogage selon le niveau
void debugPrint(int level, const char* format, ...) {
    if (level <= DEBUG_LEVEL) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
    }
}

#endif // DEBUG_H