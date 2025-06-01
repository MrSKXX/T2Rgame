#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../tickettorideapi/ticketToRide.h"
#include "../tickettorideapi/clientAPI.h"
#include "gamestate.h"
#include "player.h"
#include "strategy/strategy.h"
#include "rules.h"

#define MAX_TURNS 200

void printGameResult(int finalScore, char* finalResultsMessage) {
    printf("\n=== FINAL RESULTS ===\n");
    
    if (finalResultsMessage && strlen(finalResultsMessage) > 0) {
        printf("%s\n", finalResultsMessage);
    } else {
        printf("Could not retrieve results from server.\n");
    }
    
    printf("Our score: %d\n", finalScore);
    printf("====================\n\n");
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
    
    return (strstr(message, "winner") != NULL || 
            strstr(message, "Total score") != NULL || 
            strstr(message, "GAME OVER") != NULL ||
            strstr(message, "last turn has ended") != NULL ||
            strstr(message, "won") != NULL ||
            strstr(message, "Final Score") != NULL);
}

int main() {
    const char* serverAddress = "82.29.170.160";
    unsigned int serverPort = 15001;
    const char* playerName = "Georges"; 
    
    printf("=== Ticket to Ride AI - %s ===\n", playerName);
    
    // Connexion
    ResultCode result = connectToCGS(serverAddress, serverPort, playerName);
    if (result != ALL_GOOD) {
        printf("Connection failed: 0x%x\n", result);
        return 1;
    }
    
    // Configuration
    const char* gameSettings = "TRAINING NICE_BOT";
    GameData gameData;

    result = sendGameSettings(gameSettings, &gameData);
    if (result != ALL_GOOD) {
        printf("Settings failed: 0x%x\n", result);
        return 1;
    }

    printf("Game started: %s, Seed: %d, Starter: %d\n", 
           gameData.gameName, gameData.gameSeed, gameData.starter);
    
    // Initialisation
    GameState gameState;
    
    initPlayer(&gameState, &gameData);

    if (gameData.starter == 0) {
        printf("We start the game!\n");
        playFirstTurn(&gameState);
    }

    printBoard();
    
    // Variables
    int turnCounter = 0;
    bool firstTurn = true;
    int consecutiveErrors = 0;
    char lastErrorMessage[2048] = {0};
    bool gameRunning = true;
    
    // Boucle principale
    while (gameRunning && turnCounter < MAX_TURNS) {
        turnCounter++;
        
        MoveData dummyMove;
        MoveResult currentResult = {0};
        ResultCode currentCode = getMove(&dummyMove, &currentResult);
        
        // Détecter fin de partie
        if (currentResult.message && isGameOver(currentResult.message)) {
            printf("Game over: %s\n", currentResult.message);
            strncpy(lastErrorMessage, currentResult.message, sizeof(lastErrorMessage)-1);
            cleanupMoveResult(&currentResult);
            break;
        }
        
        bool itsOurTurn = false;
        
        if (currentCode == ALL_GOOD) {
            updateAfterOpponentMove(&gameState, &dummyMove);
            
            if (gameState.opponentWagonsLeft <= 2) {
                gameState.lastTurn = 1;
            }
            
            if (!currentResult.replay) {
                itsOurTurn = true;
            }
        }
        else if (currentCode == SERVER_ERROR && currentResult.message && 
                 strstr(currentResult.message, "It's our turn")) {
            itsOurTurn = true;
        }
        
        cleanupMoveResult(&currentResult);
        
        // Notre tour
        if (itsOurTurn) {
            updateBoardState(&gameState);
            
            // Log seulement les info importantes
            if (turnCounter % 5 == 0 || gameState.lastTurn) {
                printf("Turn %d - Wagons: Us=%d, Opp=%d\n", 
                       turnCounter, gameState.wagonsLeft, gameState.opponentWagonsLeft);
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
                
                if (gameState.wagonsLeft <= 2) {
                    gameState.lastTurn = 1;
                }
            } 
            else {
                MoveResult errorResult = {0};
                getMove(&dummyMove, &errorResult);
                
                if (errorResult.message) {
                    if (isGameOver(errorResult.message)) {
                        strncpy(lastErrorMessage, errorResult.message, sizeof(lastErrorMessage)-1);
                        cleanupMoveResult(&errorResult);
                        break;
                    }
                    strncpy(lastErrorMessage, errorResult.message, sizeof(lastErrorMessage)-1);
                }
                
                cleanupMoveResult(&errorResult);
                
                consecutiveErrors++;
                if (consecutiveErrors > 3) {
                    gameRunning = false;
                }
            }
            
            // Log score seulement périodiquement
            if (turnCounter % 10 == 0 || gameState.lastTurn) {
                int currentScore = calculateScore(&gameState);
                printf("Score: %d\n", currentScore);
            }
        }
    }
    
    // Fin de partie
    printf("\n===== GAME OVER =====\n");
    int finalScore = calculateScore(&gameState);
    
    printBoard();
    
    int completedObjectives = 0;
    for (int i = 0; i < gameState.nbObjectives; i++) {
        if (isObjectiveCompleted(&gameState, gameState.objectives[i])) {
            completedObjectives++;
        }
    }
    
    char finalResultsMessage[2048] = {0};
    
    if (strlen(lastErrorMessage) > 0 && 
        (strstr(lastErrorMessage, "score") || 
         strstr(lastErrorMessage, "winner") || 
         strstr(lastErrorMessage, "won") || 
         strstr(lastErrorMessage, "Total"))) {
        
        strncpy(finalResultsMessage, lastErrorMessage, sizeof(finalResultsMessage)-1);
    }
    else {
        snprintf(finalResultsMessage, sizeof(finalResultsMessage),
                "Final score: %d\nWagons left: %d\nObjectives completed: %d/%d\n",
                finalScore, gameState.wagonsLeft, 
                completedObjectives, gameState.nbObjectives);
    }
    
    printGameResult(finalScore, finalResultsMessage);
    
    if (gameData.gameName) free(gameData.gameName);
    if (gameData.trackData) free(gameData.trackData);
    
    quitGame();
    
    return 0;
}