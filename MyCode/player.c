#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "player.h"
#include "gamestate.h"
#include "strategy/strategy.h"
#include "rules.h"

extern void checkObjectivesPaths(GameState* state);

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
    
    bool chooseObjectives[3] = {true, true, true};
    
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
        chooseObjectives[0] = true;
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
    
    checkObjectivesPaths(state);

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
        int moveResult = decideNextMove(state, &myMove);
        if (moveResult != 1) {
            myMove.action = DRAW_BLIND_CARD;
        }
    }
    
    // Validation de sécurité pour CLAIM_ROUTE
    if (myMove.action == CLAIM_ROUTE) {
        CardColor color = myMove.claimRoute.color;
        
        if (color < PURPLE || color > LOCOMOTIVE) {
            printf("ERROR: Invalid color detected: %d, correcting to GREEN (8)\n", color);
            myMove.claimRoute.color = GREEN;
        }
        
        int routeIndex = findRouteIndex(state, myMove.claimRoute.from, myMove.claimRoute.to);
        if (routeIndex >= 0) {
            int length = state->routes[routeIndex].length;
            CardColor routeColor = state->routes[routeIndex].color;
            
            if (routeColor != LOCOMOTIVE && color != routeColor && 
                color != state->routes[routeIndex].secondColor && color != LOCOMOTIVE) {
                
                printf("CORRECTION: Wrong color for route %d->%d (chosen: %d, route: %d)\n", 
                      myMove.claimRoute.from, myMove.claimRoute.to, color, routeColor);
                
                if (state->nbCardsByColor[routeColor] >= length) {
                    myMove.claimRoute.color = routeColor;
                    myMove.claimRoute.nbLocomotives = 0;
                } else if (state->nbCardsByColor[LOCOMOTIVE] >= length) {
                    myMove.claimRoute.color = LOCOMOTIVE;
                    myMove.claimRoute.nbLocomotives = length;
                } else if (state->nbCardsByColor[routeColor] + state->nbCardsByColor[LOCOMOTIVE] >= length) {
                    myMove.claimRoute.color = routeColor;
                    myMove.claimRoute.nbLocomotives = length - state->nbCardsByColor[routeColor];
                } else {
                    printf("ERROR: Not enough cards for this route! Drawing instead.\n");
                    myMove.action = DRAW_BLIND_CARD;
                }
            }
        }
    }
    
    // Vérification ultime pour CLAIM_ROUTE
    if (myMove.action == CLAIM_ROUTE) {
        if (myMove.claimRoute.from < 0 || myMove.claimRoute.from >= state->nbCities || 
            myMove.claimRoute.to < 0 || myMove.claimRoute.to >= state->nbCities) {
            printf("FATAL ERROR: Invalid cities: %d -> %d\n", 
                  myMove.claimRoute.from, myMove.claimRoute.to);
            myMove.action = DRAW_BLIND_CARD;
        }
        else {
            bool routeFound = false;
            for (int i = 0; i < state->nbTracks; i++) {
                if ((state->routes[i].from == myMove.claimRoute.from && 
                     state->routes[i].to == myMove.claimRoute.to) ||
                    (state->routes[i].from == myMove.claimRoute.to && 
                     state->routes[i].to == myMove.claimRoute.from)) {
                    
                    routeFound = true;
                    
                    if (state->routes[i].owner != 0) {
                        printf("FATAL ERROR: Route already taken: %d -> %d\n", 
                              myMove.claimRoute.from, myMove.claimRoute.to);
                        myMove.action = DRAW_BLIND_CARD;
                    }
                    break;
                }
            }
            
            if (!routeFound) {
                printf("FATAL ERROR: Route does not exist: %d -> %d\n", 
                      myMove.claimRoute.from, myMove.claimRoute.to);
                myMove.action = DRAW_BLIND_CARD;
            }
        }
        
        if (myMove.claimRoute.color < PURPLE || myMove.claimRoute.color > LOCOMOTIVE) {
            printf("FATAL ERROR: Invalid color: %d\n", myMove.claimRoute.color);
            myMove.action = DRAW_BLIND_CARD;
        }
    }
    
    returnCode = sendMove(&myMove, &myMoveResult);
    
    if (returnCode == SERVER_ERROR || returnCode == PARAM_ERROR) {
        printf("Game end or error: 0x%x\n", returnCode);
        
        if (myMoveResult.message && (
            strstr(myMoveResult.message, "Total score") || 
            strstr(myMoveResult.message, "winner") ||
            strstr(myMoveResult.message, "Final Score"))) {
            
            printf("\n==================================================\n");
            printf("           FINAL RESULT DETECTED                 \n");
            printf("==================================================\n");
            printf("%s\n", myMoveResult.message);
            printf("==================================================\n\n");
            
            state->lastTurn = 2;
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
            } else {
                printf("WARNING: CLAIM_ROUTE not confirmed by server, state: %d\n", myMoveResult.state);
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
            bool chooseObjectives[3] = {true, true, true};
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