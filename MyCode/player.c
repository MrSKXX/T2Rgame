#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "player.h"
#include "gamestate.h"
#include "strategy_simple.h"
#include "rules.h"

void cleanupMoveResult(MoveResult *moveResult) {
    if (moveResult->opponentMessage) free(moveResult->opponentMessage);
    if (moveResult->message) free(moveResult->message);
    moveResult->opponentMessage = NULL;
    moveResult->message = NULL;
}

void initPlayer(GameState* state, GameData* gameData) {
    if (!state || !gameData) {
        printf("Error: NULL state or gameData in initPlayer\n");
        return;
    }
    
    initGameState(state, gameData);
    
    printf("Player initialized\n");
    printf("Starting game with %d cities and %d tracks\n", state->nbCities, state->nbTracks);
    
    for (int i = 0; i < 4; i++) {
        if (gameData->cards[i] >= 0 && gameData->cards[i] < 10) {
            addCardToHand(state, gameData->cards[i]);
        } else {
            printf("Warning: Invalid card color: %d\n", gameData->cards[i]);
        }
    }
}

ResultCode playFirstTurn(GameState* state) {
    ResultCode returnCode;
    MoveData myMove;
    MoveResult myMoveResult = {0};
    
    printf("First turn: drawing objectives\n");
    
    myMove.action = DRAW_OBJECTIVES;
    
    returnCode = sendMove(&myMove, &myMoveResult);
    
    if (returnCode != ALL_GOOD) {
        printf("Error sending DRAW_OBJECTIVES: 0x%x\n", returnCode);
        if (myMoveResult.message) {
            printf("Server message: %s\n", myMoveResult.message);
        }
        cleanupMoveResult(&myMoveResult);
        return returnCode;
    }
    
    printf("Received objectives, now choosing which to keep\n");
    
    bool objectivesValid = true;
    for (int i = 0; i < 3; i++) {
        if (myMoveResult.objectives[i].from < 0 || 
            myMoveResult.objectives[i].from >= state->nbCities ||
            myMoveResult.objectives[i].to < 0 || 
            myMoveResult.objectives[i].to >= state->nbCities) {
            printf("WARNING: Invalid objective received: From %d to %d\n", 
                   myMoveResult.objectives[i].from, myMoveResult.objectives[i].to);
            objectivesValid = false;
        }
    }
    
    if (!objectivesValid) {
        printf("ERROR: Invalid objectives received from server\n");
        cleanupMoveResult(&myMoveResult);
        return PARAM_ERROR;
    }
    
    unsigned char chooseObjectives[3] = {1, 1, 1};
    
    chooseObjectivesStrategy(state, myMoveResult.objectives, chooseObjectives);
    
    bool atLeastOneChosen = false;
    for (int i = 0; i < 3; i++) {
        if (chooseObjectives[i]) {
            atLeastOneChosen = true;
            break;
        }
    }
    
    if (!atLeastOneChosen) {
        printf("WARNING: No objectives chosen, selecting the first one by default\n");
        chooseObjectives[0] = 1;
    }
    
    MoveData chooseMove;
    MoveResult chooseMoveResult = {0};
    
    chooseMove.action = CHOOSE_OBJECTIVES;
    chooseMove.chooseObjectives[0] = chooseObjectives[0];
    chooseMove.chooseObjectives[1] = chooseObjectives[1];
    chooseMove.chooseObjectives[2] = chooseObjectives[2];
    
    int objectivesToKeep = 0;
    for (int i = 0; i < 3; i++) {
        if (chooseObjectives[i]) {
            objectivesToKeep++;
        }
    }
    
    Objective chosenObjectives[3];
    int idx = 0;
    for (int i = 0; i < 3; i++) {
        if (chooseObjectives[i]) {
            chosenObjectives[idx++] = myMoveResult.objectives[i];
        }
    }
    
    cleanupMoveResult(&myMoveResult);
    
    returnCode = sendMove(&chooseMove, &chooseMoveResult);
    
    if (returnCode != ALL_GOOD) {
        printf("Error choosing objectives: 0x%x\n", returnCode);
        if (chooseMoveResult.message) {
            printf("Server message: %s\n", chooseMoveResult.message);
        }
        cleanupMoveResult(&chooseMoveResult);
        return returnCode;
    }
    
    addObjectives(state, chosenObjectives, objectivesToKeep);
    
    printf("Successfully chose objectives\n");
    cleanupMoveResult(&chooseMoveResult);
    
    return ALL_GOOD;
}

ResultCode playTurn(GameState* state) {
    ResultCode returnCode;
    MoveData myMove;
    MoveResult myMoveResult = {0};
    BoardState boardState;
    
    static int cardDrawnThisTurn = 0;
    
    returnCode = getBoardState(&boardState);
    if (returnCode != ALL_GOOD) {
        printf("Error getting board state: 0x%x\n", returnCode);
        return returnCode;
    }
    
    for (int i = 0; i < 5; i++) {
        state->visibleCards[i] = boardState.card[i];
    }

    if (cardDrawnThisTurn == 1) {
        CardColor cardToDraw = (CardColor)-1;
        for (int i = 0; i < 5; i++) {
            if (state->visibleCards[i] != LOCOMOTIVE && state->visibleCards[i] != NONE) {
                cardToDraw = state->visibleCards[i];
                break;
            }
        }
        
        if (cardToDraw != (CardColor)-1) {
            myMove.action = DRAW_CARD;
            myMove.drawCard = cardToDraw;
        } else {
            myMove.action = DRAW_BLIND_CARD;
        }
        
        cardDrawnThisTurn = 0;
    } else {
        if (state->wagonsLeft <= 1) {
            myMove.action = DRAW_BLIND_CARD;
        } else {
            int moveResult = decideNextMove(state, &myMove);
            if (moveResult != 1) {
                myMove.action = DRAW_BLIND_CARD;
            }
        }
    }
    
    // Validation finale avant envoi
    if (myMove.action == CLAIM_ROUTE) {
        int from = myMove.claimRoute.from;
        int to = myMove.claimRoute.to;
        
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            printf("Invalid cities: %d -> %d\n", from, to);
            myMove.action = DRAW_BLIND_CARD;
        }
        else {
            int routeIndex = findRouteIndex(state, from, to);
            if (routeIndex >= 0) {
                int length = state->routes[routeIndex].length;
                if (length > state->wagonsLeft) {
                    printf("Not enough wagons (need %d, have %d)\n", length, state->wagonsLeft);
                    myMove.action = DRAW_BLIND_CARD;
                }
                
                if (state->routes[routeIndex].owner != 0) {
                    printf("Route already taken\n");
                    myMove.action = DRAW_BLIND_CARD;
                }
            } else {
                printf("Route does not exist\n");
                myMove.action = DRAW_BLIND_CARD;
            }
        }
        
        if (myMove.claimRoute.color < PURPLE || myMove.claimRoute.color > LOCOMOTIVE) {
            printf("Invalid color: %d\n", myMove.claimRoute.color);
            myMove.action = DRAW_BLIND_CARD;
        }
    }
    
    returnCode = sendMove(&myMove, &myMoveResult);
    
    // PREMIÈRE VÉRIFICATION : Détecter les résultats de fin de partie dans la réponse normale
    if (returnCode == ALL_GOOD && myMoveResult.message && 
        ((strstr(myMoveResult.message, "Georges:") != NULL && strstr(myMoveResult.message, "PlayNice:") != NULL) ||
         strstr(myMoveResult.message, "Total score:") != NULL ||
         strstr(myMoveResult.message, "longest path") != NULL)) {
        
        printf("=== GAME RESULTS DETECTED IN NORMAL RESPONSE ===\n");
        printf("%s\n", myMoveResult.message);
        state->lastTurn = 2; // Marquer fin de partie
        cleanupMoveResult(&myMoveResult);
        return ALL_GOOD; // Retourner succès - la partie est finie
    }
    
    // DEUXIÈME VÉRIFICATION : Dans le state du move result
    if (returnCode == ALL_GOOD && myMoveResult.state == 1) {
        printf("=== GAME ENDED - MOVE STATE INDICATES END ===\n");
        state->lastTurn = 2; // Marquer fin de partie
        
        // Essayer de récupérer les résultats avec getMove
        MoveData finalMove;
        MoveResult finalResult = {0};
        getMove(&finalMove, &finalResult);
        
        if (finalResult.message) {
            printf("Final results message: %s\n", finalResult.message);
            // Ne pas nettoyer ici, on laisse le main() le faire
        }
        
        cleanupMoveResult(&finalResult);
        cleanupMoveResult(&myMoveResult);
        return ALL_GOOD;
    }
    
    // Gestion plus douce des erreurs de fin de partie
    if (returnCode == SERVER_ERROR || returnCode == PARAM_ERROR) {
        
        if (myMoveResult.message) {
            printf("Server response: %s\n", myMoveResult.message);
            
            // Détecter les messages de fin de partie complets
            if (strstr(myMoveResult.message, "Total score") != NULL ||
                (strstr(myMoveResult.message, "Georges:") != NULL && 
                 strstr(myMoveResult.message, "PlayNice:") != NULL) ||
                strstr(myMoveResult.message, "longest path") != NULL) {
                
                printf("=== GAME RESULTS RECEIVED ===\n");
                state->lastTurn = 2; // Marquer fin de partie
                cleanupMoveResult(&myMoveResult);
                return ALL_GOOD; // Retourner succès pour éviter erreur
            }
            
            // Messages de protocole = fin de partie aussi
            if (strstr(myMoveResult.message, "Bad protocol") ||
                strstr(myMoveResult.message, "WAIT_GAME")) {
                printf("=== GAME ENDED - PROTOCOL MESSAGE ===\n");
                state->lastTurn = 2; // Marquer fin de partie
                cleanupMoveResult(&myMoveResult);
                return ALL_GOOD; // Pas une vraie erreur - jeu fini
            }
        }
        
        cleanupMoveResult(&myMoveResult);
        return returnCode;
    }
    
    if (returnCode != ALL_GOOD) {
        printf("Error sending move: 0x%x\n", returnCode);
        cleanupMoveResult(&myMoveResult);
        return returnCode;
    }
    
    switch (myMove.action) {
        case CLAIM_ROUTE:
            if (myMoveResult.state == NORMAL_MOVE) {
                addClaimedRoute(state, myMove.claimRoute.from, myMove.claimRoute.to);
                
                int routeLength = 0;
                for (int i = 0; i < state->nbTracks; i++) {
                    if ((state->routes[i].from == myMove.claimRoute.from && state->routes[i].to == myMove.claimRoute.to) ||
                        (state->routes[i].from == myMove.claimRoute.to && state->routes[i].to == myMove.claimRoute.from)) {
                        routeLength = state->routes[i].length;
                        break;
                    }
                }
                
                removeCardsForRoute(state, myMove.claimRoute.color, routeLength, myMove.claimRoute.nbLocomotives);
                
                if (state->wagonsLeft <= 2) {
                    state->lastTurn = 1;
                }
            } else {
                printf("CLAIM_ROUTE not confirmed, state: %d\n", myMoveResult.state);
                
                // Si state == 1, c'est la fin de partie !
                if (myMoveResult.state == 1) {
                    printf("=== GAME ENDED INDICATED BY MOVE STATE ===\n");
                    state->lastTurn = 2; // Marquer fin de partie
                    
                    // Les résultats vont arriver dans le prochain getMove()
                    cardDrawnThisTurn = 0;
                    cleanupMoveResult(&myMoveResult);
                    return ALL_GOOD; // Sortir immédiatement
                }
            }
            cardDrawnThisTurn = 0;
            break;
            
        case DRAW_CARD:
            addCardToHand(state, myMove.drawCard);
            
            if (myMove.drawCard == LOCOMOTIVE) {
                cardDrawnThisTurn = 0;
            } else {
                if (cardDrawnThisTurn == 0) {
                    cardDrawnThisTurn = myMoveResult.replay ? 1 : 0;
                } else {
                    cardDrawnThisTurn = 0;
                }
            }
            break;
            
        case DRAW_BLIND_CARD:
            addCardToHand(state, myMoveResult.card);
            
            if (myMoveResult.card == LOCOMOTIVE && !myMoveResult.replay) {
                cardDrawnThisTurn = 0;
            } else {
                if (cardDrawnThisTurn == 0 && myMoveResult.replay) {
                    cardDrawnThisTurn = 1;
                } else {
                    cardDrawnThisTurn = 0;
                }
            }
            break;
            
        case DRAW_OBJECTIVES:
            {
                unsigned char chooseObjectives[3] = {1, 1, 1};
                chooseObjectivesStrategy(state, myMoveResult.objectives, chooseObjectives);
                
                MoveData chooseMove;
                MoveResult chooseMoveResult = {0};
                
                chooseMove.action = CHOOSE_OBJECTIVES;
                chooseMove.chooseObjectives[0] = chooseObjectives[0];
                chooseMove.chooseObjectives[1] = chooseObjectives[1];
                chooseMove.chooseObjectives[2] = chooseObjectives[2];
                
                int objectivesToKeep = 0;
                Objective chosenObjectives[3];
                int idx = 0;
                for (int i = 0; i < 3; i++) {
                    if (chooseObjectives[i]) {
                        objectivesToKeep++;
                        chosenObjectives[idx++] = myMoveResult.objectives[i];
                    }
                }
                
                cleanupMoveResult(&myMoveResult);
                
                returnCode = sendMove(&chooseMove, &chooseMoveResult);
                
                if (returnCode != ALL_GOOD) {
                    printf("Error choosing objectives: 0x%x\n", returnCode);
                    cleanupMoveResult(&chooseMoveResult);
                    return returnCode;
                }
                
                addObjectives(state, chosenObjectives, objectivesToKeep);
                
                cleanupMoveResult(&chooseMoveResult);
            }
            cardDrawnThisTurn = 0;
            break;
    }
    
    if (myMove.action != DRAW_OBJECTIVES) {
        cleanupMoveResult(&myMoveResult);
    }
    
    updateCityConnectivity(state);
    
    if (cardDrawnThisTurn == 1) {
        ResultCode updateResult = getBoardState(&boardState);
        if (updateResult != ALL_GOOD) {
            printf("Error getting board state for second card: 0x%x\n", updateResult);
            return updateResult;
        }
        
        for (int i = 0; i < 5; i++) {
            state->visibleCards[i] = boardState.card[i];
        }
        
        MoveData secondCardMove;
        MoveResult secondCardResult = {0};
        
        CardColor cardToDraw = (CardColor)-1;
        for (int i = 0; i < 5; i++) {
            if (state->visibleCards[i] != LOCOMOTIVE && state->visibleCards[i] != NONE) {
                cardToDraw = state->visibleCards[i];
                break;
            }
        }
        
        if (cardToDraw != (CardColor)-1) {
            secondCardMove.action = DRAW_CARD;
            secondCardMove.drawCard = cardToDraw;
        } else {
            secondCardMove.action = DRAW_BLIND_CARD;
        }
        
        returnCode = sendMove(&secondCardMove, &secondCardResult);
        
        if (returnCode != ALL_GOOD) {
            printf("Error drawing second card: 0x%x\n", returnCode);
            cleanupMoveResult(&secondCardResult);
            return returnCode;
        }
        
        if (secondCardMove.action == DRAW_CARD) {
            addCardToHand(state, secondCardMove.drawCard);
        } else {
            addCardToHand(state, secondCardResult.card);
        }
        
        cardDrawnThisTurn = 0;
        cleanupMoveResult(&secondCardResult);
    }
    
    return ALL_GOOD;
}