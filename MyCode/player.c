#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "player.h"
#include "gamestate.h"
#include "strategy.h"
#include "rules.h"

// Fonction pour afficher les informations d'une carte
void printCardName(CardColor card) {
    const char* cardNames[] = {"None", "Purple", "White", "Blue", "Yellow", 
                              "Orange", "Black", "Red", "Green", "Locomotive"};
    if (card >= 0 && card < 10) {
        printf("Card color: %s\n", cardNames[card]);
    } else {
        printf("Unknown card: %d\n", card);
    }
}

// Fonction pour afficher les informations d'un objectif
void printObjective(Objective objective) {
    printf("From city %d to city %d, score %d\n", 
           objective.from, objective.to, objective.score);
    printf("  From: ");
    printCity(objective.from);
    printf(" to ");
    printCity(objective.to);
    printf("\n");
}

// Libère la mémoire allouée dans MoveResult
void cleanupMoveResult(MoveResult *moveResult) {
    if (moveResult->opponentMessage) free(moveResult->opponentMessage);
    if (moveResult->message) free(moveResult->message);
    moveResult->opponentMessage = NULL;
    moveResult->message = NULL;
}

// Initialise le joueur
void initPlayer(GameState* state, StrategyType strategy, GameData* gameData) {
    // Vérifier les paramètres
    if (!state || !gameData) {
        printf("Error: NULL state or gameData in initPlayer\n");
        return;
    }
    
    // Initialise l'état du jeu avec les données initiales
    initGameState(state, gameData);
    
    printf("Player initialized with strategy type: %d\n", strategy);
    printf("Starting game with %d cities and %d tracks\n", state->nbCities, state->nbTracks);
    
    // Vérifier l'état après initialisation
    printf("Debug - Wagons left after init: %d\n", state->wagonsLeft);
    
    // Initialisation des cartes en main (les 4 cartes initiales)
    for (int i = 0; i < 4; i++) {
        if (gameData->cards[i] >= 0 && gameData->cards[i] < 10) {
            addCardToHand(state, gameData->cards[i]);
        } else {
            printf("Warning: Invalid card color: %d\n", gameData->cards[i]);
        }
    }
    
    // Affiche l'état initial
    printGameState(state);
}

// Gère le premier tour (choix des objectifs)
ResultCode playFirstTurn(GameState* state) {
    ResultCode returnCode;
    MoveData myMove;
    MoveResult myMoveResult;
    
    printf("First turn: drawing objectives\n");
    
    // Demande de piocher des objectifs
    myMove.action = DRAW_OBJECTIVES;
    
    // Envoie la requête
    returnCode = sendMove(&myMove, &myMoveResult);
    
    if (returnCode != ALL_GOOD) {
        printf("Error sending DRAW_OBJECTIVES: 0x%x\n", returnCode);
        cleanupMoveResult(&myMoveResult);
        return returnCode;
    }
    
    printf("Received objectives, now choosing which to keep\n");
    
    // Affiche les objectifs reçus
    for (int i = 0; i < 3; i++) {
        printf("Objective %d: ", i+1);
        printObjective(myMoveResult.objectives[i]);
    }
    
    // Choisit quels objectifs garder (stratégie simple: garder tous les objectifs)
    bool chooseObjectives[3] = {true, true, true};
    
    // Si nous avons une stratégie plus avancée pour choisir les objectifs
    chooseObjectivesStrategy(state, myMoveResult.objectives, chooseObjectives);
    
    // Prépare la réponse avec les choix d'objectifs
    MoveData chooseMove;
    MoveResult chooseMoveResult;
    
    chooseMove.action = CHOOSE_OBJECTIVES;
    chooseMove.chooseObjectives[0] = chooseObjectives[0];
    chooseMove.chooseObjectives[1] = chooseObjectives[1];
    chooseMove.chooseObjectives[2] = chooseObjectives[2];
    
    // Compte combien d'objectifs nous gardons pour ajouter à notre état
    int objectivesToKeep = 0;
    for (int i = 0; i < 3; i++) {
        if (chooseObjectives[i]) {
            objectivesToKeep++;
        }
    }
    
    // Ajoute les objectifs choisis à notre état
    Objective chosenObjectives[3];
    int idx = 0;
    for (int i = 0; i < 3; i++) {
        if (chooseObjectives[i]) {
            chosenObjectives[idx++] = myMoveResult.objectives[i];
        }
    }
    addObjectives(state, chosenObjectives, objectivesToKeep);
    
    // Libère la mémoire
    cleanupMoveResult(&myMoveResult);
    
    // Envoie les choix
    returnCode = sendMove(&chooseMove, &chooseMoveResult);
    
    if (returnCode != ALL_GOOD) {
        printf("Error choosing objectives: 0x%x\n", returnCode);
        cleanupMoveResult(&chooseMoveResult);
        return returnCode;
    }
    
    printf("Successfully chose objectives\n");
    cleanupMoveResult(&chooseMoveResult);
    
    return ALL_GOOD;
}

// Joue un tour normal
ResultCode playTurn(GameState* state, StrategyType strategy) {
    ResultCode returnCode;
    MoveData myMove;
    MoveResult myMoveResult;
    BoardState boardState;
    
    // Récupère l'état du plateau (cartes visibles)
    returnCode = getBoardState(&boardState);
    if (returnCode != ALL_GOOD) {
        printf("Error getting board state: 0x%x\n", returnCode);
        return returnCode;
    }
    
    // Met à jour les cartes visibles dans notre état
    for (int i = 0; i < 5; i++) {
        state->visibleCards[i] = boardState.card[i];
    }
    
    // Décide de l'action à effectuer selon la stratégie
    if (!decideNextMove(state, strategy, &myMove)) {
        // Si aucune décision n'a été prise, on pioche une carte aveugle par défaut
        printf("No specific move decided, drawing blind card by default\n");
        myMove.action = DRAW_BLIND_CARD;
    }
    
    // Affiche l'action choisie
    printf("Decided action: ");
    switch (myMove.action) {
        case CLAIM_ROUTE:
            printf("Claim route from ");
            printCity(myMove.claimRoute.from);
            printf(" to ");
            printCity(myMove.claimRoute.to);
            printf(" with color ");
            printCardName(myMove.claimRoute.color);
            printf(" using %d locomotives\n", myMove.claimRoute.nbLocomotives);
            break;
            
        case DRAW_CARD:
            printf("Draw visible card: ");
            printCardName(myMove.drawCard);
            break;
            
        case DRAW_BLIND_CARD:
            printf("Draw blind card\n");
            break;
            
        case DRAW_OBJECTIVES:
            printf("Draw objectives\n");
            break;
            
        default:
            printf("Unknown action type: %d\n", myMove.action);
    }
    
    // Envoie l'action
    returnCode = sendMove(&myMove, &myMoveResult);
    
    // Vérifier si c'est la fin du jeu en recherchant dans le message
    if (returnCode == SERVER_ERROR || returnCode == PARAM_ERROR) {
        printf("Game has ended. Return code: 0x%x\n", returnCode);
        
        // Si le message contient "winner", c'est une fin normale de partie
        if (myMoveResult.message && strstr(myMoveResult.message, "winner")) {
            printf("GAME OVER - Partie terminée normalement. Message: %s\n", myMoveResult.message);
            
            // Essayons d'extraire le nom du gagnant
            char winner[100] = "unknown";
            sscanf(myMoveResult.message, "{\"state\": 1, \"winner\": \"%99[^\"]\"}", winner);
            printf("Le gagnant est: %s\n", winner);
            
            // C'est une fin normale, pas une erreur
            cleanupMoveResult(&myMoveResult);
            return SERVER_ERROR; // Indique la fin du jeu, mais ce n'est pas une erreur
        }
        
        if (myMoveResult.opponentMessage) {
            printf("Server message: %s\n", myMoveResult.opponentMessage);
        }
        if (myMoveResult.message) {
            printf("Message: %s\n", myMoveResult.message);
        }
        cleanupMoveResult(&myMoveResult);
        return returnCode;
    }
    
    if (returnCode != ALL_GOOD) {
        printf("Error sending move: 0x%x\n", returnCode);
        cleanupMoveResult(&myMoveResult);
        return returnCode;
    }
    
    // Traite le résultat de l'action
    switch (myMove.action) {
        case CLAIM_ROUTE:
            // Met à jour notre état après avoir pris une route
            addClaimedRoute(state, myMove.claimRoute.from, myMove.claimRoute.to);
            
            // Trouve la longueur de la route
            int routeLength = 0;
            for (int i = 0; i < state->nbTracks; i++) {
                if ((state->routes[i].from == myMove.claimRoute.from && state->routes[i].to == myMove.claimRoute.to) ||
                    (state->routes[i].from == myMove.claimRoute.to && state->routes[i].to == myMove.claimRoute.from)) {
                    routeLength = state->routes[i].length;
                    break;
                }
            }
            
            // Retire les cartes utilisées
            removeCardsForRoute(state, myMove.claimRoute.color, routeLength, myMove.claimRoute.nbLocomotives);
            printf("Successfully claimed route\n");
            break;
            
        case DRAW_CARD:
            // Cartes visibles - pas besoin de traiter le résultat
            addCardToHand(state, myMove.drawCard);
            printf("Successfully drew visible card\n");
            break;
            
        case DRAW_BLIND_CARD:
            // Ajoute la carte piochée à notre main
            addCardToHand(state, myMoveResult.card);
            printf("Successfully drew blind card: ");
            printCardName(myMoveResult.card);
            break;
            
        case DRAW_OBJECTIVES:
            // Affiche les objectifs reçus
            printf("Received objectives, now choosing which to keep\n");
            for (int i = 0; i < 3; i++) {
                printf("Objective %d: ", i+1);
                printObjective(myMoveResult.objectives[i]);
            }
            
            // Choisit quels objectifs garder
            bool chooseObjectives[3] = {true, true, true};
            chooseObjectivesStrategy(state, myMoveResult.objectives, chooseObjectives);
            
            // Prépare la réponse
            MoveData chooseMove;
            MoveResult chooseMoveResult;
            
            chooseMove.action = CHOOSE_OBJECTIVES;
            chooseMove.chooseObjectives[0] = chooseObjectives[0];
            chooseMove.chooseObjectives[1] = chooseObjectives[1];
            chooseMove.chooseObjectives[2] = chooseObjectives[2];
            
            // Compte combien d'objectifs nous gardons
            int objectivesToKeep = 0;
            for (int i = 0; i < 3; i++) {
                if (chooseObjectives[i]) objectivesToKeep++;
            }
            
            // Crée un tableau des objectifs choisis
            Objective chosenObjectives[3];
            int idx = 0;
            for (int i = 0; i < 3; i++) {
                if (chooseObjectives[i]) {
                    chosenObjectives[idx++] = myMoveResult.objectives[i];
                }
            }
            
            // Libère la mémoire
            cleanupMoveResult(&myMoveResult);
            
            // Envoie les choix
            returnCode = sendMove(&chooseMove, &chooseMoveResult);
            
            if (returnCode != ALL_GOOD) {
                printf("Error choosing objectives: 0x%x\n", returnCode);
                cleanupMoveResult(&chooseMoveResult);
                return returnCode;
            }
            
            // Ajoute les objectifs choisis à notre état
            addObjectives(state, chosenObjectives, objectivesToKeep);
            
            printf("Successfully chose objectives\n");
            cleanupMoveResult(&chooseMoveResult);
            break;
    }
    
    // Libère la mémoire si ce n'est pas déjà fait
    if (myMove.action != DRAW_OBJECTIVES) {
        cleanupMoveResult(&myMoveResult);
    }
    
    // Met à jour la connectivité des villes après notre action
    updateCityConnectivity(state);
    
    return ALL_GOOD;
}