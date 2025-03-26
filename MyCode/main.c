#define JSMN_STATIC  
#include "../tickettorideapi/codingGameServer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // Pour sleep()
#include "../tickettorideapi/ticketToRide.h"
// gcc -o client main.c game_state.c move_logic.c path_finding.c strategy.c utils.c ../tickettorideapi/codingGameServer.c ../tickettorideapi/ticketToRide.c -I../tickettorideapi

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

// Fonction pour libérer la mémoire allouée dans MoveResult
void freeMoveResultMemory(MoveResult *moveResult) {
    if (moveResult->opponentMessage) free(moveResult->opponentMessage);
    if (moveResult->message) free(moveResult->message);
}

int main() {
    const char* serverAddress = "82.64.1.174";
    unsigned int serverPort = 15001;
    const char* playerName = "George0";
    
    printf("Attempting to connect to server at %s:%d\n", serverAddress, serverPort);
    
    // Connect to the server
    ResultCode result = connectToCGS(serverAddress, serverPort);
    if (result != ALL_GOOD) {
        printf("Failed to connect to server. Error code: 0x%x\n", result);
        return 1;
    }
    printf("Successfully connected to server!\n");
    
    // Send player name
    result = sendName(playerName);
    if (result != ALL_GOOD) {
        printf("Failed to send name to server. Error code: 0x%x\n", result);
        return 1;
    }
    printf("Successfully sent player name: %s\n", playerName);
    
    // Configure game settings
    GameSettings gameSettings = GameSettingsDefaults;
    
    // Prepare to receive game data
    GameData gameData = GameDataDefaults;
    
    // Send game settings and get game data
    result = sendGameSettings(gameSettings, &gameData);
    if (result != ALL_GOOD) {
        printf("Failed to send game settings. Error code: 0x%x\n", result);
        return 1;
    }
    
    printf("Successfully started game: %s\n", gameData.gameName);
    printf("Game seed: %d\n", gameData.gameSeed);
    printf("Starter: %d (1 = you, 2 = opponent)\n", gameData.starter);
    
    // Print initial board state
    printBoard();
    
    // Main game variables
    ResultCode returnCode;
    MoveData opponentMove;
    MoveResult opponentMoveResult;
    BoardState boardState;
    int gameRunning = 1;
    int firstMove = 1;
    
    // Main game loop
    while (gameRunning) {
        // Print current board state
        printBoard();
        
        // Get board state
        returnCode = getBoardState(&boardState);
        if (returnCode != ALL_GOOD) {
            printf("Error getting board state: 0x%x\n", returnCode);
            break;
        }
        
        // Try to get opponent's move
        returnCode = getMove(&opponentMove, &opponentMoveResult);
        
        // Handle our turn
        if (returnCode == OTHER_ERROR) {
            printf("It's our turn to play\n");
            
            // First move - draw objectives
            if (firstMove) {
                printf("First move: drawing objectives\n");
                MoveData myMove;
                MoveResult myMoveResult;
                
                myMove.action = DRAW_OBJECTIVES;
                
                // Send move
                returnCode = sendMove(&myMove, &myMoveResult);
                
                if (returnCode != ALL_GOOD) {
                    printf("Error sending move: 0x%x\n", returnCode);
                    gameRunning = 0;
                    continue;
                }
                
                printf("Received objectives, now choosing which to keep\n");
                
                // Display objectives
                for (int i = 0; i < 3; i++) {
                    printf("Objective %d: ", i+1);
                    printObjective(myMoveResult.objectives[i]);
                }
                
                // Free memory
                freeMoveResultMemory(&myMoveResult);
                
                // Choose objectives
                MoveData chooseMove;
                MoveResult chooseMoveResult;
                
                chooseMove.action = CHOOSE_OBJECTIVES;
                chooseMove.chooseObjectives[0] = true;
                chooseMove.chooseObjectives[1] = true;
                chooseMove.chooseObjectives[2] = true;
                
                // Send choice
                returnCode = sendMove(&chooseMove, &chooseMoveResult);
                
                if (returnCode != ALL_GOOD) {
                    printf("Error choosing objectives: 0x%x\n", returnCode);
                    gameRunning = 0;
                } else {
                    printf("Successfully chose objectives\n");
                    firstMove = 0;
                }
                
                // Free memory
                freeMoveResultMemory(&chooseMoveResult);
            } else {
                // Regular turn - draw blind card
                MoveData myMove;
                MoveResult myMoveResult;
                
                myMove.action = DRAW_BLIND_CARD;
                
                // Send move
                returnCode = sendMove(&myMove, &myMoveResult);
                
                if (returnCode != ALL_GOOD) {
                    printf("Error sending move: 0x%x\n", returnCode);
                    gameRunning = 0;
                } else {
                    printf("Successfully drew a blind card: %d\n", myMoveResult.card);
                    printCardName(myMoveResult.card);
                }
                
                // Free memory
                freeMoveResultMemory(&myMoveResult);
            }
        }
        // Handle opponent's move
        else if (returnCode == ALL_GOOD) {
            printf("Opponent made a move of type: %d\n", opponentMove.action);
            
            // Process opponent's move by type
            switch (opponentMove.action) {
                case CLAIM_ROUTE:
                    printf("Opponent claimed route from %d to %d with color %d\n", 
                           opponentMove.claimRoute.from, 
                           opponentMove.claimRoute.to,
                           opponentMove.claimRoute.color);
                    break;
                case DRAW_CARD:
                    printf("Opponent drew visible card: %d\n", opponentMove.drawCard);
                    printCardName(opponentMove.drawCard);
                    break;
                case DRAW_BLIND_CARD:
                    printf("Opponent drew a blind card\n");
                    break;
                case DRAW_OBJECTIVES:
                    printf("Opponent drew objectives\n");
                    break;
                case CHOOSE_OBJECTIVES:
                    printf("Opponent chose objectives\n");
                    break;
                default:
                    printf("Unknown move type: %d\n", opponentMove.action);
            }
            
            // Free memory
            freeMoveResultMemory(&opponentMoveResult);
        }
        // Handle game end
        else if (returnCode == SERVER_ERROR || returnCode == PARAM_ERROR) {
            printf("Game has ended with code: 0x%x\n", returnCode);
            gameRunning = 0;
        }
        // Handle unexpected errors
        else {
            printf("Unexpected error: 0x%x\n", returnCode);
            gameRunning = 0;
        }
        
        // Small pause to avoid server overload
        sleep(1);
    }
    
    // Clean up allocated memory
    if (gameData.gameName) free(gameData.gameName);
    if (gameData.trackData) free(gameData.trackData);
    
    // Quit the game
    quitGame();
    
    return 0;
}