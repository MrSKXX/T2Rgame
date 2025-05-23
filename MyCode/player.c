#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "player.h"
#include "gamestate.h"
#include "strategy.h"
#include "rules.h"

extern void checkObjectivesPaths(GameState* state);

void printCardName(CardColor card) {
    const char* cardNames[] = {"None", "Purple", "White", "Blue", "Yellow", 
                              "Orange", "Black", "Red", "Green", "Locomotive"};
    if (card >= 0 && card < 10) {
        printf("Card color: %s\n", cardNames[card]);
    } else {
        printf("Unknown card: %d\n", card);
    }
}

void printObjective(Objective objective) {
    printf("From city %d to city %d, score %d\n", 
           objective.from, objective.to, objective.score);
    printf("  From: ");
    printCity(objective.from);
    printf(" to ");
    printCity(objective.to);
    printf("\n");
}

void cleanupMoveResult(MoveResult *moveResult) {
    if (moveResult->opponentMessage) free(moveResult->opponentMessage);
    if (moveResult->message) free(moveResult->message);
    moveResult->opponentMessage = NULL;
    moveResult->message = NULL;
}

void initPlayer(GameState* state, StrategyType strategy, GameData* gameData) {
    if (!state || !gameData) {
        printf("Error: NULL state or gameData in initPlayer\n");
        return;
    }
    
    initGameState(state, gameData);
    
    printf("Player initialized with strategy type: %d\n", strategy);
    printf("Starting game with %d cities and %d tracks\n", state->nbCities, state->nbTracks);
    
    printf("Debug - Wagons left after init: %d\n", state->wagonsLeft);
    
    for (int i = 0; i < 4; i++) {
        if (gameData->cards[i] >= 0 && gameData->cards[i] < 10) {
            addCardToHand(state, gameData->cards[i]);
        } else {
            printf("Warning: Invalid card color: %d\n", gameData->cards[i]);
        }
    }
    
    printGameState(state);
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
        } else {
            printf("Objective %d: ", i+1);
            printObjective(myMoveResult.objectives[i]);
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

ResultCode playTurn(GameState* state, StrategyType strategy) {
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
        printf("Second card draw this turn\n");
        
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
            printf("Drawing second visible card: %d\n", cardToDraw);
        } else {
            myMove.action = DRAW_BLIND_CARD;
            printf("Drawing second blind card\n");
        }
        
        cardDrawnThisTurn = 0;
    } else {
        if (!decideNextMove(state, strategy, &myMove)) {
            printf("No specific move decided, drawing blind card\n");
            myMove.action = DRAW_BLIND_CARD;
        }
    }
    
    // Validation de sécurité pour CLAIM_ROUTE
    if (myMove.action == CLAIM_ROUTE) {
        CardColor color = myMove.claimRoute.color;
        
        if (color < PURPLE || color > LOCOMOTIVE) {
            printf("ERREUR CRITIQUE: Couleur invalide détectée: %d, correction à GREEN (8)\n", color);
            myMove.claimRoute.color = GREEN;
        }
        
        int routeIndex = findRouteIndex(state, myMove.claimRoute.from, myMove.claimRoute.to);
        if (routeIndex >= 0) {
            int length = state->routes[routeIndex].length;
            CardColor routeColor = state->routes[routeIndex].color;
            
            if (routeColor != LOCOMOTIVE && color != routeColor && 
                color != state->routes[routeIndex].secondColor && color != LOCOMOTIVE) {
                
                printf("CORRECTION: Couleur incorrecte pour route %d->%d (couleur choisie: %d, route: %d)\n", 
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
                    printf("ERREUR CRITIQUE: Pas assez de cartes pour cette route! Piochage à la place.\n");
                    myMove.action = DRAW_BLIND_CARD;
                }
            }
        }
    }
    
    printf("Action: ");
    switch (myMove.action) {
        case CLAIM_ROUTE:
            printf("Claim route %d -> %d (color %d, locos %d)\n", 
                   myMove.claimRoute.from, myMove.claimRoute.to, 
                   myMove.claimRoute.color, myMove.claimRoute.nbLocomotives);
            break;
        case DRAW_CARD:
            printf("Draw visible card %d\n", myMove.drawCard);
            break;
        case DRAW_BLIND_CARD:
            printf("Draw blind card\n");
            break;
        case DRAW_OBJECTIVES:
            printf("Draw objectives\n");
            break;
        default:
            printf("Unknown action %d\n", myMove.action);
    }

    // Vérification ultime pour CLAIM_ROUTE
    if (myMove.action == CLAIM_ROUTE) {
        if (myMove.claimRoute.from < 0 || myMove.claimRoute.from >= state->nbCities || 
            myMove.claimRoute.to < 0 || myMove.claimRoute.to >= state->nbCities) {
            printf("ERREUR FATALE: Tentative de prendre une route avec villes invalides: %d -> %d\n", 
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
                        printf("ERREUR FATALE: Tentative de prendre une route déjà prise: %d -> %d\n", 
                              myMove.claimRoute.from, myMove.claimRoute.to);
                        myMove.action = DRAW_BLIND_CARD;
                    }
                    break;
                }
            }
            
            if (!routeFound) {
                printf("ERREUR FATALE: Route inexistante: %d -> %d\n", 
                      myMove.claimRoute.from, myMove.claimRoute.to);
                myMove.action = DRAW_BLIND_CARD;
            }
        }
        
        if (myMove.claimRoute.color < PURPLE || myMove.claimRoute.color > LOCOMOTIVE) {
            printf("ERREUR FATALE: Couleur invalide: %d\n", myMove.claimRoute.color);
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
            printf("           RÉSULTAT FINAL DÉTECTÉ                 \n");
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
                printf("Route claimed successfully\n");
            } else {
                printf("WARNING: CLAIM_ROUTE not confirmed by server, state: %d\n", myMoveResult.state);
            }
            cardDrawnThisTurn = 0;
            break;
            
        case DRAW_CARD:
            addCardToHand(state, myMove.drawCard);
            printf("Card drawn successfully\n");
            
            if (myMove.drawCard == LOCOMOTIVE) {
                printf("Drew a locomotive - turn ends\n");
                cardDrawnThisTurn = 0;
            } else {
                if (cardDrawnThisTurn == 0) {
                    printf("Drew non-locomotive card - can draw second card\n");
                    cardDrawnThisTurn = myMoveResult.replay ? 1 : 0;
                } else {
                    printf("Drew second card - turn ends\n");
                    cardDrawnThisTurn = 0;
                }
            }
            break;
            
        case DRAW_BLIND_CARD:
            addCardToHand(state, myMoveResult.card);
            printf("Drew blind card: %d\n", myMoveResult.card);
            
            if (myMoveResult.card == LOCOMOTIVE && !myMoveResult.replay) {
                printf("Blind card is a locomotive that doesn't allow second draw\n");
                cardDrawnThisTurn = 0;
            } else {
                if (cardDrawnThisTurn == 0 && myMoveResult.replay) {
                    printf("Can draw a second card\n");
                    cardDrawnThisTurn = 1;
                } else {
                    printf("Turn ends\n");
                    cardDrawnThisTurn = 0;
                }
            }
            break;
            
        case DRAW_OBJECTIVES:
            printf("Received objectives to choose from\n");
            
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
            
            printf("Chose %d objectives\n", objectivesToKeep);
            cleanupMoveResult(&chooseMoveResult);
            
            cardDrawnThisTurn = 0;
            break;
    }
    
    if (myMove.action != DRAW_OBJECTIVES) {
        cleanupMoveResult(&myMoveResult);
    }
    
    updateCityConnectivity(state);
    
    if (cardDrawnThisTurn == 1) {
        printf("\nNow drawing second card\n");
        
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
            printf("Drawing second visible card: %d\n", cardToDraw);
        } else {
            secondCardMove.action = DRAW_BLIND_CARD;
            printf("Drawing second blind card\n");
        }
        
        returnCode = sendMove(&secondCardMove, &secondCardResult);
        
        if (returnCode != ALL_GOOD) {
            printf("Error drawing second card: 0x%x\n", returnCode);
            cleanupMoveResult(&secondCardResult);
            return returnCode;
        }
        
        if (secondCardMove.action == DRAW_CARD) {
            addCardToHand(state, secondCardMove.drawCard);
            printf("Second visible card drawn\n");
        } else {
            addCardToHand(state, secondCardResult.card);
            printf("Second blind card drawn: %d\n", secondCardResult.card);
        }
        
        cardDrawnThisTurn = 0;
        cleanupMoveResult(&secondCardResult);
    }
    
    return ALL_GOOD;
}