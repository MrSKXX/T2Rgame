CC = gcc
CFLAGS = -Wall -Wextra -g

SRCS = main.c gamestate.c player.c rules.c strategy.c manual.c ../tickettorideapi/ticketToRide.c ../tickettorideapi/codingGameServer.c
OBJS = $(SRCS:.c=.o)
EXEC = tickettoridebot

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(EXEC)