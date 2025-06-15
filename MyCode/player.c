#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "player.h"
#include "gamestate.h"
#include "strategy.h"
#include "rules.h"

void cleanupMoveResult(MoveResult *moveResult) {
    if (moveResult->opponentMessage) free(moveResult->opponentMessage);
    if (moveResult->message) free(moveResult->message);
    moveResult->opponentMessage = NULL;
    moveResult->message = NULL;
}

void initPlayer(GameState* state, GameData* gameData) {
    if (!state || !gameData) {
        return;
    }
    
    initGameState(state, gameData);
    
    for (int i = 0; i < 4; i++) {
        if (gameData->cards[i] >= 0 && gameData->cards[i] < 10) {
            addCardToHand(state, gameData->cards[i]);
        }
    }
}

ResultCode playFirstTurn(GameState* state) {
    ResultCode returnCode;
    MoveData myMove;
    MoveResult myMoveResult = {0};
    
    myMove.action = DRAW_OBJECTIVES;
    
    returnCode = sendMove(&myMove, &myMoveResult);
    
    if (returnCode != ALL_GOOD) {
        cleanupMoveResult(&myMoveResult);
        return returnCode;
    }
    
    bool objectivesValid = true;
    for (int i = 0; i < 3; i++) {
        if ((int)myMoveResult.objectives[i].from >= state->nbCities ||
            (int)myMoveResult.objectives[i].to >= state->nbCities) {
            objectivesValid = false;
        }
    }
    
    if (!objectivesValid) {
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
        cleanupMoveResult(&chooseMoveResult);
        return returnCode;
    }
    
    addObjectives(state, chosenObjectives, objectivesToKeep);
    
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
        return returnCode;
    }
    
    if (state->wagonsLeft <= 0) {
        state->lastTurn = 1;
        return ALL_GOOD; 
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
    
    if (myMove.action == CLAIM_ROUTE) {
        int from = myMove.claimRoute.from;
        int to = myMove.claimRoute.to;
        
        if (from < 0 || from >= state->nbCities || to < 0 || to >= state->nbCities) {
            myMove.action = DRAW_BLIND_CARD;
        }
        else {
            int routeIndex = findRouteIndex(state, from, to);
            if (routeIndex >= 0) {
                int length = state->routes[routeIndex].length;
                if (length > state->wagonsLeft) {
                    myMove.action = DRAW_BLIND_CARD;
                }
                
                if (state->routes[routeIndex].owner != 0) {
                    myMove.action = DRAW_BLIND_CARD;
                }
            } else {
                myMove.action = DRAW_BLIND_CARD;
            }
        }
        
        if (myMove.claimRoute.color < PURPLE || myMove.claimRoute.color > LOCOMOTIVE) {
            myMove.action = DRAW_BLIND_CARD;
        }
    }
    
    returnCode = sendMove(&myMove, &myMoveResult);
    if (returnCode == ALL_GOOD && myMoveResult.message) {
        if (strstr(myMoveResult.message, "[sendCGSMove]") != NULL ||
            strstr(myMoveResult.message, "Total score:") != NULL ||
            strstr(myMoveResult.message, "✔Objective") != NULL ||
            strstr(myMoveResult.message, "longest path") != NULL) {
            
            state->lastTurn = 2;
            cleanupMoveResult(&myMoveResult);
            return ALL_GOOD;
        }
    }
    
    if (returnCode == ALL_GOOD && myMoveResult.message && 
        ((strstr(myMoveResult.message, "Georges:") != NULL && strstr(myMoveResult.message, "PlayNice:") != NULL) ||
         strstr(myMoveResult.message, "Total score:") != NULL ||
         strstr(myMoveResult.message, "longest path") != NULL)) {
        
        state->lastTurn = 2;
        cleanupMoveResult(&myMoveResult);
        return ALL_GOOD;
    }
    
    if (returnCode == ALL_GOOD && myMoveResult.state == 1) {
        state->lastTurn = 2;
        
        MoveData finalMove;
        MoveResult finalResult = {0};
        getMove(&finalMove, &finalResult);
        
        cleanupMoveResult(&finalResult);
        cleanupMoveResult(&myMoveResult);
        return ALL_GOOD;
    }
    
    if (returnCode == SERVER_ERROR || returnCode == PARAM_ERROR) {
        if (myMoveResult.message) {
            if (strstr(myMoveResult.message, "Total score") != NULL ||
                (strstr(myMoveResult.message, "Georges:") != NULL && 
                 strstr(myMoveResult.message, "PlayNice:") != NULL) ||
                strstr(myMoveResult.message, "longest path") != NULL) {
                
                state->lastTurn = 2;
                cleanupMoveResult(&myMoveResult);
                return ALL_GOOD;
            }
            
            if (strstr(myMoveResult.message, "Bad protocol") ||
                strstr(myMoveResult.message, "WAIT_GAME")) {
                state->lastTurn = 2;
                cleanupMoveResult(&myMoveResult);
                return ALL_GOOD;
            }
        }
        
        cleanupMoveResult(&myMoveResult);
        return returnCode;
    }
    
    if (returnCode != ALL_GOOD) {
        cleanupMoveResult(&myMoveResult);
        return returnCode;
    }
    
    switch (myMove.action) {
        case CLAIM_ROUTE:
            if (myMoveResult.state == NORMAL_MOVE) {
                addClaimedRoute(state, myMove.claimRoute.from, myMove.claimRoute.to);
                
                int routeLength = 0;
                for (int i = 0; i < state->nbTracks; i++) {
                    if (((unsigned int)state->routes[i].from == myMove.claimRoute.from && 
                         (unsigned int)state->routes[i].to == myMove.claimRoute.to) ||
                        ((unsigned int)state->routes[i].from == myMove.claimRoute.to && 
                         (unsigned int)state->routes[i].to == myMove.claimRoute.from)) {
                        routeLength = state->routes[i].length;
                        break;
                    }
                }
                
                removeCardsForRoute(state, myMove.claimRoute.color, routeLength, myMove.claimRoute.nbLocomotives);
                
                if (state->wagonsLeft <= 2) {
                    state->lastTurn = 1;
                }
            } else {
                if (myMoveResult.state == 1) {
                    state->lastTurn = 2;
                    cardDrawnThisTurn = 0;
                    cleanupMoveResult(&myMoveResult);
                    return ALL_GOOD;
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
                    cleanupMoveResult(&chooseMoveResult);
                    return returnCode;
                }
                
                addObjectives(state, chosenObjectives, objectivesToKeep);
                
                cleanupMoveResult(&chooseMoveResult);
            }
            cardDrawnThisTurn = 0;
            break;
            
        case CHOOSE_OBJECTIVES:
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