/*
 * main
 * 
 * 
 * Structure globale:
 * 1) Initialisation:
 *    - Connexion au serveur
 *    - Envoi du nom du joueur
 *    - Configuration des paramètres du jeu
 *    - Initialisation de l'état du jeu et de la stratégie
 * 
 * 2) Boucle principale:
 *    - Récupération de l'état du plateau
 *    - Alternance entre notre tour et celui de l'adversaire
 *    - Notre tour: appel à playTurn ou playManualTurn selon le mode
 *    - Tour adverse: mise à jour de notre état d'après le coup adverse
 *    - Détection de la fin de partie
 * 
 * 3) Fin de partie:
 *    - Affichage du plateau final
 *    - Calcul et affichage du score final
 *    - Libération des ressources
 *    - Déconnexion du serveur
 * 
 * Cette fonction utilise une constante MANUAL_MODE pour basculer entre
 * le mode IA et le mode manuel, permettant de tester les deux approches.
 */

#define JSMN_STATIC  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // Pour sleep()
#include "../tickettorideapi/codingGameServer.h"
#include "../tickettorideapi/ticketToRide.h"
#include "gamestate.h"
#include "player.h"
#include "strategy.h"
#include "rules.h"   // Pour isLastTurn et calculateScore
#include "manual.h"  // Inclusion du nouveau module pour le mode manuel

// Définition du mode de jeu (0 = IA, 1 = manuel)
#define MANUAL_MODE 0

int main() {
    const char* serverAddress = "82.64.1.174";
    unsigned int serverPort = 15001;
    const char* playerName = "Georges2222"; 
    
    printf("Ticket to Ride AI Player - %s\n", playerName);
    printf("Attempting to connect to server at %s:%d\n", serverAddress, serverPort);
    
    // Connect to the servers
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
        
        // Try to get opponent's move
        returnCode = getMove(&opponentMove, &opponentMoveResult);
        
        // Handle our turn
        if (returnCode == OTHER_ERROR) {
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
        }
        // Handle opponent's move
        else if (returnCode == ALL_GOOD) {
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
            cleanupMoveResult(&opponentMoveResult);
        }
        // Handle game end
        else if (returnCode == SERVER_ERROR || returnCode == PARAM_ERROR) {
            printf("Game has ended with code: 0x%x\n", returnCode);
            
            // Vérifier si c'est une fin normale (annonce d'un gagnant)
            if (opponentMoveResult.message && strstr(opponentMoveResult.message, "winner")) {
                char winner[100] = "unknown";
                sscanf(opponentMoveResult.message, "{\"state\": 1, \"winner\": \"%99[^\"]\"}", winner);
                printf("GAME OVER - Partie terminée normalement après %d tours\n", gameState.turnCount);
                printf("Le gagnant est: %s\n", winner);
            }
            
            printf("Final board state:\n");
            printBoard();
            printf("Final game state:\n");
            printGameState(&gameState);
            
            // Calculate final score
            int finalScore = calculateScore(&gameState);
            printf("Final score: %d\n", finalScore);
            printf("Partie terminée en %d tours.\n", gameState.turnCount);
            
            // Free memory
            cleanupMoveResult(&opponentMoveResult);
            
            gameRunning = 0;
        }
        // Handle unexpected errors
        else {
            printf("Unexpected error: 0x%x\n", returnCode);
            
            // Check if we received any error message from the server
            if (opponentMoveResult.message) {
                printf("Server message: %s\n", opponentMoveResult.message);
                cleanupMoveResult(&opponentMoveResult);
            }
            
            // If it's a serious error, exit the game
            if (returnCode != OTHER_ERROR) {
                gameRunning = 0;
            }
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