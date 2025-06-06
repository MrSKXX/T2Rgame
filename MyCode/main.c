#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "../tickettorideapi/ticketToRide.h"
#include "../tickettorideapi/clientAPI.h"
#include "gamestate.h"
#include "player.h"
#include "strategy.h"
#include "rules.h"

#define MAX_TURNS 200
#define NUMBER_OF_GAMES 3

typedef struct {
    int gameNumber;
    int finalScore;
    int wagonsLeft;
    int objectivesCompleted;
    int totalObjectives;
    char serverResults[2048];
    bool hasServerResults;
} GameResult;

void printGameResult(int gameNumber, int finalScore, char* finalResultsMessage, bool hasServerResults) {
    printf("\n=== GAME %d RESULTS ===\n", gameNumber);
    
    if (finalResultsMessage && strlen(finalResultsMessage) > 0) {
        printf("%s\n", finalResultsMessage);
    } else {
        printf("Could not retrieve results from server.\n");
    }
    
    printf("Our score: %d %s\n", finalScore, hasServerResults ? "[Server Results]" : "[Local Calculation]");
    printf("======================\n\n");
}

bool updateBoardState(GameState* gameState) {
    BoardState boardState;
    ResultCode result = getBoardState(&boardState);
    
    if (result != ALL_GOOD) return false;
    
    for (int i = 0; i < 5; i++) {
        gameState->visibleCards[i] = boardState.card[i];
    }
    
    return true;
}

bool isGameOver(char* message) {
    if (!message) return false;
    
    return (strstr(message, "Total score:") != NULL && strstr(message, "pts") != NULL) ||
           (strstr(message, "Georges:") != NULL && strstr(message, "PlayNice:") != NULL && strstr(message, "Objective") != NULL) ||
           (strstr(message, "Player Georges has the longest path") != NULL) ||
           (strstr(message, "Player PlayNice has the longest path") != NULL) ||
           (strstr(message, "✔Objective") != NULL && strstr(message, "✘Objective") != NULL) ||
           (strstr(message, "[getCGSMove]") != NULL);
}

int playOneGame(int gameNumber, GameResult* gameResult) {
    printf("\n========================================\n");
    printf("           STARTING GAME %d\n", gameNumber);
    printf("========================================\n");
    
    const char* serverAddress = "82.29.170.160";
    unsigned int serverPort = 15001;
    const char* playerName = "Georges123";
    
    ResultCode result = connectToCGS(serverAddress, serverPort, playerName);
    if (result != ALL_GOOD) {
        printf("Connection failed for game %d: 0x%x\n", gameNumber, result);
        return -1;
    }
    
    const char* gameSettings = "TRAINING NICE_BOT";
    GameData gameData;

    result = sendGameSettings("", &gameData);
    if (result != ALL_GOOD) {
        printf("Settings failed for game %d: 0x%x\n", gameNumber, result);
        return -1;
    }

    printf("Game %d started: %s, Seed: %d, Starter: %d\n", 
           gameNumber, gameData.gameName, gameData.gameSeed, gameData.starter);
    
    GameState gameState;
    initPlayer(&gameState, &gameData);

    if (gameData.starter == 0) {
        printf("We start game %d!\n", gameNumber);
        playFirstTurn(&gameState);
    }
    
    int turnCounter = 0;
    bool firstTurn = true;
    int consecutiveErrors = 0;
    char lastErrorMessage[2048] = {0};
    bool gameRunning = true;
    bool gameEnded = false;
    
    while (gameRunning && turnCounter < MAX_TURNS && !gameEnded) {
        turnCounter++;
        
        MoveData dummyMove;
        MoveResult currentResult = {0};
        ResultCode currentCode = getMove(&dummyMove, &currentResult);
        
        if (currentResult.state != 0) {
            printf("=== GAME %d ENDED (MoveState=%d) ===\n", gameNumber, currentResult.state);
            if (currentResult.message) {
                printf("Final server message:\n%s\n", currentResult.message);
                strncpy(lastErrorMessage, currentResult.message, sizeof(lastErrorMessage)-1);
            }
            cleanupMoveResult(&currentResult);
            gameRunning = false;
            gameEnded = true;
            break;
        }
        
        if (currentResult.message && isGameOver(currentResult.message)) {
            printf("=== GAME %d RESULTS RECEIVED ===\n", gameNumber);
            printf("%s\n", currentResult.message);
            strncpy(lastErrorMessage, currentResult.message, sizeof(lastErrorMessage)-1);
            cleanupMoveResult(&currentResult);
            gameRunning = false;
            gameEnded = true;
            break;
        }
        
        if (currentResult.message && 
            ((strstr(currentResult.message, "Georges:") != NULL && 
              strstr(currentResult.message, "PlayNice:") != NULL) ||
             strstr(currentResult.message, "Total score:") != NULL)) {
            
            printf("=== GAME %d RESULTS DETECTED - STOPPING ALL ACTIONS ===\n", gameNumber);
            printf("%s\n", currentResult.message);
            strncpy(lastErrorMessage, currentResult.message, sizeof(lastErrorMessage)-1);
            cleanupMoveResult(&currentResult);
            
            gameRunning = false;
            gameEnded = true;
            break;
        }
        
        bool itsOurTurn = false;
        
        if (currentCode == ALL_GOOD) {
            updateAfterOpponentMove(&gameState, &dummyMove);
            
            if (currentResult.message && 
                (strstr(currentResult.message, "[getCGSMove]") != NULL ||
                 strstr(currentResult.message, "Total score:") != NULL ||
                 strstr(currentResult.message, "✔Objective") != NULL ||
                 strstr(currentResult.message, "longest path") != NULL)) {
                
                printf("=== GAME %d RESULTS DETECTED IN OPPONENT MOVE ===\n", gameNumber);
                printf("%s\n", currentResult.message);
                strncpy(lastErrorMessage, currentResult.message, sizeof(lastErrorMessage)-1);
                cleanupMoveResult(&currentResult);
                
                gameRunning = false;
                gameEnded = true;
                break;
            }
            
            if (gameState.opponentWagonsLeft <= 0) {
                printf("Game %d: Opponent out of wagons - LAST TURN TRIGGERED\n", gameNumber);
                gameState.lastTurn = 1;
            } else if (gameState.opponentWagonsLeft <= 2) {
                gameState.lastTurn = 1;
                printf("Game %d: Last turn triggered by opponent (≤2 wagons)\n", gameNumber);
            }
            
            if (!currentResult.replay && !gameEnded) {
                itsOurTurn = true;
            }
        }
        else if (currentCode == SERVER_ERROR) {
            if (currentResult.message) {
                printf("Game %d server message: %s\n", gameNumber, currentResult.message);
                
                if (strstr(currentResult.message, "Bad protocol, should send 'WAIT_GAME") != NULL) {
                    printf("=== GAME %d PROTOCOL END MESSAGE ===\n", gameNumber);
                    strncpy(lastErrorMessage, currentResult.message, sizeof(lastErrorMessage)-1);
                    gameEnded = true;
                    cleanupMoveResult(&currentResult);
                    break;
                }
                
                if (isGameOver(currentResult.message)) {
                    printf("=== GAME %d FINAL RESULTS IN ERROR MESSAGE ===\n", gameNumber);
                    printf("%s\n", currentResult.message);
                    strncpy(lastErrorMessage, currentResult.message, sizeof(lastErrorMessage)-1);
                    cleanupMoveResult(&currentResult);
                    
                    gameRunning = false;
                    gameEnded = true;
                    break;
                }
                
                if (strstr(currentResult.message, "It's our turn")) {
                    itsOurTurn = true;
                }
            }
        }
        
        cleanupMoveResult(&currentResult);
        
        if (itsOurTurn && !gameEnded && gameRunning) {
            updateBoardState(&gameState);
            
            if (gameState.wagonsLeft <= 0) {
                printf("Game %d: We are out of wagons - LAST TURN TRIGGERED\n", gameNumber);
                gameState.lastTurn = 1;
            }
            
            if (turnCounter % 10 == 0 || gameState.lastTurn) {
                printf("Game %d Turn %d - Wagons: Us=%d, Opp=%d%s\n", 
                       gameNumber, turnCounter, gameState.wagonsLeft, gameState.opponentWagonsLeft,
                       gameState.lastTurn ? " [LAST TURN]" : "");
            }
            
            if (strlen(lastErrorMessage) > 0 && 
                (strstr(lastErrorMessage, "Total score:") != NULL ||
                 strstr(lastErrorMessage, "Georges:") != NULL)) {
                printf("=== GAME %d RESULTS ALREADY SEEN - NOT PLAYING ===\n", gameNumber);
                gameRunning = false;
                gameEnded = true;
                break;
            }
            
            ResultCode playCode;
            
            if (firstTurn) {
                playCode = playFirstTurn(&gameState);
                if (playCode == ALL_GOOD) {
                    firstTurn = false;
                }
            } else {
                playCode = playTurn(&gameState);
            }
            
            if (playCode == ALL_GOOD) {
                consecutiveErrors = 0;
                
                if (gameState.lastTurn == 2) {
                    printf("=== GAME %d ENDED DURING OUR TURN ===\n", gameNumber);
                    gameRunning = false;
                    gameEnded = true;
                    break;
                }
                
                if (gameState.wagonsLeft <= 2) {
                    gameState.lastTurn = 1;
                    printf("Game %d: Last turn triggered by us (≤2 wagons)\n", gameNumber);
                }
                
                if (gameState.wagonsLeft <= 0) {
                    printf("Game %d: We exhausted wagons - staying in loop to catch results\n", gameNumber);
                    MoveResult immediateCheck = {0};
                    ResultCode immediateCode = getMove(&dummyMove, &immediateCheck);
                    
                    if (immediateCheck.state != 0) {
                        printf("=== GAME %d ENDED IMMEDIATELY AFTER WAGON EXHAUSTION ===\n", gameNumber);
                        if (immediateCheck.message) {
                            printf("%s\n", immediateCheck.message);
                            strncpy(lastErrorMessage, immediateCheck.message, sizeof(lastErrorMessage)-1);
                        }
                        cleanupMoveResult(&immediateCheck);
                        gameRunning = false;
                        gameEnded = true;
                        break;
                    }
                    
                    if (immediateCheck.message && isGameOver(immediateCheck.message)) {
                        printf("=== GAME %d RESULTS CAPTURED AFTER WAGON EXHAUSTION ===\n", gameNumber);
                        printf("%s\n", immediateCheck.message);
                        strncpy(lastErrorMessage, immediateCheck.message, sizeof(lastErrorMessage)-1);
                        cleanupMoveResult(&immediateCheck);
                        gameRunning = false;
                        gameEnded = true;
                        break;
                    }
                    
                    cleanupMoveResult(&immediateCheck);
                }
            } 
            else {
                printf("Play error in game %d: 0x%x\n", gameNumber, playCode);
                
                MoveResult errorResult = {0};
                ResultCode errorCode = getMove(&dummyMove, &errorResult);
                
                if (errorResult.state != 0) {
                    printf("=== GAME %d ENDED AFTER PLAY ERROR (state=%d) ===\n", gameNumber, errorResult.state);
                    if (errorResult.message) {
                        printf("%s\n", errorResult.message);
                        strncpy(lastErrorMessage, errorResult.message, sizeof(lastErrorMessage)-1);
                    }
                    cleanupMoveResult(&errorResult);
                    gameRunning = false;
                    gameEnded = true;
                    break;
                }
                
                if (errorResult.message) {
                    if (strstr(errorResult.message, "Bad protocol") && 
                        strstr(errorResult.message, "WAIT_GAME")) {
                        printf("=== GAME %d PROTOCOL ERROR - STAYING IN LOOP FOR RESULTS ===\n", gameNumber);
                        printf("Server says: %s\n", errorResult.message);
                        cleanupMoveResult(&errorResult);
                        
                        consecutiveErrors++;
                        if (consecutiveErrors > 10) {
                            printf("Game %d: Too many protocol errors, ending\n", gameNumber);
                            gameRunning = false;
                        }
                        continue;
                    }
                    
                    if (isGameOver(errorResult.message)) {
                        printf("=== GAME %d COMPLETE RESULTS RECEIVED ===\n", gameNumber);
                        printf("%s\n", errorResult.message);
                        strncpy(lastErrorMessage, errorResult.message, sizeof(lastErrorMessage)-1);
                        cleanupMoveResult(&errorResult);
                        
                        gameRunning = false;
                        gameEnded = true;
                        break;
                    }
                    
                    strncpy(lastErrorMessage, errorResult.message, sizeof(lastErrorMessage)-1);
                }
                
                cleanupMoveResult(&errorResult);
                
                consecutiveErrors++;
                if (consecutiveErrors > 10) {
                    printf("Game %d: Too many consecutive errors, ending game\n", gameNumber);
                    gameRunning = false;
                }
            }
        }
        
        if (turnCounter >= MAX_TURNS - 5) {
            printf("Game %d: Approaching max turns, waiting a bit more for server results...\n", gameNumber);
        }
        
        if (turnCounter >= MAX_TURNS) {
            printf("Game %d: Max turns reached, ending game\n", gameNumber);
            gameRunning = false;
        }
    }
    
    printf("\n===== GAME %d OVER =====\n", gameNumber);
    int finalScore = calculateScore(&gameState);
    
    int completedObjectives = 0;
    for (int i = 0; i < gameState.nbObjectives; i++) {
        if (isObjectiveCompleted(&gameState, gameState.objectives[i])) {
            completedObjectives++;
        }
    }
    
    char finalResultsMessage[2048] = {0};
    bool hasServerResults = false;
    
    if (strlen(lastErrorMessage) > 0) {
        strncpy(finalResultsMessage, lastErrorMessage, sizeof(finalResultsMessage)-1);
        hasServerResults = true;
    } else {
        snprintf(finalResultsMessage, sizeof(finalResultsMessage),
                "Game %d final score: %d\nWagons left: %d\nObjectives completed: %d/%d\n",
                gameNumber, finalScore, gameState.wagonsLeft, 
                completedObjectives, gameState.nbObjectives);
        hasServerResults = false;
    }
    
    printGameResult(gameNumber, finalScore, finalResultsMessage, hasServerResults);
    
    gameResult->gameNumber = gameNumber;
    gameResult->finalScore = finalScore;
    gameResult->wagonsLeft = gameState.wagonsLeft;
    gameResult->objectivesCompleted = completedObjectives;
    gameResult->totalObjectives = gameState.nbObjectives;
    gameResult->hasServerResults = hasServerResults;
    
    if (hasServerResults) {
        strncpy(gameResult->serverResults, lastErrorMessage, sizeof(gameResult->serverResults)-1);
    } else {
        snprintf(gameResult->serverResults, sizeof(gameResult->serverResults),
                "Game %d final score: %d\nWagons left: %d\nObjectives: %d/%d",
                gameNumber, finalScore, gameState.wagonsLeft, 
                completedObjectives, gameState.nbObjectives);
    }
    
    if (gameData.gameName) free(gameData.gameName);
    if (gameData.trackData) free(gameData.trackData);
    
    printf("Game %d: Disconnecting...\n", gameNumber);
    quitGame();
    
    printf("Game %d completed successfully!\n\n", gameNumber);
    
    sleep(1);
    
    return 0;
}

int main() {
    printf("=== Ticket to Ride AI - Multi-Game Session ===\n");
    printf("Playing %d games against NICE_BOT\n\n", NUMBER_OF_GAMES);
    
    GameResult gameResults[NUMBER_OF_GAMES];
    int successfulGames = 0;
    
    for (int gameNum = 1; gameNum <= NUMBER_OF_GAMES; gameNum++) {
        int result = playOneGame(gameNum, &gameResults[successfulGames]);
        
        if (result == 0) {
            successfulGames++;
            printf("✓ Game %d completed successfully\n", gameNum);
        } else {
            printf("✗ Game %d failed\n", gameNum);
        }
        
        printf("\n");
    }
    
    printf("\n========================================\n");
    printf("           FINAL SESSION SUMMARY\n");
    printf("========================================\n");
    printf("Games completed: %d/%d\n\n", successfulGames, NUMBER_OF_GAMES);
    
    if (successfulGames > 0) {
        int totalScore = 0;
        int totalObjectives = 0;
        int totalObjectivesCompleted = 0;
        int gamesWithServerResults = 0;
        
        for (int i = 0; i < successfulGames; i++) {
            GameResult* gr = &gameResults[i];
            
            printf("Game %d: Score=%d, Objectives=%d/%d, Wagons left=%d %s\n", 
                   gr->gameNumber, gr->finalScore, gr->objectivesCompleted,
                   gr->totalObjectives, gr->wagonsLeft,
                   gr->hasServerResults ? "[Server Results]" : "[Local Only]");
            
            totalScore += gr->finalScore;
            totalObjectivesCompleted += gr->objectivesCompleted;
            totalObjectives += gr->totalObjectives;
            
            if (gr->hasServerResults) {
                gamesWithServerResults++;
            }
        }
        
        printf("\n--- AVERAGES ---\n");
        printf("Average score: %.1f\n", (float)totalScore / successfulGames);
        printf("Average objectives completed: %.1f/%.1f\n", 
               (float)totalObjectivesCompleted / successfulGames,
               (float)totalObjectives / successfulGames);
        printf("Objective completion rate: %.1f%%\n", 
               ((float)totalObjectivesCompleted / totalObjectives) * 100);
        printf("Server results captured: %d/%d games (%.1f%%)\n", 
               gamesWithServerResults, successfulGames,
               (float)gamesWithServerResults / successfulGames * 100);
    }
    
    printf("\nSession completed!\n");
    return 0;
}