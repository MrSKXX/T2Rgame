CC = gcc
CFLAGS = -Wall -Wextra -g

# Fichiers sources principaux
MAIN_SRCS = main.c gamestate.c player.c rules.c strategy.c

# Fichiers API
API_SRCS = ../tickettorideapi/ticketToRide.c ../tickettorideapi/clientAPI.c

# Tous les fichiers sources
SRCS = $(MAIN_SRCS) $(API_SRCS)

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

# Règle pour forcer la recompilation complète
rebuild: clean all

.PHONY: all clean rebuild