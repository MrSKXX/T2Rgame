CC = gcc
CFLAGS = -Wall -Wextra -g

# Fichiers sources principaux
MAIN_SRCS = main.c gamestate.c player.c rules.c debug.c

# Fichiers sources de stratégie (dans le dossier strategy/)
STRATEGY_SRCS = strategy/strategy_core.c \
                strategy/game_analysis.c \
                strategy/objective_analysis.c \
                strategy/pathfinding.c \
                strategy/card_management.c \
                strategy/opponent_modeling.c \
                strategy/execution.c

# Fichiers API
API_SRCS = ../tickettorideapi/ticketToRide.c ../tickettorideapi/clientAPI.c

# Tous les fichiers sources
SRCS = $(MAIN_SRCS) $(STRATEGY_SRCS) $(API_SRCS)

# Fichiers objets
OBJS = $(SRCS:.c=.o)

# Exécutable
EXEC = tickettoridebot

# Règle principale
all: $(EXEC)

# Création de l'exécutable
$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compilation des fichiers .c en .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Nettoyage
clean:
	rm -f $(OBJS) $(EXEC)
	rm -f strategy/*.o

# Règle pour forcer la recompilation complète
rebuild: clean all

.PHONY: all clean rebuild