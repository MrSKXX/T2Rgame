#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // Pour sleep()
#include "../tickettorideapi/ticketToRide.h"
#include "../tickettorideapi/clientAPI.h"
#include "gamestate.h"
#include "player.h"
#include "strategy.h"
#include "rules.h"
#include "manual.h"

// Définition du mode de jeu (0 = IA, 1 = manuel)
#define MANUAL_MODE 0

int main() {
    const char* serverAddress = "82.29.170.160";
    unsigned int serverPort = 15001;
    const char* playerName = "Georges2222"; 
    
    printf("Ticket to Ride AI Player - %s\n", playerName);
    printf("Attempting to connect to server at %s:%d\n", serverAddress, serverPort);
    
    // Connect to the server
    ResultCode result = connectToCGS(serverAddress, serverPort, playerName);
    if (result != ALL_GOOD) {
        printf("Failed to connect to server. Error code: 0x%x\n", result);
        return 1;
    }
    printf("Successfully connected to server!\n");
    
    // Configure game settings
    const char* gameSettings = "TRAINING NICE_BOT"; // Par exemple, jouer contre NICE_BOT
    
    // Prepare to receive game data
    GameData gameData;
    
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
    
    // Initialiser notre état de jeu et stratégie
    GameState gameState;
    StrategyType strategy = STRATEGY_BASIC;
    
    initPlayer(&gameState, strategy, &gameData);
    
    // Main game variables
    ResultCode returnCode;
    MoveData opponentMove;
    MoveResult opponentMoveResult;
    int gameRunning = 1;
    int firstMove = 1;
    int ourTurn = 0; // Par défaut, supposons que c'est le tour de l'adversaire
    
    // Main game loop
    while (gameRunning) {
        // Print current board state
        printBoard();
        
        // Get board state to update our visible cards
        BoardState boardState;
        returnCode = getBoardState(&boardState);
        if (returnCode != ALL_GOOD) {
            printf("Error getting board state: 0x%x\n", returnCode);
            break;
        }
        
        // Update our visible cards
        for (int i = 0; i < 5; i++) {
            gameState.visibleCards[i] = boardState.card[i];
        }
        
        // Incrémentation et affichage du compteur de tours
        gameState.turnCount++;
        printf("\n===== TOUR N°%d =====\n", gameState.turnCount);
        printf("Wagons restants - Nous: %d, Adversaire: %d\n", 
               gameState.wagonsLeft, gameState.opponentWagonsLeft);
               
        // Déterminer à qui c'est le tour
        if (!ourTurn) {
            // Essayer de récupérer le coup de l'adversaire
            returnCode = getMove(&opponentMove, &opponentMoveResult);
            
            if (returnCode == ALL_GOOD) {
                // C'était bien le tour de l'adversaire
                printf("Opponent made a move of type: %d\n", opponentMove.action);
                
                // Update our game state based on opponent's move
                updateAfterOpponentMove(&gameState, &opponentMove);
                
                // Process opponent's move by type for logging
                switch (opponentMove.action) {
                    case CLAIM_ROUTE:
                        printf("Opponent claimed route from %d to %d with color %d\n", 
                               opponentMove.claimRoute.from, 
                               opponentMove.claimRoute.to,
                               opponentMove.claimRoute.color);
                        break;
                    case DRAW_CARD:
                        printf("Opponent drew visible card: %d\n", opponentMove.drawCard);
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
                if (opponentMoveResult.opponentMessage) free(opponentMoveResult.opponentMessage);
                if (opponentMoveResult.message) free(opponentMoveResult.message);
                
                // Maintenant c'est notre tour (sauf si l'adversaire rejoue)
                ourTurn = !opponentMoveResult.replay;
            } 
            else if (returnCode == OTHER_ERROR) {
                // Erreur lors de la récupération du coup de l'adversaire
                // Cela signifie probablement que c'est à notre tour de jouer
                printf("Error getting opponent move: seems like it's our turn\n");
                ourTurn = 1;
            }
            else {
                // Autre erreur - peut-être fin de partie
                printf("Error getting opponent move: 0x%x\n", returnCode);
                gameRunning = 0;
                continue;
            }
        }
        
        // Notre tour de jouer
        if (ourTurn) {
            printf("It's our turn to play\n");
            
            // Choix du mode de jeu
            if (MANUAL_MODE) {
                // Mode manuel
                if (firstMove) {
                    returnCode = playManualFirstTurn(&gameState);
                    firstMove = 0;
                } else {
                    returnCode = playManualTurn(&gameState);
                }
                
                if (returnCode != ALL_GOOD) {
                    // Vérifier si c'est un message de fin de partie
                    printf("Error in manual turn: 0x%x\n", returnCode);
                    gameRunning = 0;
                    continue;
                }
            } else {
                // Mode IA automatique (code existant)
                if (firstMove) {
                    returnCode = playFirstTurn(&gameState);
                    
                    if (returnCode != ALL_GOOD) {
                        printf("Error in first turn: 0x%x\n", returnCode);
                        gameRunning = 0;
                        continue;
                    }
                    
                    firstMove = 0;
                } else {
                    // Regular turn
                    returnCode = playTurn(&gameState, strategy);
                    
                    if (returnCode != ALL_GOOD) {
                        printf("Error in turn: 0x%x\n", returnCode);
                        gameRunning = 0;
                        continue;
                    }
                }
            }
            
            // Check if it's the last turn
            if (isLastTurn(&gameState)) {
                printf("LAST TURN: We have <= 2 wagons left\n");
                gameState.lastTurn = 1;
            }
            
            // C'est maintenant le tour de l'adversaire
            ourTurn = 0;
        }
        
        // Print current game state for debugging
        if (MANUAL_MODE) {
            printManualGameState(&gameState);
        } else {
            printGameState(&gameState);
        }
        
        // Calculate current score
        int currentScore = calculateScore(&gameState);
        printf("Current score: %d\n", currentScore);
        
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