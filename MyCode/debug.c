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
